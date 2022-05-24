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
#include <chrono>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "DepthPass.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerClass("DepthPass", "Creates a depth-buffer using the scene's active camera", DepthPass::create);
}

const char* DepthPass::kDesc = "Creates a depth-buffer using the scene's active camera";

namespace {
    const std::string kProgramFile = "RenderPasses/DepthPass/DepthPass.ps.slang";
    const std::string kComputeDownSampleDepthProgramFile = "RenderPasses/DepthPass/DepthPass.HiZ.cs.slang";
    
    const std::string kDepth = "depth";
    const std::string kHiMaxZ = "hiMaxZ";
    const std::string kDepthFormat = "depthFormat";
    const std::string kDisableAlphaTest = "disableAlphaTest";
    const std::string kBuildHiZ = "buildHiZ";
    const std::string kMaxHiZMipLevel ="maxMipLevels";
}  // namespace

void DepthPass::parseDictionary(const Dictionary& dict) {
    for (const auto& [key, value] : dict) {
        if (key == kBuildHiZ) setHiZEnabled(value);
        else if (key == kMaxHiZMipLevel) setHiZMaxMipLevels(value);
        else if (key == kDepthFormat) setDepthBufferFormat(value);
        else if (key == kDisableAlphaTest) setAlphaTestDisabled(value);
        else logWarning("Unknown field '" + key + "' in a ForwardLightingPass dictionary");
    }
}

Dictionary DepthPass::getScriptingDictionary() {
    Dictionary d;
    d[kDepthFormat] = mDepthFormat;
    d[kBuildHiZ] = mHiZenabled;
    d[kMaxHiZMipLevel] = mHiZMaxMipLevels;
    d[kDisableAlphaTest] = mAlphaTestDisabled;
    return d;
}

DepthPass::SharedPtr DepthPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    //auto pThis = new DepthPass(pRenderContext->device(), dict);

    //if(pThis) {
    //    pThis->parseDictionary(dict);
    //}

    return SharedPtr(new DepthPass(pRenderContext->device(), dict));
}

DepthPass::DepthPass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice) {
    if (mHiZenabled) {
        mpDownSampleDepthPass = ComputePass::create(pDevice, kComputeDownSampleDepthProgramFile, "main", {{"MAX_PASS", ""}});
    }

    Program::Desc desc;
    desc.addShaderLibrary(kProgramFile).psEntry("main");
    
    auto pProgram = GraphicsProgram::create(pDevice, desc);

    // Create a GPU sample generator.
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
    assert(mpSampleGenerator);
    pProgram->addDefines(mpSampleGenerator->getDefines());

    mpState = GraphicsState::create(pDevice);
    mpState->setProgram(pProgram);
    mpFbo = Fbo::create(pDevice);

    Sampler::Desc depthSamplerDesc;
    depthSamplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point)
      .setMaxAnisotropy(0)
      .setComparisonMode(Sampler::ComparisonMode::Disabled)
      .setBorderColor({1.0, 1.0, 1.0, 1.0})
      .setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Border);
  
    mpDepthSampler = Sampler::create(pDevice, depthSamplerDesc);  // depth sampler

    mSampleNumber = 0;

    parseDictionary(dict);
}

RenderPassReflection DepthPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    
    uint2 frameDims = compileData.defaultTexDims;

    reflector.addOutput(kDepth, "Depth-buffer")
        .bindFlags(Resource::BindFlags::DepthStencil)
        .format(mDepthFormat)
        .texture2D(0, 0, 0);

    uint8_t hiZMipLevelsCount = 1;      
    if ( compileData.defaultTexDims[0] != 0 && compileData.defaultTexDims[1] != 0 ) {
        hiZMipLevelsCount = std::min(std::max( Texture::getMaxMipCount({compileData.defaultTexDims[0] / 2, compileData.defaultTexDims[1] / 2, 1}), mHiZMaxMipLevels), mHiZMaxMipLevels);
    }

    reflector.addOutput(kHiMaxZ, "Hierarchical MaxZ buffer")
        .flags(RenderPassReflection::Field::Flags::Optional)
        .bindFlags(ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess)
        .format(ResourceFormat::R32Float)
        .texture2D(compileData.defaultTexDims[0] / 2, compileData.defaultTexDims[1] / 2, 1, hiZMipLevelsCount, 1);

    return reflector;
}

void DepthPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    mpScene = pScene;
    mpVars = nullptr;

    if (mpScene) {
        auto pProgram = mpState->getProgram();
        pProgram->addDefines(mpScene->getSceneDefines());
        //mpState->getProgram()->addDefine("DISABLE_RAYTRACING", "");
        pProgram->addDefine("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        pProgram->setTypeConformances(mpScene->getTypeConformances());
        mpVars = GraphicsVars::create(pRenderContext->device(), mpState->getProgram()->getReflector());
    }
}

void DepthPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if (!mpScene) return;

    const auto& pDepth = renderData[kDepth]->asTexture();

    mpFbo->attachDepthStencilTarget(pDepth);

    mpState->setFbo(mpFbo);
    pRenderContext->clearDsv(pDepth->getDSV().get(), 1, 0);

    mpVars["PerFrameCB"]["gSamplesPerFrame"]  = mFrameSampleCount;
    mpVars["PerFrameCB"]["gSampleNumber"] = mSampleNumber++;

    mpScene->rasterize(pRenderContext, mpState.get(), mpVars.get(), mCullMode);

    // caclulate hierarchical z buffers
    auto pHiMaxZ = renderData[kHiMaxZ]->asTexture();
    
    if (pHiMaxZ && mHiZenabled) {
        uint prevMipWidth, prevMipHeight, currMipWidth, currMipHeight;

        mpDownSampleDepthPass["gDepthSampler"] = mpDepthSampler;

        mpDownSampleDepthPass["CB"]["gMipLevel"] = 0;
        mpDownSampleDepthPass["CB"]["prevMipDim"] = int2({pDepth->getWidth(0), pDepth->getHeight(0)});
        mpDownSampleDepthPass["CB"]["currMipDim"] = int2({pHiMaxZ->getWidth(0), pHiMaxZ->getHeight(0)});

        mpDownSampleDepthPass["gSourceBuffer"] = pDepth;
        mpDownSampleDepthPass["gOutputBuffer"].setUav(pHiMaxZ->getUAV(0, 0, 1));

        mpDownSampleDepthPass->execute(pRenderContext, pHiMaxZ->getWidth(0), pHiMaxZ->getHeight(0));

        for(uint32_t mipLevel = 1; mipLevel < pHiMaxZ->getMipCount(); mipLevel++) {
      
            prevMipWidth = pHiMaxZ->getWidth(mipLevel - 1);
            prevMipHeight = pHiMaxZ->getHeight(mipLevel - 1);

            currMipWidth = pHiMaxZ->getWidth(mipLevel);
            currMipHeight = pHiMaxZ->getHeight(mipLevel);

            mpDownSampleDepthPass["CB"]["gMipLevel"] = mipLevel;
            mpDownSampleDepthPass["CB"]["prevMipDim"] = int2({prevMipWidth, prevMipHeight});
            mpDownSampleDepthPass["CB"]["currMipDim"] = int2({currMipWidth, currMipHeight});

            const Texture* pSrcTex = dynamic_cast<const Texture*>(pHiMaxZ.get());
            const Texture* pDstTex = dynamic_cast<const Texture*>(pHiMaxZ.get());

            const ResourceViewInfo* pSrcViewInfo = &pHiMaxZ->getSRV(mipLevel - 1, 1, 0, 1)->getViewInfo();
            const ResourceViewInfo* pDstViewInfo = &pHiMaxZ->getUAV(mipLevel, 0, 1)->getViewInfo();

            pRenderContext->resourceBarrier(pSrcTex, Resource::State::CopySource, pSrcViewInfo);
            pRenderContext->resourceBarrier(pDstTex, Resource::State::CopyDest, pDstViewInfo);

            mpDownSampleDepthPass["gSourceBuffer"].setSrv(pHiMaxZ->getSRV(mipLevel - 1, 1, 0, 1));
            mpDownSampleDepthPass["gOutputBuffer"].setUav(pHiMaxZ->getUAV(mipLevel, 0, 1));
            
            mpDownSampleDepthPass->execute(pRenderContext, currMipWidth, currMipHeight);
        }
    }
}

DepthPass& DepthPass::setDepthBufferFormat(ResourceFormat format) {
    if (isDepthStencilFormat(format) == false) {
        logWarning("DepthPass buffer format must be a depth-stencil format");
    } else {
        mDepthFormat = format;
        mPassChangedCB();
    }
    return *this;
}

DepthPass& DepthPass::setDepthStencilState(const DepthStencilState::SharedPtr& pDsState) {
    mpState->setDepthStencilState(pDsState);
    return *this;
}

void DepthPass::setHiZEnabled(bool value) { 
    if(mHiZenabled == value) return;

    mHiZenabled = value;    
};

void DepthPass::setAlphaTestDisabled(bool value) {
    if(mAlphaTestDisabled == value) return;

    if(mAlphaTestDisabled) {
        mpState->getProgram()->addDefine("DISABLE_ALPHA_TEST", "");
    } else {
        mpState->getProgram()->removeDefine("DISABLE_ALPHA_TEST");
    }
}

void DepthPass::prepareVars() {
    assert(mpVars);

    if (!mDirty) return;

    mpState->getProgram()->addDefines(mpSampleGenerator->getDefines());

    bool success = mpSampleGenerator->setShaderData(mpVars["PerFrameCB"]["gSampleGenerator"]);
    if (!success) throw std::runtime_error("Failed to bind GPU sample generator");

    mDirty = false;
}