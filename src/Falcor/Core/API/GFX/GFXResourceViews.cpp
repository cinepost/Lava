/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
#include "stdafx.h"

#include "GFXFormats.h"

#include "Falcor/Core/API/ResourceViews.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Device.h"

namespace Falcor {

template<typename T>
ResourceView<T>::~ResourceView() {
    mpDevice->releaseResource(mApiHandle);
}

template<>
ResourceView<CbvHandle>::~ResourceView<CbvHandle>() {};

ShaderResourceView::SharedPtr ShaderResourceView::create(Device::SharedPtr pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(depthToColorFormat(pTexture->getFormat()));
    desc.type = gfx::IResourceView::Type::ShaderResource;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mostDetailedMip;
    desc.subresourceRange.mipLevelCount = mipCount;
    FALCOR_GFX_CALL(pDevice->getApiHandle()->createTextureView(static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get()), desc, handle.writeRef()));
    return SharedPtr(new ShaderResourceView(pDevice, pTexture, handle, mostDetailedMip, mipCount, firstArraySlice, arraySize));
}

static void fillBufferViewDesc(gfx::IResourceView::Desc& desc, ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount) {
    auto format = depthToColorFormat(pBuffer->getFormat());
    desc.format = getGFXFormat(format);

    uint32_t bufferElementSize = 0;
    uint64_t bufferElementCount = 0;
    if (pBuffer->isTyped()) {
        FALCOR_ASSERT(getFormatPixelsPerBlock(format) == 1);
        bufferElementSize = getFormatBytesPerBlock(format);
        bufferElementCount = pBuffer->getElementCount();
    } else if (pBuffer->isStructured()) {
        bufferElementSize = pBuffer->getStructSize();
        bufferElementCount = pBuffer->getElementCount();
        desc.format = gfx::Format::Unknown;
        desc.bufferElementSize = bufferElementSize;
    } else {
        desc.format = gfx::Format::Unknown;
        bufferElementSize = 4;
        bufferElementCount = pBuffer->getSize();
    }

    bool useDefaultCount = (elementCount == ShaderResourceView::kMaxPossible);
    assert(useDefaultCount || (firstElement + elementCount) <= bufferElementCount); // Check range
    desc.bufferRange.firstElement = firstElement;
    desc.bufferRange.elementCount = useDefaultCount ? (bufferElementCount - firstElement) : elementCount;
}

ShaderResourceView::SharedPtr ShaderResourceView::create(Device::SharedPtr pDevice, ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount) {
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.type = gfx::IResourceView::Type::ShaderResource;
    fillBufferViewDesc(desc, pBuffer, firstElement, elementCount);

    FALCOR_GFX_CALL(pDevice->getApiHandle()->createBufferView(static_cast<gfx::IBufferResource*>(pBuffer->getApiHandle().get()), nullptr, desc, handle.writeRef()));
    return SharedPtr(new ShaderResourceView(pDevice, pBuffer, handle, firstElement, elementCount));
}

ShaderResourceView::SharedPtr ShaderResourceView::create(Device::SharedPtr pDevice, Dimension dimension) {
    // Create a null view of the specified dimension.
    return SharedPtr(new ShaderResourceView(pDevice, std::weak_ptr<Resource>(), nullptr, 0, 0));
}

#if FALCOR_D3D12_AVAILABLE
D3D12DescriptorCpuHandle ShaderResourceView::getD3D12CpuHeapHandle() const {
    gfx::InteropHandle handle;
    FALCOR_GFX_CALL(mApiHandle->getNativeHandle(&handle));
    assert(handle.api == gfx::InteropHandleAPI::D3D12CpuDescriptorHandle);

    D3D12DescriptorCpuHandle result;
    result.ptr = handle.handleValue;
    return result;
}
#endif

DepthStencilView::SharedPtr DepthStencilView::create(Device::SharedPtr pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    auto gfxTexture = static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get());

    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(pTexture->getFormat());
    desc.type = gfx::IResourceView::Type::DepthStencil;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mipLevel;
    desc.subresourceRange.mipLevelCount = 1;
    desc.subresourceRange.aspectMask = gfx::TextureAspect::Depth;
    desc.renderTarget.shape = gfxTexture->getDesc()->type;
    FALCOR_GFX_CALL(pDevice->getApiHandle()->createTextureView(static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get()), desc, handle.writeRef()));
    return SharedPtr(new DepthStencilView(pDevice, pTexture, handle, mipLevel, firstArraySlice, arraySize));
}

