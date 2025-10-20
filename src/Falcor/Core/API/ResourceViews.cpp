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

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/NativeHandle.h"
#include "ResourceViews.h"

namespace Falcor {

/*
NativeHandle ResourceView::getNativeHandle() const {
    FALCOR_ASSERT(mpDevice != nullptr && mpResource != nullptr);
    gfx::InteropHandle gfxNativeHandle = {};
    FALCOR_GFX_CALL(mGfxResourceView->getNativeHandle(&gfxNativeHandle));

    if (mpResource) {
        if (mpResource->getType() == Resource::Type::Buffer) {
            if (mGfxResourceView->getViewDesc()->format == gfx::Format::Unknown) {
                return NativeHandle(reinterpret_cast<VkBuffer>(gfxNativeHandle.handleValue));
            } else {
                return NativeHandle(reinterpret_cast<VkBufferView>(gfxNativeHandle.handleValue));
            }
        } else {
            return NativeHandle(reinterpret_cast<VkImageView>(gfxNativeHandle.handleValue));
        }
    }  
    return {};
}
*/

ResourceView::~ResourceView() {
    if (mGfxResourceView) mpDevice->releaseResource(mGfxResourceView);
}

void ResourceView::invalidate() {
    if (mpDevice) {
        mpDevice->releaseResource(mGfxResourceView);
        mGfxResourceView = nullptr;
        mpResource = nullptr;
        mpDevice = nullptr;
    }
}

Falcor::SharedPtr<ShaderResourceView> ShaderResourceView::create(
    Device* pDevice,
    Texture* pTexture,
    uint32_t mostDetailedMip,
    uint32_t mipCount,
    uint32_t firstArraySlice,
    uint32_t arraySize)
{
    FALCOR_CHECK(is_set(pTexture->getBindFlags(), ResourceBindFlags::ShaderResource), "Texture does not have SRV bind flag set.");
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(depthToColorFormat(pTexture->getFormat()));
    desc.type = gfx::IResourceView::Type::ShaderResource;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mostDetailedMip;
    desc.subresourceRange.mipLevelCount = mipCount;
    FALCOR_GFX_CALL(pDevice->getGfxDevice()->createTextureView(pTexture->getGfxTextureResource(), desc, handle.writeRef()));
    return Falcor::SharedPtr<ShaderResourceView>(new ShaderResourceView(pDevice, pTexture, handle, mostDetailedMip, mipCount, firstArraySlice, arraySize)
    );
}

Falcor::SharedPtr<ShaderResourceView> ShaderResourceView::create(Device* pDevice, Buffer* pBuffer, uint64_t offset, uint64_t size){
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.type = gfx::IResourceView::Type::ShaderResource;
    desc.format = getGFXFormat(pBuffer->getFormat());
    desc.bufferRange.offset = offset;
    desc.bufferRange.size = size == kEntireBuffer ? 0 : size;

    FALCOR_GFX_CALL(pDevice->getGfxDevice()->createBufferView(pBuffer->getGfxBufferResource(), nullptr, desc, handle.writeRef()));
    return Falcor::SharedPtr<ShaderResourceView>(new ShaderResourceView(pDevice, pBuffer, handle, offset, size));
}

Falcor::SharedPtr<ShaderResourceView> ShaderResourceView::create(Device* pDevice, Dimension dimension) {
    // Create a null view of the specified dimension.
    return Falcor::SharedPtr<ShaderResourceView>(new ShaderResourceView(pDevice, nullptr, nullptr, 0, 0));
}

Falcor::SharedPtr<DepthStencilView> DepthStencilView::create(
    Device* pDevice,
    Texture* pTexture,
    uint32_t mipLevel,
    uint32_t firstArraySlice,
    uint32_t arraySize)
{
    FALCOR_CHECK(is_set(pTexture->getBindFlags(), ResourceBindFlags::DepthStencil), "Texture does not have DSV bind flag set.");
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(pTexture->getFormat());
    desc.type = gfx::IResourceView::Type::DepthStencil;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mipLevel;
    desc.subresourceRange.mipLevelCount = 1;
    desc.subresourceRange.aspectMask = gfx::TextureAspect::Depth;
    desc.renderTarget.shape = pTexture->getGfxTextureResource()->getDesc()->type;
    FALCOR_GFX_CALL(pDevice->getGfxDevice()->createTextureView(pTexture->getGfxTextureResource(), desc, handle.writeRef()));
    return Falcor::SharedPtr<DepthStencilView>(new DepthStencilView(pDevice, pTexture, handle, mipLevel, firstArraySlice, arraySize));
}

Falcor::SharedPtr<DepthStencilView> DepthStencilView::create(Device* pDevice, Dimension dimension){
    return Falcor::SharedPtr<DepthStencilView>(new DepthStencilView(pDevice, nullptr, nullptr, 0, 0, 0));
}

Falcor::SharedPtr<UnorderedAccessView> UnorderedAccessView::create(
    Device* pDevice,
    Texture* pTexture,
    uint32_t mipLevel,
    uint32_t firstArraySlice,
    uint32_t arraySize)
{
    FALCOR_CHECK(is_set(pTexture->getBindFlags(), ResourceBindFlags::UnorderedAccess), "Texture does not have UAV bind flag set.");
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(pTexture->getFormat());
    desc.type = gfx::IResourceView::Type::UnorderedAccess;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mipLevel;
    desc.subresourceRange.mipLevelCount = 1;
    FALCOR_GFX_CALL(pDevice->getGfxDevice()->createTextureView(pTexture->getGfxTextureResource(), desc, handle.writeRef()));
    return Falcor::SharedPtr<UnorderedAccessView>(new UnorderedAccessView(pDevice, pTexture, handle, mipLevel, firstArraySlice, arraySize));
}

Falcor::SharedPtr<UnorderedAccessView> UnorderedAccessView::create(Device* pDevice, Buffer* pBuffer, uint64_t offset, uint64_t size){
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.type = gfx::IResourceView::Type::UnorderedAccess;
    desc.format = getGFXFormat(pBuffer->getFormat());
    desc.bufferRange.offset = offset;
    desc.bufferRange.size = size == kEntireBuffer ? 0 : size;
    FALCOR_GFX_CALL(pDevice->getGfxDevice()->createBufferView(
        pBuffer->getGfxBufferResource(),
        pBuffer->getUAVCounter() ? pBuffer->getUAVCounter()->getGfxBufferResource() : nullptr,
        desc,
        handle.writeRef()
    ));
    return Falcor::SharedPtr<UnorderedAccessView>(new UnorderedAccessView(pDevice, pBuffer, handle, offset, size));
}

Falcor::SharedPtr<UnorderedAccessView> UnorderedAccessView::create(Device* pDevice, Dimension dimension) {
    return Falcor::SharedPtr<UnorderedAccessView>(new UnorderedAccessView(pDevice, nullptr, nullptr, 0, 0));
}

Falcor::SharedPtr<RenderTargetView> RenderTargetView::create(
    Device* pDevice,
    Texture* pTexture,
    uint32_t mipLevel,
    uint32_t firstArraySlice,
    uint32_t arraySize)
{
    FALCOR_CHECK(is_set(pTexture->getBindFlags(), ResourceBindFlags::RenderTarget), "Texture does not have RTV bind flag set.");
    Slang::ComPtr<gfx::IResourceView> handle;
    gfx::IResourceView::Desc desc = {};
    desc.format = getGFXFormat(pTexture->getFormat());
    desc.type = gfx::IResourceView::Type::RenderTarget;
    desc.subresourceRange.baseArrayLayer = firstArraySlice;
    desc.subresourceRange.layerCount = arraySize;
    desc.subresourceRange.mipLevel = mipLevel;
    desc.subresourceRange.mipLevelCount = 1;
    desc.subresourceRange.aspectMask = gfx::TextureAspect::Color;
    desc.renderTarget.shape = pTexture->getGfxTextureResource()->getDesc()->type;
    FALCOR_GFX_CALL(pDevice->getGfxDevice()->createTextureView(pTexture->getGfxTextureResource(), desc, handle.writeRef()));
    return Falcor::SharedPtr<RenderTargetView>(new RenderTargetView(pDevice, pTexture, handle, mipLevel, firstArraySlice, arraySize));
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
	        FALCOR_UNREACHABLE();
	        return gfx::IResource::Type::Texture2D;
    }
}

Falcor::SharedPtr<RenderTargetView> RenderTargetView::create(Device* pDevice, Dimension dimension) {
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
    FALCOR_GFX_CALL(pDevice->getGfxDevice()->createTextureView(nullptr, desc, handle.writeRef()));
    return Falcor::SharedPtr<RenderTargetView>(new RenderTargetView(pDevice, nullptr, handle, 0, 0, 0));
}

#ifdef SCRIPTING
	FALCOR_SCRIPT_BINDING(ResourceView) {
		pybind11::class_<ShaderResourceView, ShaderResourceView::SharedPtr>(m, "ShaderResourceView");
		pybind11::class_<RenderTargetView, RenderTargetView::SharedPtr>(m, "RenderTargetView");
		pybind11::class_<UnorderedAccessView, UnorderedAccessView::SharedPtr>(m, "UnorderedAccessView");
		pybind11::class_<DepthStencilView, DepthStencilView::SharedPtr>(m, "DepthStencilView");
	}
#endif

} // namespace Falcor
