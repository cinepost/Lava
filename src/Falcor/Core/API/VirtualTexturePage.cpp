#include <vector>
#include <string>
#include <memory>

#include "Device.h"
#include "Buffer.h"
#include "VirtualTexturePage.h"

namespace Falcor {

VirtualTexturePage::SharedPtr VirtualTexturePage::create(const Device::SharedPtr& pDevice, const std::shared_ptr<Texture>& pTexture, uint32_t mipLevel, uint32_t layer) {
    return SharedPtr(new VirtualTexturePage(pDevice, pTexture, mipLevel, layer));
}

VirtualTexturePage::VirtualTexturePage(const Device::SharedPtr& pDevice, const std::shared_ptr<Texture>& pTexture, uint32_t mipLevel, uint32_t layer): mpDevice(pDevice), mpTexture(pTexture), mMipLevel(mipLevel), mLayer(layer) {
    // Pages are initially not backed up by memory (non-resident)
    mImageMemoryBind = {};
    mImageMemoryBind.memory = VK_NULL_HANDLE;

    VkImageSubresource subResource = {};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.mipLevel = mMipLevel;
    subResource.arrayLayer = mLayer;
    mImageMemoryBind.subresource = subResource;
    mImageMemoryBind.flags = VK_SPARSE_MEMORY_BIND_METADATA_BIT;

}

VirtualTexturePage::~VirtualTexturePage() {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE) release();
}

bool VirtualTexturePage::isResident() const {
    return (mImageMemoryBind.memory != VK_NULL_HANDLE);
}

size_t VirtualTexturePage::usedMemSize() const {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE)
        return mDevMemSize;

    return 0;
}

// Allocate Vulkan memory for the virtual page
void VirtualTexturePage::allocate() {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE) {
        LOG_ERR("VirtualTexturePage already allocated !!!");
        return;
    }

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = NULL;
    memAllocInfo.allocationSize = mDevMemSize;
    memAllocInfo.memoryTypeIndex = mpTexture->memoryTypeIndex();
    
    if (VK_FAILED(vkAllocateMemory(mpDevice->getApiHandle(), &memAllocInfo, nullptr, &mImageMemoryBind.memory))) {
        throw std::runtime_error("Error allocating memory for virtual texture page !!!");
    }

    mpTexture->mSparseResidentMemSize += mDevMemSize;

    // Sparse image memory binding
    mImageMemoryBind.extent = mExtent;
    mImageMemoryBind.offset = mOffset;
}

// Release Vulkan memory allocated for this page
void VirtualTexturePage::release() {
    if (mImageMemoryBind.memory == VK_NULL_HANDLE) {
        LOG_ERR("VirtualTexturePage already released !!!");
        return;
    }

    vkFreeMemory(mpDevice->getApiHandle(), mImageMemoryBind.memory, nullptr);

    mpTexture->mSparseResidentMemSize -= mDevMemSize;
    mImageMemoryBind.memory = VK_NULL_HANDLE;
}

}  // namespace Falcor
