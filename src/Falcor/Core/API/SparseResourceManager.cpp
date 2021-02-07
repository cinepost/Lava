#include <unistd.h>

#include <array>

#include "Falcor/stdafx.h"
#include "SparseResourceManager.h"

#include "Falcor/Utils/Image/LTX_Bitmap.h"
#include "Falcor/Utils/Debug/debug.h"

//#include "boost/asio/post.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
namespace fs = boost::filesystem;

namespace Falcor {
    
SparseResourceManager::SparseResourceManager() {
    mpDevice = nullptr;
    mInitialized = false;
    //mpThreadPool = nullptr;
}

SparseResourceManager::~SparseResourceManager() {
    if(!mpDevice)
        return;

    //delete mpThreadPool;

    clear();
}

void SparseResourceManager::setVirtualTexturingEnabled(bool on_off) {
    auto& manager = instance();
    assert(!manager.mInitialized);

    manager.mSparseTexturesEnabled = on_off;
}

void SparseResourceManager::setForceTexturesConversion(bool on_off) {
    auto& manager = instance();
    assert(!manager.mInitialized);

    manager.mForceTexturesConversion = on_off;
}

bool SparseResourceManager::init(const InitDesc& initDesc) {
    mDesc = initDesc;

    if(initDesc.pDevice) {
        mpDevice = initDesc.pDevice;
        if(!checkDeviceFeatures(mpDevice)) {
            return false;
        }
    } else {
        LOG_ERR("No rendering device provided !!!");
        return false;
    }

    // check texture cache directory exist 
    try {
        fs::path pathObj(initDesc.cacheDir);
        // Check if path exists and is of a directory file
        if (fs::exists(pathObj) && fs::is_directory(pathObj)) {
            mInitialized = true;
        } else {
            if(fs::create_directory(pathObj)) {
                LOG_DBG("Created texture cache directory %s", initDesc.cacheDir.c_str());
                mInitialized = true;
            } else if (fs::create_directories(pathObj)) {
                LOG_DBG("Created texture cache directory %s", initDesc.cacheDir.c_str());
                mInitialized = true;
            } else {
                LOG_ERR("Unable to create texture cache directory %s", initDesc.cacheDir.c_str());
                return false;
            }
        }
    } catch (fs::filesystem_error & e) {
        LOG_ERR("Can't access texture cache directory !!!\n %s", e.what());
        logError(e.what());

        return false;
    }

    mpCtx = mpDevice->getRenderContext();
    //mpThreadPool = new boost::asio::thread_pool();

    clear();

    return mInitialized;
}

void SparseResourceManager::clear() {
    if(!mInitialized) return;

    mTexturesMap.clear();
    mLoadedTexturesMap.clear();

    hostCacheMemSize = mDesc.cacheMemSize;
    hostCacheMemSizeLeft = hostCacheMemSize;

    deviceCacheMemSize = mDesc.deviceCacheMemSize;
    deviceCacheMemSizeLeft = deviceCacheMemSize;
}

bool SparseResourceManager::checkDeviceFeatures(const std::shared_ptr<Device>&  pDevice) {
    VkPhysicalDeviceFeatures &deviceFeatures = pDevice->deviceFeatures;

    if (deviceFeatures.sparseBinding && deviceFeatures.sparseResidencyImage2D) {
        enabledFeatures.shaderResourceResidency = VK_TRUE;
        enabledFeatures.shaderResourceMinLod = VK_TRUE;
        enabledFeatures.sparseBinding = VK_TRUE;
        enabledFeatures.sparseResidencyImage2D = VK_TRUE;
    } else {
        LOG_ERR("Sparse binding not supported !!!");
        return false;
    }

    return true;
}

ResourceFormat SparseResourceManager::compressedFormat(ResourceFormat format) {
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

Texture::SharedPtr SparseResourceManager::createSparseTexture2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, Texture::BindFlags bindFlags) {
    bindFlags = Texture::updateBindFlags(pDevice, bindFlags, false, mipLevels, format, "SparseTexture2D");
    Texture::SharedPtr pTexture = Texture::SharedPtr(new Texture(pDevice, width, height, 1, arraySize, mipLevels, 1, format, Texture::Type::Texture2D, bindFlags));

    if(pTexture) {
        pTexture->mIsSparse = true;
        mHasSparseResources = true;
    }

    return pTexture;
}

Texture::SharedPtr SparseResourceManager::createTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags, bool compress) {
    auto search = mLoadedTexturesMap.find(filename);
    if(search != mLoadedTexturesMap.end()) {
        LOG_DBG("Using already loaded texture %s", filename.c_str());
        return search->second;
    }

    auto pTexture = Texture::createFromFile(pDevice, filename, generateMipLevels, loadAsSrgb, bindFlags);

    if(pTexture)
        mLoadedTexturesMap[filename] = pTexture;

    return pTexture;
}