DepthStencilView::SharedPtr DepthStencilView::create(Device::SharedPtr pDevice, Dimension dimension) {
    return SharedPtr(new DepthStencilView(pDevice, std::weak_ptr<Resource>(), nullptr, 0, 0, 0));
}

#if FALCOR_D3D12_AVAILABLE
D3D12DescriptorCpuHandle DepthStencilView::getD3D12CpuHeapHandle() const {
    gfx::InteropHandle handle;
    FALCOR_GFX_CALL(mApiHandle->getNativeHandle(&handle));
    assert(handle.api == gfx::InteropHandleAPI::D3D12CpuDescriptorHandle);

    D3D12DescriptorCpuHandle result;
    result.ptr = handle.handleValue;
    return result;
}
#endif

UnorderedAccessView::SharedPtr UnorderedAccessView::create(Device::SharedPtr pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(pTexture->getFormat());
    desc.type = gfx::IResourceView::Type::UnorderedAccess;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mipLevel;
    desc.subresourceRange.mipLevelCount = 1;
    FALCOR_GFX_CALL(pDevice->getApiHandle()->createTextureView(static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get()), desc, handle.writeRef()));
    return SharedPtr(new UnorderedAccessView(pDevice, pTexture, handle, mipLevel, firstArraySlice, arraySize));
}

UnorderedAccessView::SharedPtr UnorderedAccessView::create(Device::SharedPtr pDevice, ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount) {
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.type = gfx::IResourceView::Type::UnorderedAccess;
    fillBufferViewDesc(desc, pBuffer, firstElement, elementCount);
    FALCOR_GFX_CALL(pDevice->getApiHandle()->createBufferView(
        static_cast<gfx::IBufferResource*>(pBuffer->getApiHandle().get()),
        pBuffer->getUAVCounter() ? static_cast<gfx::IBufferResource*>(pBuffer->getUAVCounter()->getApiHandle().get()) : nullptr,
        desc,
        handle.writeRef()));
    return SharedPtr(new UnorderedAccessView(pDevice, pBuffer, handle, firstElement, elementCount));
}

UnorderedAccessView::SharedPtr UnorderedAccessView::create(Device::SharedPtr pDevice, Dimension dimension) {
    return SharedPtr(new UnorderedAccessView(pDevice, std::weak_ptr<Resource>(), nullptr, 0, 0));
}

#if FALCOR_D3D12_AVAILABLE
D3D12DescriptorCpuHandle UnorderedAccessView::getD3D12CpuHeapHandle() const {
    gfx::InteropHandle handle;
    FALCOR_GFX_CALL(mApiHandle->getNativeHandle(&handle));
    assert(handle.api == gfx::InteropHandleAPI::D3D12CpuDescriptorHandle);

    D3D12DescriptorCpuHandle result;
    result.ptr = handle.handleValue;
    return result;
}
#endif

RenderTargetView::~RenderTargetView() = default;

RenderTargetView::SharedPtr RenderTargetView::create(Device::SharedPtr pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    auto gfxTexture = static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get());
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(pTexture->getFormat());
    desc.type = gfx::IResourceView::Type::RenderTarget;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mipLevel;
    desc.subresourceRange.mipLevelCount = 1;
    desc.subresourceRange.aspectMask = gfx::TextureAspect::Color;
    desc.renderTarget.shape = gfxTexture->getDesc()->type;
    FALCOR_GFX_CALL(pDevice->getApiHandle()->createTextureView(gfxTexture, desc, handle.writeRef()));
    return SharedPtr(new RenderTargetView(pDevice, pTexture, handle, mipLevel, firstArraySlice, arraySize));
}

gfx::IResource::Type getGFXResourceType(RenderTargetView::Dimension dim) {
    switch (dim) {
        case RenderTargetView::Dimension::Buffer:
            return gfx::IResource::Type::Buffer;
        case RenderTargetView::Dimension::Texture1D:
        case RenderTargetView::Dimension::Texture1DArray:
            return gfx::IResource::Type::Texture1D;
        case RenderTargetView::Dimension::Texture2D:
        case RenderTargetView::Dimension::Texture2DMS:
        case RenderTargetView::Dimension::Texture2DMSArray:
        case RenderTargetView::Dimension::Texture2DArray:
            return gfx::IResource::Type::Texture2D;
        case RenderTargetView::Dimension::Texture3D:
            return gfx::IResource::Type::Texture3D;
        case RenderTargetView::Dimension::TextureCube:
        case RenderTargetView::Dimension::TextureCubeArray:
            return gfx::IResource::Type::TextureCube;
        default:
            assert(false);
            return gfx::IResource::Type::Texture2D;
    }
}

