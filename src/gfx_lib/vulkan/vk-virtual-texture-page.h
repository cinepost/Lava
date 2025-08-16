// vk-texture.h
#pragma once

#include <vector>

#include "vk-base.h"
#include "vk-device.h"

namespace gfx {

using namespace Slang;

namespace vk {

class VirtualTexturePageResourceImpl: public IVirtualTexturePageResource {
    public:
        VirtualTexturePageResourceImpl(DeviceImpl* device, const Offset& offset, const Extent& extent, uint32_t mipLevel, uint32_t layer);
        ~VirtualTexturePageResourceImpl();

        VkOffset3D offsetVK() const { return {mOffset.x, mOffset.y, mOffset.z}; }
        VkExtent3D extentVK() const { return {mExtent.width, mExtent.height, mExtent.depth}; }

        virtual bool allocate() override;
        virtual void release() override;

        virtual size_t getUsedMemSize() const override;

    protected:
        RefPtr<DeviceImpl> m_device;

        VkSparseImageMemoryBind mImageMemoryBind;   // Sparse image memory bind for this page
        VkDeviceSize mDevMemSize;                   // Page memory size in bytes
        uint32_t mMemoryTypeBits;

        VmaAllocation mAllocation;
};

} // namespace vk
} // namespace gfx
