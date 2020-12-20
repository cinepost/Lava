/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include <vector>

#include "Falcor/stdafx.h"
#include "Texture.h"
#include "Device.h"
#include "RenderContext.h"
#include "Falcor/Utils/Threading.h"

#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {

Texture::BindFlags Texture::updateBindFlags(Device::SharedPtr pDevice, Texture::BindFlags flags, bool hasInitData, uint32_t mipLevels, ResourceFormat format, const std::string& texType) {
    if ((mipLevels == Texture::kMaxPossible) && hasInitData) {
        flags |= Texture::BindFlags::RenderTarget;
    }

    Texture::BindFlags supported = getFormatBindFlags(pDevice, format);
    supported |= ResourceBindFlags::Shared;
    if ((flags & supported) != flags) {
        logError("Error when creating " + texType + " of format " + to_string(format) + ". The requested bind-flags are not supported.\n"
            "Requested = (" + to_string(flags) + "), supported = (" + to_string(supported) + ").\n\n"
            "The texture will be created only with the supported bind flags, which may result in a crash or a rendering error.");
        flags = flags & supported;
    }

    return flags;
}


Texture::SharedPtr Texture::createFromApiHandle(std::shared_ptr<Device> device, ApiHandle handle, Type type, uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels, State initState, BindFlags bindFlags) {
    assert(handle);
    switch (type) {
        case Resource::Type::Texture1D:
            assert(height == 1 && depth == 1 && sampleCount == 1);
            break;
        case Resource::Type::Texture2D:
            assert(depth == 1 && sampleCount == 1);
            break;
        case Resource::Type::Texture2DMultisample:
            assert(depth == 1);
            break;
        case Resource::Type::Texture3D:
            assert(sampleCount == 1);
            break;
        case Resource::Type::TextureCube:
            assert(depth == 1 && sampleCount == 1);
            break;
        default:
            should_not_get_here();
            break;
    }

    Texture::SharedPtr pTexture = SharedPtr(new Texture(device, width, height, depth, arraySize, mipLevels, sampleCount, format, type, bindFlags));
    pTexture->mApiHandle = handle;
    pTexture->mState.global = initState;
    pTexture->mState.isGlobal = true;
    return pTexture;
}

Texture::SharedPtr Texture::create1D(std::shared_ptr<Device> device, uint32_t width, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags) {
    bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "Texture1D");
    Texture::SharedPtr pTexture = SharedPtr(new Texture(device, width, 1, 1, arraySize, mipLevels, 1, format, Type::Texture1D, bindFlags));
    pTexture->apiInit(pData, (mipLevels == kMaxPossible));
    return pTexture;
}

Texture::SharedPtr Texture::create2D(std::shared_ptr<Device> device, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags) {
    bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "Texture2D");
    Texture::SharedPtr pTexture = SharedPtr(new Texture(device, width, height, 1, arraySize, mipLevels, 1, format, Type::Texture2D, bindFlags));
    pTexture->apiInit(pData, (mipLevels == kMaxPossible));
    return pTexture;
}

Texture::SharedPtr Texture::create3D(std::shared_ptr<Device> device, uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t mipLevels, const void* pData, BindFlags bindFlags, bool sparse) {
    bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "Texture3D");
    Texture::SharedPtr pTexture = SharedPtr(new Texture(device, width, height, depth, 1, mipLevels, 1, format, Type::Texture3D, bindFlags));
    pTexture->apiInit(pData, (mipLevels == kMaxPossible));
    return pTexture;
}

Texture::SharedPtr Texture::createCube(std::shared_ptr<Device> device, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags) {
    bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "TextureCube");
    Texture::SharedPtr pTexture = SharedPtr(new Texture(device, width, height, 1, arraySize, mipLevels, 1, format, Type::TextureCube, bindFlags));
    pTexture->apiInit(pData, (mipLevels == kMaxPossible));
    return pTexture;
}

Texture::SharedPtr Texture::create2DMS(std::shared_ptr<Device> device, uint32_t width, uint32_t height, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, BindFlags bindFlags) {
    bindFlags = updateBindFlags(device, bindFlags, false, 1, format, "Texture2DMultisample");
    Texture::SharedPtr pTexture = SharedPtr(new Texture(device, width, height, 1, arraySize, 1, sampleCount, format, Type::Texture2DMultisample, bindFlags));
    pTexture->apiInit(nullptr, false);
    return pTexture;
}

Texture::Texture(std::shared_ptr<Device> device, uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels, uint32_t sampleCount, ResourceFormat format, Type type, BindFlags bindFlags)
    : Resource(device, type, bindFlags, 0), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(mipLevels), mSampleCount(sampleCount), mArraySize(arraySize), mFormat(format) {
    
    LOG_DBG("Create texture %zu width %u height %u format %s bindFlags %s", id(), width, height, to_string(format).c_str(),to_string(bindFlags).c_str());

    assert(width > 0 && height > 0 && depth > 0);
    assert(arraySize > 0 && mipLevels > 0 && sampleCount > 0);
    assert(format != ResourceFormat::Unknown);

    if (mMipLevels == kMaxPossible) {
        uint32_t dims = width | height | depth;
        mMipLevels = bitScanReverse(dims) + 1;
    }
    mState.perSubresource.resize(mMipLevels * mArraySize, mState.global);
}