RenderTargetView::SharedPtr RenderTargetView::create(Device::SharedPtr pDevice, Dimension dimension) {
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = gfx::Format::R8G8B8A8_UNORM;
    desc.type = gfx::IResourceView::Type::RenderTarget;
    desc.subresourceRange.baseArrayLayer = 0;
    desc.subresourceRange.layerCount = 1;
    desc.subresourceRange.mipLevel = 0;
    desc.subresourceRange.mipLevelCount = 1;
    desc.subresourceRange.aspectMask = gfx::TextureAspect::Color;
    desc.renderTarget.shape = getGFXResourceType(dimension);
    FALCOR_GFX_CALL(pDevice->getApiHandle()->createTextureView(nullptr, desc, handle.writeRef()));
    return SharedPtr(new RenderTargetView(pDevice, std::weak_ptr<Resource>(), handle, 0, 0, 0));
}

#if FALCOR_D3D12_AVAILABLE
D3D12DescriptorCpuHandle RenderTargetView::getD3D12CpuHeapHandle() const {
    gfx::InteropHandle handle;
    FALCOR_GFX_CALL(mApiHandle->getNativeHandle(&handle));
    assert(handle.api == gfx::InteropHandleAPI::D3D12CpuDescriptorHandle);

    D3D12DescriptorCpuHandle result;
    result.ptr = handle.handleValue;
    return result;
}
#endif

#if FALCOR_D3D12_AVAILABLE
D3D12DescriptorSet::SharedPtr createCbvDescriptor(Device::SharedPtr pDevice, const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc, Resource::ApiHandle resHandle) {
    D3D12DescriptorSet::Layout layout;
    layout.addRange(ShaderResourceType::Cbv, 0, 1);
    D3D12DescriptorSet::SharedPtr handle = D3D12DescriptorSet::create(pDevice->getD3D12CpuDescriptorPool(), layout);
    pDevice->getD3D12Handle()->CreateConstantBufferView(&desc, handle->getCpuHandle(0));

    return handle;
}
#endif // FALCOR_D3D12_AVAILABLE

ConstantBufferView::SharedPtr ConstantBufferView::create(Device::SharedPtr pDevice, ConstBufferSharedPtrRef pBuffer) {
    // GFX doesn't need constant buffer view.
    // We provide a raw D3D12 implementation for applications
    // that wish to use the raw D3D12DescriptorSet API.
#if FALCOR_D3D12_AVAILABLE
    assert(pBuffer);
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
    desc.BufferLocation = pBuffer->getGpuAddress();
    desc.SizeInBytes = (uint32_t)pBuffer->getSize();
    Resource::ApiHandle resHandle = pBuffer->getApiHandle();

    return SharedPtr(new ConstantBufferView(pDevice, pBuffer, createCbvDescriptor(desc, resHandle)));
#else
    assert(pBuffer);
    throw std::runtime_error("ConstantBufferView is not supported in GFX.");
#endif // FALCOR_D3D12_AVAILABLE
}

ConstantBufferView::SharedPtr ConstantBufferView::create(Device::SharedPtr pDevice) {
    // GFX doesn't support constant buffer view.
    // We provide a raw D3D12 implementation for applications
    // that wish to use the raw D3D12DescriptorSet API.
#if FALCOR_D3D12_AVAILABLE
    // Create a null view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};

    return SharedPtr(new ConstantBufferView(pDevice, std::weak_ptr<Resource>(), createCbvDescriptor(desc, nullptr)));
#else
    return SharedPtr(new ConstantBufferView(pDevice, std::weak_ptr<Resource>(), nullptr));
#endif // FALCOR_D3D12_AVAILABLE
}

#if FALCOR_D3D12_AVAILABLE
D3D12DescriptorCpuHandle ConstantBufferView::getD3D12CpuHeapHandle() const {
    return mApiHandle->getCpuHandle(0);
}
#endif

using ResourceViewImpl = ResourceView<Slang::ComPtr<gfx::IResourceView>>;
template ResourceSharedPtr ResourceViewImpl::getResource() const;
template const ResourceViewImpl::ApiHandle& ResourceViewImpl::getApiHandle() const;
template const ResourceViewInfo& ResourceViewImpl::getViewInfo() const;
#if FALCOR_ENABLE_CUDA
template void* ResourceViewImpl::getCUDADeviceAddress() const;
#endif

}  // namespace Falcor
