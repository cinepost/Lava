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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#include "GBufferBase.h"
#include "GBuffer/GBufferRaster.h"
#include "VBuffer/VBufferRaster.h"
#include "GBuffer/GBufferRT.h"
#include "VBuffer/VBufferRT.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    
    lib.registerPass(GBufferRaster::kInfo, GBufferRaster::create);
    lib.registerPass(GBufferRT::kInfo, GBufferRT::create);
    lib.registerPass(VBufferRaster::kInfo, VBufferRaster::create);
    lib.registerPass(VBufferRT::kInfo, VBufferRT::create);

    Falcor::ScriptBindings::registerBinding(GBufferBase::registerBindings);
    Falcor::ScriptBindings::registerBinding(GBufferRT::registerBindings);
}

GBufferBase::GBufferBase(Device::SharedPtr pDevice, Info info): RenderPass(pDevice, info) {
    assert(pDevice);
}

void GBufferBase::registerBindings(pybind11::module& m) {
    pybind11::enum_<GBufferBase::SamplePattern> samplePattern(m, "SamplePattern");
    samplePattern.value("Center", GBufferBase::SamplePattern::Center);
    samplePattern.value("DirectX", GBufferBase::SamplePattern::DirectX);
    samplePattern.value("Halton", GBufferBase::SamplePattern::Halton);
    samplePattern.value("Stratified", GBufferBase::SamplePattern::Stratified);
}

namespace {
// Scripting options.
    const char kSamplePattern[] = "samplePattern";
    const char kSampleCount[] = "sampleCount";
    const char kUseAlphaTest[] = "useAlphaTest";
    const char kDisableAlphaTest[] = "disableAlphaTest"; ///< Deprecated for "useAlphaTest".
    const char kAdjustShadingNormals[] = "adjustShadingNormals";
    const char kForceCullMode[] = "forceCullMode";
    const char kCullMode[] = "cull";
}

void GBufferBase::parseDictionary(const Dictionary& dict) {
    for (const auto& [key, value] : dict) {
        if (key == kSamplePattern) mSamplePattern = value;
        else if (key == kSampleCount) mSampleCount = value;
        else if (key == kUseAlphaTest) mUseAlphaTest = value;
        else if (key == kAdjustShadingNormals) mAdjustShadingNormals = value;
        else if (key == kForceCullMode) mForceCullMode = value;
        else if (key == kCullMode) mCullMode = value;
        // TODO: Check for unparsed fields, including those parsed in derived classes.
    }

    // Handle deprecated "disableAlphaTest" value.
    if (dict.keyExists(kDisableAlphaTest) && !dict.keyExists(kUseAlphaTest)) mUseAlphaTest = !dict[kDisableAlphaTest];
}

Dictionary GBufferBase::getScriptingDictionary() {
    Dictionary dict;
    dict[kSamplePattern] = mSamplePattern;
    dict[kSampleCount] = mSampleCount;
    dict[kUseAlphaTest] = mUseAlphaTest;
    dict[kAdjustShadingNormals] = mAdjustShadingNormals;
    dict[kForceCullMode] = mForceCullMode;
    dict[kCullMode] = mCullMode;
    return dict;
}

void GBufferBase::compile(RenderContext* pContext, const CompileData& compileData) {
    mDirty = true;
    mFrameDim = compileData.defaultTexDims;
    mInvFrameDim = 1.f / float2(mFrameDim);

    if (mpScene) {
        //auto pCamera = mpScene->getCamera();
        //pCamera->setPatternGenerator(pCamera->getPatternGenerator(), mInvFrameDim);
    }
}

void GBufferBase::resolvePerFrameSparseResources(RenderContext* pRenderContext, const RenderData& renderData) { }

