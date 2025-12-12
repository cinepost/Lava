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
#include "Falcor/stdafx.h"

#include "FBO.h"
#include "Texture.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/FBO.h"
#include "Falcor/Core/API/BlitContext.h"
#include "Falcor/Core/API/BlitToBufferContext.h"
#include "Falcor/Core/API/BlitToBufferReduction.slangh"

#include "Falcor/Core/State/GraphicsState.h"
#include "Falcor/RenderGraph/BasePasses/FullScreenPass.h"
#include "Falcor/RenderGraph/BasePasses/ComputePass.h"

#include "RenderContext.h"


namespace Falcor {

namespace {

void ensureFboAttachmentResourceStates(RenderContext* pCtx, Fbo* pFbo) {
    printf("ensureFboAttachmentResourceStates\n");

    if (pFbo) {
        for (uint32_t i = 0; i < pFbo->getMaxColorTargetCount(); i++) {
            auto pTexture = pFbo->getColorTexture(i);
            if (pTexture) {
                auto pRTV = pFbo->getRenderTargetView(i);
                pCtx->resourceBarrier(pTexture.get(), Resource::State::RenderTarget, &pRTV->getViewInfo());
            }
        }

        auto& pTexture = pFbo->getDepthStencilTexture();
        
        if (pTexture) {
            auto pDSV = pFbo->getDepthStencilView();
            pCtx->resourceBarrier(pTexture.get(), Resource::State::DepthStencil, &pDSV->getViewInfo());
        }
    }

    printf("ensureFboAttachmentResourceStates done\n");
}


gfx::PrimitiveTopology getGFXPrimitiveTopology(Vao::Topology topology) {
    switch (topology) {
        case Vao::Topology::Undefined:
            return gfx::PrimitiveTopology::TriangleList;
        case Vao::Topology::PointList:
            return gfx::PrimitiveTopology::PointList;
        case Vao::Topology::LineList:
            return gfx::PrimitiveTopology::LineList;
        case Vao::Topology::LineStrip:
            return gfx::PrimitiveTopology::LineStrip;
        case Vao::Topology::TriangleList:
            return gfx::PrimitiveTopology::TriangleList;
        case Vao::Topology::TriangleStrip:
            return gfx::PrimitiveTopology::TriangleStrip;
        default:
            FALCOR_UNREACHABLE();
            return gfx::PrimitiveTopology::TriangleList;
    }
}

gfx::AccelerationStructureCopyMode getGFXAcclerationStructureCopyMode(RenderContext::RtAccelerationStructureCopyMode mode) {
    switch (mode) {
        case RenderContext::RtAccelerationStructureCopyMode::Clone:
            return gfx::AccelerationStructureCopyMode::Clone;
        case RenderContext::RtAccelerationStructureCopyMode::Compact:
            return gfx::AccelerationStructureCopyMode::Compact;
        default:
            FALCOR_UNREACHABLE();
            return gfx::AccelerationStructureCopyMode::Clone;
    }
}

} // namespace

RenderContext::RenderContext(Device* pDevice, gfx::ICommandQueue* pQueue): ComputeContext(pDevice, pQueue) {
    mpBlitContext = std::make_unique<BlitContext>(pDevice);
    mpBlitToBufferContext = std::make_unique<BlitToBufferContext>(pDevice);
}

RenderContext::~RenderContext() {

}

void RenderContext::clearFbo(const Fbo* pFbo, const float4& color, float depth, uint8_t stencil, FboAttachmentType flags) {
    bool hasDepthStencilTexture = pFbo->getDepthStencilTexture() != nullptr;
    ResourceFormat depthStencilFormat = hasDepthStencilTexture ? pFbo->getDepthStencilTexture()->getFormat() : ResourceFormat::Unknown;

    bool clearColor = (flags & FboAttachmentType::Color) != FboAttachmentType::None;
    bool clearDepth = hasDepthStencilTexture && ((flags & FboAttachmentType::Depth) != FboAttachmentType::None);
    bool clearStencil = hasDepthStencilTexture && ((flags & FboAttachmentType::Stencil) != FboAttachmentType::None) && isStencilFormat(depthStencilFormat);

    if (clearColor) {
        for (uint32_t i = 0; i < Fbo::getMaxColorTargetCount(); i++) {
            if (pFbo->getColorTexture(i)) {
                clearRtv(pFbo->getRenderTargetView(i).get(), color);
            }
        }
    }

    if (clearDepth || clearStencil) {
        clearDsv(pFbo->getDepthStencilView().get(), depth, stencil, clearDepth, clearStencil);
    }
}

void RenderContext::clearTexture(Texture* pTexture, const float4& clearColor) {
    assert(pTexture);

    // Check that the format is either Unorm, Snorm or float
    auto format = pTexture->getFormat();
    auto fType = getFormatType(format);
    if (fType == FormatType::Sint || fType == FormatType::Uint || fType == FormatType::Unknown) {
        LLOG_WRN << "RenderContext::clearTexture() - Unsupported texture format " << to_string(format) << ". The texture format must be a normalized or floating-point format";
        return;
    }

    auto bindFlags = pTexture->getBindFlags();
    // Select the right clear based on the texture's binding flags
    if (is_set(bindFlags, ResourceBindFlags::RenderTarget)) {
        clearRtv(pTexture->getRTV().get(), clearColor);
    } else if (is_set(bindFlags, ResourceBindFlags::UnorderedAccess)) {
        clearUAV(pTexture->getUAV().get(), clearColor);
    } else if (is_set(bindFlags, ResourceBindFlags::DepthStencil)) {
        if (isStencilFormat(format) && (clearColor.y != 0)) {
            LLOG_WRN << "RenderContext::clearTexture() - when clearing a depth-stencil texture the stencil value(clearColor.y) must be 0. Received " << std::to_string(clearColor.y) << ". Forcing stencil to 0";
        }
        clearDsv(pTexture->getDSV().get(), clearColor.r, 0);
    } else {
        LLOG_WRN << "Texture::clear() - The texture does not have a bind flag that allows us to clear!";
    }
}

void RenderContext::submit(bool wait) {
    ComputeContext::submit(wait);
    mpLastBoundGraphicsVars = nullptr;
}

void RenderContext::blit(const ShaderResourceView::SharedPtr& pSrc, const RenderTargetView::SharedPtr& pDst, uint4 srcRect, uint4 dstRect, Sampler::Filter filter) {
    const Sampler::ReductionMode componentsReduction[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard };
    const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f) };

    blit(pSrc, pDst, srcRect, dstRect, filter, componentsReduction, componentsTransform);
}


