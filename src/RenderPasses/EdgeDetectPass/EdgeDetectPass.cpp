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

#include "Falcor/Utils/Textures/FilterKernelsLUT.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"

#include <boost/algorithm/string.hpp>

#include "EdgeDetectPass.h"


const RenderPass::Info EdgeDetectPass::kInfo { "EdgeDetectPass", "Edge detection." };


// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regEdgeDetectPass(pybind11::module& m) {
    pybind11::class_<EdgeDetectPass, RenderPass, EdgeDetectPass::SharedPtr> pass(m, "EdgeDetectPass");
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerPass(EdgeDetectPass::kInfo, EdgeDetectPass::create);
    ScriptBindings::registerBinding(regEdgeDetectPass);
}

namespace {

    const char kShaderFile[] = "RenderPasses/EdgeDetectPass/EdgeDetect.cs.slang";
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

    // Serialized parameters
    
}

EdgeDetectPass::SharedPtr EdgeDetectPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    return SharedPtr(new EdgeDetectPass(pRenderContext->device(), dict));
}

EdgeDetectPass::EdgeDetectPass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
    mpDevice = pDevice;

    setKernelSize({3, 3});

    // Deserialize pass from dictionary.
    for (const auto& [key, value] : dict) {

    }
}

Dictionary EdgeDetectPass::getScriptingDictionary() {
    Dictionary dict;
    return dict;
}

RenderPassReflection EdgeDetectPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
    reflector.addInput(kInputDepthChannel, "Input depth data").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput(kOutputChannel, "Output detected edge data").bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource)
        .format(mOutputFormat);

    addRenderPassInputs(reflector, kEdgeDetectPassExtraInputChannels, ResourceBindFlags::ShaderResource);

    return reflector;
}

void EdgeDetectPass::compile(RenderContext* pContext, const CompileData& compileData) {
    mDirty = true;
}

void EdgeDetectPass::execute(RenderContext* pRenderContext, const RenderData& renderData) {
    // Grab our input/output buffers.
    Texture::SharedPtr pDst = renderData[kOutputChannel]->asTexture();   
    Texture::SharedPtr pSrcDepth = renderData[kInputDepthChannel]->asTexture();  
    Texture::SharedPtr pSrcNormal = renderData[kInputNormalChannel]->asTexture();
    Texture::SharedPtr pSrcVBuffer = renderData[kInputVBufferChannel]->asTexture();

    assert(pSrcDepth || pSrcVBuffer);

    const uint2 resolution = pSrcVBuffer ? uint2(pSrcVBuffer->getWidth(), pSrcVBuffer->getHeight()) : uint2(pSrcDepth->getWidth(), pSrcDepth->getHeight());
    const uint2 halfKernelSize = uint2(mKernelSize.x >> 1, mKernelSize.y >> 1);

    prepareBuffers(pRenderContext, resolution);
    prepareKernelTextures();

    // U pass
    {
        if(!mpPassU || mDirty) {
            Program::Desc desc;
            desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("passU");
            if (mpScene) desc.addTypeConformances(mpScene->getTypeConformances());

            auto defines = mpScene ? mpScene->getSceneDefines() : Program::DefineList();
            defines.add(getValidResourceDefines(kEdgeDetectPassExtraInputChannels, renderData));
            defines.add("DEPTH_KERNEL_HALF_SIZE_U", std::to_string(halfKernelSize.x));
            defines.add("DEPTH_KERNEL_HALF_SIZE_V", std::to_string(halfKernelSize.y));

            mpPassU = ComputePass::create(mpDevice, desc, defines, true);
        
            if (mpScene) mpPassU["gScene"] = mpScene->getParameterBlock();
            mpPassU["gDepth"] = pSrcDepth;

            // Optional textures
            mpPassU["gNormal"] = pSrcNormal;
            mpPassU["gVBuffer"] = pSrcVBuffer;
            
            // Kernel textures
            mpPassU["gDepthKernelU"] = mpDepthKernelU;
            mpPassU["gDepthKernelV"] = mpDepthKernelV;

            // Output
            mpPassU["gTmpDepth"] = mpTmpDepth;
        }

        auto cb_vars = mpPassU["PerFrameCB"];
        cb_vars["gResolution"] = resolution;
        cb_vars["gDepthKernelCenter"] = uint2({1, 1});
        cb_vars["gDepthDistanceRange"] = mDepthDistanceRange;

        mpPassU->execute(pRenderContext, resolution.x, resolution.y);
    }
    
    // V pass
    {
        if(!mpPassV || mDirty) {
            Program::Desc desc;
            desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("passV");
            if (mpScene) desc.addTypeConformances(mpScene->getTypeConformances());

            auto defines = mpScene ? mpScene->getSceneDefines() : Program::DefineList();
            defines.add(getValidResourceDefines(kEdgeDetectPassExtraInputChannels, renderData));
            defines.add("DEPTH_KERNEL_HALF_SIZE_U", std::to_string(halfKernelSize.x));
            defines.add("DEPTH_KERNEL_HALF_SIZE_V", std::to_string(halfKernelSize.y));

            mpPassV = ComputePass::create(mpDevice, desc, defines, true);

            if (mpScene) mpPassU["gScene"] = mpScene->getParameterBlock();
            mpPassV["gTmpDepth"] = mpTmpDepth;

            // Kernel textures
            mpPassV["gDepthKernelU"] = mpDepthKernelU;
            mpPassV["gDepthKernelV"] = mpDepthKernelV;

            // Output
            mpPassV["gOutput"] = pDst;
        }

        auto cb_vars = mpPassV["PerFrameCB"];
        cb_vars["gResolution"] = resolution;
        cb_vars["gDepthKernelCenter"] = uint2({1, 1});
        cb_vars["gDepthDistanceRange"] = mDepthDistanceRange;
        
        mpPassV->execute(pRenderContext, resolution.x, resolution.y);
    }

    mDirty = false;
}

