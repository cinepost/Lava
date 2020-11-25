#include "Falcor/stdafx.h"
#include "TextureManager.h"

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

Texture::SharedPtr TextureManager::createTexture2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, Texture::BindFlags bindFlags) {
    bindFlags = Texture::updateBindFlags(pDevice, bindFlags, pData != nullptr, mipLevels, format, "Texture2D");
    Texture::SharedPtr pTexture = Texture::SharedPtr(new Texture(pDevice, width, height, 1, arraySize, mipLevels, 1, format, Texture::Type::Texture2D, bindFlags));
    return pTexture;
}

Texture::SharedPtr TextureManager::createTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags, bool compress) {
    assert(initialized);
    assert(mpDevice == pDevice);

    bool doCompression = false;

    auto search = mLoadedTexturesMap.find(filename);
    if(search != mLoadedTexturesMap.end()) {
        LOG_DBG("Using already loaded texture %s", filename.c_str());
        return search->second;
    }

    std::string fullpath;
    if (findFileInDataDirectories(filename, fullpath) == false) {
        logError("Error when loading image file. Can't find image file " + filename);
        return nullptr;
    }

    Texture::SharedPtr pTex;
    if (hasSuffix(filename, ".dds")) {
        //pTex = createTextureFromDDSFile(device, fullpath, generateMipLevels, loadAsSrgb, bindFlags);
        assert(1==2 && "Unimplemented !!!");
    } else {
        Bitmap::UniqueConstPtr pBitmap = Bitmap::createFromFile(pDevice, fullpath, true);
        if (pBitmap) {
            ResourceFormat texFormat = pBitmap->getFormat();
            if (doCompression) {
                LOG_DBG("Compressing texture from format %s", to_string(texFormat).c_str());
                texFormat = compressedFormat(texFormat);
            }

            if (loadAsSrgb) {
                texFormat = linearToSrgbFormat(texFormat);
            }

            pTex = createTexture2D(pDevice, pBitmap->getWidth(), pBitmap->getHeight(), texFormat, 1, generateMipLevels ? Texture::kMaxPossible : 1, pBitmap->getData(), bindFlags);
            pTex->setSourceFilename(fullpath);

            pTex->mIsSparse = true;

            try {
                pTex->apiInit(pBitmap->getData(), generateMipLevels);
            } catch (const std::runtime_error& e) {
                LOG_ERR("Error initializing texture %s !!!\n %s", fullpath.c_str(), e.what());
                pTex = nullptr;
                return pTex;
            } catch (...) {
                LOG_ERR("Error initializing texture %s !!!", fullpath.c_str());
                pTex = nullptr;
                return pTex;
            }

            uint32_t deviceMemRequiredSize = pTex->getTextureSizeInBytes();
            LOG_DBG("Texture require %u bytes of device memory", deviceMemRequiredSize);
            if(deviceMemRequiredSize <= deviceCacheMemSizeLeft) {
                deviceCacheMemSizeLeft = deviceCacheMemSize - deviceMemRequiredSize;
            } else {
                LOG_ERR("No texture space left for %s !!!", fullpath.c_str());
                pTex = nullptr;
            }
            
        }
    }

    if (pTex != nullptr) {
        mLoadedTexturesMap[filename] = pTex;

        for(auto& pPage: pTex->pages()) {
            pPage->allocate();
            fillPage(pPage);
        }
    }

    return pTex;
}

void TextureManager::fillPage(VirtualTexturePage::SharedPtr pPage) {
    // Generate some random image data and upload as a buffer
    const uint32_t elementCount = pPage->width() * pPage->height();

    std::vector<uint8_t> initData;
    initData.resize(elementCount * 4);

    for(size_t i = 0; i < initData.size(); i++) {
        initData[i] = 255;
    }

    //auto pBuffer = Buffer::createTyped(mpDevice, ResourceFormat::BGRX8Unorm, elementCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, initData.data());

    auto pCtx = mpDevice->getRenderContext();
    assert(pCtx);

    uint32_t firstSubresource = 0;
    uint32_t subresourceCount = 1;

    pCtx->updateTextureSubresources(pPage->texture().get(), firstSubresource, subresourceCount, initData.data(), pPage->offset(), pPage->extent());
}

void TextureManager::printStats() {
    std::cout << "-------------- TextureManager stats --------------\n";
    
    printf("Host cache mem cap: %u bytes\n", hostCacheMemSize);
    printf("Host cache mem used: %u bytes\n", hostCacheMemSize - hostCacheMemSizeLeft);
    
    std::cout << "---\n";
    
    printf("Device cache mem cap: %u bytes\n", deviceCacheMemSize);
    printf("Device cache mem used: %u bytes\n", deviceCacheMemSize - deviceCacheMemSizeLeft);

    std::cout << "--------------------------------------------------\n";
}

}  // namespace Falcor
