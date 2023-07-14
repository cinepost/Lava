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

#include "EdgeDetectPass.h"
#include "EdgeDetectPass.slangh"


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

static inline bool validChannel(uint value) {
    if(value < static_cast<uint>(EdgeDetectOutputChannel::Count)) return true;
    return false;
}


namespace {

    const char kShaderFile[] = "RenderPasses/EdgeDetectPass/EdgeDetect.cs.slang";
    const char kLowPassShaderFile[] = "RenderPasses/EdgeDetectPass/EdgeDetect.lowpass.cs.slang";
    const std::string kShaderModel = "6_5";

    const char kOutputChannel[]      = "output";
    
    const char kInputDepthChannel[]  = "depth";
    const char kInputNormalChannel[] = "normal";
    const char kInputVBufferChannel[] = "vbuffer";
    const char kInputMaterialIDChannel[] = "materialID";
    const char kInputInstanceIDChannel[] = "instanceID";

    const ChannelList kEdgeDetectPassExtraInputChannels = {
        { kInputVBufferChannel,          "gVBuffer",          "VBuffer",                       true /* optional */, HitInfo::kDefaultFormat },
        // We work with either VBuffer or with followind ...
        { kInputDepthChannel,            "gDepth",            "Depth buffer",                  true /* optional */, ResourceFormat::Unknown },
        { kInputNormalChannel,           "gNormal",           "Normal buffer",                 true /* optional */, ResourceFormat::Unknown },
        { kInputMaterialIDChannel,       "gMaterialID",       "Material ID buffer",            true /* optional */, ResourceFormat::Unknown },
        { kInputInstanceIDChannel,       "gInstanceID",       "Instance ID buffer",            true /* optional */, ResourceFormat::Unknown },
    };

    const std::string kTraceDepth = "traceDepth";
    const std::string kTraceNormal = "traceNormal";
    const std::string kTraceMaterialID = "traceMaterialID";
    const std::string kTraceInstanceID = "traceInstanceID";
    const std::string kIgnoreAlpha = "ignoreAlpha";
    
    const std::string kDepthDistanceRange = "depthDistanceRange";
    const std::string kNormalThresholdRange = "normalThresholdRange";
    
    const std::string kDepthKernelSize = "depthKernelSize";
    const std::string kNormalKernelSize = "normalKernelSize";
    const std::string kMaterialKernelSize = "materialKernelSize";
    const std::string kInstanceKernelSize = "instanceKernelSize";

    const std::string kDepthOuputChannel = "depthOuputChannel";
    const std::string kNormalOuputChannel = "normalOuputChannel";
    const std::string kMaterialOuputChannel = "materialOuputChannel";
    const std::string kInstanceOuputChannel = "instanceOuputChannel";
    
    const std::string kLowPassFilterSize = "lowPassFilterSize";
}

EdgeDetectPass::SharedPtr EdgeDetectPass::create(RenderContext* pRenderContext, const Dictionary& dict) {
    auto pThis = SharedPtr(new EdgeDetectPass(pRenderContext->device(), dict));

    for (const auto& [key, value] : dict) {
        if (key == kTraceDepth) pThis->setTraceDepth(value);
        else if (key == kTraceNormal) pThis->setTraceNormal(value);
        else if (key == kTraceMaterialID) pThis->setTraceMaterialID(value);
        else if (key == kTraceInstanceID) pThis->setTraceInstanceID(value);
        else if (key == kIgnoreAlpha) pThis->setTraceAlpha(!value);
        else if (key == kDepthDistanceRange) pThis->setDepthDistanceRange(value);
        else if (key == kNormalThresholdRange) pThis->setNormalThresholdRange(value);

        else if (key == kDepthKernelSize) pThis->setDepthKernelSize(value);
        else if (key == kNormalKernelSize) pThis->setNormalKernelSize(value);
        else if (key == kMaterialKernelSize) pThis->setMaterialKernelSize(value);
        else if (key == kInstanceKernelSize) pThis->setInstanceKernelSize(value);
        
        else if (key == kDepthOuputChannel) pThis->setDepthOutputChannel(value);
        else if (key == kNormalOuputChannel) pThis->setNormalOutputChannel(value);
        else if (key == kMaterialOuputChannel) pThis->setMaterialOutputChannel(value);
        else if (key == kInstanceOuputChannel) pThis->setInstanceOutputChannel(value);

        else if (key == kLowPassFilterSize) pThis->setLowPassFilterSize(uint(value));
    }

    return pThis;
}