void RenderContext::blit(const ShaderResourceView::SharedPtr& pSrc, const RenderTargetView::SharedPtr& pDst, uint4 srcRect, uint4 dstRect, Sampler::Filter filter, const Sampler::ReductionMode componentsReduction[4], const float4 componentsTransform[4]) {
    assert(mpBlitContext);
    auto& blitCtx = *mpBlitContext;

    // Fetch textures from views.
    assert(pSrc && pDst);
    auto pSrcResource = pSrc->getResource();
    auto pDstResource = pDst->getResource();
    if (pSrcResource->getType() == Resource::Type::Buffer || pDstResource->getType() == Resource::Type::Buffer) {
        throw std::runtime_error("RenderContext::blit does not support buffers");
    }

    const Texture* pSrcTexture = dynamic_cast<const Texture*>(pSrcResource);
    const Texture* pDstTexture = dynamic_cast<const Texture*>(pDstResource);
    assert(pSrcTexture != nullptr && pDstTexture != nullptr);

    // Clamp rectangles to the dimensions of the source/dest views.
    const uint32_t srcMipLevel = pSrc->getViewInfo().mostDetailedMip;
    const uint32_t dstMipLevel = pDst->getViewInfo().mostDetailedMip;
    const uint2 srcSize(pSrcTexture->getWidth(srcMipLevel), pSrcTexture->getHeight(srcMipLevel));
    const uint2 dstSize(pDstTexture->getWidth(dstMipLevel), pDstTexture->getHeight(dstMipLevel));

    srcRect.z = std::min(srcRect.z, srcSize.x);
    srcRect.w = std::min(srcRect.w, srcSize.y);
    dstRect.z = std::min(dstRect.z, dstSize.x);
    dstRect.w = std::min(dstRect.w, dstSize.y);

    if (srcRect.x >= srcRect.z || srcRect.y >= srcRect.w || dstRect.x >= dstRect.z || dstRect.y >= dstRect.w) {
        LLOG_DBG << "RenderContext::blit() called with out-of-bounds src/dst rectangle";
        return; // No blit necessary
    }

    // Determine the type of blit.
    const uint32_t sampleCount = pSrcTexture->getSampleCount();
    const bool complexBlit =
        !((componentsReduction[0] == Sampler::ReductionMode::Standard) && (componentsReduction[1] == Sampler::ReductionMode::Standard) && (componentsReduction[2] == Sampler::ReductionMode::Standard) && (componentsReduction[3] == Sampler::ReductionMode::Standard) &&
            (componentsTransform[0] == float4(1.0f, 0.0f, 0.0f, 0.0f)) && (componentsTransform[1] == float4(0.0f, 1.0f, 0.0f, 0.0f)) && (componentsTransform[2] == float4(0.0f, 0.0f, 1.0f, 0.0f)) && (componentsTransform[3] == float4(0.0f, 0.0f, 0.0f, 1.0f)));

    auto isFullView = [](const auto& view, const Texture* tex) {
        const auto& info = view->getViewInfo();
        return info.mostDetailedMip == 0 && info.firstArraySlice == 0 && info.mipCount == tex->getMipCount() && info.arraySize == tex->getArraySize();
    };
    const bool srcFullRect = srcRect.x == 0 && srcRect.y == 0 && srcRect.z == srcSize.x && srcRect.w == srcSize.y;
    const bool dstFullRect = dstRect.x == 0 && dstRect.y == 0 && dstRect.z == dstSize.x && dstRect.w == dstSize.y;

    const bool fullCopy =
        !complexBlit &&
        isFullView(pSrc, pSrcTexture) && srcFullRect &&
        isFullView(pDst, pDstTexture) && dstFullRect &&
        pSrcTexture->compareDesc(pDstTexture);

    // Take fast path to copy the entire resource if possible. This has many requirements;
    // the source/dest must have identical size/format/etc. and the views and rects must cover the full resources.
    if (fullCopy) {
        copyResource(pDstResource, pSrcResource);
        return;
    }

    // At this point, we have to run a shader to perform the blit.
    // The implementation has some limitations. Check that all requirements are fullfilled.

    // Complex blit doesn't work with multi-sampled textures.
    if (complexBlit && sampleCount > 1) throw std::runtime_error("RenderContext::blit() does not support sample count > 1 for complex blit");

    // Validate source format. Only single-sampled basic blit handles integer source format.
    // All variants support casting to integer destination format.
    if (isIntegerFormat(pSrcTexture->getFormat())) {
        if (sampleCount > 1) throw std::runtime_error("RenderContext::blit() requires non-integer source format for multi-sampled textures");
        else if (complexBlit) throw std::runtime_error("RenderContext::blit() requires non-integer source format for complex blit");
    }

    // Blit does not support texture arrays or mip maps.
    if (!(pSrc->getViewInfo().arraySize == 1 && pSrc->getViewInfo().mipCount == 1) ||
        !(pDst->getViewInfo().arraySize == 1 && pDst->getViewInfo().mipCount == 1))
    {
        throw std::runtime_error("RenderContext::blit() does not support texture arrays or mip maps");
    }

    // Configure program.
    blitCtx.mpPass->addDefine("SAMPLE_COUNT", std::to_string(sampleCount));
    blitCtx.mpPass->addDefine("COMPLEX_BLIT", complexBlit ? "1" : "0");
    blitCtx.mpPass->addDefine("SRC_INT", isIntegerFormat(pSrcTexture->getFormat()) ? "1" : "0");
    blitCtx.mpPass->addDefine("DST_INT", isIntegerFormat(pDstTexture->getFormat()) ? "1" : "0");

    if (complexBlit) {
        assert(sampleCount <= 1);

        Sampler::SharedPtr usedSampler[4];
        for (uint32_t i = 0; i < 4; i++) {
            assert(componentsReduction[i] != Sampler::ReductionMode::Comparison);        // Comparison mode not supported.

            if (componentsReduction[i] == Sampler::ReductionMode::Min) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearMinSampler : blitCtx.mpPointMinSampler;
            else if (componentsReduction[i] == Sampler::ReductionMode::Max) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearMaxSampler : blitCtx.mpPointMaxSampler;
            else usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearSampler : blitCtx.mpPointSampler;
        }

        blitCtx.mpPass->getVars()->setSampler("gSamplerR", usedSampler[0]);
        blitCtx.mpPass->getVars()->setSampler("gSamplerG", usedSampler[1]);
        blitCtx.mpPass->getVars()->setSampler("gSamplerB", usedSampler[2]);
        blitCtx.mpPass->getVars()->setSampler("gSamplerA", usedSampler[3]);

        // Parameters for complex blit
        for (uint32_t i = 0; i < 4; i++) {
            if (blitCtx.prevComponentsTransform[i] != componentsTransform[i]) {
                blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.compTransVarOffset[i], componentsTransform[i]);
                blitCtx.prevComponentsTransform[i] = componentsTransform[i];
            }
        }
    } else {
        blitCtx.mpPass->getVars()->setSampler("gSampler", (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearSampler : blitCtx.mpPointSampler);
    }

    float2 srcRectOffset(0.0f);
    float2 srcRectScale(1.0f);
    if (!srcFullRect) {
        srcRectOffset = float2(srcRect.x, srcRect.y) / float2(srcSize);
        srcRectScale = float2(srcRect.z - srcRect.x, srcRect.w - srcRect.y) / float2(srcSize);
    }

    GraphicsState::Viewport dstViewport(0.0f, 0.0f, (float)dstSize.x, (float)dstSize.y, 0.0f, 1.0f);
    if (!dstFullRect) {
        dstViewport = GraphicsState::Viewport((float)dstRect.x, (float)dstRect.y, (float)(dstRect.z - dstRect.x), (float)(dstRect.w - dstRect.y), 0.0f, 1.0f);
    }

    // Update buffer/state
    if (srcRectOffset != blitCtx.prevSrcRectOffset) {
        blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.offsetVarOffset, srcRectOffset);
        blitCtx.prevSrcRectOffset = srcRectOffset;
    }

    if (srcRectScale != blitCtx.prevSrcReftScale) {
        blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.scaleVarOffset, srcRectScale);
        blitCtx.prevSrcReftScale = srcRectScale;
    }

    Texture::SharedPtr pSharedTex = pDstResource->asTexture();
    blitCtx.mpFbo->attachColorTarget(pSharedTex, 0, pDst->getViewInfo().mostDetailedMip, pDst->getViewInfo().firstArraySlice, pDst->getViewInfo().arraySize);
    blitCtx.mpPass->getVars()->setSrv(blitCtx.texBindLoc, pSrc);
    blitCtx.mpPass->getState()->setViewport(0, dstViewport);
    blitCtx.mpPass->execute(this, blitCtx.mpFbo, false);

    // Release the resources we bound
    blitCtx.mpFbo->attachColorTarget(nullptr, 0);
    blitCtx.mpPass->getVars()->setSrv(blitCtx.texBindLoc, nullptr);
}

