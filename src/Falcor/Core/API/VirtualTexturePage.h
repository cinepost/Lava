#ifndef SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
#define SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_

#include <array>
#include <vector>
#include <string>
#include <memory>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/Object.h"
#include "Falcor/Core/API/GFXAPI.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include "gfx_lib/vulkan/vk-virtual-texture-page.h"

namespace Falcor {

class Device;
class Texture;
class TextureManager;

// Virtual texture page as a part of the partially resident texture
// Contains memory bindings, offsets and status information
class FALCOR_API VirtualTexturePage: public Object {
	FALCOR_OBJECT(VirtualTexturePage)
  public:
  		static constexpr uint32_t kInvalidID = 0xffffffff;
		using PageData = std::array<uint8_t, 65536>;

		/** Create a new vertex buffer layout object.
			\return New object, or throws an exception on error.
		*/
		static SharedPtr create(const Falcor::SharedPtr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer, uint32_t index, uint32_t size, uint32_t memoryTypeBits);

		~VirtualTexturePage();

		bool isResident() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->isResident(); 
		}
		
		bool allocate() { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->allocateMemory(); 
		}
		
		void release() { 
			assert(mpVirtualTexturePageResource); 
			mpVirtualTexturePageResource->releaseMemory(); 
		}

		const Falcor::SharedPtr<Device>& getDevice() const { return mpDevice; }

		const gfx::IVirtualTexturePageResource::Offset3D& offsetGFX() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->getOffset(); 
		}

		const gfx::IVirtualTexturePageResource::Extent3D& extentGFX() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->getExtent(); 
		}

		size_t getUsedMemSize() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->getUsedMemSize(); 
		}

		uint32_t width() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->getWidth(); 
		}
		
		uint32_t height() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->getHeight(); 
		}
		
		uint32_t depth() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->getDepth(); 
		}

		uint32_t mipLevel() const { 
			assert(mpVirtualTexturePageResource); 
			return mpVirtualTexturePageResource->getMipLevel(); 
		}
		
		uint32_t index() const { return mIndex; }

		//uint32_t id() const { return mID; }

		const Falcor::SharedPtr<Texture>& texture() const { return mpTexture; }

		const gfx::IVirtualTexturePageResource* getGfxTexturePageResource() const { return mpVirtualTexturePageResource.get(); }

  	public:
  		VirtualTexturePage(const Falcor::SharedPtr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer, uint32_t index, uint32_t size, uint32_t memoryTypeBits);

 	protected:
		const Falcor::SharedPtr<Device>   mpDevice;
		const Falcor::SharedPtr<Texture>  mpTexture;

		Slang::ComPtr<gfx::IVirtualTexturePageResource> mpVirtualTexturePageResource;

		uint32_t mIndex;

		friend class Texture;
		friend class TextureManager;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_VIRTUALTEXTUREPAGE_H_
