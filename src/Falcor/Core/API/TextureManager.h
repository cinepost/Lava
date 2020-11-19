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

namespace Falcor {

class dlldecl TextureManager {
 public:
    struct InitDesc {
        std::string cacheDir = "/tmp/lava/tex"; // converted textures cache directory
        uint32_t    cacheMemSize = 1024;        // memory cache size in megabytes

        std::shared_ptr<Device> pDevice;
        uint32_t    deviceCacheMemSize = 512;   // GPU memory cache size in megabytes
    };

    static TextureManager& instance() {
        static TextureManager instance;     // Guaranteed to be destroyed.
        return instance;                    // Instantiated on first use.
    }

 public:
    bool init(const InitDesc& initDesc);

    TextureManager(TextureManager const&)    = delete;
    void operator=(TextureManager const&)    = delete;

 public:
    Texture::SharedPtr createTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags = Texture::BindFlags::ShaderResource);

 private:
    TextureManager();
    bool checkDeviceFeatures(const std::shared_ptr<Device>&  pDevice);

    Texture::SharedPtr createTexture2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, Texture::BindFlags bindFlags);


    bool initialized = false;

    VkPhysicalDeviceFeatures enabledFeatures{};

    std::shared_ptr<Device>  mpDevice = nullptr;
    uint32_t    cacheMemSize = 1024;
    uint32_t    deviceCacheMemSize = 512;

    std::map<size_t, Texture::SharedPtr> mTexturesMap;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_TEXTUREMANAGER_H_