void RenderContext::blitToBuffer(const ShaderResourceView::SharedPtr& pSrc, const Buffer::SharedPtr& pBuffer, uint32_t bufferWidthStrideInPixels, Falcor::ResourceFormat dstFormat, uint4 srcRect, uint4 dstRect, Sampler::Filter filter, const Sampler::ReductionMode componentsReduction[4], const float4 componentsTransform[4]) {
    assert(mpBlitToBufferContext);
    auto& blitCtx = *mpBlitToBufferContext;

    // Fetch textures from views.
    assert(pSrc && pBuffer);
    auto pSrcResource = pSrc->getResource();
    if (pSrcResource->getType() == Resource::Type::Buffer) {
        throw std::runtime_error("RenderContext::blitToBuffer() does not support buffer source !");
    }

    // Check if buffer size is divisable by dstFormat size.
    if(pBuffer->getSize() % getFormatBytesPerBlock(dstFormat) != 0) {
        throw std::runtime_error("RenderContext::blitToBuffer() distination buffer size and dstFormat size are not divisable !");
    }
    
    const Texture* pSrcTexture = dynamic_cast<const Texture*>(pSrcResource);
    assert(pSrcTexture != nullptr && pBuffer != nullptr);

    // Clamp rectangles to the dimensions of the source/dest views.
    const uint32_t srcMipLevel = pSrc->getViewInfo().mostDetailedMip;
    const uint2 srcSize(pSrcTexture->getWidth(srcMipLevel), pSrcTexture->getHeight(srcMipLevel));

    const uint32_t bufferWidthStrideBytes = getFormatBytesPerBlock(dstFormat) * bufferWidthStrideInPixels;
    
    if(pBuffer->getSize() % bufferWidthStrideBytes != 0) {
        throw std::runtime_error("RenderContext::blitToBuffer() distination buffer size does not fit requested geometry !");
    }

    const uint32_t bufferHeight = pBuffer->getSize() / bufferWidthStrideBytes;
    const uint2 dstSize(bufferWidthStrideInPixels, bufferHeight);

    const float2 srcHalfPixelSize(.5f / srcSize.x, .5f / srcSize.y);

    srcRect.z = std::min(srcRect.z, srcSize.x);
    srcRect.w = std::min(srcRect.w, srcSize.y);
    dstRect.z = std::min(dstRect.z, dstSize.x);
    dstRect.w = std::min(dstRect.w, dstSize.y);

    if (srcRect.x >= srcRect.z || srcRect.y >= srcRect.w ||
        dstSize.x == 0 || dstSize.y == 0 || ((dstSize.x * dstSize.y) == 0)) // TODO: check dstSize is smaller or equal to actual buffer size
    {
        LLOG_DBG << "RenderContext::blitToBuffer() called with out-of-bounds src/dst rectangle";
        return; // No blit necessary
    }

    // Determine the type of blit.
    const uint32_t sampleCount = pSrcTexture->getSampleCount();
    const bool complexBlit =
        !((componentsReduction[0] == Sampler::ReductionMode::Standard) && (componentsReduction[1] == Sampler::ReductionMode::Standard) && (componentsReduction[2] == Sampler::ReductionMode::Standard) && (componentsReduction[3] == Sampler::ReductionMode::Standard) &&
            (componentsTransform[0] == float4(1.0f, 0.0f, 0.0f, 0.0f)) && (componentsTransform[1] == float4(0.0f, 1.0f, 0.0f, 0.0f)) && (componentsTransform[2] == float4(0.0f, 0.0f, 1.0f, 0.0f)) && (componentsTransform[3] == float4(0.0f, 0.0f, 0.0f, 1.0f)));

    auto isFullTextureView = [](const auto& view, const Texture* tex) {
        const auto& info = view->getViewInfo();
        return info.mostDetailedMip == 0 && info.firstArraySlice == 0 && info.mipCount == tex->getMipCount() && info.arraySize == tex->getArraySize();
    };

    auto isFullBufferView = [](const auto& dstSize, const Buffer::SharedPtr& buff) {
        return (dstSize.x * dstSize.y) == buff->getElementCount();
    };

    const bool srcFullRect = srcRect.x == 0 && srcRect.y == 0 && srcRect.z == srcSize.x && srcRect.w == srcSize.y;
    const bool dstFullRect = dstRect.x == 0 && dstRect.y == 0 && dstRect.z == dstSize.x && dstRect.w == dstSize.y;

    const bool fullCopy =
        !complexBlit &&
        isFullTextureView(pSrc, pSrcTexture) && srcFullRect &&
        isFullBufferView(dstSize, pBuffer) && dstFullRect &&
        (pSrcTexture->getFormat() == pBuffer->getFormat());

    // Take fast path to copy the entire resource if possible. This has many requirements;
    // the source/dest must have identical size/format/etc. and the views and rects must cover the full resources.
    
    if (fullCopy) {
        copyResource(pBuffer.get(), pSrcResource);
        return;
    }

    // At this point, we have to run a shader to perform the blit.
    // The implementation has some limitations. Check that all requirements are fullfilled.

    // Complex blit doesn't work with multi-sampled textures.
    if (complexBlit && sampleCount > 1) throw std::runtime_error("RenderContext::blitToBuffer() does not support sample count > 1 for complex blit");

    // Validate source format. Only single-sampled basic blit handles integer source format.
    // All variants support casting to integer destination format.
    if (isIntegerFormat(pSrcTexture->getFormat())) {
        if (sampleCount > 1) throw std::runtime_error("RenderContext::blitToBuffer() requires non-integer source format for multi-sampled textures");
        else if (complexBlit) throw std::runtime_error("RenderContext::blitToBuffer() requires non-integer source format for complex blit");
    }

    // Blit does not support texture arrays or mip maps.
    if (!(pSrc->getViewInfo().arraySize == 1 && pSrc->getViewInfo().mipCount == 1)) {
        throw std::runtime_error("RenderContext::blitToBuffer() does not support texture arrays or mip maps");
    }

    bool isDstHalfFormat = false;
    uint32_t outputPixelStrideBytes = getFormatBytesPerBlock(dstFormat);

    // Check output buffer format.
    uint32_t formatType = FORMAT_TYPE_UNKNOWN;
    switch (getFormatType(dstFormat)) {
        case FormatType::Float:
            for(uint32_t i = 0; i < getFormatChannelCount(dstFormat); i++) {
                if (getNumChannelBits(dstFormat, i) == 0) continue;
                if (getNumChannelBits(dstFormat, i) == 16) {
                    isDstHalfFormat = true;
                } else {
                    isDstHalfFormat = false;
                }
            }
            formatType = FORMAT_TYPE_FLOAT;
            break;
        case FormatType::Unorm:
            formatType = FORMAT_TYPE_UNORM;
            break;
        case FormatType::Snorm:
            formatType = FORMAT_TYPE_SNORM;
            break;
        case FormatType::Sint:
            formatType = FORMAT_TYPE_SINT;
            break;
        case FormatType::Uint:
            formatType = FORMAT_TYPE_UINT;
            break;
        default:
            LLOG_ERR << "RenderContext::blitToBuffer() - Output buffer format unsupported. Aborting.";
            return;
    }

    // Configure program.
    blitCtx.mpPass->addDefine("SAMPLE_COUNT", std::to_string(sampleCount));
    blitCtx.mpPass->addDefine("COMPLEX_BLIT", complexBlit ? "1" : "0");
    blitCtx.mpPass->addDefine("SRC_INT", isIntegerFormat(pSrcTexture->getFormat()) ? "1" : "0");
    blitCtx.mpPass->addDefine("DST_INT", isIntegerFormat(dstFormat) ? "1" : "0");
    blitCtx.mpPass->addDefine("DST_HALF_FLOAT", isDstHalfFormat ? "1" : "0");
    blitCtx.mpPass->addDefine("FORMAT_TYPE", std::to_string(formatType));
    blitCtx.mpPass->addDefine("PIXEL_STRIDE_BYTES", std::to_string(outputPixelStrideBytes));

    LLOG_TRC << "DST INT " << ( isIntegerFormat(dstFormat) ? "1" : "0");
    LLOG_TRC << "DST FORMAT " << to_string(dstFormat);
    LLOG_TRC << "SAMPLE COUNT " << std::to_string(sampleCount);
    LLOG_TRC << "PIXEL_STRIDE_BYTES " << std::to_string(outputPixelStrideBytes);
    LLOG_TRC << "FORMAT_TYPE " << std::to_string(formatType);

    if (complexBlit) {

        LLOG_DBG << "complexBlit";
        assert(sampleCount <= 1);

        Sampler::SharedPtr usedSampler[4];
        for (uint32_t i = 0; i < 4; i++) {
            assert(componentsReduction[i] != Sampler::ReductionMode::Comparison);        // Comparison mode not supported.

            if (componentsReduction[i] == Sampler::ReductionMode::Min) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearMinSampler : blitCtx.mpPointMinSampler;
            else if (componentsReduction[i] == Sampler::ReductionMode::Max) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearMaxSampler : blitCtx.mpPointMaxSampler;
            else usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearSampler : blitCtx.mpPointSampler;
        }

        blitCtx.mpPass->getVars()->setSampler("gSamplerR", usedSampler[0]);
        blitCtx.mpPass->getVars()->setSampler("gSamplerG", usedSampler[1]);
        blitCtx.mpPass->getVars()->setSampler("gSamplerB", usedSampler[2]);
        blitCtx.mpPass->getVars()->setSampler("gSamplerA", usedSampler[3]);

        // Parameters for complex blit
        for (uint32_t i = 0; i < 4; i++) {
            if (blitCtx.prevComponentsTransform[i] != componentsTransform[i]) {
                blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.compTransVarOffset[i], componentsTransform[i]);
                blitCtx.prevComponentsTransform[i] = componentsTransform[i];
            }
        }
    } else {
        LLOG_DBG << "non complexBlit";
        blitCtx.mpPass->getVars()->setSampler("gSampler", (filter == Sampler::Filter::Linear) ? blitCtx.mpLinearSampler : blitCtx.mpPointSampler);
    }
    
    float2 srcRectOffset(0.0f);
    float2 srcRectScale(1.0f);
    if (!srcFullRect) {
        srcRectOffset = float2(srcRect.x, srcRect.y) / float2(srcSize);
        srcRectScale = float2(srcRect.z - srcRect.x, srcRect.w - srcRect.y) / float2(srcSize);
    }

    // Update buffer/state
    if (srcRectOffset != blitCtx.prevSrcRectOffset) {
        blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.offsetVarOffset, srcRectOffset);
        blitCtx.prevSrcRectOffset = srcRectOffset;
    }

    if (srcRectScale != blitCtx.prevSrcReftScale) {
        blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.scaleVarOffset, srcRectScale);
        blitCtx.prevSrcReftScale = srcRectScale;
    }

    blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.resolutionVarOffset, dstSize);
    blitCtx.mpBlitParamsBuffer->setVariable(blitCtx.srcPixelHalfSizeVarOffset, srcHalfPixelSize);
    
    blitCtx.mpPass->getVars()->setSrv(blitCtx.texBindLoc, pSrc);
    blitCtx.mpPass->getVars()->setBuffer(blitCtx.buffBindLoc, pBuffer);
    
    LLOG_TRC << "blitToBuffer::execute()";
    blitCtx.mpPass->execute(this, dstSize.x, dstSize.y);

    // Release the resources we bound
    blitCtx.mpPass->getVars()->setSrv(blitCtx.texBindLoc, nullptr);
    blitCtx.mpPass->getVars()->setBuffer(blitCtx.buffBindLoc, nullptr);
}

