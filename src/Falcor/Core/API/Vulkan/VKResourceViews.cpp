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
#include "Falcor/Core/API/ResourceViews.h"
#include "Falcor/Core/API/Resource.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DescriptorSet.h"
#include "Falcor/Core/API/Formats.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {

using TypedBufferBase = Buffer;


VkImageAspectFlags getAspectFlagsFromFormat(ResourceFormat format, bool ignoreStencil = false);

template<typename ApiHandleType>
ResourceView<ApiHandleType>::~ResourceView() {
    gpDevice->releaseResource(mApiHandle);
}

Texture::SharedPtr createBlackTexture() {
    uint8_t blackPixel[4] = { 0 };
    return Texture::create2D(1, 1, ResourceFormat::RGBA8Unorm, 1, 1, blackPixel, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess);
}

// ResourceWeakPtr getEmptyTexture() Why it was weak_ptr at all !?
Texture::SharedPtr getEmptyTexture() {
    static Texture::SharedPtr sBlackTexture = createBlackTexture();
    return sBlackTexture;
}

VkImageViewType getViewType(Resource::Type type, bool isArray) {
    switch (type) {
        case Resource::Type::Texture1D:
            return isArray ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case Resource::Type::Texture2D:
        case Resource::Type::Texture2DMultisample:
            return isArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case Resource::Type::Texture3D:
            assert(isArray == false);
            return VK_IMAGE_VIEW_TYPE_3D;
        case Resource::Type::TextureCube:
            return isArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            should_not_get_here();
            return VK_IMAGE_VIEW_TYPE_2D;
    }
}

VkImageViewCreateInfo initializeImageViewInfo(const Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
    VkImageViewCreateInfo outInfo = {};

    ResourceFormat texFormat = pTexture->getFormat();
    LOG_DBG("Texture format: %s", to_string(texFormat).c_str()); 

    outInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    outInfo.image = pTexture->getApiHandle();
    outInfo.viewType = getViewType(pTexture->getType(), pTexture->getArraySize() > 1);
    outInfo.format = getVkFormat(texFormat);
    outInfo.subresourceRange.aspectMask = getAspectFlagsFromFormat(texFormat, true);
    outInfo.subresourceRange.baseMipLevel = mostDetailedMip;
    outInfo.subresourceRange.levelCount = mipCount;

    if (pTexture->getType() == Resource::Type::TextureCube) {
        firstArraySlice *= 6;
        arraySize *= 6;
    }

    outInfo.subresourceRange.baseArrayLayer = firstArraySlice;
    outInfo.subresourceRange.layerCount = arraySize;

    return outInfo;
}

VkBufferViewCreateInfo initializeBufferViewInfo(const Buffer* pBuffer) {
    VkBufferViewCreateInfo outInfo = {};

    ResourceFormat buffFormat = pBuffer->getFormat();
    LOG_DBG("Buffer format: %s", to_string(buffFormat).c_str()); 

    outInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    outInfo.buffer = pBuffer->getApiHandle();
    outInfo.offset = 0;
    outInfo.range = VK_WHOLE_SIZE;
    outInfo.format = getVkFormat(buffFormat);
    return outInfo;
}

