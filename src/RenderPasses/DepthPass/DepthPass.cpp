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

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "DepthPass.h"

const RenderPass::Info DepthPass::kInfo { "DepthPass", "Creates a depth-buffer using the scene's active camera." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(DepthPass::kInfo, DepthPass::create);
}


namespace {
    const std::string kProgramFile = "RenderPasses/DepthPass/DepthPass.3d.slang";
    
    const std::string kDepth = "depth";
    const std::string kDepthFormat = "depthFormat";
    const std::string kDisableAlphaTest = "disableAlphaTest";
}  // namespace

void DepthPass::parseDictionary(const Dictionary& dict) {
    for (const auto& [key, value] : dict) {
        if (key == kDepthFormat) setDepthBufferFormat(value);
        else if (key == kDisableAlphaTest) setAlphaTestDisabled(value);
    }
}

Dictionary DepthPass::getScriptingDictionary() {
    Dictionary d;
    d[kDepthFormat] = mDepthFormat;
    d[kDisableAlphaTest] = mAlphaTestDisabled;
    return d;
}

DepthPass::SharedPtr DepthPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new DepthPass(pRenderContext->device(), dict));
}

DepthPass::DepthPass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
    Program::Desc desc;
    desc.addShaderLibrary(kProgramFile).vsEntry("vsMain").psEntry("psMain");
    
    auto pProgram = GraphicsProgram::create(pDevice, desc);

    // Create a GPU sample generator.
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
    assert(mpSampleGenerator);

    mpState = GraphicsState::create(pDevice);
    
    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthFunc(DepthStencilState::Func::LessEqual).setDepthWriteMask(true);
    mpState->setDepthStencilState(DepthStencilState::create(dsDesc));

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
    reflector.addOutput(kDepth, "Depth-buffer").bindFlags(Resource::BindFlags::DepthStencil).format(mDepthFormat); //.texture2D(mOutputSize.x, mOutputSize.y);
    return reflector;
}

void DepthPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    (if mpScene == pScene) return;
    mpScene = pScene;
    mDirty = true;
}

void DepthPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    if(!mpScene) return;

    const auto& pDepth = renderData[kDepth]->asTexture();
    mpFbo->attachDepthStencilTarget(pDepth);

    if(mDirty || !mpVars) {
        auto pProgram = mpState->getProgram();
        pProgram->addDefines(mpScene->getSceneDefines());
        pProgram->addDefines(mpSampleGenerator->getDefines());
        pProgram->addDefine("USE_ALPHA_TEST", mUseAlphaTest ? "1" : "0");
        pProgram->setTypeConformances(mpScene->getTypeConformances());            
        mpVars = GraphicsVars::create(pRenderContext->device(), mpState->getProgram()->getReflector());
    }

    mpState->setFbo(mpFbo);

    bool clearDepth = true;
    bool clearStencil = false;
    pRenderContext->clearDsv(pDepth->getDSV().get(), .0f, 1, clearDepth, clearStencil);

    mpScene->rasterize(pRenderContext, mpState.get(), mpVars.get(), mCullMode);
    mDirty = false;
}

DepthPass& DepthPass::setDepthBufferFormat(ResourceFormat format) {
    if (isDepthStencilFormat(format) == false) {
        LLOG_WRN << "DepthPass buffer format must be a depth-stencil format";
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

void DepthPass::setAlphaTestDisabled(bool value) {
    if(mAlphaTestDisabled == value) return;

    if(mAlphaTestDisabled) {
        mpState->getProgram()->addDefine("DISABLE_ALPHA_TEST", "");
    } else {
        mpState->getProgram()->removeDefine("DISABLE_ALPHA_TEST");
    }
}

void DepthPass::setOutputSize(const uint2& outputSize) {
    if (outputSize != mOutputSize) {
        mOutputSize = outputSize;
        requestRecompile();
    }
}
