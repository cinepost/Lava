#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/VirtualTexturePage.h"

#include <vector>
#include <string>
#include <memory>


namespace Falcor {

VirtualTexturePage::SharedPtr VirtualTexturePage::create(const Texture::SharedPtr& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer, uint32_t index, uint32_t size, uint32_t memoryTypeBits) {
    return make_shared_ptr<VirtualTexturePage>(pTexture, offset, extent, mipLevel, layer, index, size, memoryTypeBits);
}

VirtualTexturePage::VirtualTexturePage(const Texture::SharedPtr& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer, uint32_t index, uint32_t size, uint32_t memoryTypeBits): 
	mpDevice(pTexture->getDevice()), 
	mpTexture(pTexture), 
	mIndex(index) 
{
    // create resource
	mpVirtualTexturePageResource = mpDevice->getGfxDevice()->createVirtualTexturePageResource({offset.x, offset.y, offset.z}, {extent.x, extent.y, extent.z} , mipLevel, layer, size, memoryTypeBits);
	
	assert(mpVirtualTexturePageResource);
}

VirtualTexturePage::~VirtualTexturePage() {
    mpVirtualTexturePageResource->release();
}


}  // namespace Falcor
