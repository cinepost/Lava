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

#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Textures/BlueNoiseTexture.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "glm/gtc/random.hpp"

#include "glm/gtx/string_cast.hpp"



// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerClass("ForwardLightingPass", "Computes direct and indirect illumination and applies shadows for the current scene", ForwardLightingPass::create);
}

const char* ForwardLightingPass::kDesc = "The pass computes the lighting results for the current scene. It will compute direct-illumination, indirect illumination from the light-probe and apply shadows (if a visibility map is provided).\n"
"The pass can output the world-space normals and screen-space motion vectors, both are optional";

namespace {
    const std::string kDepth = "depth";
    const std::string kColor = "color";
    const std::string kMotionVecs = "motionVecs";
    const std::string kNormals = "normals";
    const std::string kVisBuffer = "visibilityBuffer";

    const std::string kFrameSampleCount = "frameSampleCount";
    const std::string kSuperSampleCount = "superSampleCount";
    const std::string kSuperSampling = "enableSuperSampling";
}

ForwardLightingPass::SharedPtr ForwardLightingPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new ForwardLightingPass(pRenderContext->device()));
    pThis->setColorFormat(ResourceFormat::RGBA32Float)
        .setMotionVecFormat(ResourceFormat::RG16Float).setNormalMapFormat(ResourceFormat::RGBA8Unorm).setSuperSampleCount(1).usePreGeneratedDepthBuffer(true);

    for (const auto& [key, value] : dict)
    {
        if (key == kSuperSampleCount) pThis->setSuperSampleCount(value);
        else if (key == kFrameSampleCount) pThis->setFrameSampleCount(value);
        else if (key == kSuperSampling) pThis->setSuperSampling(value);
        else logWarning("Unknown field '" + key + "' in a ForwardLightingPass dictionary");
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

ForwardLightingPass::ForwardLightingPass(Device::SharedPtr pDevice): RenderPass(pDevice) {
    GraphicsProgram::SharedPtr mpProgram = GraphicsProgram::createFromFile(pDevice, "RenderPasses/ForwardLightingPass/ForwardLightingPass.slang", "", "ps");
    mpProgram->addDefine("_MS_DISABLE_ALPHA_TEST", "");
    mpProgram->removeDefine("_MS_DISABLE_ALPHA_TEST"); // TODO: move to execute
    mpProgram->addDefine("ENABLE_DEFERED_AO", "1");

    // Create a GPU sample generator.
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
    assert(mpSampleGenerator);
    mpProgram->addDefines(mpSampleGenerator->getDefines());

    mpState = GraphicsState::create(pDevice);
    mpState->setProgram(mpProgram);

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

    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addInputOutput(kColor, "Color texture").format(mColorFormat).texture2D(0, 0, mSuperSampleCount);

    auto& depthField = reflector.addInputOutput(kDepth, "Pre-initialized depth-buffer").bindFlags(Resource::BindFlags::DepthStencil);

    if (mNormalMapFormat != ResourceFormat::Unknown) {
        reflector.addOutput(kNormals, "World-space normal, [0,1] range. Don't forget to transform it to [-1, 1] range").format(mNormalMapFormat).texture2D(0, 0, mSuperSampleCount);
    }

    if (mMotionVecFormat != ResourceFormat::Unknown) {
        reflector.addOutput(kMotionVecs, "Screen-space motion vectors").format(mMotionVecFormat).texture2D(0, 0, mSuperSampleCount);
    }

    return reflector;
}

void ForwardLightingPass::compile(RenderContext* pRenderContext, const CompileData& compileData) {
    mFrameDim = compileData.defaultTexDims;
    auto pDevice = pRenderContext->device();

    if(mUseSSAO) {
        mpState->getProgram()->addDefine("USE_SSAO", "1");
    }

    mpNoiseOffsetGenerator = StratifiedSamplePattern::create(mFrameSampleCount);
    mpBlueNoiseTexture = BlueNoiseTexture::create(pDevice);
}

void ForwardLightingPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    mpScene = pScene;

    if (mpScene) mpState->getProgram()->addDefines(mpScene->getSceneDefines());

    mpVars = GraphicsVars::create(pRenderContext->device(), mpState->getProgram()->getReflector());

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    setSampler(Sampler::create(pRenderContext->device(), samplerDesc));
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
    
    if(renderData[kNormals])
        mpFbo->attachColorTarget(renderData[kNormals]->asTexture(), 1);
    
    if(renderData[kMotionVecs])
        mpFbo->attachColorTarget(renderData[kMotionVecs]->asTexture(), 2);

    for (uint32_t i = 1; i < 3; i++) {
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

    if(renderData[kVisBuffer])
        mpVars->setTexture(kVisBuffer, renderData[kVisBuffer]->asTexture());
    
    mpState->setFbo(mpFbo);
    mpScene->rasterize(pContext, mpState.get(), mpVars.get(), mCullMode);
    //mpScene->rasterizeX(pContext, mpState.get(), mpVars.get(), mCullMode);

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
    //mpVars["gTlas"] = mpScene->getTlas();

    bool success = mpSampleGenerator->setShaderData(mpVars["PerFrameCB"]["gSampleGenerator"]);
    if (!success) throw std::runtime_error("Failed to bind GPU sample generator");

    mEnvMapDirty = false;
}

ForwardLightingPass& ForwardLightingPass::setColorFormat(ResourceFormat format) {
    mColorFormat = format;
    mPassChangedCB();
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setNormalMapFormat(ResourceFormat format) {
    mNormalMapFormat = format;
    mPassChangedCB();
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setMotionVecFormat(ResourceFormat format) {
    mMotionVecFormat = format;
    if (mMotionVecFormat != ResourceFormat::Unknown) {
        mpState->getProgram()->addDefine("_OUTPUT_MOTION_VECTORS");
    } else {
        mpState->getProgram()->removeDefine("_OUTPUT_MOTION_VECTORS");
    }
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

ForwardLightingPass& ForwardLightingPass::setSampler(const Sampler::SharedPtr& pSampler) {
    mpVars->setSampler("gSampler", pSampler);
    return *this;
}

ForwardLightingPass& ForwardLightingPass::setRasterizerState(const RasterizerState::SharedPtr& pRsState) {
    mpRsState = pRsState;
    mpState->setRasterizerState(mpRsState);
    return *this;
}