Texture::SharedPtr SparseResourceManager::createSparseTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags, bool compress) {
    assert(mpDevice == pDevice);

    if (!mSparseTexturesEnabled)
        return createTextureFromFile(pDevice, filename, generateMipLevels, loadAsSrgb, bindFlags);

    std::string fullpath;
    if (findFileInDataDirectories(filename, fullpath) == false) {
        logError("Error when loading image file. Can't find image file " + filename);
        return nullptr;
    }

    std::string ltxFilename = fullpath + ".ltx";

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

    if(mForceTexturesConversion || !fs::exists(ltxFilename) ) {
        LOG_DBG("Converting texture %s to LTX format ...",  fullpath.c_str());
        LTX_Bitmap::convertToKtxFile(pDevice, fullpath, ltxFilename, true);
        LOG_DBG("Conversion done %s", ltxFilename.c_str());
    }

    auto pLtxBitmap = LTX_Bitmap::createFromFile(pDevice, ltxFilename, true);
    if (!pLtxBitmap) {
        LOG_ERR("Error loading converted ltx bitmap from %s !!!", ltxFilename.c_str());
        pTex = nullptr;
        return pTex;
    }
    ResourceFormat texFormat = pLtxBitmap->getFormat();

    if (loadAsSrgb) {
        texFormat = linearToSrgbFormat(texFormat);
    }

    pTex = createSparseTexture2D(pDevice, pLtxBitmap->getWidth(), pLtxBitmap->getHeight(), texFormat, 1, pLtxBitmap->getMipLevelsCount(), bindFlags);
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

const VirtualTexturePage::SharedPtr SparseResourceManager::addTexturePage(const Texture::SharedPtr pTexture,  uint32_t index, int3 offset, uint3 extent, const uint64_t size, const uint32_t mipLevel, uint32_t layer) {
    assert(pTexture);

    auto pPage = VirtualTexturePage::create(mpDevice, pTexture, offset, extent, mipLevel, layer);
    if (!pPage) return nullptr;

    pPage->mDevMemSize = size;
    pPage->mIndex = index;
    pPage->mID = static_cast<uint32_t>(mPages.size());
    
    mPages.push_back(pPage);
    return mPages.back();
}

void SparseResourceManager::loadPages(const Texture::SharedPtr& pTexture, const std::vector<uint32_t>& pageIDs) {
    assert(mInitialized);
    if(!mInitialized)
        return;

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
    fillMipTail(pTexture);

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
        mpCtx->updateTexturePage(pPage.get(), pTmpPageData);
    }

    fclose(pFile);

    //fillMipTail(pTexture);
    pTexture->updateSparseBindInfo();
}

void SparseResourceManager::fillMipTail(const Texture::SharedPtr& pTexture) {
    assert(mInitialized);
    if(!mInitialized)
        return;

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

void SparseResourceManager::fillPage(VirtualTexturePage::SharedPtr pPage, const void* pData) {
    mpCtx->updateTexturePage(pPage.get(), pData);
}

void SparseResourceManager::printStats() {
    std::cout << "-------------- SparseResourceManager stats --------------\n";
    
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
