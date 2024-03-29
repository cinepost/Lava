#ifndef SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
#define SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_

#include <array>
#include <vector>
#include <string>
#include <memory>

#include "Falcor/Core/Framework.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"


#if defined(FALCOR_GFX_VK) 
namespace gfx {
	namespace vk {
		class DeviceImpl;
	}
}
#endif

namespace Falcor {

class Device;
class Texture;
class TextureManager;

// Virtual texture page as a part of the partially resident texture
// Contains memory bindings, offsets and status information
class dlldecl VirtualTexturePage: public std::enable_shared_from_this<VirtualTexturePage>  {
  public:
		using SharedPtr = std::shared_ptr<VirtualTexturePage>;
		using SharedConstPtr = std::shared_ptr<const VirtualTexturePage>;

		using PageData = std::array<uint8_t, 65536>;

		/** Create a new vertex buffer layout object.
			\return New object, or throws an exception on error.
		*/
		static SharedPtr create(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer);

		~VirtualTexturePage();

		bool isResident() const { return mIsResident; }
		bool allocate();
		void release();

		const std::shared_ptr<Device>& device() const { return mpDevice; }

		uint3 offset() const { return {mOffset.x, mOffset.y, mOffset.z}; }
		const VkOffset3D& offsetVK() const { return mOffset; }
		uint3 extent() const { return {mExtent.width, mExtent.height, mExtent.depth}; }
		const VkExtent3D& extentVK() const { return mExtent; }

#if defined(FALCOR_GFX)
		gfx::ITextureResource::Offset3D offsetGFX() const { return {mOffset.x, mOffset.y, mOffset.z}; }
		gfx::ITextureResource::Extents extentGFX() const { return {static_cast<gfx::GfxCount>(mExtent.width), static_cast<gfx::GfxCount>(mExtent.height), static_cast<gfx::GfxCount>(mExtent.depth)}; }
#endif

		size_t usedMemSize() const;

		const uint32_t width() const { return mExtent.width; }
		const uint32_t height() const { return mExtent.height; }
		const uint32_t depth() const { return mExtent.depth; }

		const uint32_t mipLevel() const { return mMipLevel; }
		const uint32_t index() const { return mIndex; }

		const uint32_t id() const { return mID; }

		const std::shared_ptr<Texture> texture() const { return mpTexture; }

  public:
  	VirtualTexturePage(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer);

 	protected:
		const std::shared_ptr<Device>   mpDevice;
		const std::shared_ptr<Texture>  mpTexture;

		bool mIsResident = false;

		VkOffset3D mOffset;
		VkExtent3D mExtent;
		VkSparseImageMemoryBind mImageMemoryBind;		// Sparse image memory bind for this page
		VkDeviceSize mDevMemSize;                   // Page memory size in bytes
		uint32_t mMipLevel;                         // Mip level that this page belongs to
		uint32_t mLayer;                            // Array layer that this page belongs to
		uint32_t mIndex;    												// Texture related page index 
		uint32_t mID;       												// Global page id (Texture manager index)
		uint32_t mMemoryTypeBits;

		VmaAllocation mAllocation;

		friend class Texture;
		friend class TextureManager;
#if defined(FALCOR_GFX_VK)
		friend class gfx::vk::DeviceImpl;
#endif
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
