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
#include "Falcor/Core/API/BlitToBufferReduction.slangh"


#include "RenderContext.h"

namespace Falcor {

uint4 RenderContext::kMaxRect = { 0, 0, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max() };

RenderContext::SharedPtr RenderContext::create(Device::SharedPtr pDevice, CommandQueueHandle queue) {
    return SharedPtr(new RenderContext(pDevice, queue));
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
    if (is_set(bindFlags, Resource::BindFlags::RenderTarget)) {
        clearRtv(pTexture->getRTV().get(), clearColor);
    } else if (is_set(bindFlags, Resource::BindFlags::UnorderedAccess)) {
        clearUAV(pTexture->getUAV().get(), clearColor);
    } else if (is_set(bindFlags, Resource::BindFlags::DepthStencil)) {
        if (isStencilFormat(format) && (clearColor.y != 0)) {
            LLOG_WRN << "RenderContext::clearTexture() - when clearing a depth-stencil texture the stencil value(clearColor.y) must be 0. Received " << std::to_string(clearColor.y) << ". Forcing stencil to 0";
        }
        clearDsv(pTexture->getDSV().get(), clearColor.r, 0);
    } else {
        LLOG_WRN << "Texture::clear() - The texture does not have a bind flag that allows us to clear!";
    }
}

void RenderContext::flush(bool wait) {
    ComputeContext::flush(wait);
    mpLastBoundGraphicsVars = nullptr;
}

void RenderContext::blit(const ShaderResourceView::SharedPtr& pSrc, const RenderTargetView::SharedPtr& pDst, uint4 srcRect, uint4 dstRect, Sampler::Filter filter)
{
    const Sampler::ReductionMode componentsReduction[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard };
    const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f) };

    blit(pSrc, pDst, srcRect, dstRect, filter, componentsReduction, componentsTransform);
}


