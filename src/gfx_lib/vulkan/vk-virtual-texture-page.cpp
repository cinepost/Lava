// vk-texture.cpp
#include "lava_utils_lib/logging.h"

#include "vk-virtual-texture-page.h"

namespace gfx {

using namespace Slang;

namespace vk {

VirtualTexturePageResourceImpl::VirtualTexturePageResourceImpl(DeviceImpl* device, const Offset& offset, const Extent& extent, uint32_t mipLevel, uint32_t layer)
    : VirtualTexturePageResource(offset, extent, mipLevel, layer), m_device(device)
{
    // Pages are initially not backed up by memory (non-resident)
    mImageMemoryBind = {};
    mImageMemoryBind.memory = VK_NULL_HANDLE;

    VkImageSubresource subResource = {};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.mipLevel = mipLevel;
    subResource.arrayLayer = layer;

    mImageMemoryBind.subresource = subResource;
    mImageMemoryBind.flags = VK_SPARSE_MEMORY_BIND_METADATA_BIT;
    mImageMemoryBind.offset = mOffset;
    mImageMemoryBind.extent = mExtent;
    mImageMemoryBind.memory = VK_NULL_HANDLE;
}

VirtualTexturePageResourceImpl::~VirtualTexturePageResourceImpl() {
    release();
}

size_t VirtualTexturePageResourceImpl::getUsedMemSize() const {
   return (mImageMemoryBind.memory != VK_NULL_HANDLE) ? mDevMemSize : 0;
}

// Allocate Vulkan memory for the virtual page
bool VirtualTexturePageResourceImpl::allocate() {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE) {
        // VirtualTexturePage already allocated
        return false;
    }
    
    VkMemoryRequirements memRequirements = {};
    memRequirements.size = mDevMemSize;
    memRequirements.alignment = mDevMemSize;
    memRequirements.memoryTypeBits = mMemoryTypeBits;

    VmaAllocationCreateInfo vmaMemAllocInfo = {};
    vmaMemAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocationInfo vmaAllocInfo = {};

    VkResult result = vmaAllocateMemory(mpDevice->allocator(), &memRequirements, &vmaMemAllocInfo, &mAllocation, &vmaAllocInfo);

    if( result != VK_SUCCESS ){
        LLOG_ERR << "Error allocating virtual page memory !!! VkResult: " << to_string(result);
        return false;
    }   

    mImageMemoryBind.memory = vmaAllocInfo.deviceMemory;
    mImageMemoryBind.memoryOffset = vmaAllocInfo.offset;

    mpTexture->mSparseResidentMemSize += mDevMemSize;
    mIsResident = true;
    return true;
}

// Release Vulkan memory allocated for this page
void VirtualTexturePageResourceImpl::release() {
    if (mImageMemoryBind.memory == VK_NULL_HANDLE) {
        return;
    }
    vmaFreeMemory(mpDevice->allocator(), mAllocation);

    mpTexture->mSparseResidentMemSize -= mDevMemSize;
    mImageMemoryBind.memory = VK_NULL_HANDLE;
    mIsResident = false;
}


} // namespace vk
} // namespace gfx