void RenderContext::clearRtv(const RenderTargetView* pRtv, const float4& color) {
    resourceBarrier(pRtv->getResource(), Resource::State::RenderTarget);
    gfx::ClearValue clearValue = {};
    memcpy(clearValue.color.floatValues, &color, sizeof(float) * 4);
    auto encoder = getLowLevelData()->getResourceCommandEncoder();
    encoder->clearResourceView(pRtv->getGfxResourceView(), &clearValue, gfx::ClearResourceViewFlags::FloatClearValues);
    mCommandsPending = true;
}

void RenderContext::clearDsv(const DepthStencilView* pDsv, float depth, uint8_t stencil, bool clearDepth, bool clearStencil) {
    resourceBarrier(pDsv->getResource(), Resource::State::DepthStencil);
    gfx::ClearValue clearValue = {};
    clearValue.depthStencil.depth = depth;
    clearValue.depthStencil.stencil = stencil;
    auto encoder = getLowLevelData()->getResourceCommandEncoder();
    gfx::ClearResourceViewFlags::Enum flags = gfx::ClearResourceViewFlags::None;
    if (clearDepth) flags = (gfx::ClearResourceViewFlags::Enum)((int)flags | gfx::ClearResourceViewFlags::ClearDepth);
    if (clearStencil) flags = (gfx::ClearResourceViewFlags::Enum)((int)flags | gfx::ClearResourceViewFlags::ClearStencil);
    encoder->clearResourceView(pDsv->getGfxResourceView(), &clearValue, flags);
    mCommandsPending = true;
}