template<typename ViewClass>
using CreateFuncType = std::function<typename ViewClass::SharedPtr(Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)>;

template<typename ViewClass, typename ViewMapType>
typename ViewClass::SharedPtr findViewCommon(Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize, ViewMapType& viewMap, CreateFuncType<ViewClass> createFunc) {
    uint32_t resMipCount = 1;
    uint32_t resArraySize = 1;

    resArraySize = pTexture->getArraySize();
    resMipCount = pTexture->getMipCount();

    if (firstArraySlice >= resArraySize) {
        logWarning("First array slice is OOB when creating resource view. Clamping");
        firstArraySlice = resArraySize - 1;
    }

    if (mostDetailedMip >= resMipCount) {
        logWarning("Most detailed mip is OOB when creating resource view. Clamping");
        mostDetailedMip = resMipCount - 1;
    }

    if (mipCount == Resource::kMaxPossible) {
        mipCount = resMipCount - mostDetailedMip;
    } else if (mipCount + mostDetailedMip > resMipCount) {
        logWarning("Mip count is OOB when creating resource view. Clamping");
        mipCount = resMipCount - mostDetailedMip;
    }

    if (arraySize == Resource::kMaxPossible) {
        arraySize = resArraySize - firstArraySlice;
    } else if (arraySize + firstArraySlice > resArraySize) {
        logWarning("Array size is OOB when creating resource view. Clamping");
        arraySize = resArraySize - firstArraySlice;
    }

    ResourceViewInfo view = ResourceViewInfo(mostDetailedMip, mipCount, firstArraySlice, arraySize);

    if (viewMap.find(view) == viewMap.end()) {
        viewMap[view] = createFunc(pTexture, mostDetailedMip, mipCount, firstArraySlice, arraySize);
    }
    return viewMap[view];
}

DepthStencilView::SharedPtr Texture::getDSV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
        return DepthStencilView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, firstArraySlice, arraySize);
    };

    return findViewCommon<DepthStencilView>(this, mipLevel, 1, firstArraySlice, arraySize, mDsvs, createFunc);
}

UnorderedAccessView::SharedPtr Texture::getUAV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
        return UnorderedAccessView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, firstArraySlice, arraySize);
    };

    return findViewCommon<UnorderedAccessView>(this, mipLevel, 1, firstArraySlice, arraySize, mUavs, createFunc);
}

ShaderResourceView::SharedPtr Texture::getSRV() {
    return getSRV(0);
}

UnorderedAccessView::SharedPtr Texture::getUAV() {
    return getUAV(0);
}

RenderTargetView::SharedPtr Texture::getRTV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
    auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
        assert(pTexture->device());
        return RenderTargetView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, firstArraySlice, arraySize);
    };

    auto result = findViewCommon<RenderTargetView>(this, mipLevel, 1, firstArraySlice, arraySize, mRtvs, createFunc);
    if (!result) {
        LOG_ERR("ERROR findViewCommon<RenderTargetView> returned NULL");
    }

    return result;
}

ShaderResourceView::SharedPtr Texture::getSRV(uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
    auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
        return ShaderResourceView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, mipCount, firstArraySlice, arraySize);
    };

    if(mIsSparse)
        updateSparseBindInfo();

    return findViewCommon<ShaderResourceView>(this, mostDetailedMip, mipCount, firstArraySlice, arraySize, mSrvs, createFunc);
}

void Texture::captureToFile(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename, Bitmap::FileFormat format, Bitmap::ExportFlags exportFlags) {
    uint32_t channels;
    ResourceFormat resourceFormat;
    std::vector<uint8_t> textureData;

    readTextureData(mipLevel, arraySlice, textureData, resourceFormat, channels);

    auto func = [=]() {
        Bitmap::saveImage(filename, getWidth(mipLevel), getHeight(mipLevel), format, exportFlags, resourceFormat, true, (void*)(textureData.data()));
    };

    Threading::dispatchTask(func);
}

void Texture::captureToFileBlocking(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename, Bitmap::FileFormat format, Bitmap::ExportFlags exportFlags) {
    uint32_t channels;
    ResourceFormat resourceFormat;
    std::vector<uint8_t> textureData;

    readTextureData(mipLevel, arraySlice, textureData, resourceFormat, channels);
    Bitmap::saveImage(filename, getWidth(mipLevel), getHeight(mipLevel), format, exportFlags, resourceFormat, true, (void*)(textureData.data()));
}

