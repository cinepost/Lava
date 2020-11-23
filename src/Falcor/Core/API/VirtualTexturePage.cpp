#include <vector>
#include <string>
#include <memory>

#include "Device.h"
#include "VirtualTexturePage.h"

namespace Falcor {

VirtualTexturePage::SharedPtr VirtualTexturePage::create(const Device::SharedPtr& pDevice) {
    return SharedPtr(new VirtualTexturePage(pDevice));
}

VirtualTexturePage::VirtualTexturePage(const Device::SharedPtr& pDevice): mpDevice(pDevice) {
    // Pages are initially not backed up by memory (non-resident)
    mImageMemoryBind.memory = VK_NULL_HANDLE;
}

bool VirtualTexturePage::isResident() {
    return (mImageMemoryBind.memory != VK_NULL_HANDLE);
}

// Allocate Vulkan memory for the virtual page
void VirtualTexturePage::allocate(uint32_t memoryTypeIndex) {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE)
        return;

    mImageMemoryBind = {};

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = NULL;
    memAllocInfo.allocationSize = mDevMemSize;
    memAllocInfo.memoryTypeIndex = memoryTypeIndex;
    
    if (VK_FAILED(vkAllocateMemory(mpDevice->getApiHandle(), &memAllocInfo, nullptr, &mImageMemoryBind.memory))) {
        throw std::runtime_error("Error allocating memory for virtual texture page !!!");
    }

    VkImageSubresource subResource{};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.mipLevel = mMipLevel;
    subResource.arrayLayer = mLayer;

    // Sparse image memory binding
    mImageMemoryBind.subresource = subResource;
    mImageMemoryBind.extent = mExtent;
    mImageMemoryBind.offset = mOffset;
}

// Release Vulkan memory allocated for this page
void VirtualTexturePage::release() {
    if (mImageMemoryBind.memory == VK_NULL_HANDLE)
        return;

    vkFreeMemory(mpDevice->getApiHandle(), mImageMemoryBind.memory, nullptr);
    mImageMemoryBind.memory = VK_NULL_HANDLE;
}


}  // namespace Falcor
