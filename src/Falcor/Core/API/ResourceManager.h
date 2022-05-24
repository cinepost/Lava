#ifndef SRC_FALCOR_CORE_API_ResourceManager_H_
#define SRC_FALCOR_CORE_API_ResourceManager_H_

#include <map>
#include <string>
#include <algorithm>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/Image/LTX_Bitmap.h"
//#include "boost/asio/thread_pool.hpp"

#include "Texture.h"
#include "VirtualTexturePage.h"

namespace Falcor {

class Device;

class dlldecl ResourceManager: public std::enable_shared_from_this<ResourceManager> {
  using SharedPtr = std::shared_ptr<ResourceManager>;
  public:
    ~ResourceManager();

  public:
    Texture::SharedPtr createTextureFromFile(const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags = Texture::BindFlags::ShaderResource, bool compress = true);
    Texture::SharedPtr createSparseTextureFromFile(const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags = Texture::BindFlags::ShaderResource, bool compress = true);

    const VirtualTexturePage::SharedPtr addTexturePage(const Texture::SharedPtr pTexture, uint32_t index, int3 offset, uint3 extent, const uint64_t size, uint32_t memoryTypeBits, const uint32_t mipLevel, uint32_t layer);

    const std::string& getCacheDirPath() const { return mCacheDir; } 
    void printStats();

    bool hasSparseResources() const { return mHasSparseResources; }

    void loadPages(const Texture::SharedPtr& pTexture, const std::vector<uint32_t>& pageIDs); 
    void fillMipTail(const Texture::SharedPtr& pTexture);

    const VmaAllocator& allocator() const { return mpDevice->mAllocator; }

 protected:
    static SharedPtr create(std::shared_ptr<Device> pDevice);

 private:
    ResourceManager(std::shared_ptr<Device> pDevice);
    bool checkDeviceFeatures(std::shared_ptr<Device> pDevice);

    Texture::SharedPtr  createSparseTexture2D(uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, Texture::BindFlags bindFlags);

    void fillPage(VirtualTexturePage::SharedPtr pPage, const void* pData = nullptr);

    static ResourceFormat      compressedFormat(ResourceFormat format);

    friend class Device;

    bool mForceTexturesConversion = false;
    bool mSparseTexturesEnabled = true;
    bool mHasSparseResources = false;

    VkPhysicalDeviceFeatures enabledFeatures{};

    std::shared_ptr<Device>  mpDevice = nullptr;

    RenderContext* mpCtx;

    //boost::asio::thread_pool* mpThreadPool = nullptr;

    std::string mCacheDir = "";
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

#endif  // SRC_FALCOR_CORE_API_ResourceManager_H_