// SharedConstPtr = std::shared_ptr<const Resource>;
VkResource<VkImageView, VkBufferView>::SharedPtr createViewCommon(const Resource::SharedConstPtr& pSharedPtr, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
    const Resource* pResource = pSharedPtr.get();
    assert(pResource);

    switch (pResource->getApiHandle().getType()) {
        case VkResourceType::Image: {
            VkImageViewCreateInfo info = initializeImageViewInfo((const Texture*)pResource, mostDetailedMip, mipCount, firstArraySlice, arraySize);
            VkImageView imageView;
            vk_call(vkCreateImageView(gpDevice->getApiHandle(), &info, nullptr, &imageView));
            return VkResource<VkImageView, VkBufferView>::SharedPtr::create(imageView, nullptr);
        }

        case VkResourceType::Buffer: {
            // We only create views for typed Buffers
            VkBufferView bufferView = {};
            const Buffer* pBuffer = dynamic_cast<const Buffer*>(pResource);

            if (pBuffer->isTyped()) {
                VkBufferViewCreateInfo info = initializeBufferViewInfo(pBuffer);
                vk_call(vkCreateBufferView(gpDevice->getApiHandle(), &info, nullptr, &bufferView));
            }

            return VkResource<VkImageView, VkBufferView>::SharedPtr::create(bufferView, nullptr);
        }

        default:
            LOG_WARN("VkResourceType unknown");
            should_not_get_here();
            return VkResource<VkImageView, VkBufferView>::SharedPtr();
    }
}

ShaderResourceView::SharedPtr ShaderResourceView::create(ConstTextureSharedPtrRef pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
    if (!pTexture && getNullView()) {
        LOG_WARN("getNullView");
        return getNullView();
    }

    // Resource::ApiHandle resHandle = pTexture->getApiHandle();
    auto view = createViewCommon(pTexture, mostDetailedMip, mipCount, firstArraySlice, arraySize);
    return SharedPtr(new ShaderResourceView(pTexture, view, mostDetailedMip, mipCount, firstArraySlice, arraySize));
}

ShaderResourceView::SharedPtr ShaderResourceView::create(ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount) {
    if (!pBuffer && getNullView()) {
        LOG_WARN("getNullView");
        return getNullView();
    }

    if (pBuffer->getApiHandle().getType() == VkResourceType::Image) {
        logWarning("Cannot create DepthStencilView from a texture!");
        return getNullView();
    }
    
    // Resource::ApiHandle resHandle = nullptr;
    // if (pBuffer) {
    //    desc = createBufferSrvDesc(pBuffer.get(), firstElement, elementCount);
    //    resHandle = pBuffer->getApiHandle();
    // }
    //auto view = createViewCommon(pBuffer, 0, 0, 0, 0);
    
    const Resource* pResource = pBuffer.get();
    assert(pResource);

    //auto view = createViewCommon(pBuffer, 0, 0, 0, 0);
    LOG_DBG("pBuffer type %s", to_string(pBuffer->getType()).c_str());
    
    // We only create views for typed Buffers
    VkBufferView bufferView = {};
    const Buffer* buffer = dynamic_cast<const Buffer*>(pResource);

    if (buffer->isTyped()) {
        VkBufferViewCreateInfo info = initializeBufferViewInfo(buffer);
        vk_call(vkCreateBufferView(gpDevice->getApiHandle(), &info, nullptr, &bufferView));
    }
    auto view = VkResource<VkImageView, VkBufferView>::SharedPtr::create(bufferView, nullptr);

    return SharedPtr(new ShaderResourceView(pBuffer, view, firstElement, elementCount));
}

DepthStencilView::SharedPtr DepthStencilView::create(ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    // const Resource* pResource = pTexture.get();
    if (!pTexture && getNullView()) {
        LOG_WARN("getNullView");
        return getNullView();
    }

    if (pTexture->getApiHandle().getType() == VkResourceType::Buffer) {
        logWarning("Cannot create DepthStencilView from a buffer!");
        return getNullView();
    }

    auto view = createViewCommon(pTexture, mipLevel, 1, firstArraySlice, arraySize);
    return SharedPtr(new DepthStencilView(pTexture, view, mipLevel, firstArraySlice, arraySize));
}

UnorderedAccessView::SharedPtr UnorderedAccessView::create(ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    LOG_DBG("UnorderedAccessView::create texture");
    if (!pTexture && getNullView()) {
        LOG_WARN("getNullView");
        return getNullView();
    }

    if (pTexture->getApiHandle().getType() == VkResourceType::Buffer) {
        logWarning("Cannot create UnorderedAccessView from a buffer!");
        return getNullView();;
    }

    auto view = createViewCommon(pTexture, mipLevel, 1, firstArraySlice, arraySize);
    return SharedPtr(new UnorderedAccessView(pTexture, view, mipLevel, firstArraySlice, arraySize));
}

