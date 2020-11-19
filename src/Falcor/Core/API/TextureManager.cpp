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

bool TextureManager::init(const InitDesc& initDesc) {
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

    cacheMemSize = initDesc.cacheMemSize;
    deviceCacheMemSize = initDesc.deviceCacheMemSize;

    return initialized;
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

Texture::SharedPtr TextureManager::createTexture2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, Texture::BindFlags bindFlags) {
    bindFlags = Texture::updateBindFlags(pDevice, bindFlags, pData != nullptr, mipLevels, format, "Texture2D");
    Texture::SharedPtr pTexture = Texture::SharedPtr(new Texture(pDevice, width, height, 1, arraySize, mipLevels, 1, format, Texture::Type::Texture2D, bindFlags));
    return pTexture;
}

Texture::SharedPtr TextureManager::createTextureFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags) {
    assert(mpDevice == pDevice);

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
            if (loadAsSrgb) {
                texFormat = linearToSrgbFormat(texFormat);
            }

            pTex = createTexture2D(pDevice, pBitmap->getWidth(), pBitmap->getHeight(), texFormat, 1, generateMipLevels ? Texture::kMaxPossible : 1, pBitmap->getData(), bindFlags);
            pTex->createImage(pBitmap->getData());
            uint32_t deviceMemRequiredSize = pTex->getTextureSizeInBytes();
            LOG_WARN("Texture require %u bytes of device memory", deviceMemRequiredSize);
            pTex->apiInit(pBitmap->getData(), generateMipLevels);
        }
    }

    if (pTex != nullptr) {
        pTex->setSourceFilename(fullpath);
    }

    return pTex;
}

}  // namespace Falcor
