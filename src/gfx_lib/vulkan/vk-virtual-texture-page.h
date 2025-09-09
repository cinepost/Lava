// vk-texture.h
#pragma once

#include <vector>

#include "vk-device.h"

namespace gfx {

using namespace Slang;

namespace vk {

class VirtualTexturePageResourceImpl: public VirtualTexturePageResource {
    public:
        VirtualTexturePageResourceImpl(DeviceImpl* device, const Offset3D& offset, const Extent3D& extent, uint32_t mipLevel, uint32_t layer);
        ~VirtualTexturePageResourceImpl();

        VkOffset3D offsetVK() const { return {mOffset.x, mOffset.y, mOffset.z}; }
        VkExtent3D extentVK() const { return {mExtent.width, mExtent.height, mExtent.depth}; }

        virtual bool isResident() const override;

        virtual bool allocateMemory() override;
        virtual void releaseMemory() override;

        virtual size_t getUsedMemSize() const override;

    protected:
        RefPtr<DeviceImpl> m_device;

        VkSparseImageMemoryBind mImageMemoryBind;   // Sparse image memory bind for this page
        VkDeviceSize mDevMemSize;                   // Page memory size in bytes
        uint32_t mMemoryTypeBits;

        VmaAllocation mAllocation;

        friend class DeviceImpl;
};

} // namespace vk
} // namespace gfx
