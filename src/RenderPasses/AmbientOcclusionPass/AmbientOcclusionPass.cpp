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
#include <algorithm>
#include <pybind11/embed.h>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/RenderGraph/RenderPassHelpers.h"

#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"

#include <boost/algorithm/string.hpp>

#include "AmbientOcclusionPass.h"


const RenderPass::Info AmbientOcclusionPass::kInfo { "AmbientOcclusionPass", "Ambient occlusion." };


// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regAmbientOcclusionPass(pybind11::module& m) {
    pybind11::class_<AmbientOcclusionPass, RenderPass, AmbientOcclusionPass::SharedPtr> pass(m, "AmbientOcclusionPass");
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(AmbientOcclusionPass::kInfo, AmbientOcclusionPass::create);
    ScriptBindings::registerBinding(regAmbientOcclusionPass);
}

namespace {

    const char kShaderFile[] = "RenderPasses/AmbientOcclusionPass/AmbientOcclusionPass.raytrace.cs.slang";
    const std::string kShaderModel = "6_5";

    const char kOutputChannel[]      = "output";
    
    const char kInputDepthChannel[]  = "depth";
    const char kInputNormalChannel[] = "normal";
    const char kInputVBufferChannel[] = "vbuffer";

    const ChannelList kEdgeDetectPassExtraInputChannels = {
        { kInputDepthChannel,            "gDepth",            "Depth buffer",                  true /* optional */, ResourceFormat::Unknown },
        { kInputNormalChannel,           "gNormal",           "Normal buffer",                 true /* optional */, ResourceFormat::Unknown },
        { kInputVBufferChannel,          "gVBuffer",          "VBuffer",                       true /* optional */, HitInfo::kDefaultFormat },
    };

    const std::string kShadingRate = "shadingRate";
    const std::string kDistanceRange = "distanceRange";
    
}

AmbientOcclusionPass::SharedPtr AmbientOcclusionPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new AmbientOcclusionPass(pRenderContext->device(), dict));

    for (const auto& [key, value] : dict) {
        if (key == kShadingRate) pThis->setShadingRate(value);
        else if (key == kDistanceRange) pThis->setDistanceRange(value);
    }

    return pThis;
}

AmbientOcclusionPass::AmbientOcclusionPass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
    mpDevice = pDevice;

    // Create a GPU sample generator.
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
}

Dictionary AmbientOcclusionPass::getScriptingDictionary() {
    Dictionary dict;
    return dict;
}

RenderPassReflection AmbientOcclusionPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addOutput(kOutputChannel, "Output ambient occlusion data").bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource)
        .format(mOutputFormat);

    addRenderPassInputs(reflector, kEdgeDetectPassExtraInputChannels, ResourceBindFlags::ShaderResource);

    return reflector;
}

void AmbientOcclusionPass::compile(RenderContext* pContext, const CompileData& compileData) {
    mDirty = true;
}

void AmbientOcclusionPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    // Grab our input/output buffers.
    Texture::SharedPtr pDst = renderData[kOutputChannel]->asTexture();   
    Texture::SharedPtr pSrcDepth = renderData[kInputDepthChannel]->asTexture();  
    Texture::SharedPtr pSrcNormal = renderData[kInputNormalChannel]->asTexture();
    Texture::SharedPtr pSrcVBuffer = renderData[kInputVBufferChannel]->asTexture();

    assert(pSrcDepth || pSrcVBuffer);

    const uint2 resolution = pSrcVBuffer ? uint2(pSrcVBuffer->getWidth(), pSrcVBuffer->getHeight()) : uint2(pSrcDepth->getWidth(), pSrcDepth->getHeight());

    // Raytraced occlusion pass
    {
        if(!mpPassRayTrace || mDirty) {
            Program::Desc desc;
            desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("passRayQuery");
            if (mpScene) desc.addTypeConformances(mpScene->getTypeConformances());

            auto defines = mpScene ? mpScene->getSceneDefines() : Program::DefineList();
            defines.add(getValidResourceDefines(kEdgeDetectPassExtraInputChannels, renderData));
            defines.add("_SHADING_RATE", std::to_string(mShadingRate));
            defines.add(mpSampleGenerator->getDefines());

            mpPassRayTrace = ComputePass::create(mpDevice, desc, defines, true);
        
            if (mpScene) mpPassRayTrace["gScene"] = mpScene->getParameterBlock();

            // Optional textures
            mpPassRayTrace["gDepth"] = pSrcDepth;
            mpPassRayTrace["gNormal"] = pSrcNormal;
            mpPassRayTrace["gVBuffer"] = pSrcVBuffer;

            // Output
            mpPassRayTrace["gOutput"] = pDst;
        }

        auto cb_vars = mpPassRayTrace["PerFrameCB"];
        cb_vars["gSampleNumber"] = mSampleNumber++;
        cb_vars["gResolution"] = resolution;
        cb_vars["gDistanceRange"] = mDistanceRange;
        
        mpPassRayTrace->execute(pRenderContext, resolution.x, resolution.y);
    }

    mDirty = false;
}

void AmbientOcclusionPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(pRenderContext->device() != pScene->device()) {
        LLOG_ERR << "Unable to set scene created on different device!";
        mpScene = nullptr;
    }
    
    if(mpScene != pScene) {
        mpScene = pScene;
        mDirty = true;
    }
}

void AmbientOcclusionPass::setOutputFormat(ResourceFormat format) {
    if(mOutputFormat == format) return;
    mOutputFormat = format;
    mPassChangedCB();
    mDirty = true;
}

AmbientOcclusionPass& AmbientOcclusionPass::setShadingRate(int rate) {
    rate = std::max(1u, static_cast<uint>(rate));
    if(mShadingRate == rate) return *this;
    mShadingRate = rate;
    mDirty = true;
    return *this;
}

void AmbientOcclusionPass::setDistanceRange(float2 range) { 
    mDistanceRange.x = std::min(range.x, range.y);
    mDistanceRange.y = std::max(range.x, range.y); 
}
