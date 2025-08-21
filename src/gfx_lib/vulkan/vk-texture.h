// vk-texture.h
#pragma once

#include <vector>

#include "vk-base.h"
#include "vk-device.h"

namespace gfx {

using namespace Slang;

namespace vk {

class TextureResourceImpl : public TextureResource {
    public:
        typedef TextureResource Parent;
        TextureResourceImpl(const Desc& desc, DeviceImpl* device);
        ~TextureResourceImpl();

        VkImage m_image = VK_NULL_HANDLE;
        VkFormat m_vkformat = VK_FORMAT_R8G8B8A8_UNORM;
        
        VmaAllocation   mAllocation = VK_NULL_HANDLE;
        VmaAllocationInfo mAllocationInfo = {};

        std::vector<VmaAllocation> mTailAllocations;

        VkSemaphore mBindSparseSemaphore = VK_NULL_HANDLE;
        VkMemoryRequirements mMemRequirements;

        uint32_t mMemoryTypeIndex;                                      // @todo: Comment

        IVirtualTexturePageResource::Extent mSparsePageRes;
        uint32_t mSparseBindsCount;

        VkBindSparseInfo mBindSparseInfo;                               // Sparse queue binding information
        std::vector<VkSparseImageMemoryBind> mSparseImageMemoryBinds;   // Sparse image memory bindings of all memory-backed virtual tables
        std::vector<VkSparseMemoryBind> mOpaqueMemoryBinds;             // Sparse opaque memory bindings for the mip tail (if present)
        VkSparseImageMemoryBindInfo mImageMemoryBindInfo;               // Sparse image memory bind info
        VkSparseImageOpaqueMemoryBindInfo mOpaqueMemoryBindInfo;        // Sparse image opaque memory bind info (mip tail)
        VkSparseImageMemoryRequirements mSparseImageMemoryRequirements; // @todo: Comment

        VkSparseImageMemoryBind mMipTailimageMemoryBind{};

        bool mTailMemoryAllocated = false;
        bool mIsSparse = false;
        bool m_isWeakImageReference = false;
        RefPtr<DeviceImpl> m_device;

        uint32_t getMemoryTypeIndex() const { return mMemoryTypeIndex; }

        const VkSparseImageMemoryRequirements& getSparseImageMemoryRequirements() const { return mSparseImageMemoryRequirements; }

        const VkMemoryRequirements& getMemoryRequirements() const { return mMemRequirements; }

        const VkBindSparseInfo* getBingSparseInfo() const { return &mBindSparseInfo; }

        virtual bool isSparse() const override { return mIsSparse; }

        virtual uint32_t sparseDataBindsCount() const override { return mSparseBindsCount; }

        virtual const ITextureResource::Extents& sparseDataPageRes() const override { return mSparsePageRes; }

        virtual SLANG_NO_THROW Result SLANG_MCALL getNativeResourceHandle(InteropHandle* outHandle) override;

        virtual SLANG_NO_THROW Result SLANG_MCALL getSharedHandle(InteropHandle* outHandle) override;

        virtual SLANG_NO_THROW Result SLANG_MCALL setDebugName(const char* name) override;
};

} // namespace vk
} // namespace gfx
