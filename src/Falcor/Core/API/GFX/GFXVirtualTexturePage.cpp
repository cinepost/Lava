#include <vector>
#include <string>
#include <memory>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/VirtualTexturePage.h"

namespace Falcor {

VirtualTexturePage::VirtualTexturePage(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer): mpDevice(pTexture->device()), mpTexture(pTexture), mMipLevel(mipLevel), mLayer(layer) {
    // create resource
	Slang::ComPtr<gfx::IVirtualTexturePageResource> texturePageResource = mpDevice->getGfxDevice()->createVirtualTexturePageResource(this, offset, extent, mipLevel, layer);
	assert(texturePageResource);

	mpVirtualTexturePageResource = texturePageResource;
}

VirtualTexturePage::~VirtualTexturePage() {
    mpVirtualTexturePageResource->release();
}


}  // namespace Falcor