void GBufferBase::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    // Update refresh flag if options that affect the output have changed.
    auto& dict = renderData.getDictionary();
    if (mOptionsChanged) {
        auto flags = dict.getValue(kRenderPassRefreshFlags, RenderPassRefreshFlags::None);
        dict[Falcor::kRenderPassRefreshFlags] = flags | Falcor::RenderPassRefreshFlags::RenderOptionsChanged;
        mOptionsChanged = false;
    }

    // Pass flag for adjust shading normals to subsequent passes via the dictionary.
    // Adjusted shading normals cannot be passed via the VBuffer, so this flag allows consuming passes to compute them when enabled.
    dict[Falcor::kRenderPassGBufferAdjustShadingNormals] = mAdjustShadingNormals;


    // Setup camera with sample generator.
    if (mpScene) mpScene->getCamera()->setPatternGenerator(mpSampleGenerator, mInvFrameDim);
}

void GBufferBase::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(mpScene == pScene) return;

    mDirty = true;    
    mpScene = pScene;
    mFrameCount = 0;
    updateSamplePattern();

    if (mpScene) {
        // Trigger graph recompilation if we need to change the V-buffer format.
        ResourceFormat format = mpScene->getHitInfo().getFormat();
        if (format != mVBufferFormat) {
            mVBufferFormat = format;
            mPassChangedCB();
        }
    }
}

static CPUSampleGenerator::SharedPtr createSamplePattern(GBufferBase::SamplePattern type, uint32_t sampleCount) {
    switch (type) {
    case GBufferBase::SamplePattern::Center:
        return nullptr;
    case GBufferBase::SamplePattern::DirectX:
        return DxSamplePattern::create(sampleCount);
    case GBufferBase::SamplePattern::Halton:
        return HaltonSamplePattern::create(sampleCount);
    case GBufferBase::SamplePattern::Stratified:
        return StratifiedSamplePattern::create(sampleCount);
    default:
        should_not_get_here();
        return nullptr;
    }
}

void GBufferBase::updateFrameDim(const uint2 frameDim) {
    assert(frameDim.x > 0 && frameDim.y > 0);
    
    if(mFrameDim == frameDim) return;

    mFrameDim = frameDim;
    mInvFrameDim = 1.f / float2(frameDim);

    // Update sample generator for camera jitter.
    //if (mpScene) mpScene->getCamera()->setPatternGenerator(mpSampleGenerator, mInvFrameDim);

    mDirty = true;
}

void GBufferBase::updateSamplePattern() {
    mpSampleGenerator = createSamplePattern(mSamplePattern, mSampleCount);
    if (mpSampleGenerator) mSampleCount = mpSampleGenerator->getSampleCount();
}

GBufferBase& GBufferBase::setDepthBufferFormat(ResourceFormat format) {
    if(mDepthFormat != format) { 
        mDepthFormat = format;
        mDirty = true;
    }
    return *this;
}

Texture::SharedPtr GBufferBase::getOutput(const RenderData& renderData, const std::string& name) const {
    // This helper fetches the render pass output with the given name and verifies it has the correct size.
    assert(mFrameDim.x > 0 && mFrameDim.y > 0);

    auto pTex = renderData[name]->asTexture();
    if (!pTex) return nullptr;

    if ((pTex->getWidth() != mFrameDim.x) || (pTex->getHeight() != mFrameDim.y)) {
        throw std::runtime_error("GBufferBase: Pass output '" + name + "' has mismatching size. All outputs must be of the same size.");
    }
    return pTex;
}

void GBufferBase::setCullMode(RasterizerState::CullMode mode) { 
    if(mCullMode == mode) return;
    mCullMode = mode; 
    mDirty = true;
}

const std::string& GBufferBase::to_define_string(RasterizerState::CullMode mode) {
    static const std::string kCullNone = "CULL_NONE";
    static const std::string kCullFront = "CULL_FRONT";
    static const std::string kCullBack = "CULL_BACK";

    switch(mode) {
        case RasterizerState::CullMode::Front:
            return kCullFront;
        case RasterizerState::CullMode::Back:
            return kCullBack;
        default:
            return kCullNone;
    }
}