EdgeDetectPass::EdgeDetectPass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
    mpDevice = pDevice;
}

Dictionary EdgeDetectPass::getScriptingDictionary() {
    Dictionary dict;
    return dict;
}

RenderPassReflection EdgeDetectPass::reflect(const CompileData& compileData) {
    RenderPassReflection reflector;
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
    Texture::SharedPtr pSrcVBuffer = renderData[kInputVBufferChannel]->asTexture();
    Texture::SharedPtr pSrcDepth = renderData[kInputDepthChannel]->asTexture(); 

    if(!pSrcVBuffer && !pSrcDepth) {
        LLOG_ERR << "Either Depth or VBuffer required for EdgeDetectPass !!!";
        return;
    }

    Texture::SharedPtr pSrcNormal = renderData[kInputNormalChannel]->asTexture();
    Texture::SharedPtr pSrcMaterialID = renderData[kInputMaterialIDChannel]->asTexture(); 
    Texture::SharedPtr pSrcInstanceID = renderData[kInputInstanceIDChannel]->asTexture();

    Texture::SharedPtr pDst = renderData[kOutputChannel]->asTexture();


    const uint2 resolution = pSrcVBuffer ? uint2(pSrcVBuffer->getWidth(), pSrcVBuffer->getHeight()) : uint2(pSrcDepth->getWidth(), pSrcDepth->getHeight());

    prepareBuffers(pRenderContext, resolution);
    prepareKernelTextures();

    const uint32_t depthKernelHalfSize = mpDepthKernelU ? (mpDepthKernelU->getWidth(0) >> 1) : 0u;
    const uint32_t normalKernelHalfSize = mpNormalKernelU ? (mpNormalKernelU->getWidth(0) >> 1) : 0u;
    const uint32_t materialKernelHalfSize = mpMaterialKernelU ? (mpMaterialKernelU->getWidth(0) >> 1) : 0u;
    const uint32_t instanceKernelHalfSize = mpInstanceKernelU ? (mpInstanceKernelU->getWidth(0) >> 1) : 0u;

    // Low-pass
    if( (!mpLowPass || mDirty) && pSrcVBuffer && mpTmpVBuffer && (mLowPassFilterSize != 0)) {
        Program::Desc desc;
        desc.addShaderLibrary(kLowPassShaderFile).setShaderModel(kShaderModel).csEntry("lowPass");
        auto defines = Program::DefineList();
        defines.add("_FILTER_SIZE", std::to_string(mLowPassFilterSize));

        mpLowPass = ComputePass::create(mpDevice, desc, defines, true);
        mpLowPass["gVBuffer"] = pSrcVBuffer;
        mpLowPass["gOutputVBuffer"] = mpTmpVBuffer;
    }

    if(mpLowPass) {
        auto cb_vars = mpLowPass["PerFrameCB"];
        cb_vars["gResolution"] = resolution;

        mpLowPass->execute(pRenderContext, resolution.x, resolution.y);
    }

    // U pass
    if(!mpPassU || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("passU");
        if (mpScene) desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene ? mpScene->getSceneDefines() : Program::DefineList();
        defines.add(getValidResourceDefines(kEdgeDetectPassExtraInputChannels, renderData));
        defines.add("is_valid_gTmpDepth", mpTmpDepth != nullptr ? "1" : "0");
        defines.add("is_valid_gTmpNormal", mpTmpNormal != nullptr ? "1" : "0");
        defines.add("is_valid_gTmpMaterialID", mpTmpMaterialID != nullptr ? "1" : "0");
        defines.add("is_valid_gTmpInstanceID", mpTmpInstanceID != nullptr ? "1" : "0");
        defines.add("_DEPTH_KERNEL_HALF_SIZE", std::to_string(depthKernelHalfSize));
        defines.add("_NORMAL_KERNEL_HALF_SIZE", std::to_string(normalKernelHalfSize));
        defines.add("_MATERIAL_KERNEL_HALF_SIZE", std::to_string(materialKernelHalfSize));
        defines.add("_INSTANCE_KERNEL_HALF_SIZE", std::to_string(instanceKernelHalfSize));
        defines.add("_TRACE_DEPTH", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceDepth) ? "1" : "0");
        defines.add("_TRACE_NORMAL", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceNormal) ? "1" : "0");
        defines.add("_TRACE_MATERIAL_ID", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceMaterialID) ? "1" : "0");
        defines.add("_TRACE_INSTANCE_ID", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceInstanceID) ? "1" : "0");
        defines.add("_TRACE_ALPHA", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceAlpha) ? "1" : "0");

        mpPassU = ComputePass::create(mpDevice, desc, defines, true);
    
        if (mpScene) mpPassU["gScene"] = mpScene->getParameterBlock();
        mpPassU["gVBuffer"] = pSrcVBuffer;
        mpPassU["gDepth"] = pSrcDepth;
        mpPassU["gNormal"] = pSrcNormal;
        mpPassU["gMaterialID"] = pSrcMaterialID;
        mpPassU["gInstanceID"] = pSrcInstanceID;
        
        // Kernel textures
        mpPassU["gDepthKernelU"] = mpDepthKernelU;
        mpPassU["gDepthKernelV"] = mpDepthKernelV;

        mpPassU["gNormalKernelU"] = mpNormalKernelU;
        mpPassU["gNormalKernelV"] = mpNormalKernelV;
        
        mpPassU["gMaterialKernelU"] = mpMaterialKernelU;
        mpPassU["gMaterialKernelV"] = mpMaterialKernelV;
        
        mpPassU["gInstanceKernelU"] = mpInstanceKernelU;
        mpPassU["gInstanceKernelV"] = mpInstanceKernelV;

        // Output
        mpPassU["gTmpDepth"] = mpTmpDepth;
        mpPassU["gTmpNormal"] = mpTmpNormal;
        mpPassU["gTmpMaterialID"] = mpTmpMaterialID;
        mpPassU["gTmpInstanceID"] = mpTmpInstanceID;
    }

    auto cb_vars_u = mpPassU["PerFrameCB"];
    cb_vars_u["gResolution"] = resolution;
    cb_vars_u["gDepthKernelCenter"] = depthKernelHalfSize;
    cb_vars_u["gNormalKernelCenter"] = normalKernelHalfSize;
    cb_vars_u["gMaterialKernelCenter"] = materialKernelHalfSize;
    cb_vars_u["gInstanceKernelCenter"] = instanceKernelHalfSize;
    cb_vars_u["gDepthDistanceRange"] = mDepthDistanceRange;
    cb_vars_u["gNormalThresholdRange"] = mNormalThresholdRange;

    mpPassU->execute(pRenderContext, resolution.x, resolution.y);

    
    // V pass
    if(!mpPassV || mDirty) {
        Program::Desc desc;
        desc.addShaderLibrary(kShaderFile).setShaderModel(kShaderModel).csEntry("passV");
        if (mpScene) desc.addTypeConformances(mpScene->getTypeConformances());

        auto defines = mpScene ? mpScene->getSceneDefines() : Program::DefineList();
        defines.add(getValidResourceDefines(kEdgeDetectPassExtraInputChannels, renderData));
        defines.add("is_valid_gTmpDepth", mpTmpDepth != nullptr ? "1" : "0");
        defines.add("is_valid_gTmpNormal", mpTmpNormal != nullptr ? "1" : "0");
        defines.add("is_valid_gTmpMaterialID", mpTmpMaterialID != nullptr ? "1" : "0");
        defines.add("is_valid_gTmpInstanceID", mpTmpInstanceID != nullptr ? "1" : "0");
        defines.add("_DEPTH_KERNEL_HALF_SIZE", std::to_string(depthKernelHalfSize));
        defines.add("_NORMAL_KERNEL_HALF_SIZE", std::to_string(normalKernelHalfSize));
        defines.add("_MATERIAL_KERNEL_HALF_SIZE", std::to_string(materialKernelHalfSize));
        defines.add("_INSTANCE_KERNEL_HALF_SIZE", std::to_string(instanceKernelHalfSize));
        defines.add("_TRACE_DEPTH", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceDepth) ? "1" : "0");
        defines.add("_TRACE_NORMAL", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceNormal) ? "1" : "0");
        defines.add("_TRACE_MATERIAL_ID", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceMaterialID) ? "1" : "0");
        defines.add("_TRACE_INSTANCE_ID", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceInstanceID) ? "1" : "0");
        defines.add("_TRACE_ALPHA", is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceAlpha) ? "1" : "0");

        mpPassV = ComputePass::create(mpDevice, desc, defines, true);

        if (mpScene) mpPassV["gScene"] = mpScene->getParameterBlock();
        mpPassV["gVBuffer"] = pSrcVBuffer;
        mpPassV["gTmpDepth"] = mpTmpDepth;
        mpPassV["gTmpNormal"] = mpTmpNormal;
        mpPassU["gTmpMaterialID"] = mpTmpMaterialID;
        mpPassU["gTmpInstanceID"] = mpTmpInstanceID;

        // Kernel textures
        mpPassV["gDepthKernelU"] = mpDepthKernelU;
        mpPassV["gDepthKernelV"] = mpDepthKernelV;

        mpPassV["gNormalKernelU"] = mpNormalKernelU;
        mpPassV["gNormalKernelV"] = mpNormalKernelV;
        
        mpPassU["gMaterialKernelU"] = mpMaterialKernelU;
        mpPassU["gMaterialKernelV"] = mpMaterialKernelV;
        
        mpPassU["gInstanceKernelU"] = mpInstanceKernelU;
        mpPassU["gInstanceKernelV"] = mpInstanceKernelV;

        // Output
        mpPassV["gOutput"] = pDst;
    }

    auto cb_vars_v = mpPassV["PerFrameCB"];
    cb_vars_v["gResolution"] = resolution;
    cb_vars_v["gDepthKernelCenter"] = depthKernelHalfSize;
    cb_vars_v["gNormalKernelCenter"] = normalKernelHalfSize;
    cb_vars_v["gMaterialKernelCenter"] = materialKernelHalfSize;
    cb_vars_v["gInstanceKernelCenter"] = instanceKernelHalfSize;

    cb_vars_v["gDepthChannelMultiplyer"] = channelMultiplyer(mDepthOutputChannel);
    cb_vars_v["gNormalChannelMultiplyer"] = channelMultiplyer(mNormalOutputChannel);
    cb_vars_v["gMaterialChannelMultiplyer"] = channelMultiplyer(mMaterialOutputChannel);
    cb_vars_v["gInstanceChannelMultiplyer"] = channelMultiplyer(mInstanceOutputChannel);

    cb_vars_v["gFullAlpha"] = fullAlphaColor();

    cb_vars_v["gDepthDistanceRange"] = mDepthDistanceRange;
    cb_vars_v["gNormalThresholdRange"] = mNormalThresholdRange;
    
    mpPassV->execute(pRenderContext, resolution.x, resolution.y);

    mDirty = false;
}

