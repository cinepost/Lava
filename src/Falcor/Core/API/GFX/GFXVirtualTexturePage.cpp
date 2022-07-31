#include <vector>
#include <string>
#include <memory>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/VirtualTexturePage.h"

namespace Falcor {

// Allocate Vulkan memory for the virtual page
void VirtualTexturePage::allocate() {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE) {
        LLOG_ERR << "VirtualTexturePage already allocated !!!";
        return;
    }

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = NULL;
    memAllocInfo.allocationSize = mDevMemSize;
    memAllocInfo.memoryTypeIndex = mpTexture->memoryTypeIndex();
    
    //if (VK_FAILED(vkAllocateMemory(mpDevice->getApiHandle(), &memAllocInfo, nullptr, &mImageMemoryBind.memory))) {
    //    throw std::runtime_error("Error allocating memory for virtual texture page !!!");
    //}

    VkMemoryRequirements memRequirements = {};
    memRequirements.size = mDevMemSize;
    memRequirements.alignment = mDevMemSize;
    memRequirements.memoryTypeBits = mMemoryTypeBits;

    VmaAllocationCreateInfo vmaMemAllocInfo = {};
    vmaMemAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocationInfo vmaAllocInfo = {};

    vmaAllocateMemory(mpDevice->allocator(), &memRequirements, &vmaMemAllocInfo, &mAllocation, &vmaAllocInfo);   

    mImageMemoryBind.memory = vmaAllocInfo.deviceMemory;
    mImageMemoryBind.memoryOffset = vmaAllocInfo.offset;

    mpTexture->mSparseResidentMemSize += mDevMemSize;
}

// Release Vulkan memory allocated for this page
void VirtualTexturePage::release() {
    if (mImageMemoryBind.memory == VK_NULL_HANDLE) {
        LLOG_ERR << "VirtualTexturePage already released !!!";
        return;
    }
    vmaFreeMemory(mpDevice->allocator(), mAllocation);

    mpTexture->mSparseResidentMemSize -= mDevMemSize;
    mImageMemoryBind.memory = VK_NULL_HANDLE;
}

}  // namespace Falcor
