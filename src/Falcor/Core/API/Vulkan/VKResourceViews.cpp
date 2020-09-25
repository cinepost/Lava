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

static Texture::SharedPtr sBlackTextures[FALCOR_MAX_DEVICES] = {nullptr};
static Buffer::SharedPtr sZeroBuffers[FALCOR_MAX_DEVICES] = {nullptr};
static Buffer::SharedPtr sZeroTypedBuffers[FALCOR_MAX_DEVICES] = {nullptr};

VkImageAspectFlags getAspectFlagsFromFormat(ResourceFormat format, bool ignoreStencil = false);

template<typename ApiHandleType>
ResourceView<ApiHandleType>::~ResourceView() {
    auto _pResource = getResource();
    if (_pResource) {
        _pResource->device()->releaseResource(mApiHandle);
    }
}

Texture::SharedPtr createBlackTexture(Device::SharedPtr pDevice) {
    uint8_t blackPixel[4] = { 0 };
    return Texture::create2D(pDevice, 1, 1, ResourceFormat::RGBA8Unorm, 1, 1, blackPixel, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess);
}

Buffer::SharedPtr createZeroBuffer(Device::SharedPtr pDevice) {
    static const uint32_t zero = 0;
    return Buffer::create(pDevice, sizeof(uint32_t), Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, &zero);
}

Buffer::SharedPtr createZeroTypedBuffer(Device::SharedPtr pDevice) {
    static const uint32_t zero = 0;
    return Buffer::createTyped<uint32_t>(pDevice, 1, Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, &zero);
}

Texture::SharedPtr getEmptyTexture(Device::SharedPtr pDevice) {
    assert(pDevice);
    auto uid = pDevice->uid();
    if(!sBlackTextures[uid]) sBlackTextures[uid] = createBlackTexture(pDevice);
    return sBlackTextures[uid];
}

Buffer::SharedPtr getEmptyBuffer(Device::SharedPtr pDevice) {
    assert(pDevice);
    auto uid = pDevice->uid();
    if(!sZeroBuffers[uid]) sZeroBuffers[uid] = createZeroBuffer(pDevice);
    return sZeroBuffers[uid];
}

Buffer::SharedPtr getEmptyTypedBuffer(Device::SharedPtr pDevice) {
    assert(pDevice);
    auto uid = pDevice->uid();
    if(!sZeroTypedBuffers[uid]) sZeroTypedBuffers[uid] = createZeroTypedBuffer(pDevice);
    return sZeroTypedBuffers[uid];
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

    outInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    outInfo.buffer = pBuffer->getApiHandle();
    outInfo.offset = 0;
    outInfo.range = VK_WHOLE_SIZE;
    outInfo.format = getVkFormat(buffFormat);
    return outInfo;
}

VkResource<VkImageView, VkBufferView>::SharedPtr createViewCommon(const Resource::SharedConstPtr& pSharedPtr, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
    const Resource* pResource = pSharedPtr.get();
    assert(pResource);

    LOG_DBG("1");
    switch (pResource->getApiHandle().getType()) {
        case VkResourceType::Image: {
            LOG_DBG("2");
            VkImageViewCreateInfo info = initializeImageViewInfo((const Texture*)pResource, mostDetailedMip, mipCount, firstArraySlice, arraySize);
            VkImageView imageView;
            LOG_DBG("2.2");
            assert(pResource->device());
            vk_call(vkCreateImageView(pResource->device()->getApiHandle(), &info, nullptr, &imageView));
            LOG_DBG("2.3");
            return VkResource<VkImageView, VkBufferView>::SharedPtr::create(pResource->device(), imageView, nullptr);
        }

        case VkResourceType::Buffer: {
            // We only create views for typed Buffers
            VkBufferView bufferView = {};
            const Buffer* pBuffer = dynamic_cast<const Buffer*>(pResource);

            if (pBuffer->isTyped()) {
                VkBufferViewCreateInfo info = initializeBufferViewInfo(pBuffer);
                vk_call(vkCreateBufferView(pResource->device()->getApiHandle(), &info, nullptr, &bufferView));
            }

            return VkResource<VkImageView, VkBufferView>::SharedPtr::create(pResource->device(), bufferView, nullptr);
        }

        default:
            should_not_get_here();
            return VkResource<VkImageView, VkBufferView>::SharedPtr();
    }
}

ShaderResourceView::SharedPtr ShaderResourceView::create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
    if (!pTexture) {
        return getNullView(pDevice);
    }

    // Resource::ApiHandle resHandle = pTexture->getApiHandle();
    auto view = createViewCommon(pTexture, mostDetailedMip, mipCount, firstArraySlice, arraySize);
    return SharedPtr(new ShaderResourceView(pDevice, pTexture, view, mostDetailedMip, mipCount, firstArraySlice, arraySize));
}