float4 EdgeDetectPass::channelMultiplyer(EdgeDetectOutputChannel ch) {
    switch(ch) {
        case EdgeDetectOutputChannel::R:
            return {1.0, 0.0, 0.0, 0.0};
        case EdgeDetectOutputChannel::G:
            return {0.0, 1.0, 0.0, 0.0};
        case EdgeDetectOutputChannel::B:
            return {0.0, 0.0, 1.0, 0.0};
        case EdgeDetectOutputChannel::A:
            return {0.0, 0.0, 0.0, 1.0};
        default:
            return {1.0, 1.0, 1.0, 0.0};
    }
}

float4 EdgeDetectPass::fullAlphaColor() {
    if ((mDepthOutputChannel != EdgeDetectOutputChannel::A) && (mNormalOutputChannel != EdgeDetectOutputChannel::A) && 
        (mMaterialOutputChannel != EdgeDetectOutputChannel::A) && (mInstanceOutputChannel != EdgeDetectOutputChannel::A)) return {0.0, 0.0, 0.0, 1.0};
    return {0.0, 0.0, 0.0, 0.0};
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

    prepareBuffer(mpTmpDepth, resolution.x, resolution.y, ResourceFormat::RG32Float, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceDepth));
    prepareBuffer(mpTmpNormal, resolution.x, resolution.y, ResourceFormat::RG32Float, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceNormal));
    //prepareBuffer(mpTmpMaterialID, resolution.x, resolution.y, ResourceFormat::RG8Snorm, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceMaterialID));
    //prepareBuffer(mpTmpInstanceID, resolution.x, resolution.y, ResourceFormat::RG8Snorm, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceInstanceID));

    prepareBuffer(mpTmpMaterialID, resolution.x, resolution.y, ResourceFormat::RG16Float, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceMaterialID));
    prepareBuffer(mpTmpInstanceID, resolution.x, resolution.y, ResourceFormat::RG16Float, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceInstanceID));

    prepareBuffer(mpTmpVBuffer, resolution.x, resolution.y, HitInfo::kDefaultFormat, mLowPassFilterSize != 0);
}

