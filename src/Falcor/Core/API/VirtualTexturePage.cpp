#include <vector>
#include <string>
#include <memory>

#include "Device.h"
#include "Buffer.h"
#include "Texture.h"
#include "VirtualTexturePage.h"

namespace Falcor {

VirtualTexturePage::SharedPtr VirtualTexturePage::create(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer) {
    return SharedPtr(new VirtualTexturePage(pTexture, offset, extent, mipLevel, layer));
}

VirtualTexturePage::VirtualTexturePage(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer): mpDevice(pTexture->device()), mpTexture(pTexture), mMipLevel(mipLevel), mLayer(layer) {
    mOffset = {offset[0], offset[1], offset[2]};
    mExtent = {extent[0], extent[1], extent[2]};

    // Pages are initially not backed up by memory (non-resident)
    mImageMemoryBind = {};
    mImageMemoryBind.memory = VK_NULL_HANDLE;

    VkImageSubresource subResource = {};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.mipLevel = mMipLevel;
    subResource.arrayLayer = mLayer;

    mImageMemoryBind.subresource = subResource;
    mImageMemoryBind.flags = VK_SPARSE_MEMORY_BIND_METADATA_BIT;
    mImageMemoryBind.offset = mOffset;
    mImageMemoryBind.extent = mExtent;
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

}  // namespace Falcor
