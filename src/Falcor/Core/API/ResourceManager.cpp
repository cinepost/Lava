#include <unistd.h>

#include <array>

#include "blosc.h"

#include "Falcor/stdafx.h"
#include "ResourceManager.h"

#include "Falcor/Utils/Image/LTX_Bitmap.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/ConfigStore.h"

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
namespace fs = boost::filesystem;

namespace Falcor {

ResourceManager::SharedPtr ResourceManager::create(std::shared_ptr<Device> pDevice) {
    assert(pDevice);
    auto pMgr = new ResourceManager(pDevice);

    if(!pMgr->checkDeviceFeatures(pDevice)) {
        LOG_ERR("No capable rendering device provided !!!");
        return nullptr;
    };

    const auto& config = ConfigStore::instance();

    // check texture cache directory exist 
    try {
        std::string mCacheDir = config.get<std::string>("cache_dir", "/tmp/lava/cache");
        fs::path pathObj(mCacheDir);
        // Check if path exists and is of a directory file
        if (!(fs::exists(pathObj) && fs::is_directory(pathObj))) {
            if(fs::create_directory(pathObj)) {
                LOG_DBG("Created resources cache directory %s", mCacheDir.c_str());
            } else if (fs::create_directories(pathObj)) {
                LOG_DBG("Created resources cache directory %s", mCacheDir.c_str());
            } else {
                LOG_ERR("Unable to create texture cache directory %s", mCacheDir.c_str());
                //return nullptr; TODO: use in-memory file system if cache directory not available
            }
        }
    } catch (fs::filesystem_error & e) {
        LOG_ERR("Can't access texture cache directory !!!\n %s", e.what());
        logError(e.what());

        //return nullptr; TODO: use in-memory file system if cache directory not available
    }

    pMgr->mSparseTexturesEnabled = true; // TODO: should be dependent on device features !! 
    pMgr->deviceCacheMemSize = config.get<int>("deviceCacheMemSize", 536870912);  
    pMgr->hostCacheMemSize = config.get<int>("hostCacheMemSize", 1073741824); 

    return SharedPtr(pMgr);
}
    
ResourceManager::ResourceManager(std::shared_ptr<Device> pDevice) {
    LOG_DBG("ResourceManager::ResourceManager");
    mpDevice = pDevice;
    mpCtx = pDevice->getRenderContext();

    blosc_init();
}

ResourceManager::~ResourceManager() {
    LOG_DBG("ResourceManager::~ResourceManager");
    
    blosc_destroy();

    if(!mpDevice)
        return;
}

bool ResourceManager::checkDeviceFeatures(std::shared_ptr<Device> pDevice) {
    /*
    VkPhysicalDeviceFeatures &deviceFeatures = pDevice->mDeviceFeatures;

    if (deviceFeatures.sparseBinding && deviceFeatures.sparseResidencyImage2D) {
        enabledFeatures.shaderResourceResidency = VK_TRUE;
        enabledFeatures.shaderResourceMinLod = VK_TRUE;
        enabledFeatures.sparseBinding = VK_TRUE;
        enabledFeatures.sparseResidencyImage2D = VK_TRUE;
    } else {
        LOG_ERR("Sparse binding not supported !!!");
        return false;
    }
    */
    return true;
}

ResourceFormat ResourceManager::compressedFormat(ResourceFormat format) {
    switch(format) {
        case ResourceFormat::RGBA32Float:    // 4xfloat32 HDR format
            return ResourceFormat::BC1RGBAUnorm;
        
        case ResourceFormat::RGB32Float: 
            return ResourceFormat::BC1RGBUnorm;

        case ResourceFormat::RGBA16Float:    // 4xfloat16 HDR format
            return ResourceFormat::BC1RGBAUnorm;

        case ResourceFormat::RGB16Float:     // 3xfloat16 HDR format
            return ResourceFormat::BC1RGBUnorm;

        case ResourceFormat::BGRA8Unorm:
        case ResourceFormat::BGRX8Unorm:
            return ResourceFormat::BC1RGBUnorm;

        case ResourceFormat::RG8Unorm:
        case ResourceFormat::R8Unorm:
        default:
            LOG_WARN("Unable to find appropriate compression format for source format %s", to_string(format).c_str());
            break;
    }

    return format;
}

Texture::SharedPtr ResourceManager::createSparseTexture2D(uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, Texture::BindFlags bindFlags) {
    bindFlags = Texture::updateBindFlags(mpDevice, bindFlags, false, mipLevels, format, "SparseTexture2D");
    Texture::SharedPtr pTexture = Texture::SharedPtr(new Texture(mpDevice, width, height, 1, arraySize, mipLevels, 1, format, Texture::Type::Texture2D, bindFlags));

    if(pTexture) {
        pTexture->mIsSparse = true;
        mHasSparseResources = true;
    }

    return pTexture;
}

Texture::SharedPtr ResourceManager::createTextureFromFile(const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags, bool compress) {
    auto search = mLoadedTexturesMap.find(filename);
    if(search != mLoadedTexturesMap.end()) {
        LOG_DBG("Using already loaded texture %s", filename.c_str());
        return search->second;
    }

    auto pTexture = Texture::createFromFile(mpDevice, filename, generateMipLevels, loadAsSrgb, bindFlags);

    if(pTexture)
        mLoadedTexturesMap[filename] = pTexture;

    return pTexture;
}

Texture::SharedPtr ResourceManager::createSparseTextureFromFile(const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags, bool compress) {
    fs::path fullpath;
    if (findFileInDataDirectories(filename, fullpath) == false) {
        logError("Error when loading image file. Can't find image file " + filename);
        return nullptr;
    }

    std::string ext = fullpath.extension().string();

    const auto& configStore = ConfigStore::instance();
    bool vtoff = configStore.get<bool>("vtoff", true);
    if (!mSparseTexturesEnabled || vtoff) {
        if (ext == ".ltx") {
            logWarning("Virtual texturing disabled. Unable to use LTX texture " + filename);
            return nullptr;
        }
        return createTextureFromFile(filename, generateMipLevels, loadAsSrgb, bindFlags);
    }

    std::string ltxFilename = fullpath.string() + ".ltx";

    bool doCompression = false;

    auto search = mLoadedTexturesMap.find(ltxFilename);
    if(search != mLoadedTexturesMap.end()) {
        LOG_DBG("Using already loaded sparse texture %s", ltxFilename.c_str());
        return search->second;
    }


    Texture::SharedPtr pTex;
    if (hasSuffix(filename, ".dds")) {
        LOG_ERR("Sparse texture handling for DDS format unimplemented !!!");
        return nullptr;
    } 
        
    //if (doCompression) {
    //    LOG_DBG("Compressing texture from format %s", to_string(texFormat).c_str());
    //    texFormat = compressedFormat(texFormat);
    //}

    if(!vtoff) {
        bool ltxMagicMatch = false;

        if(fs::exists(ltxFilename) && LTX_Bitmap::checkFileMagic(ltxFilename, true)) ltxMagicMatch = true;

        if(configStore.get<bool>("fconv", true) || !fs::exists(ltxFilename) || !ltxMagicMatch ) {
            LOG_DBG("Converting texture %s to LTX format ...",  fullpath.c_str());

            LTX_Bitmap::TLCParms tlcParms;
            tlcParms.compressorName = configStore.get<std::string>("vtex_tlc", "none");
            tlcParms.compressionLevel = (uint8_t)configStore.get<int>("vtex_tlc_level", 0);
            if (!LTX_Bitmap::convertToKtxFile(mpDevice, fullpath.string(), ltxFilename, tlcParms, true)) {
                LOG_ERR("Error converting texture to %s", ltxFilename.c_str());
            } else {
                LOG_DBG("Conversion done %s", ltxFilename.c_str());
            }
        }
    }

    auto pLtxBitmap = LTX_Bitmap::createFromFile(mpDevice, ltxFilename, true);
    if (!pLtxBitmap) {
        LOG_ERR("Error loading converted ltx bitmap from %s !!!", ltxFilename.c_str());
        pTex = nullptr;
        return pTex;
    }
    ResourceFormat texFormat = pLtxBitmap->getFormat();

    if (loadAsSrgb) {
        texFormat = linearToSrgbFormat(texFormat);
    }

    pTex = createSparseTexture2D(pLtxBitmap->getWidth(), pLtxBitmap->getHeight(), texFormat, 1, pLtxBitmap->getMipLevelsCount(), bindFlags);
    pTex->setSourceFilename(ltxFilename);

    try {
        pTex->apiInit(nullptr, generateMipLevels);
    } catch (const std::runtime_error& e) {
        LOG_ERR("Error initializing sparse texture %s !!!\n %s", ltxFilename.c_str(), e.what());
        pTex = nullptr;
        return pTex;
    } catch (...) {
        LOG_ERR("Error initializing sparse texture %s !!!", ltxFilename.c_str());
        pTex = nullptr;
        return pTex;
    }

    uint32_t deviceMemRequiredSize = pTex->getTextureSizeInBytes();
    LOG_DBG("Texture require %u bytes of device memory", deviceMemRequiredSize);
    if(deviceMemRequiredSize <= deviceCacheMemSizeLeft) {
        deviceCacheMemSizeLeft = deviceCacheMemSize - deviceMemRequiredSize;
    } else {
        LOG_ERR("No texture space left for %s !!!", ltxFilename.c_str());
        pTex = nullptr;
    }
    
    if (pTex != nullptr) {
        mLoadedTexturesMap[ltxFilename] = pTex;
        mTextureLTXBitmapsMap[pTex->id()] = std::move(pLtxBitmap);
    }

    return pTex;
}

const VirtualTexturePage::SharedPtr ResourceManager::addTexturePage(const Texture::SharedPtr pTexture,  uint32_t index, int3 offset, uint3 extent, const uint64_t size, uint32_t memoryTypeBits, const uint32_t mipLevel, uint32_t layer) {
    assert(pTexture);

    auto pPage = VirtualTexturePage::create(mpDevice, pTexture, offset, extent, mipLevel, layer);
    if (!pPage) return nullptr;

    pPage->mMemoryTypeBits = memoryTypeBits;
    pPage->mDevMemSize = size;
    pPage->mIndex = index;
    pPage->mID = static_cast<uint32_t>(mPages.size());
    
    mPages.push_back(pPage);
    return mPages.back();
}

void ResourceManager::loadPages(const Texture::SharedPtr& pTexture, const std::vector<uint32_t>& pageIDs) {
    assert(mpDevice);
    assert(pTexture.get());

    uint32_t textureID = pTexture->id();

    auto it = mTextureLTXBitmapsMap.find(textureID);
    if (it == mTextureLTXBitmapsMap.end()) {
        LOG_ERR("Not LTX_Bitmap stored for texture %u", textureID);
        return;
    }

    //std::vector<uint32_t> sortedPageIDs = pageIDs;
    //std::sort (sortedPageIDs.begin(), sortedPageIDs.end());

    // allocate pages
    for( uint32_t pageID: pageIDs ) {
        mPages[pageID]->allocate();
    }
    pTexture->updateSparseBindInfo();
    //fillMipTail(pTexture);

    // read data and fill pages
    auto pLtxBitmap = mTextureLTXBitmapsMap[textureID];
    std::string ltxFilename = pLtxBitmap->getFilename();
    auto pFile = fopen(ltxFilename.c_str(), "rb");
    std::vector<uint8_t> tmpPage(65536);
    auto pTmpPageData = tmpPage.data();
    
    for( uint32_t pageID: pageIDs ) {
        auto pPage = mPages[pageID];
        //LOG_DBG("Load texture page index %u level %u", pPage->index(), pPage->mipLevel());
        pLtxBitmap->readPageData(pPage->index(), pTmpPageData, pFile);
        mpDevice->getRenderContext()->updateTexturePage(pPage.get(), pTmpPageData);
    }

    fclose(pFile);

    //fillMipTail(pTexture);
    pTexture->updateSparseBindInfo();
}

void ResourceManager::fillMipTail(const Texture::SharedPtr& pTexture) {
    assert(mpDevice);
    assert(pTexture.get());

    uint32_t textureID = pTexture->id();

    auto it = mTextureLTXBitmapsMap.find(textureID);
    if (it == mTextureLTXBitmapsMap.end()) {
        LOG_ERR("Not LTX_Bitmap stored for texture %u", textureID);
        return;
    }

    auto pLtxBitmap = mTextureLTXBitmapsMap[textureID];
    auto pTex = pTexture.get();

    //uint32_t mipLevel = pTex->mSparseImageMemoryRequirements.imageMipTailFirstLod;
    //uint32_t width = std::max(pTex->mWidth >> pTex->mSparseImageMemoryRequirements.imageMipTailFirstLod, 1u);
    //uint32_t height = std::max(pTex->mHeight >> pTex->mSparseImageMemoryRequirements.imageMipTailFirstLod, 1u);
    //uint32_t depth = 1;

    VkSparseImageMemoryBind mipTailimageMemoryBind{};

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = NULL;
    memAllocInfo.allocationSize = pTex->mSparseImageMemoryRequirements.imageMipTailSize;
    memAllocInfo.memoryTypeIndex = pTex->mMemoryTypeIndex;
    
    if ( VK_FAILED(vkAllocateMemory(mpDevice->getApiHandle(), &memAllocInfo, nullptr, &mipTailimageMemoryBind.memory)) ) {
        LOG_ERR("Could not allocate memory !!!");
        return;
    }

    for (uint32_t mipLevel = pTex->mMipTailStart; mipLevel < pTex->mMipLevels; mipLevel++) {

        const uint32_t width = std::max(pTex->mWidth >> mipLevel, 1u);
        const uint32_t height = std::max(pTex->mHeight >> mipLevel, 1u);
        const uint32_t depth = 1;

        std::vector<unsigned char> tmpPage(65536, 255);

        LOG_WARN("Fill mip tail level %u", mipLevel);
        mpCtx->updateMipTailData(pTex, {0,0,0}, { width, height, depth }, mipLevel, tmpPage.data());
        //mpCtx->updateSubresourceData(pTex, 0, tmpPage.data(), uint3(0), { width, height, depth });
    }
    mpCtx->flush(true);
}

void ResourceManager::fillPage(VirtualTexturePage::SharedPtr pPage, const void* pData) {
    mpCtx->updateTexturePage(pPage.get(), pData);
}

void ResourceManager::printStats() {
    std::cout << "-------------- ResourceManager stats --------------\n";
    
    printf("Host cache mem cap: %u bytes\n", hostCacheMemSize);
    printf("Host cache mem used: %u bytes\n", hostCacheMemSize - hostCacheMemSizeLeft);
    
    std::cout << "---\n";
    
    size_t usedDeviceMemSize = 0;
    for (auto const& elem: mLoadedTexturesMap) {
        usedDeviceMemSize += elem.second->getTextureSizeInBytes();
    }

    printf("Device cache mem cap: %u bytes\n", deviceCacheMemSize);
    printf("Device cache mem used: %zu bytes\n", usedDeviceMemSize); //deviceCacheMemSize - deviceCacheMemSizeLeft);

    std::cout << "--------------------------------------------------\n";
}

}  // namespace Falcor
