#include <vector>
#include <string>
#include <memory>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/VirtualTexturePage.h"

namespace Falcor {

VirtualTexturePage::VirtualTexturePage(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer): mpDevice(pTexture->device()), mpTexture(pTexture), mMipLevel(mipLevel), mLayer(layer) {
    mOffset = {offset[0], offset[1], offset[2]};
    mExtent = {extent[0], extent[1], extent[2]};

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

VirtualTexturePage::~VirtualTexturePage() {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE) release();
}

size_t VirtualTexturePage::usedMemSize() const {
    if (mImageMemoryBind.memory != VK_NULL_HANDLE)
        return mDevMemSize;

    return 0;
}

// Allocate Vulkan memory for the virtual page
void VirtualTexturePage::allocate() {
	if (mImageMemoryBind.memory != VK_NULL_HANDLE) {
		// VirtualTexturePage already allocated
		return;
	}

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = NULL;
	memAllocInfo.allocationSize = mDevMemSize;
	memAllocInfo.memoryTypeIndex = mpTexture->memoryTypeIndex();
	
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
		return;
	}   

	mImageMemoryBind.memory = vmaAllocInfo.deviceMemory;
	mImageMemoryBind.memoryOffset = vmaAllocInfo.offset;

	mpTexture->mSparseResidentMemSize += mDevMemSize;
	mIsResident = true;
}

// Release Vulkan memory allocated for this page
void VirtualTexturePage::release() {
	if (mImageMemoryBind.memory == VK_NULL_HANDLE) {
		return;
	}
	vmaFreeMemory(mpDevice->allocator(), mAllocation);

	mpTexture->mSparseResidentMemSize -= mDevMemSize;
	mImageMemoryBind.memory = VK_NULL_HANDLE;
	mIsResident = false;
}

}  // namespace Falcor
