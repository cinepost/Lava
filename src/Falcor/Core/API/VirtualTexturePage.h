#ifndef SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
#define SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_

#include <array>
#include <vector>
#include <string>
#include <memory>

#include "Falcor/Core/Framework.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include "gfx_lib/vulkan/vk-virtual-texture-page.h"

namespace Falcor {

class Device;
class Texture;
class TextureManager;

// Virtual texture page as a part of the partially resident texture
// Contains memory bindings, offsets and status information
class dlldecl VirtualTexturePage: public std::enable_shared_from_this<VirtualTexturePage>  {
  public:
  		static constexpr uint32_t kInvalidID = 0xffffffff;
		using SharedPtr = std::shared_ptr<VirtualTexturePage>;
		using SharedConstPtr = std::shared_ptr<const VirtualTexturePage>;

		using PageData = std::array<uint8_t, 65536>;

		/** Create a new vertex buffer layout object.
			\return New object, or throws an exception on error.
		*/
		static SharedPtr create(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer);

		~VirtualTexturePage();

		bool isResident() const { return mpVirtualTexturePageResource->isResident(); }
		bool allocate() { return mpVirtualTexturePageResource->allocate(); }
		void release() { mpVirtualTexturePageResource->release(); }

		const std::shared_ptr<Device>& device() const { return mpDevice; }

		gfx::IVirtualTexturePageResource::Offset offsetGFX() const { return mpVirtualTexturePageResource->getOffset(); }
		gfx::IVirtualTexturePageResource::Extent extentGFX() const { return mpVirtualTexturePageResource->getExtent(); }

		size_t getUsedMemSize() const { return mpVirtualTexturePageResource->getUsedMemSize(); }

		uint32_t width() const { return mpVirtualTexturePageResource->getWidth(); }
		uint32_t height() const { return mpVirtualTexturePageResource->getHeight(); }
		uint32_t depth() const { return mpVirtualTexturePageResource->getDepth(); }

		uint32_t mipLevel() const { return mpVirtualTexturePageResource->getMipLevel(); }
		uint32_t index() const { return mpVirtualTexturePageResource->getIndex(); }

		//uint32_t id() const { return mID; }

		const std::shared_ptr<Texture>& texture() const { return mpTexture; }

  	public:
  		VirtualTexturePage(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer);

 	protected:
		const std::shared_ptr<Device>   mpDevice;
		const std::shared_ptr<Texture>  mpTexture;

		Slang::ComPtr<gfx::IVirtualTexturePageResource> mpVirtualTexturePageResource;

		uint32_t mID;

		friend class Texture;
		friend class TextureManager;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