ShaderResourceView::SharedPtr ShaderResourceView::create(std::shared_ptr<Device> pDevice, ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount) {
    if (!pBuffer) {
        return getNullBufferView(pDevice);
    }

    if (!pBuffer) {
        VkBufferView bufferView = {};
        auto view = VkResource<VkImageView, VkBufferView>::SharedPtr::create(pDevice, bufferView, nullptr);
        return SharedPtr(new ShaderResourceView(pDevice, pBuffer, view, firstElement, elementCount));
    }

    if (pBuffer->getApiHandle().getType() == VkResourceType::Image) {
        logWarning("Cannot create DepthStencilView from a texture!");
        return getNullBufferView(pDevice);
    }
    
    const Resource* pResource = pBuffer.get();
    assert(pResource);

    // We only create views for typed Buffers
    VkBufferView bufferView = {};
    const Buffer* buffer = dynamic_cast<const Buffer*>(pResource);

    if (buffer->isTyped()) {
        VkBufferViewCreateInfo info = initializeBufferViewInfo(buffer);
        vk_call(vkCreateBufferView(pBuffer->device()->getApiHandle(), &info, nullptr, &bufferView));
    }

    auto view = VkResource<VkImageView, VkBufferView>::SharedPtr::create(pDevice, bufferView, nullptr);

    return SharedPtr(new ShaderResourceView(pDevice, pBuffer, view, firstElement, elementCount));
}

DepthStencilView::SharedPtr DepthStencilView::create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    if (!pTexture) {
        return getNullView(pDevice);
    }

    if (pTexture->getApiHandle().getType() == VkResourceType::Buffer) {
        logWarning("Cannot create DepthStencilView from a buffer!");
        return getNullView(pDevice);
    }

    auto view = createViewCommon(pTexture, mipLevel, 1, firstArraySlice, arraySize);
    return SharedPtr(new DepthStencilView(pDevice, pTexture, view, mipLevel, firstArraySlice, arraySize));
}

UnorderedAccessView::SharedPtr UnorderedAccessView::create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    if (!pTexture) {
        return getNullView(pDevice);
    }

    if (pTexture->getApiHandle().getType() == VkResourceType::Buffer) {
        logWarning("Cannot create UnorderedAccessView from a buffer!");
        return getNullView(pDevice);
    }

    auto view = createViewCommon(pTexture, mipLevel, 1, firstArraySlice, arraySize);
    return SharedPtr(new UnorderedAccessView(pDevice, pTexture, view, mipLevel, firstArraySlice, arraySize));
}

UnorderedAccessView::SharedPtr UnorderedAccessView::create(std::shared_ptr<Device> pDevice, ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount) {
    if (!pBuffer) {
        return getNullBufferView(pDevice);
    } 

    if (!pBuffer) {
        VkBufferView bufferView = {};
        auto view = VkResource<VkImageView, VkBufferView>::SharedPtr::create(pDevice, bufferView, nullptr);
        return SharedPtr(new UnorderedAccessView(pDevice, pBuffer, view, firstElement, elementCount));
    }

    if (pBuffer->getApiHandle().getType() == VkResourceType::Image) {
        logWarning("Cannot create UnorderedAccessView from a texture!");
        return getNullBufferView(pBuffer->device());
    }

    const Resource* pResource = pBuffer.get();
    assert(pResource);
    
    // We only create views for typed Buffers
    VkBufferView bufferView = {};
    const Buffer* buffer = dynamic_cast<const Buffer*>(pResource);

    if (buffer->isTyped()) {
        VkBufferViewCreateInfo info = initializeBufferViewInfo(buffer);
        vk_call(vkCreateBufferView(pDevice->getApiHandle(), &info, nullptr, &bufferView));
    }

    auto view = VkResource<VkImageView, VkBufferView>::SharedPtr::create(pDevice, bufferView, nullptr);
    return SharedPtr(new UnorderedAccessView(pDevice, pBuffer, view, firstElement, elementCount));
}

RenderTargetView::~RenderTargetView() {
    static std::vector<RenderTargetView::ApiHandle> hdl;
    hdl.push_back(mApiHandle);
}

RenderTargetView::SharedPtr RenderTargetView::create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    if (!pTexture) return getNullView(pDevice);

    // Check type
    if (pTexture->getApiHandle().getType() == VkResourceType::Buffer) {
        logWarning("Cannot create RenderTargetView from a buffer!");
        return getNullView(pDevice);
    }

    // Create view
    auto view = createViewCommon(pTexture, mipLevel, 1, firstArraySlice, arraySize);
    return SharedPtr(new RenderTargetView(pDevice, pTexture, view, mipLevel, firstArraySlice, arraySize));
}

ConstantBufferView::SharedPtr ConstantBufferView::create(std::shared_ptr<Device> pDevice, ConstBufferSharedPtrRef pBuffer) {
    if (!pBuffer) return getNullView(pDevice);

    VkBufferView bufferView = {};    
    auto handle = VkResource<VkImageView, VkBufferView>::SharedPtr::create(pDevice, bufferView, nullptr);

    return SharedPtr(new ConstantBufferView(pDevice, pBuffer, handle));
}

}  // namespace Falcor
