/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "ForwardLightingPass.h"


#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Textures/BlueNoiseTexture.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "glm/gtc/random.hpp"

#include "glm/gtx/string_cast.hpp"

const RenderPass::Info ForwardLightingPass::kInfo
{
    "ForwardLightingPass",

    "Computes direct and indirect illumination and applies shadows for the current scene (if visibility map is provided).\n"
    "The pass can output the world-space normals and screen-space motion vectors, both are optional."
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(ForwardLightingPass::kInfo, ForwardLightingPass::create);
}

namespace {
    const char kShaderFile[] = "RenderPasses/ForwardLightingPass/ForwardLightingPass.3d.slang";

    const std::string kDepth = "depth";
    const std::string kColor = "color";

    const ChannelList kForwardLightingPassExtraChannels = {
        { "posW",             "gOutPosition",       "World position buffer",         true /* optional */, ResourceFormat::RGBA32Float },
        { "albedo",           "gOutAlbedo",         "Albedo color buffer",           true /* optional */, ResourceFormat::RGBA16Float },
        { "normals",          "gOutNormals",        "Normals buffer",                true /* optional */, ResourceFormat::RGBA16Float },
        { "shadows",          "gOutShadows",        "Shadows buffer",                true /* optional */, ResourceFormat::RGBA16Float },
        { "motion_vecs",      "gOutMotionVecs",     "Motion vectors buffer",         true /* optional */, ResourceFormat::RG16Float },
    };

    const std::string kVisBuffer = "visibilityBuffer";

    const std::string kFrameSampleCount = "frameSampleCount";
    const std::string kSuperSampleCount = "superSampleCount";
    const std::string kSuperSampling = "enableSuperSampling";
}

ForwardLightingPass::SharedPtr ForwardLightingPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new ForwardLightingPass(pRenderContext->device()));
    pThis->setColorFormat(ResourceFormat::RGBA32Float).setSuperSampleCount(1).usePreGeneratedDepthBuffer(true);
        //.setMotionVecFormat(ResourceFormat::RG16Float).setNormalMapFormat(ResourceFormat::RGBA8Unorm).setSuperSampleCount(1).usePreGeneratedDepthBuffer(true);

    for (const auto& [key, value] : dict) {
        if (key == kSuperSampleCount) pThis->setSuperSampleCount(value);
        else if (key == kFrameSampleCount) pThis->setFrameSampleCount(value);
        else if (key == kSuperSampling) pThis->setSuperSampling(value);
        else LLOG_WRN << "Unknown field '" << key << "' in a ForwardLightingPass dictionary";
    }

    return pThis;
}

Dictionary ForwardLightingPass::getScriptingDictionary() {
    Dictionary d;
    d[kFrameSampleCount] = mFrameSampleCount;
    d[kSuperSampleCount] = mSuperSampleCount;
    d[kSuperSampling] = mEnableSuperSampling;
    return d;
}

ForwardLightingPass::ForwardLightingPass(Device::SharedPtr pDevice): RenderPass(pDevice, kInfo) {
    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::createFromFile(pDevice, kShaderFile, "vsMain", "psMain");
    pProgram->addDefine("_MS_DISABLE_ALPHA_TEST", "");
    pProgram->removeDefine("_MS_DISABLE_ALPHA_TEST"); // TODO: move to execute
    pProgram->addDefine("ENABLE_DEFERED_AO", "1");

    // Create a GPU sample generator.
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
    assert(mpSampleGenerator);
    pProgram->addDefines(mpSampleGenerator->getDefines());

    mpState = GraphicsState::create(pDevice);
    mpState->setProgram(pProgram);

    mpFbo = Fbo::create(pDevice);

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::Equal);
    mpDsNoDepthWrite = DepthStencilState::create(dsDesc);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
      .setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);
    mpNoiseSampler = Sampler::create(pDevice, samplerDesc);

    mSampleNumber = 0;
}

RenderPassReflection ForwardLightingPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;

    const auto& texDims = compileData.defaultTexDims;

    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addInputOutput(kColor, "Color texture").format(mColorFormat).texture2D(0, 0, mSuperSampleCount);
    reflector.addInputOutput(kDepth, "Pre-initialized depth-buffer").bindFlags(Resource::BindFlags::DepthStencil);

    addRenderPassOutputs(reflector, kForwardLightingPassExtraChannels, Resource::BindFlags::UnorderedAccess);

    return reflector;
}

void ForwardLightingPass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mFrameDim = compileData.defaultTexDims;
    auto pDevice = pRenderContext->device();

    mpNoiseOffsetGenerator = StratifiedSamplePattern::create(mFrameSampleCount);
    mpBlueNoiseTexture = BlueNoiseTexture::create(pDevice);
}

void ForwardLightingPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    mpScene = pScene;
    mpVars = nullptr;

    if (mpScene) {
        mpState->getProgram()->addDefines(mpScene->getSceneDefines());
        mpState->getProgram()->setTypeConformances(mpScene->getTypeConformances());
    }
}

void ForwardLightingPass::initDepth(RenderContext* pContext, const RenderData& renderData) {
    const auto& pTexture = renderData[kDepth]->asTexture();

    if (pTexture) {
        mpState->setDepthStencilState(mpDsNoDepthWrite);
        mpFbo->attachDepthStencilTarget(pTexture);
    } else {
        mpState->setDepthStencilState(nullptr);
        if (mpFbo->getDepthStencilTexture() == nullptr) {
            auto pDepth = Texture::create2D(pContext->device(), mpFbo->getWidth(), mpFbo->getHeight(), ResourceFormat::D32Float, 1, 1, nullptr, Resource::BindFlags::DepthStencil);
            mpFbo->attachDepthStencilTarget(pDepth);
        }
    }
}

