#ifndef SRC_FALCOR_CORE_API_TEXTUREMANAGER_H_
#define SRC_FALCOR_CORE_API_TEXTUREMANAGER_H_

#include <map>
#include <string>
#include <algorithm>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Falcor/Core/API/Device.h"

#include "Texture.h"
#include "VirtualTexturePage.h"

namespace Falcor {


class dlldecl TextureManager {
 public:
    struct InitDesc {
        std::string cacheDir = "/tmp/lava/tex"; // converted textures cache directory
        uint32_t    cacheMemSize = 1073741824;        // memory cache size in bytes

        std::shared_ptr<Device> pDevice;
        uint32_t    deviceCacheMemSize = 536870912;   // GPU memory cache size in bytes
    };

    static TextureManager& instance() {
        static TextureManager instance;     // Guaranteed to be destroyed.
        return instance;                    // Instantiated on first use.
    }

 public:
    ~TextureManager();
    bool init(const InitDesc& initDesc);
    void clear();

    TextureManager(TextureManager const&)    = delete;
    void operator=(TextureManager const&)    = delete;

 public:
    Texture::SharedPtr createTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags = Texture::BindFlags::ShaderResource, bool compress = true);

    const VirtualTexturePage::SharedPtr addTexturePage(const Texture::SharedPtr pTexture, int3 offset, uint3 extent, const uint64_t size, const uint32_t mipLevel, uint32_t layer);

    void printStats();

 private:
    TextureManager();
    bool checkDeviceFeatures(const std::shared_ptr<Device>&  pDevice);

    Texture::SharedPtr  createTexture2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, Texture::BindFlags bindFlags);

    void fillPage(VirtualTexturePage::SharedPtr pPage, const void* pData = nullptr);

    static ResourceFormat      compressedFormat(ResourceFormat format);

    bool initialized = false;

    VkPhysicalDeviceFeatures enabledFeatures{};

    InitDesc mDesc;

    std::shared_ptr<Device>  mpDevice = nullptr;
    uint32_t    hostCacheMemSize;
    uint32_t    hostCacheMemSizeLeft;
    uint32_t    deviceCacheMemSize;
    uint32_t    deviceCacheMemSizeLeft;

    std::map<size_t, Texture::SharedPtr> mTexturesMap;
    std::vector<VirtualTexturePage::SharedPtr> mPages;

    std::map<std::string, Texture::SharedPtr> mLoadedTexturesMap;
    std::map<std::string, Bitmap::UniqueConstPtr> mLoadedBitmapsMap;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_TEXTUREMANAGER_H_