void EdgeDetectPass::prepareKernelTextures() {
    if (!mDirty) return;

    static const float sobel3_du[] = {1.f, 0.f,-1.f};
    static const float sobel3_dv[] = {1.f, 2.f, 1.f};

    static const float sobel5_du[] = {1.f, 1.f, 0.f, -1.f, -1.f};
    static const float sobel5_dv[] = {1.f, 4.f, 7.f, 4.f, 1.f};

    static const float prewitt3_du[] = {1.f, 0.f,-1.f};
    static const float prewitt3_dv[] = {1.f, 1.f, 1.f};

    static const float prewitt5_du[] = {1.f, 1.f, 0.f, -1.f, -1.f};
    static const float prewitt5_dv[] = {1.f, 1.f, 1.f, 1.f, 1.f};

    auto prepareKernel = [&](Texture::SharedPtr& pBufU, Texture::SharedPtr& pBufV, uint32_t kernelSize, ResourceFormat format, EdgeDetectPass::EdgeKernelType kernelType, bool bufUsed) {
        if (!bufUsed) {pBufU = nullptr; pBufV = nullptr; return; }

        // fail safe... TODO: remove later
        if(kernelSize < 3) kernelSize = 3;
        else if (kernelSize > 3) kernelSize = 5;

        // (Re-)create buffer if needed.
        if (!pBufU || pBufU->getWidth(0) != kernelSize || pBufU->getFormat() != format ||
            !pBufV || pBufV->getWidth(0) != kernelSize || pBufV->getFormat() != format) {
            mDirty = true;

            const float* pDataU;
            const float* pDataV;
            switch(kernelType) {
                case EdgeDetectPass::EdgeKernelType::Prewitt:
                    if (kernelSize == 3) { pDataU = prewitt3_du; pDataV = prewitt3_dv; }
                    else { pDataU = prewitt5_du; pDataV = prewitt5_dv; }
                    break;
                default:
                    if (kernelSize == 3) { pDataU = sobel3_du; pDataV = sobel3_dv; }
                    else { pDataU = sobel5_du; pDataV = sobel5_dv; }
                    break;
            }

            pBufU = Texture::create1D(mpDevice, kernelSize, format, 1, 1, pDataU, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            pBufV = Texture::create1D(mpDevice, kernelSize, format, 1, 1, pDataV, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess);
            assert(pBufU && pBufV);
        }
    };

    prepareKernel(mpDepthKernelU, mpDepthKernelV, mDepthKernelSize, ResourceFormat::R32Float, mDepthKernelType, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceDepth));
    prepareKernel(mpNormalKernelU, mpNormalKernelV, mNormalKernelSize, ResourceFormat::R32Float, mNormalKernelType, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceNormal));
    prepareKernel(mpMaterialKernelU, mpMaterialKernelV, mMaterialKernelSize, ResourceFormat::R32Float, mMaterialKernelType, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceMaterialID));
    prepareKernel(mpInstanceKernelU, mpInstanceKernelV, mInstanceKernelSize, ResourceFormat::R32Float, mInstanceKernelType, is_set(mEdgeDetectFlags, EdgeDetectFlags::TraceInstanceID));
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