void RenderContext::blit(const ShaderResourceView::SharedPtr& pSrc, const RenderTargetView::SharedPtr& pDst, uint4 srcRect, uint4 dstRect, Sampler::Filter filter, const Sampler::ReductionMode componentsReduction[4], const float4 componentsTransform[4])
{
    auto& blitData = getBlitContext();

    // Fetch textures from views.
    assert(pSrc && pDst);
    auto pSrcResource = pSrc->getResource();
    auto pDstResource = pDst->getResource();
    if (pSrcResource->getType() == Resource::Type::Buffer || pDstResource->getType() == Resource::Type::Buffer) {
        throw std::runtime_error("RenderContext::blit does not support buffers");
    }

    const Texture* pSrcTexture = dynamic_cast<const Texture*>(pSrcResource.get());
    const Texture* pDstTexture = dynamic_cast<const Texture*>(pDstResource.get());
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
        copyResource(pDstResource.get(), pSrcResource.get());
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
    blitData.pPass->addDefine("SAMPLE_COUNT", std::to_string(sampleCount));
    blitData.pPass->addDefine("COMPLEX_BLIT", complexBlit ? "1" : "0");
    blitData.pPass->addDefine("SRC_INT", isIntegerFormat(pSrcTexture->getFormat()) ? "1" : "0");
    blitData.pPass->addDefine("DST_INT", isIntegerFormat(pDstTexture->getFormat()) ? "1" : "0");

    if (complexBlit) {
        assert(sampleCount <= 1);

        Sampler::SharedPtr usedSampler[4];
        for (uint32_t i = 0; i < 4; i++) {
            assert(componentsReduction[i] != Sampler::ReductionMode::Comparison);        // Comparison mode not supported.

            if (componentsReduction[i] == Sampler::ReductionMode::Min) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearMinSampler : blitData.pPointMinSampler;
            else if (componentsReduction[i] == Sampler::ReductionMode::Max) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearMaxSampler : blitData.pPointMaxSampler;
            else usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearSampler : blitData.pPointSampler;
        }

        blitData.pPass->getVars()->setSampler("gSamplerR", usedSampler[0]);
        blitData.pPass->getVars()->setSampler("gSamplerG", usedSampler[1]);
        blitData.pPass->getVars()->setSampler("gSamplerB", usedSampler[2]);
        blitData.pPass->getVars()->setSampler("gSamplerA", usedSampler[3]);

        // Parameters for complex blit
        for (uint32_t i = 0; i < 4; i++) {
            if (blitData.prevComponentsTransform[i] != componentsTransform[i]) {
                blitData.pBlitParamsBuffer->setVariable(blitData.compTransVarOffset[i], componentsTransform[i]);
                blitData.prevComponentsTransform[i] = componentsTransform[i];
            }
        }
    } else {
        blitData.pPass->getVars()->setSampler("gSampler", (filter == Sampler::Filter::Linear) ? blitData.pLinearSampler : blitData.pPointSampler);
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
    if (srcRectOffset != blitData.prevSrcRectOffset) {
        blitData.pBlitParamsBuffer->setVariable(blitData.offsetVarOffset, srcRectOffset);
        blitData.prevSrcRectOffset = srcRectOffset;
    }

    if (srcRectScale != blitData.prevSrcReftScale) {
        blitData.pBlitParamsBuffer->setVariable(blitData.scaleVarOffset, srcRectScale);
        blitData.prevSrcReftScale = srcRectScale;
    }

    Texture::SharedPtr pSharedTex = std::static_pointer_cast<Texture>(pDstResource);
    blitData.pFbo->attachColorTarget(pSharedTex, 0, pDst->getViewInfo().mostDetailedMip, pDst->getViewInfo().firstArraySlice, pDst->getViewInfo().arraySize);
    blitData.pPass->getVars()->setSrv(blitData.texBindLoc, pSrc);
    blitData.pPass->getState()->setViewport(0, dstViewport);
    blitData.pPass->execute(this, blitData.pFbo, false);

    // Release the resources we bound
    blitData.pPass->getVars()->setSrv(blitData.texBindLoc, nullptr);
}

void RenderContext::blitToBuffer(const ShaderResourceView::SharedPtr& pSrc, const Buffer::SharedPtr& pBuffer, uint32_t bufferWidthStrideInPixels, Falcor::ResourceFormat dstFormat, uint4 srcRect, uint4 dstRect, Sampler::Filter filter, const Sampler::ReductionMode componentsReduction[4], const float4 componentsTransform[4])
{
    auto& blitData = getBlitToBufferContext();

    // Fetch textures from views.
    assert(pSrc && pDst);
    auto pSrcResource = pSrc->getResource();
    if (pSrcResource->getType() == Resource::Type::Buffer) {
        throw std::runtime_error("RenderContext::blitToBuffer() does not support buffer source !");
    }

    // Check if buffer size is divisable by dstFormat size.
    if(pBuffer->getSize() % getFormatBytesPerBlock(dstFormat) != 0) {
        throw std::runtime_error("RenderContext::blitToBuffer() distination buffer size and dstFormat size are not divisable !");
    }
    
    const Texture* pSrcTexture = dynamic_cast<const Texture*>(pSrcResource.get());
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
    
    //if (fullCopy) {
    //    copyResource(pBuffer.get(), pSrcResource.get());
    //    return;
    //}

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
    blitData.pPass->addDefine("SAMPLE_COUNT", std::to_string(sampleCount));
    blitData.pPass->addDefine("COMPLEX_BLIT", complexBlit ? "1" : "0");
    blitData.pPass->addDefine("SRC_INT", isIntegerFormat(pSrcTexture->getFormat()) ? "1" : "0");
    blitData.pPass->addDefine("DST_INT", isIntegerFormat(dstFormat) ? "1" : "0");
    blitData.pPass->addDefine("DST_HALF_FLOAT", isDstHalfFormat ? "1" : "0");
    blitData.pPass->addDefine("FORMAT_TYPE", std::to_string(formatType));
    blitData.pPass->addDefine("PIXEL_STRIDE_BYTES", std::to_string(outputPixelStrideBytes));

    LLOG_WRN << "DST INT " << ( isIntegerFormat(dstFormat) ? "1" : "0");
    LLOG_WRN << "DST FORMAT " << to_string(dstFormat);
    LLOG_WRN << "SAMPLE COUNT " << std::to_string(sampleCount);
    LLOG_WRN << "PIXEL_STRIDE_BYTES " << std::to_string(outputPixelStrideBytes);
    LLOG_WRN << "FORMAT_TYPE " << std::to_string(formatType);

    if (complexBlit) {

        LLOG_WRN << "complexBlit";
        assert(sampleCount <= 1);

        Sampler::SharedPtr usedSampler[4];
        for (uint32_t i = 0; i < 4; i++) {
            assert(componentsReduction[i] != Sampler::ReductionMode::Comparison);        // Comparison mode not supported.

            if (componentsReduction[i] == Sampler::ReductionMode::Min) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearMinSampler : blitData.pPointMinSampler;
            else if (componentsReduction[i] == Sampler::ReductionMode::Max) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearMaxSampler : blitData.pPointMaxSampler;
            else usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearSampler : blitData.pPointSampler;
        }

        blitData.pPass->getVars()->setSampler("gSamplerR", usedSampler[0]);
        blitData.pPass->getVars()->setSampler("gSamplerG", usedSampler[1]);
        blitData.pPass->getVars()->setSampler("gSamplerB", usedSampler[2]);
        blitData.pPass->getVars()->setSampler("gSamplerA", usedSampler[3]);

        // Parameters for complex blit
        for (uint32_t i = 0; i < 4; i++) {
            if (blitData.prevComponentsTransform[i] != componentsTransform[i]) {
                blitData.pBlitParamsBuffer->setVariable(blitData.compTransVarOffset[i], componentsTransform[i]);
                blitData.prevComponentsTransform[i] = componentsTransform[i];
            }
        }
    } else {
        LLOG_WRN << "non complexBlit";
        blitData.pPass->getVars()->setSampler("gSampler", (filter == Sampler::Filter::Linear) ? blitData.pLinearSampler : blitData.pPointSampler);
    }
    
    float2 srcRectOffset(0.0f);
    float2 srcRectScale(1.0f);
    if (!srcFullRect) {
        srcRectOffset = float2(srcRect.x, srcRect.y) / float2(srcSize);
        srcRectScale = float2(srcRect.z - srcRect.x, srcRect.w - srcRect.y) / float2(srcSize);
    }

    // Update buffer/state
    if (srcRectOffset != blitData.prevSrcRectOffset) {
        blitData.pBlitParamsBuffer->setVariable(blitData.offsetVarOffset, srcRectOffset);
        blitData.prevSrcRectOffset = srcRectOffset;
    }

    if (srcRectScale != blitData.prevSrcReftScale) {
        blitData.pBlitParamsBuffer->setVariable(blitData.scaleVarOffset, srcRectScale);
        blitData.prevSrcReftScale = srcRectScale;
    }

    blitData.pBlitParamsBuffer->setVariable(blitData.resolutionVarOffset, dstSize);
    blitData.pBlitParamsBuffer->setVariable(blitData.srcPixelHalfSizeVarOffset, srcHalfPixelSize);
    
    blitData.pPass->getVars()->setSrv(blitData.texBindLoc, pSrc);
    blitData.pPass->getVars()->setBuffer(blitData.buffBindLoc, pBuffer);
    
    LLOG_WRN << "blitToBuffer::execute()";
    blitData.pPass->execute(this, dstSize.x, dstSize.y);

    // Release the resources we bound
    blitData.pPass->getVars()->setSrv(blitData.texBindLoc, nullptr);
    blitData.pPass->getVars()->setBuffer(blitData.buffBindLoc, nullptr);
}

}  // namespace Falcor