void EdgeDetectPass::prepareBuffers(RenderContext* pRenderContext, uint2 resolution) {
    auto prepareBuffer = [&](Texture::SharedPtr& pBuf, uint32_t width, uint32_t height, ResourceFormat format, bool bufUsed) {
        if (!bufUsed) {pBuf = nullptr; return; }

        // (Re-)create buffer if needed.
        if (!pBuf || pBuf->getWidth() != width || pBuf->getHeight() != height) {
            mDirty = true;
            pBuf = Texture::create2D(mpDevice, width, height, format, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            assert(pBuf);
            
            if (getFormatType(format) == FormatType::Float) pRenderContext->clearUAV(pBuf->getUAV().get(), float4(0.f));
            else pRenderContext->clearUAV(pBuf->getUAV().get(), uint4(0));
        }
    };

    prepareBuffer(mpTmpDepth, resolution.x, resolution.y, ResourceFormat::RG32Float, true);
}

void EdgeDetectPass::prepareKernelTextures() {
    if (!mDirty) return;

    static const float du[] = {1, 0,-1};
    static const float dv[] = {1, 2, 1};

    mpDepthKernelU = Texture::create2D(mpDevice, 3, 3, ResourceFormat::R32Float, 1, 1, du, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
    mpDepthKernelV = Texture::create2D(mpDevice, 3, 3, ResourceFormat::R32Float, 1, 1, dv, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
    mDirty = true;
}

void EdgeDetectPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
    if(pRenderContext->device() != pScene->device()) {
        LLOG_ERR << "Unable to set scene created on different device!";
        mpScene = nullptr;
    }
    
    if(mpScene != pScene) {
        mpScene = pScene;
        mDirty = true;
    }
}

void EdgeDetectPass::setOutputFormat(ResourceFormat format) {
    if(mOutputFormat == format) return;
    mOutputFormat = format;
    mPassChangedCB();
    mDirty = true;
}

void EdgeDetectPass::setKernelSize(uint2 size) {
    if(size.x < 3u || size.y < 3u) {
        size = {std::max(3u, size.x), std::max(3u, size.y)};
        LLOG_WRN << "EdgeDetectPass doesn't support kernel sizes less than 3x3 ! Clamping applied.";
    }

    if(size.x % 2 == 0) LLOG_WRN << "EdgeDetectPass doesn't support kernel width should be an odd number ! Width changed to " << std::to_string(++size.x);
    if(size.y % 2 == 0) LLOG_WRN << "EdgeDetectPass doesn't support kernel height should be an odd number ! Height changed to " << std::to_string(++size.y);
        
    if(mKernelSize != size) {
        mKernelSize = size;
        mDirty = true;
    }
}

void EdgeDetectPass::setDepthDistanceRange(float2 range) { 
    mDepthDistanceRange.x = std::min(range.x, range.y);
    mDepthDistanceRange.y = std::max(range.x, range.y); 
}

void EdgeDetectPass::setEdgeDetectFlags(EdgeDetectFlags flags) {
    if(mEdgeDetectFlags != flags) {
        mEdgeDetectFlags = flags;
        mDirty = true;
    }
}