void EdgeDetectPass::setDepthOutputChannel(uint value) {
    if(!validChannel(value)) return;
    EdgeDetectOutputChannel channel = static_cast<EdgeDetectOutputChannel>(value);
    if(mDepthOutputChannel == channel) return;
    mDepthOutputChannel = channel;
}

void EdgeDetectPass::setNormalOutputChannel(uint value) {
    if(!validChannel(value)) return;
    EdgeDetectOutputChannel channel = static_cast<EdgeDetectOutputChannel>(value);
    if(mNormalOutputChannel == channel) return;
    mNormalOutputChannel = channel;
}

void EdgeDetectPass::setMaterialOutputChannel(uint value) {
    if(!validChannel(value)) return;
    EdgeDetectOutputChannel channel = static_cast<EdgeDetectOutputChannel>(value);
    if(mMaterialOutputChannel == channel) return;
    mMaterialOutputChannel = channel;
}

void EdgeDetectPass::setInstanceOutputChannel(uint value) {
    if(!validChannel(value)) return;
    EdgeDetectOutputChannel channel = static_cast<EdgeDetectOutputChannel>(value);
    if(mInstanceOutputChannel == channel) return;
    mInstanceOutputChannel = channel;
}

void EdgeDetectPass::setDepthKernelSize(uint size) {
    if (size > 4) size = 5;
    else if(size > 2) size = 3;
    else size = 0;

    if(mDepthKernelSize != size) {
        mDepthKernelSize = size;
        mDirty = true;
    }
}