void RenderContext::drawInstanced(GraphicsState* pState, ProgramVars* pVars, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
    auto encoder = drawCallCommon(pState, pVars);
    encoder->drawInstanced(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
    mCommandsPending = true;
}

void RenderContext::draw(GraphicsState* pState, ProgramVars* pVars, uint32_t vertexCount, uint32_t startVertexLocation) {
    auto encoder = drawCallCommon(pState, pVars);
    encoder->draw(vertexCount, startVertexLocation);
    mCommandsPending = true;
}

void RenderContext::drawIndexedInstanced(GraphicsState* pState, ProgramVars* pVars, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
    auto encoder = drawCallCommon(pState, pVars);
    encoder->drawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
    mCommandsPending = true;
}

void RenderContext::drawIndexed(GraphicsState* pState, ProgramVars* pVars, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) {
    auto encoder = drawCallCommon(pState, pVars);
    encoder->drawIndexed(indexCount, startIndexLocation, baseVertexLocation);
    mCommandsPending = true;
}

void RenderContext::drawIndirect(GraphicsState* pState, ProgramVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset) {
    resourceBarrier(pArgBuffer, Resource::State::IndirectArg);
    auto encoder = drawCallCommon(pState, pVars);
    encoder->drawIndirect(
        maxCommandCount,
        pArgBuffer->getGfxBufferResource(),
        argBufferOffset,
        pCountBuffer ? pCountBuffer->getGfxBufferResource() : nullptr,
        countBufferOffset);
    mCommandsPending = true;
}

void RenderContext::drawIndexedIndirect(GraphicsState* pState, ProgramVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset) {
    resourceBarrier(pArgBuffer, Resource::State::IndirectArg);
    auto encoder = drawCallCommon(pState, pVars);
    FALCOR_GFX_CALL(encoder->drawIndexedIndirect(
        maxCommandCount,
        pArgBuffer->getGfxBufferResource(),
        argBufferOffset,
        pCountBuffer ? pCountBuffer->getGfxBufferResource() : nullptr,
        countBufferOffset
    ));
    mCommandsPending = true;
}

void RenderContext::raytrace(Program* pProgram, RtProgramVars* pVars, uint32_t width, uint32_t height, uint32_t depth) {
    auto pRtso = pProgram->getRtso(pVars);

    pVars->prepareShaderTable(this, pRtso.get());
    pVars->prepareDescriptorSets(this);

    auto rtEncoder = mpLowLevelData->getRayTracingCommandEncoder();
    FALCOR_GFX_CALL(rtEncoder->bindPipelineWithRootObject(pRtso->getGfxPipelineState(), pVars->getShaderObject()));
    FALCOR_GFX_CALL(rtEncoder->dispatchRays(0, pVars->getShaderTable(), width, height, depth));
    mCommandsPending = true;
}

void RenderContext::resolveSubresource(const Texture::SharedPtr& pSrc, uint32_t srcSubresource, const Texture::SharedPtr& pDst, uint32_t dstSubresource) {
    // TODO it would be better to just use barriers on the subresources.
    resourceBarrier(pSrc.get(), Resource::State::ResolveSource);
    resourceBarrier(pDst.get(), Resource::State::ResolveDest);

    auto resourceEncoder = getLowLevelData()->getResourceCommandEncoder();
    gfx::SubresourceRange srcRange = {};
    srcRange.baseArrayLayer = pSrc->getSubresourceArraySlice(srcSubresource);
    srcRange.layerCount = 1;
    srcRange.mipLevel = pSrc->getSubresourceMipLevel(srcSubresource);
    srcRange.mipLevelCount = 1;

    gfx::SubresourceRange dstRange = {};
    dstRange.baseArrayLayer = pDst->getSubresourceArraySlice(dstSubresource);
    dstRange.layerCount = 1;
    dstRange.mipLevel = pDst->getSubresourceMipLevel(dstSubresource);
    dstRange.mipLevelCount = 1;

    resourceEncoder->resolveResource(
        pSrc->getGfxTextureResource(),
        gfx::ResourceState::ResolveSource,
        srcRange,
        pDst->getGfxTextureResource(),
        gfx::ResourceState::ResolveDestination,
        dstRange);
    mCommandsPending = true;
}

void RenderContext::resolveResource(const Texture::SharedPtr& pSrc, const Texture::SharedPtr& pDst) {
    resourceBarrier(pSrc.get(), Resource::State::ResolveSource);
    resourceBarrier(pDst.get(), Resource::State::ResolveDest);

    auto resourceEncoder = getLowLevelData()->getResourceCommandEncoder();

    gfx::SubresourceRange srcRange = {};
    srcRange.layerCount = pSrc->getArraySize();
    srcRange.mipLevelCount = pSrc->getMipCount();
    gfx::SubresourceRange dstRange = {};
    dstRange.layerCount = pDst->getArraySize();
    dstRange.mipLevelCount = pDst->getMipCount();

    resourceEncoder->resolveResource(
        pSrc->getGfxTextureResource(),
        gfx::ResourceState::ResolveSource,
        srcRange,
        pDst->getGfxTextureResource(),
        gfx::ResourceState::ResolveDestination,
        dstRange
    );
    mCommandsPending = true;
}

void RenderContext::buildAccelerationStructure(const RtAccelerationStructure::BuildDesc& desc, uint32_t postBuildInfoCount, RtAccelerationStructurePostBuildInfoDesc* pPostBuildInfoDescs) {
    GFXAccelerationStructureBuildInputsTranslator translator = {};

    gfx::IAccelerationStructure::BuildDesc buildDesc = {};
    buildDesc.dest = desc.dest->getGfxAccelerationStructure();
    buildDesc.scratchData = desc.scratchData;
    buildDesc.source = desc.source ? desc.source->getGfxAccelerationStructure() : nullptr;
    buildDesc.inputs = translator.translate(desc.inputs);

    std::vector<gfx::AccelerationStructureQueryDesc> queryDescs(postBuildInfoCount);
    for (uint32_t i = 0; i < postBuildInfoCount; i++) {
        queryDescs[i].firstQueryIndex = pPostBuildInfoDescs[i].index;
        queryDescs[i].queryPool = pPostBuildInfoDescs[i].pool->getGFXQueryPool();
        queryDescs[i].queryType = getGFXAccelerationStructurePostBuildQueryType(pPostBuildInfoDescs[i].type);
    }
    auto rtEncoder = getLowLevelData()->getRayTracingCommandEncoder();
    rtEncoder->buildAccelerationStructure(buildDesc, (int)postBuildInfoCount, queryDescs.data());
    mCommandsPending = true;
}

void RenderContext::copyAccelerationStructure(RtAccelerationStructure* dest, RtAccelerationStructure* source, RenderContext::RtAccelerationStructureCopyMode mode) {
    auto rtEncoder = getLowLevelData()->getRayTracingCommandEncoder();
    rtEncoder->copyAccelerationStructure(dest->getGfxAccelerationStructure(), source->getGfxAccelerationStructure(), getGFXAcclerationStructureCopyMode(mode));
    mCommandsPending = true;
}

gfx::IRenderCommandEncoder* RenderContext::drawCallCommon(GraphicsState* pState, ProgramVars* pVars) {
    // Insert barriers for bound resources.
    pVars->prepareDescriptorSets(this);

    // Insert barriers for render targets.
    ensureFboAttachmentResourceStates(this, pState->getFbo().get());

    // Insert barriers for vertex/index buffers.
    auto pGso = pState->getGSO(pVars).get();
    if (pGso != mpLastBoundGraphicsStateObject) {
        auto pVao = pState->getVao().get();
        for (uint32_t i = 0; i < pVao->getVertexBuffersCount(); i++) {
            auto vertexBuffer = pVao->getVertexBuffer(i).get();
            resourceBarrier(vertexBuffer, Resource::State::VertexBuffer);
        }
        if (pVao->getIndexBuffer()) {
            auto indexBuffer = pVao->getIndexBuffer().get();
            resourceBarrier(indexBuffer, Resource::State::IndexBuffer);
        }
    }

    bool isNewEncoder = false;
    auto encoder = getLowLevelData()->getRenderCommandEncoder(
        pGso->getGFXRenderPassLayout(), pState->getFbo() ? pState->getFbo()->getGfxFramebuffer() : nullptr, isNewEncoder
    );

    FALCOR_GFX_CALL(encoder->bindPipelineWithRootObject(pGso->getGfxPipelineState(), pVars->getShaderObject()));

    if (isNewEncoder || pGso != mpLastBoundGraphicsStateObject) {
        mpLastBoundGraphicsStateObject = pGso;
        auto pVao = pState->getVao().get();
        auto pVertexLayout = pVao->getVertexLayout().get();
        for (uint32_t i = 0; i < pVao->getVertexBuffersCount(); i++) {
            auto bufferLayout = pVertexLayout->getBufferLayout(i);
            auto vertexBuffer = pVao->getVertexBuffer(i).get();
            encoder->setVertexBuffer(i, pVao->getVertexBuffer(i)->getGfxBufferResource(), bufferLayout->getElementOffset(0));
        }
        if (pVao->getIndexBuffer()) {
            auto indexBuffer = pVao->getIndexBuffer().get();
            encoder->setIndexBuffer(indexBuffer->getGfxBufferResource(), getGFXFormat(pVao->getIndexBufferFormat()));
        }
        encoder->setPrimitiveTopology(getGFXPrimitiveTopology(pVao->getPrimitiveTopology()));
        encoder->setViewports(
            (uint32_t)pState->getViewports().size(), reinterpret_cast<const gfx::Viewport*>(pState->getViewports().data())
        );
        encoder->setScissorRects(
            (uint32_t)pState->getScissors().size(), reinterpret_cast<const gfx::ScissorRect*>(pState->getScissors().data())
        );
    }

    return encoder;
}

}  // namespace Falcor