/*
UnorderedAccessView::ApiHandle createUavDescriptor(const DescriptorSet& desc, Resource::ApiHandle resHandle, Resource::ApiHandle counterHandle) {
    DescriptorSet::Layout layout;
    layout.addRange(DescriptorSet::Type::TextureUav, 0, 1);
    UnorderedAccessView::ApiHandle handle = DescriptorSet::create(gpDevice->getCpuDescriptorPool(), layout);
    gpDevice->getApiHandle()->CreateUnorderedAccessView(resHandle, counterHandle, &desc, handle->getCpuHandle(0));
    return handle;
}
*/

UnorderedAccessView::SharedPtr UnorderedAccessView::create(ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount) {
    LOG_DBG("UnorderedAccessView::create buffer");
    if (!pBuffer && getNullView()) {
        LOG_WARN("getNullView");
        return getNullView();
    }

    if (pBuffer->getApiHandle().getType() == VkResourceType::Image) {
        logWarning("Cannot create UnorderedAccessView from a texture!");
        return getNullView();
    }

    const Resource* pResource = pBuffer.get();
    assert(pResource);

    //auto view = createViewCommon(pBuffer, 0, 0, 0, 0);
    LOG_DBG("pBuffer type %s", to_string(pBuffer->getType()).c_str());
    
    // We only create views for typed Buffers
    VkBufferView bufferView = {};
    const Buffer* buffer = dynamic_cast<const Buffer*>(pResource);

    if (buffer->isTyped()) {
        VkBufferViewCreateInfo info = initializeBufferViewInfo(buffer);
        vk_call(vkCreateBufferView(gpDevice->getApiHandle(), &info, nullptr, &bufferView));
    }
    auto view = VkResource<VkImageView, VkBufferView>::SharedPtr::create(bufferView, nullptr);


    //DescriptorSet::Layout layout;
    //layout.addRange(DescriptorSet::Type::TextureUav, 0, 1);
    //auto desc = DescriptorSet::create(gpDevice->getCpuDescriptorPool(), layout);

    //desc->setUav(0, 1, uav);

    //return SharedPtr(new UnorderedAccessView(pBuffer, VK_NULL_HANDLE, firstElement, elementCount));
    return SharedPtr(new UnorderedAccessView(pBuffer, view, firstElement, elementCount));
}

RenderTargetView::~RenderTargetView() {
    static std::vector<RenderTargetView::ApiHandle> hdl;
    hdl.push_back(mApiHandle);
}

RenderTargetView::SharedPtr RenderTargetView::create(ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    if (!pTexture && getNullView()) return getNullView();

    // Check type
    if (pTexture->getApiHandle().getType() == VkResourceType::Buffer) {
        logWarning("Cannot create RenderTargetView from a buffer!");
        return getNullView();
    }

    // Create view
    auto view = createViewCommon(pTexture, mipLevel, 1, firstArraySlice, arraySize);
    return SharedPtr(new RenderTargetView(pTexture, view, mipLevel, firstArraySlice, arraySize));
}

ConstantBufferView::SharedPtr ConstantBufferView::create(ConstBufferSharedPtrRef pBuffer) {
    if (!pBuffer && getNullView()) return getNullView();

    //DescriptorSet::Layout layout;
    //layout.addRange(DescriptorSet::Type::Cbv, 0, 1);
    //auto handle = DescriptorSet::create(gpDevice->getCpuDescriptorPool(), layout);
    VkBufferView bufferView = {};
    auto handle = VkResource<VkImageView, VkBufferView>::SharedPtr::create(bufferView, nullptr);


    return SharedPtr(new ConstantBufferView(pBuffer, handle));
}

}  // namespace Falcor
