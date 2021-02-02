#ifndef SRC_FALCOR_CORE_API_SparseResourceManager_H_
#define SRC_FALCOR_CORE_API_SparseResourceManager_H_

#include <map>
#include <string>
#include <algorithm>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Image/LTX_Bitmap.h"
//#include "boost/asio/thread_pool.hpp"

#include "Texture.h"
#include "VirtualTexturePage.h"

namespace Falcor {


class dlldecl SparseResourceManager {
 public:
    struct InitDesc {
        std::string cacheDir = "/tmp/lava/tex"; // converted textures cache directory
        uint32_t    cacheMemSize = 1073741824;        // memory cache size in bytes

        std::shared_ptr<Device> pDevice;
        uint32_t    deviceCacheMemSize = 536870912;   // GPU memory cache size in bytes
    };

    static SparseResourceManager& instance() {
        static SparseResourceManager instance;     // Guaranteed to be destroyed.
        return instance;                    // Instantiated on first use.
    }

 public:
    ~SparseResourceManager();
    bool init(const InitDesc& initDesc);
    void clear();

    SparseResourceManager(SparseResourceManager const&)    = delete;
    void operator=(SparseResourceManager const&)    = delete;

 public:
    Texture::SharedPtr createTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags = Texture::BindFlags::ShaderResource, bool compress = true);
    Texture::SharedPtr createSparseTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags = Texture::BindFlags::ShaderResource, bool compress = true);

    const VirtualTexturePage::SharedPtr addTexturePage(const Texture::SharedPtr pTexture, uint32_t index, int3 offset, uint3 extent, const uint64_t size, const uint32_t mipLevel, uint32_t layer);

    const std::string& getCacheDirPath() const { return mDesc.cacheDir; } 
    void printStats();

    bool hasSparseResources() const { return mHasSparseResources; }

    static void setVirtualTexturingEnabled(bool on_off = true);
    static void setForceTexturesConversion(bool on_off = false);

    void loadPages(const Texture::SharedPtr& pTexture, const std::vector<uint32_t>& pageIDs); 
    void fillMipTail(const Texture::SharedPtr& pTexture);

 private:
    SparseResourceManager();
    bool checkDeviceFeatures(const std::shared_ptr<Device>&  pDevice);

    Texture::SharedPtr  createSparseTexture2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, Texture::BindFlags bindFlags);

    void fillPage(VirtualTexturePage::SharedPtr pPage, const void* pData = nullptr);

    static ResourceFormat      compressedFormat(ResourceFormat format);

    bool mForceTexturesConversion = false;
    bool mSparseTexturesEnabled = true;
    bool mInitialized = false;
    bool mHasSparseResources = false;

    VkPhysicalDeviceFeatures enabledFeatures{};

    InitDesc mDesc;

    std::shared_ptr<Device>  mpDevice = nullptr;

    RenderContext* mpCtx;

    //boost::asio::thread_pool* mpThreadPool = nullptr;

    uint32_t    hostCacheMemSize;
    uint32_t    hostCacheMemSizeLeft;
    uint32_t    deviceCacheMemSize;
    uint32_t    deviceCacheMemSizeLeft;

    std::map<size_t, Texture::SharedPtr> mTexturesMap;
    std::vector<VirtualTexturePage::SharedPtr> mPages;

    std::map<std::string, Texture::SharedPtr> mLoadedTexturesMap;
    std::map<std::string, Bitmap::UniqueConstPtr> mLoadedBitmapsMap;
    std::map<uint32_t, LTX_Bitmap::SharedConstPtr> mTextureLTXBitmapsMap;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_SparseResourceManager_H_
