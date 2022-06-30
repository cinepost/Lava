#ifndef SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
#define SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_

#include <vector>
#include <string>
#include <memory>

#include "Falcor/Core/Framework.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

namespace Falcor {

class Device;
class Texture;
class ResourceManager;

// Virtual texture page as a part of the partially resident texture
// Contains memory bindings, offsets and status information
class dlldecl VirtualTexturePage: public std::enable_shared_from_this<VirtualTexturePage>  {
  public:
		using SharedPtr = std::shared_ptr<VirtualTexturePage>;
		using SharedConstPtr = std::shared_ptr<const VirtualTexturePage>;

		/** Create a new vertex buffer layout object.
			\return New object, or throws an exception on error.
		*/
		static SharedPtr create(const std::shared_ptr<Device> pDevice, const std::shared_ptr<Texture> pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer);

		~VirtualTexturePage();

		bool isResident() const;
		void allocate();
		void release();

		uint3 offset() const { return {mOffset.x, mOffset.y, mOffset.z}; }
		VkOffset3D offsetVK() const { return mOffset; }
		uint3 extent() const { return {mExtent.width, mExtent.height, mExtent.depth}; }
		VkExtent3D extentVK() const { return mExtent; }

		size_t usedMemSize() const;

		const uint32_t width() const { return mExtent.width; }
		const uint32_t height() const { return mExtent.height; }
		const uint32_t depth() const { return mExtent.depth; }

		const uint32_t mipLevel() const { return mMipLevel; }
		const uint32_t index() const { return mIndex; }

		const uint32_t id() const { return mID; }

		const std::shared_ptr<Texture> texture() const { return mpTexture; }

 	protected:
		VirtualTexturePage(const std::shared_ptr<Device> pDevice, const std::shared_ptr<Texture> pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer);

		const std::shared_ptr<Device>   mpDevice;
		const std::shared_ptr<Texture>  mpTexture;

		VkOffset3D mOffset;
		VkExtent3D mExtent;
		VkSparseImageMemoryBind mImageMemoryBind;		// Sparse image memory bind for this page
		VkDeviceSize mDevMemSize;                   // Page memory size in bytes
		uint32_t mMipLevel;                         // Mip level that this page belongs to
		uint32_t mLayer;                            // Array layer that this page belongs to
		uint32_t mIndex;    												// Texture page index 
		uint32_t mID;       												// Global page id
		uint32_t mMemoryTypeBits;

		VmaAllocation mAllocation;

		friend class Texture;
		friend class ResourceManager;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
