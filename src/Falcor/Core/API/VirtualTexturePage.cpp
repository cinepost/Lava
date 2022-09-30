#include <vector>
#include <string>
#include <memory>

#include "Device.h"
#include "Buffer.h"
#include "Texture.h"
#include "VirtualTexturePage.h"

namespace Falcor {

VirtualTexturePage::SharedPtr VirtualTexturePage::create(const std::shared_ptr<Texture>& pTexture, int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer) {
    return std::make_shared<VirtualTexturePage>(pTexture, offset, extent, mipLevel, layer);
}

}  // namespace Falcor