void Texture::readTextureData(uint32_t mipLevel, uint32_t arraySlice, std::vector<uint8_t>& textureData, ResourceFormat& resourceFormat, uint32_t& channels) {
    assert(mType == Type::Texture2D);
    RenderContext* pContext = mpDevice->getRenderContext();

    // Handle the special case where we have an HDR texture with less then 3 channels
    FormatType type = getFormatType(mFormat);
    channels = getFormatChannelCount(mFormat);
    resourceFormat = mFormat;

    if (type == FormatType::Float && channels < 3) {
        Texture::SharedPtr pOther = Texture::create2D(mpDevice, getWidth(mipLevel), getHeight(mipLevel), ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);
        pContext->blit(getSRV(mipLevel, 1, arraySlice, 1), pOther->getRTV(0, 0, 1));
        textureData = pContext->readTextureSubresource(pOther.get(), 0);
        resourceFormat = ResourceFormat::RGBA32Float;
    } else {
        uint32_t subresource = getSubresourceIndex(arraySlice, mipLevel);
        textureData = pContext->readTextureSubresource(this, subresource);
    }
}

void Texture::uploadInitData(const void* pData, bool autoGenMips) {
    assert(mpDevice);
    auto pRenderContext = mpDevice->getRenderContext();
    if (!pRenderContext) {
        throw std::runtime_error("Can't get device rendering context !!!");
    }
    if (autoGenMips) {
        // Upload just the first mip-level
        size_t arraySliceSize = mWidth * mHeight * getFormatBytesPerBlock(mFormat);
        const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(pData);
        uint32_t numFaces = (mType == Texture::Type::TextureCube) ? 6 : 1;

        for (uint32_t i = 0; i < mArraySize * numFaces; i++) {
            uint32_t subresource = getSubresourceIndex(i, 0);
            pRenderContext->updateSubresourceData(this, subresource, pSrc);
            pSrc += arraySliceSize;
        }
    } else {
        pRenderContext->updateTextureData(this, pData);
    }

    if (autoGenMips) {
        generateMips(mpDevice->getRenderContext());
        invalidateViews();
    }
}

void Texture::generateMips(RenderContext* pContext) {
    if (mType != Type::Texture2D) {
        logWarning("Texture::generateMips() was only tested with Texture2Ds");
    }

    if (mIsSparse) {
        logWarning("Texture::generateMips() does not work with sparse textures !!!");
        return;
    }

    // #OPTME: should blit support arrays?
    for (uint32_t m = 0; m < mMipLevels - 1; m++) {
        for (uint32_t a = 0 ; a < mArraySize ; a++) {
            auto srv = getSRV(m, 1, a, 1);
            auto rtv = getRTV(m + 1, a, 1);
            pContext->blit(srv, rtv);
        }
    }

    if (mReleaseRtvsAfterGenMips) {
        // Releasing RTVs to free space on the heap.
        // We only do it once to handle the case that generateMips() was called during load.
        // If it was called more then once, the texture is probably dynamic and it's better to keep the RTVs around
        mRtvs.clear();
        mReleaseRtvsAfterGenMips = false;
    }
}

uint32_t Texture::getMipTailStart() const { 
    assert(mIsSparse);
    return mMipTailStart; 
}

uint8_t Texture::getMaxMipCount(const uint3& size) {
    return 1 + uint8_t(glm::log2(static_cast<float>(glm::max(glm::max(size[0], size[1]), size[2]))));
}

const std::vector<VirtualTexturePage::SharedPtr>& Texture::pages() {
    return mPages;
}


#ifdef FLACOR_D3D12
uint32_t Texture::getTextureSizeInBytes() {
    ID3D12DevicePtr pDevicePtr = mpDevice->getApiHandle();
    ID3D12ResourcePtr pTexResource = this->getApiHandle();

    D3D12_RESOURCE_ALLOCATION_INFO d3d12ResourceAllocationInfo;
    D3D12_RESOURCE_DESC desc = pTexResource->GetDesc();

    assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
    assert(desc.Width == mWidth);
    assert(desc.Height == mHeight);

    d3d12ResourceAllocationInfo = pDevicePtr->GetResourceAllocationInfo(0, 1, &desc);
    return (uint32_t)d3d12ResourceAllocationInfo.SizeInBytes;
}
#endif

SCRIPT_BINDING(Texture) {
    pybind11::class_<Texture, Texture::SharedPtr> texture(m, "Texture");
    texture.def_property_readonly("width", &Texture::getWidth);
    texture.def_property_readonly("height", &Texture::getHeight);
    texture.def_property_readonly("depth", &Texture::getDepth);
    texture.def_property_readonly("mipCount", &Texture::getMipCount);
    texture.def_property_readonly("arraySize", &Texture::getArraySize);
    texture.def_property_readonly("samples", &Texture::getSampleCount);
    texture.def_property_readonly("format", &Texture::getFormat);

    auto data = [](Texture* pTexture, uint32_t subresource)
    {
        return pTexture->device()->getRenderContext()->readTextureSubresource(pTexture, subresource);
    };
    texture.def("data", data, "subresource"_a);
}

}  // namespace Falcor
