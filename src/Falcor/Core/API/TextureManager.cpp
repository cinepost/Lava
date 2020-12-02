#include "Falcor/stdafx.h"
#include "TextureManager.h"

#include "Falcor/Utils/Image/LTX_Bitmap.h"
#include "Falcor/Utils/Debug/debug.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
namespace fs = boost::filesystem;

namespace Falcor {
    
TextureManager::TextureManager() {
    mpDevice = nullptr;
    initialized = false;
}

TextureManager::~TextureManager() {
    if(!mpDevice)
        return;

    clear();
}

bool TextureManager::init(const InitDesc& initDesc) {
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
            initialized = true;
        } else {
            if(fs::create_directory(pathObj)) {
                LOG_DBG("Created texture cache directory %s", initDesc.cacheDir.c_str());
                initialized = true;
            } else if (fs::create_directories(pathObj)) {
                LOG_DBG("Created texture cache directory %s", initDesc.cacheDir.c_str());
                initialized = true;
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

    clear();

    return initialized;
}

void TextureManager::clear() {
    if(!initialized) return;

    mTexturesMap.clear();
    mLoadedTexturesMap.clear();

    hostCacheMemSize = mDesc.cacheMemSize;
    hostCacheMemSizeLeft = hostCacheMemSize;

    deviceCacheMemSize = mDesc.deviceCacheMemSize;
    deviceCacheMemSizeLeft = deviceCacheMemSize;
}

bool TextureManager::checkDeviceFeatures(const std::shared_ptr<Device>&  pDevice) {
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

/*
// Compressed formats
        BC1RGBUnorm,
        BC1RGBSrgb,
        BC1RGBAUnorm,
        BC1RGBASrgb,

        BC2RGBAUnorm,
        BC2RGBASrgb,
        
        BC3RGBAUnorm,
        BC3RGBASrgb,
        
        BC4Unorm,
        BC4Snorm,
        
        BC5Unorm,
        BC5Snorm,
        
        BC6HS16,
        BC6HU16,
        
        BC7Unorm,
        BC7Srgb,
*/

ResourceFormat TextureManager::compressedFormat(ResourceFormat format) {
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

Texture::SharedPtr TextureManager::createSparseTexture2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, Texture::BindFlags bindFlags) {
    bindFlags = Texture::updateBindFlags(pDevice, bindFlags, false, mipLevels, format, "SparseTexture2D");
    Texture::SharedPtr pTexture = Texture::SharedPtr(new Texture(pDevice, width, height, 1, arraySize, mipLevels, 1, format, Texture::Type::Texture2D, bindFlags));
    return pTexture;
}

Texture::SharedPtr TextureManager::createSparseTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags, bool compress) {
    assert(initialized);
    assert(mpDevice == pDevice);

    std::string fullpath;
    if (findFileInDataDirectories(filename, fullpath) == false) {
        logError("Error when loading image file. Can't find image file " + filename);
        return nullptr;
    }

    std::string ltxFilename = fullpath + ".ltx";

    bool doCompression = false;

    auto search = mLoadedTexturesMap.find(ltxFilename);
    if(search != mLoadedTexturesMap.end()) {
        LOG_DBG("Using already loaded texture %s", ltxFilename.c_str());
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

    LOG_WARN("LTX convert start...");
    LTX_Bitmap::convertToKtxFile(pDevice, fullpath, ltxFilename, true);
    LOG_WARN("LTX convert done. %s", ltxFilename.c_str());

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

    pTex = createSparseTexture2D(pDevice, pLtxBitmap->getWidth(), pLtxBitmap->getHeight(), texFormat, 1, generateMipLevels ? Texture::kMaxPossible : 1, bindFlags);
    pTex->setSourceFilename(ltxFilename);

    pTex->mIsSparse = true;

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

        std::vector<uint8_t> tmpPageData;
        tmpPageData.resize(65536); // 64K page temp data

        // allocate and bind pages
        
        for(auto& pPage: pTex->pages()) {
            if(pPage->mipLevel() < 3)
                pPage->allocate();
        }
        pTex->updateSparseBindInfo();
        
        // fill pages
        for(auto& pPage: pTex->pages()) {
            LOG_DBG("Mip level %u", pPage->mipLevel());
            if(pPage->mipLevel() < 3) {
                pLtxBitmap->readPageData(pPage->index(), tmpPageData.data());

                fillPage(pPage, tmpPageData.data());
            }
        }
    }

    return pTex;
}

const VirtualTexturePage::SharedPtr TextureManager::addTexturePage(const Texture::SharedPtr pTexture,  uint32_t index, int3 offset, uint3 extent, const uint64_t size, const uint32_t mipLevel, uint32_t layer) {
        assert(pTexture);

        //LOG_DBG("Creating VirtualTexturePage of size %zu, mipLevel: %u layer %u", size, mipLevel, layer);

        auto pPage = VirtualTexturePage::create(mpDevice, pTexture, mipLevel, layer);
        if (!pPage) return nullptr;

        pPage->mOffset = {offset[0], offset[1], offset[2]};
        pPage->mExtent = {extent[0], extent[1], extent[2]};
        pPage->mDevMemSize = size;
        pPage->mIndex = index;
        pPage->mID = static_cast<uint32_t>(mPages.size());
        
        mPages.push_back(pPage);
        return mPages.back();
    }

void TextureManager::fillPage(VirtualTexturePage::SharedPtr pPage, const void* pData) {
    mpCtx->updateTexturePage(pPage.get(), pData);
}

void TextureManager::printStats() {
    std::cout << "-------------- TextureManager stats --------------\n";
    
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