void EdgeDetectPass::setNormalKernelSize(uint size) {
    if (size > 4) size = 5;
    else if(size > 2) size = 3;
    else size = 0;

    if(mNormalKernelSize != size) {
        mNormalKernelSize = size;
        mDirty = true;
    }
}

void EdgeDetectPass::setMaterialKernelSize(uint size) {
    if (size > 4) size = 5;
    else if(size > 2) size = 3;
    else size = 0;

    if(mMaterialKernelSize != size) {
        mMaterialKernelSize = size;
        mDirty = true;
    }
}

void EdgeDetectPass::setInstanceKernelSize(uint size) {
    if (size > 4) size = 5;
    else if(size > 2) size = 3;
    else size = 0;

    if(mInstanceKernelSize != size) {
        mInstanceKernelSize = size;
        mDirty = true;
    }
}


void EdgeDetectPass::setLowPassFilterSize(uint size) {
    if (size > 4) size = 5;
    else if(size > 2) size = 3;
    else size = 0;

    if(mLowPassFilterSize != size) {
        mLowPassFilterSize = size;
        mDirty = true;
    }
}

void EdgeDetectPass::setDepthDistanceRange(float2 range) { 
    mDepthDistanceRange.x = std::min(range.x, range.y);
    mDepthDistanceRange.y = std::max(range.x, range.y); 
}


void EdgeDetectPass::setNormalThresholdRange(float2 range) { 
    mNormalThresholdRange.x = std::min(range.x, range.y);
    mNormalThresholdRange.y = std::max(range.x, range.y); 
}

void EdgeDetectPass::setEdgeDetectFlags(EdgeDetectFlags flags) {
    if(mEdgeDetectFlags != flags) {
        mEdgeDetectFlags = flags;
        mDirty = true;
    }
}

void EdgeDetectPass::setTraceDepth(bool state) {
    auto prevFlags = mEdgeDetectFlags;
    if (state) setEdgeDetectFlags(mEdgeDetectFlags | EdgeDetectFlags::TraceDepth);
    else setEdgeDetectFlags(mEdgeDetectFlags & ~EdgeDetectFlags::TraceDepth);
    mDirty = prevFlags != mEdgeDetectFlags;
}

void EdgeDetectPass::setTraceNormal(bool state) {
    auto prevFlags = mEdgeDetectFlags;
    if (state) setEdgeDetectFlags(mEdgeDetectFlags | EdgeDetectFlags::TraceNormal);
    else setEdgeDetectFlags(mEdgeDetectFlags & ~EdgeDetectFlags::TraceNormal);
    mDirty = prevFlags != mEdgeDetectFlags;
}

void EdgeDetectPass::setTraceMaterialID(bool state) {
    auto prevFlags = mEdgeDetectFlags;
    if (state) setEdgeDetectFlags(mEdgeDetectFlags | EdgeDetectFlags::TraceMaterialID);
    else setEdgeDetectFlags(mEdgeDetectFlags & ~EdgeDetectFlags::TraceMaterialID);
    mDirty = prevFlags != mEdgeDetectFlags;
}

void EdgeDetectPass::setTraceInstanceID(bool state) {
    auto prevFlags = mEdgeDetectFlags;
    if (state) setEdgeDetectFlags(mEdgeDetectFlags | EdgeDetectFlags::TraceInstanceID);
    else setEdgeDetectFlags(mEdgeDetectFlags & ~EdgeDetectFlags::TraceInstanceID);
    mDirty = prevFlags != mEdgeDetectFlags;
}

void EdgeDetectPass::setTraceAlpha(bool state) {
    auto prevFlags = mEdgeDetectFlags;
    if (state) setEdgeDetectFlags(mEdgeDetectFlags | EdgeDetectFlags::TraceAlpha);
    else setEdgeDetectFlags(mEdgeDetectFlags & ~EdgeDetectFlags::TraceAlpha);
    mDirty = prevFlags != mEdgeDetectFlags;
}