void ForwardLightingPass::initFbo(RenderContext* pContext, const RenderData& renderData) {
    mpFbo->attachColorTarget(renderData[kColor]->asTexture(), 0);
    
    for (uint32_t i = 1; i < 1; i++) {
        const auto& pRtv = mpFbo->getRenderTargetView(i).get();
        if (pRtv->getResource() != nullptr) pContext->clearRtv(pRtv, float4(0));
    }

    // TODO Matt (not really matt, just need to fix that since if depth is not bound the pass crashes
    if (mUsePreGenDepth == false) pContext->clearDsv(renderData[kDepth]->asTexture()->getDSV().get(), 1, 0);
}

void ForwardLightingPass::execute(RenderContext* pContext, const RenderData& renderData) {
    initDepth(pContext, renderData);
    initFbo(pContext, renderData);
    
    if (!mpScene) return;

    // For optional I/O resources, set 'is_valid_<name>' defines to inform the program of which ones it can access.
    // TODO: This should be moved to a more general mechanism using Slang.
    mpState->getProgram()->addDefines(getValidResourceDefines(kForwardLightingPassExtraChannels, renderData));
    
    // Create program vars.
    if (!mpVars) {
        mpVars = GraphicsVars::create(pContext->device(), mpState->getProgram()->getReflector());
    }

    // Prepare program vars. This may trigger shader compilation.
    // The program should have all necessary defines set at this point.
    prepareVars(pContext);

    float2 f = mpNoiseOffsetGenerator->next();
    uint2 noiseOffset = {64 * (f[0] + 0.5f), 64 * (f[1] + 0.5f)};

    mpVars["PerFrameCB"]["gRenderTargetDim"] = float2(mpFbo->getWidth(), mpFbo->getHeight());
    mpVars["PerFrameCB"]["gNoiseOffset"] = noiseOffset;
    mpVars["PerFrameCB"]["gViewInvMat"] = glm::inverse(mpScene->getCamera()->getViewMatrix());
    mpVars["PerFrameCB"]["gSamplesPerFrame"]  = mFrameSampleCount;
    mpVars["PerFrameCB"]["gSampleNumber"] = mSampleNumber++;

    // Bind extra channels as UAV buffers.
    for (const auto& channel : kForwardLightingPassExtraChannels) {
        Texture::SharedPtr pTex = renderData[channel.name]->asTexture();
        mpVars[channel.texname] = pTex;
    }
    
    mpState->setFbo(mpFbo);
    mpScene->rasterize(pContext, mpState.get(), mpVars.get(), mCullMode);
}

void ForwardLightingPass::prepareVars(RenderContext* pContext) {
    assert(mpVars);

    if (!mEnvMapDirty) return;

    // Update env map lighting
    const auto& pEnvMap = mpScene->getEnvMap();

    if (pEnvMap) {
        mpState->getProgram()->addDefine("_USE_ENV_MAP");
        if (mUseSimplifiedEnvLighting) {
            mpState->getProgram()->addDefine("_USE_SIMPLIFIED_ENV_LIGHTING");
            mpEnvMapLighting = EnvMapLighting::create(pContext, pEnvMap);
            mpEnvMapLighting->setShaderData(mpVars["gEnvMapLighting"]);
        } else {
            mpEnvMapSampler = EnvMapSampler::create(pContext, pEnvMap);
            mpEnvMapSampler->setShaderData(mpVars["PerFrameCB"]["gEnvMapSampler"]);
        }
    } else {
        mpState->getProgram()->removeDefine("_USE_ENV_MAP");
        mpEnvMapLighting = nullptr;
        mpEnvMapSampler = nullptr;
    }

    mpState->getProgram()->addDefines(mpSampleGenerator->getDefines());

    mpVars["gNoiseSampler"]     = mpNoiseSampler;
    mpVars["gNoiseTex"]         = mpBlueNoiseTexture;
    
    //bool success = mpSampleGenerator->setShaderData(mpVars["PerFrameCB"]["gSampleGenerator"]);
    //if (!success) throw std::runtime_error("Failed to bind GPU sample generator");

    mEnvMapDirty = false;
}

ForwardLightingPass& ForwardLightingPass::setColorFormat(ResourceFormat format) {
    mColorFormat = format;
    mPassChangedCB();
    return *this;
}

void ForwardLightingPass::setFrameSampleCount(uint32_t samples) {
    if (mFrameSampleCount == samples) return;

    mFrameSampleCount = samples;
    mDirty = true;
}

ForwardLightingPass& ForwardLightingPass::setSuperSampleCount(uint32_t samples) {
    mSuperSampleCount = samples;
    mPassChangedCB();
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setSuperSampling(bool enable) {
    mEnableSuperSampling = enable;
    if (mEnableSuperSampling) {
        mpState->getProgram()->addDefine("INTERPOLATION_MODE", "sample");
    } else {
        mpState->getProgram()->removeDefine("INTERPOLATION_MODE");
    }

    return *this;
}

ForwardLightingPass& ForwardLightingPass::usePreGeneratedDepthBuffer(bool enable) {
    mUsePreGenDepth = enable;
    mPassChangedCB();
    mpState->setDepthStencilState(mUsePreGenDepth ? mpDsNoDepthWrite : nullptr);

    return *this;
}

ForwardLightingPass& ForwardLightingPass::setRasterizerState(const RasterizerState::SharedPtr& pRsState) {
    mpRsState = pRsState;
    mpState->setRasterizerState(mpRsState);
    return *this;
}
