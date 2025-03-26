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
#include <atomic>

#include "Texture.h"
#include "Device.h"
#include "RenderContext.h"
#include "GFXHelpers.h"
#include "GFXAPI.h"
#include "Falcor/Utils/Threading.h"

#include <glm/glm.hpp>
#include <glm/exponential.hpp>

#include "Falcor/Utils/Debug/debug.h"
#include "lava_utils_lib/logging.h"


namespace Falcor {

namespace {

static constexpr bool kTopDown = true; // Memory layout when loading from file

gfx::IResource::Type getGfxResourceType(Texture::Type type) {
  switch (type) {
    case Texture::Type::Texture1D:
        return gfx::IResource::Type::Texture1D;
    case Texture::Type::Texture2D:
    case Texture::Type::Texture2DMultisample:
        return gfx::IResource::Type::Texture2D;
    case Texture::Type::TextureCube:
        return gfx::IResource::Type::TextureCube;
    case Texture::Type::Texture3D:
        return gfx::IResource::Type::Texture3D;
    default:
        FALCOR_UNREACHABLE();
        return gfx::IResource::Type::Unknown;
  }
}

} // namespace

//--- Static buffer creation functions
ref<Texture> Texture::create1D(
	ref<Device> pDevice, 
	uint32_t width, 
	ResourceFormat format, 
	uint32_t arraySize, 
	uint32_t mipLevels, 
	const void* pInitData, 
	ResourceBindFlags bindFlags) 
{
	return pDevice->createTexture1D( width, format, arraySize, mipLevels, pInitData, bindFlags);
}

ref<Texture> Texture::create2D(
	ref<Device> pDevice, 
	uint32_t width, 
	uint32_t height, 
	ResourceFormat format, 
	uint32_t arraySize, 
	uint32_t mipLevels, 
	const void* pInitData, 
	ResourceBindFlags bindFlags) 
{
	return pDevice->createTexture2D(width, height, format, arraySize, mipLevels, pInitData, bindFlags);
}

ref<Texture> Texture::create3D(
	ref<Device> pDevice, 
	uint32_t width, 
	uint32_t height, 
	uint32_t depth, 
	ResourceFormat format, 
	uint32_t mipLevels, 
	const void* pInitData, 
	ResourceBindFlags bindFlags)
{
	return pDevice->createTexture3D(width, height, depth, format, mipLevels, pInitData, bindFlags);
}

ref<Texture> Texture::createCube(
	ref<Device> pDevice, 
	uint32_t width, 
	uint32_t height, 
	ResourceFormat format, 
	uint32_t arraySize, 
	uint32_t mipLevels, 
	const void* pInitData, 
	ResourceBindFlags bindFlags) 
{
	return pDevice->createTextureCube(width, height, format, arraySize, mipLevels, pInitData, bindFlags);
}

ref<Texture> Texture::create2DMS(
	ref<Device> pDevice, 
	uint32_t width, 
	uint32_t height, 
	ResourceFormat format, 
	uint32_t sampleCount, 
	uint32_t arraySize, 
	ResourceBindFlags bindFlags) 
{
	return pDevice->createTexture2DMS(width, height, format, sampleCount, arraySize, bindFlags);
}

ref<Texture> Texture::createFromResource(
	ref<Device> pDevice,
  gfx::ITextureResource* pResource,
  Texture::Type type,
  ResourceFormat format,
  uint32_t width,
  uint32_t height,
  uint32_t depth,
  uint32_t arraySize,
  uint32_t mipLevels,
  uint32_t sampleCount,
  ResourceBindFlags bindFlags,
  Resource::State initState) 
{
	return pDevice->createTextureFromResource(pResource, type, format, width, height, depth, arraySize, mipLevels, sampleCount, bindFlags, initState);
}

//----------------------------------------------------------

ref<Texture> Texture::createUDIMFromFile(ref<Device> pDevice, const std::string& filename) {
	fs::path fullPath(filename);
	return createUDIMFromFile(pDevice, fullPath);
}

ref<Texture> Texture::createUDIMFromFile(ref<Device> pDevice, const fs::path& path) {
	ref<Texture> pTexture = make_ref<Texture>(pDevice, Type::Texture2D, ResourceFormat::R8Unorm, 1, 1, 1, 1, 1, 1, ResourceBindFlags::None, nullptr);
	pTexture->mIsUDIMTexture = true;
	pTexture->mSourceFilename = path.string();

	for(uint i = 0; i < 100; ++i) {
		pTexture->mUDIMTileInfos[i].pTileTexture = nullptr;
	}

	return pTexture;
}

ref<Texture> Texture::createFromFile(ref<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, ResourceBindFlags bindFlags, Bitmap::ImportFlags importFlags) {
	const fs::path fullPath(filename);
	return createFromFile(pDevice, fullPath, generateMipLevels, loadAsSrgb, bindFlags, importFlags);
}

ref<Texture> Texture::createFromFile(ref<Device> pDevice, const fs::path& path, bool generateMipLevels, bool loadAsSrgb, ResourceBindFlags bindFlags, Bitmap::ImportFlags importFlags) {
	fs::path fullPath;
	if (!findFileInDataDirectories(path, fullPath)) {
		LLOG_WRN << "Error when loading texture. Can't find file " << path;
		return nullptr;
	}

	ref<Texture> pTex;
	if (hasExtension(fullPath, "dds")) {
		try {
			//pTex = ImageIO::loadTextureFromDDS(fullPath, loadAsSrgb);
			return nullptr;
		}
		catch (const std::exception& e)
		{
			LLOG_ERR << "Error loading texture '" << fullPath << "': " << e.what();
		}
	} else {
		Bitmap::UniqueConstPtr pBitmap = Bitmap::createFromFile(pDevice, fullPath, kTopDown, importFlags);
		if (pBitmap) {
			ResourceFormat texFormat = pBitmap->getFormat();
			if (loadAsSrgb) {
				texFormat = linearToSrgbFormat(texFormat);
			}
			pTex = Texture::create2D(pDevice, pBitmap->getWidth(), pBitmap->getHeight(), texFormat, 1, generateMipLevels ? Texture::kMaxPossible : 1, pBitmap->getData(), bindFlags);
		}
	}

	if (pTex) pTex->mSourceFilename = fullPath.string();

	return pTex;
}

Texture::Texture(
    ref<Device> pDevice,
    Type type,
    ResourceFormat format,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    uint32_t arraySize,
    uint32_t mipLevels,
    uint32_t sampleCount,
    ResourceBindFlags bindFlags,
    const void* pInitData)
    : Resource(std::move(pDevice), type, bindFlags, 0), 
    mFormat(format), 
    mWidth(width), 
    mHeight(height), 
    mDepth(depth), 
    mMipLevels(mipLevels), 
    mArraySize(arraySize), 
    mSampleCount(sampleCount),
    mIsSparse(false),  
	  mIsSolid(false),
	  mMemRequirements({}) 
{
    FALCOR_ASSERT(mType != Type::Buffer);
    FALCOR_ASSERT(mFormat != ResourceFormat::Unknown);
    FALCOR_ASSERT(mWidth > 0 && mHeight > 0 && mDepth > 0);
    switch (mType)
    {
    case Resource::Type::Texture1D:
        FALCOR_ASSERT(mHeight == 1 && mDepth == 1 && mSampleCount == 1);
        break;
    case Resource::Type::Texture2D:
        FALCOR_ASSERT(mDepth == 1 && mSampleCount == 1);
        break;
    case Resource::Type::Texture2DMultisample:
        FALCOR_ASSERT(mDepth == 1);
        break;
    case Resource::Type::Texture3D:
        FALCOR_ASSERT(mSampleCount == 1);
        break;
    case Resource::Type::TextureCube:
        FALCOR_ASSERT(mDepth == 1 && mSampleCount == 1);
        break;
    default:
        FALCOR_UNREACHABLE();
        break;
    }

    FALCOR_ASSERT(mArraySize > 0 && mMipLevels > 0 && mSampleCount > 0);

    bool autoGenerateMips = pInitData && (mMipLevels == Texture::kMaxPossible);

    if (autoGenerateMips)
        mBindFlags |= ResourceBindFlags::RenderTarget;

    if (mMipLevels == kMaxPossible)
    {
        uint32_t dims = width | height | depth;
        mMipLevels = bitScanReverse(dims) + 1;
    }

    mState.perSubresource.resize(mMipLevels * mArraySize, mState.global);

    ResourceBindFlags supported = mpDevice->getFormatBindFlags(mFormat);
    supported |= ResourceBindFlags::Shared;
    if ((mBindFlags & supported) != mBindFlags)
    {
        FALCOR_THROW(
            "Error when creating {} of format {}. The requested bind-flags are not supported. Requested = ({}), supported = ({}).",
            to_string(mType),
            to_string(mFormat),
            to_string(mBindFlags),
            to_string(supported)
        );
    }

    gfx::ITextureResource::Desc desc = {};
    desc.sparse = mIsSparse;
    desc.type = getGfxResourceType(mType);

    // Default state and allowed states.
    gfx::ResourceState defaultState;
    getGFXResourceState(mBindFlags, defaultState, desc.allowedStates);

    // Always set texture to general(common) state upon creation.
    desc.defaultState = gfx::ResourceState::General;

    desc.memoryType = gfx::MemoryType::DeviceLocal;

    desc.size.width = align_to(getFormatWidthCompressionRatio(mFormat), mWidth);
    desc.size.height = align_to(getFormatHeightCompressionRatio(mFormat), mHeight);
    desc.size.depth = mDepth;

    desc.arraySize = mType == Texture::Type::TextureCube ? mArraySize * 6 : mArraySize;
    desc.numMipLevels = mMipLevels;

    desc.format = getGFXFormat(mFormat); // lookup can result in Unknown / unsupported format

    desc.sampleDesc.numSamples = mSampleCount;
    desc.sampleDesc.quality = 0;

    // Clear value.
    gfx::ClearValue clearValue;
    if ((mBindFlags & (ResourceBindFlags::RenderTarget | ResourceBindFlags::DepthStencil)) != ResourceBindFlags::None)
    {
        if ((mBindFlags & ResourceBindFlags::DepthStencil) != ResourceBindFlags::None)
        {
            clearValue.depthStencil.depth = 1.0f;
        }
        desc.optimalClearValue = &clearValue;
    }

    // Shared resource.
    if (is_set(mBindFlags, ResourceBindFlags::Shared))
    {
        desc.isShared = true;
    }

    // Validate description.
    FALCOR_ASSERT(desc.size.width > 0 && desc.size.height > 0);
    FALCOR_ASSERT(desc.numMipLevels > 0 && desc.size.depth > 0 && desc.arraySize > 0 && desc.sampleDesc.numSamples > 0);

    // Create & upload resource.
    {
        // WARNING: This is a hack to allow parallel texture loading in TextureManager.
        std::lock_guard<std::mutex> lock(mpDevice->getGlobalGfxMutex());

        FALCOR_GFX_CALL(mpDevice->getGfxDevice()->createTextureResource(desc, this, nullptr, mGfxTextureResource.writeRef()));
        FALCOR_ASSERT(mGfxTextureResource);

        if (pInitData && !mIsSparse) {
            // Prevent the texture from being destroyed while uploading the data.
            incRef();
            uploadInitData(mpDevice->getRenderContext(), pInitData, autoGenerateMips);
            decRef(false);
        }
    }
}

template<typename ViewClass>
using CreateFuncType = std::function<
    ref<ViewClass>(Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)>;

template<typename ViewClass, typename ViewMapType>
ref<ViewClass> findViewCommon(
    Texture* pTexture,
    uint32_t mostDetailedMip,
    uint32_t mipCount,
    uint32_t firstArraySlice,
    uint32_t arraySize,
    ViewMapType& viewMap,
    CreateFuncType<ViewClass> createFunc
)
{
    uint32_t resMipCount = 1;
    uint32_t resArraySize = 1;

    resArraySize = pTexture->getArraySize();
    resMipCount = pTexture->getMipCount();

    if (firstArraySlice >= resArraySize) {
        LLOG_WRN << "First array slice is OOB when creating resource view. Clamping";
        firstArraySlice = resArraySize - 1;
    }

    if (mostDetailedMip >= resMipCount) {
        LLOG_WRN << "Most detailed mip is OOB when creating resource view. Clamping";
        mostDetailedMip = resMipCount - 1;
    }

    if (mipCount == Resource::kMaxPossible) {
        mipCount = resMipCount - mostDetailedMip;
    } else if (mipCount + mostDetailedMip > resMipCount) {
        LLOG_WRN << "Mip count is OOB when creating resource view. Clamping";
        mipCount = resMipCount - mostDetailedMip;
    }

    if (arraySize == Resource::kMaxPossible)
    {
        arraySize = resArraySize - firstArraySlice;
    } else if (arraySize + firstArraySlice > resArraySize) {
        LLOG_WRN << "Array size is OOB when creating resource view. Clamping";
        arraySize = resArraySize - firstArraySlice;
    }

    ResourceViewInfo view = ResourceViewInfo(mostDetailedMip, mipCount, firstArraySlice, arraySize);

    if (viewMap.find(view) == viewMap.end()) {
        viewMap[view] = createFunc(pTexture, mostDetailedMip, mipCount, firstArraySlice, arraySize);
    }

    return viewMap[view];
}


ref<DepthStencilView> Texture::getDSV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
  auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
  { return DepthStencilView::create(pTexture->getDevice().get(), pTexture, mostDetailedMip, firstArraySlice, arraySize); };

  return findViewCommon<DepthStencilView>(this, mipLevel, 1, firstArraySlice, arraySize, mDsvs, createFunc);
}

ref<UnorderedAccessView> Texture::getUAV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
  auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
  { return UnorderedAccessView::create(pTexture->getDevice().get(), pTexture, mostDetailedMip, firstArraySlice, arraySize); };

  return findViewCommon<UnorderedAccessView>(this, mipLevel, 1, firstArraySlice, arraySize, mUavs, createFunc);
}

ref<ShaderResourceView> Texture::getSRV() {
  return getSRV(0);
}

ref<UnorderedAccessView> Texture::getUAV() {
  return getUAV(0);
}

ref<RenderTargetView> Texture::getRTV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
  auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
  { return RenderTargetView::create(pTexture->getDevice().get(), pTexture, mostDetailedMip, firstArraySlice, arraySize); };

  return findViewCommon<RenderTargetView>(this, mipLevel, 1, firstArraySlice, arraySize, mRtvs, createFunc);
}

ref<ShaderResourceView> Texture::getSRV(uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
  auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
  { return ShaderResourceView::create(pTexture->getDevice().get(), pTexture, mostDetailedMip, mipCount, firstArraySlice, arraySize); };

  return findViewCommon<ShaderResourceView>(this, mostDetailedMip, mipCount, firstArraySlice, arraySize, mSrvs, createFunc);
}

void Texture::captureToFile(
    uint32_t mipLevel, 
    uint32_t arraySlice, 
    const fs::path& path, 
    Bitmap::FileFormat format, 
    Bitmap::ExportFlags exportFlags, 
    bool async) 
{
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to capture UDIM texture !";
		return;
	}

	uint32_t channels;
	ResourceFormat resourceFormat;
	std::vector<uint8_t> textureData;

	readTextureData(mipLevel, arraySlice, textureData, resourceFormat, channels);

	auto func = [=]() {
		Bitmap::saveImage(path, getWidth(mipLevel), getHeight(mipLevel), format, exportFlags, resourceFormat, true, (void*)(textureData.data()));
	};

	if (async)
    Threading::dispatchTask(func);
  else
    func();
}

void Texture::captureToFileBlocking(
    uint32_t mipLevel, 
    uint32_t arraySlice, 
    const fs::path& path, 
    Bitmap::FileFormat format, 
    Bitmap::ExportFlags exportFlags) 
{
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to capture UDIM texture !";
		return;
	}

    captureToFile(mipLevel, arraySlice, path, format, exportFlags, false);
}

void Texture::readTextureData(uint32_t mipLevel, uint32_t arraySlice, uint8_t* textureData) {
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to read UDIM texture data !";
		return;
	}

	uint32_t channels;
	ResourceFormat resourceFormat;
	readTextureData(mipLevel, arraySlice, textureData, resourceFormat, channels);
}

void Texture::readConvertedTextureData(uint32_t mipLevel, uint32_t arraySlice, uint8_t* pTextureData, ResourceFormat dstResourceFormat) {
	assert(pTextureData);
	assert(mType == Type::Texture2D);
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to read UDIM texture data !";
		return;
	}

	RenderContext* pContext = mpDevice->getRenderContext();

	// TODO: Handle the special case where we have an HDR texture with less than 3 channels
	//uint32_t channels = getFormatChannelCount(mFormat);
	
	if (mFormat != dstResourceFormat) {
		uint32_t elementCount = getWidth(0) * getHeight(0);

		std::vector<float> testData(getWidth(0) * getHeight(0) *3);
		for (size_t i = 0; i < testData.size(); i+=3) testData[i]=1.0f;

		ref<Buffer> pBuffer = Buffer::create(mpDevice, elementCount * getFormatBytesPerBlock(dstResourceFormat), ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, MemoryType::ReadBack);
	
		uint4 srcRect = {0, 0, getWidth(0), getHeight(0)};
		uint4 dstRect = {0, 0, getWidth(0), getHeight(0)};
		uint32_t bufferWidthPixels = getWidth(0);
		
		const TextureReductionMode componentsReduction[] = { TextureReductionMode::Standard, TextureReductionMode::Standard, TextureReductionMode::Standard, TextureReductionMode::Standard };
    	const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f) };
		
		pContext->blitToBuffer(getSRV(mipLevel, 1, arraySlice, 1), pBuffer, bufferWidthPixels, dstResourceFormat, srcRect, dstRect, TextureFilteringMode::Linear, componentsReduction, componentsTransform);
		const uint8_t* pBuf = reinterpret_cast<const uint8_t*>(pBuffer->map());

		LLOG_TRC << "blitToBuffer dst buffer read size " << std::to_string(pBuffer->getSize());

		::memcpy(reinterpret_cast<void*>(pTextureData), reinterpret_cast<const void*>(pBuf), pBuffer->getSize());
		pBuffer->unmap();
	} else {
		uint32_t subresource = getSubresourceIndex(arraySlice, mipLevel);
		pContext->readTextureSubresource(this, subresource, pTextureData);
	}

}

void Texture::readTextureData(uint32_t mipLevel, uint32_t arraySlice, uint8_t* textureData, ResourceFormat& resourceFormat, uint32_t& channels) {
	assert(mType == Type::Texture2D);
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to read UDIM texture data !";
		return;
	}

	RenderContext* pContext = mpDevice->getRenderContext();

	uint32_t subresource = getSubresourceIndex(arraySlice, mipLevel);
	pContext->readTextureSubresource(this, subresource, textureData);
}

void Texture::readTextureData(uint32_t mipLevel, uint32_t arraySlice, std::vector<uint8_t>& textureData, ResourceFormat& resourceFormat, uint32_t& channels) {
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to read UDIM texture data !";
		return;
	}

	size_t data_size = getWidth(mipLevel) * getHeight(mipLevel) * getFormatBytesPerBlock(mFormat);
	if( textureData.size() < data_size) {
		LLOG_WRN << "textureData size (" << textureData.size() << ") is less than requested (" << data_size << ") ! Forcing resize.";
		textureData.resize(data_size);
	}
	readTextureData(mipLevel, arraySlice, textureData.data(), resourceFormat, channels);
}

void Texture::uploadInitData(RenderContext* pRenderContext, const void* pData, bool autoGenMips) {
	assert(mpDevice);
	assert(!mIsUDIMTexture && "UDIM texture placeholder. Unable to upload data !!!");

	if (autoGenMips) {
		// Upload just the first mip-level
		size_t arraySliceSize = mWidth * mHeight * getFormatBytesPerBlock(mFormat);
		const uint8_t* pSrc = (uint8_t*)pData;
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
		generateMips(pRenderContext);
		invalidateViews();
	}
}

void Texture::generateMips(RenderContext* pContext, bool minMaxMips) {
	assert(!mIsUDIMTexture && "UDIM texture placeholder. Unable to generate mips data !!!");

	if (mType != Type::Texture2D) {
		LLOG_WRN << "Texture::generateMips() was only tested with Texture2Ds";
	}

	if (mIsSparse) {
		LLOG_WRN << "Texture::generateMips() does not work with sparse textures !!!";
		return;
	}

	// #OPTME: should blit support arrays?
  for (uint32_t m = 0; m < mMipLevels - 1; m++) {
    for (uint32_t a = 0; a < mArraySize; a++) {
      auto srv = getSRV(m, 1, a, 1);
      auto rtv = getRTV(m + 1, a, 1);
      
      if (!minMaxMips) {
        pContext->blit(srv, rtv, RenderContext::kMaxRect, RenderContext::kMaxRect, TextureFilteringMode::Linear);
      } else {
        const TextureReductionMode redModes[] = {
          TextureReductionMode::Standard,
          TextureReductionMode::Min,
          TextureReductionMode::Max,
          TextureReductionMode::Standard,
        };

        const float4 componentsTransform[] = {
          float4(1.0f, 0.0f, 0.0f, 0.0f),
          float4(0.0f, 1.0f, 0.0f, 0.0f),
          float4(0.0f, 0.0f, 1.0f, 0.0f),
          float4(0.0f, 0.0f, 0.0f, 1.0f),
        };

        pContext->blit(
          srv, rtv, RenderContext::kMaxRect, RenderContext::kMaxRect, TextureFilteringMode::Linear, redModes, componentsTransform
        );
      }
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
	assert(!mIsUDIMTexture);

	return mMipTailStart; 
}

// static
uint8_t Texture::getMaxMipCount(const uint3& size) {
	return 1 + uint8_t(glm::log2(static_cast<float>(glm::max(glm::max(size[0], size[1]), size[2]))));
}

uint64_t Texture::getTexelCount() const {
	if (mIsUDIMTexture) return 0;
	
  uint64_t count = 0;
  for (uint32_t i = 0; i < getMipCount(); i++) {
    uint64_t texelsInMip = (uint64_t)getWidth(i) * getHeight(i) * getDepth(i);
    FALCOR_ASSERT(texelsInMip > 0);
    count += texelsInMip;
  }

  count *= getArraySize();
  FALCOR_ASSERT(count > 0);
  return count;
}

void Texture::setUDIM_ID(uint16_t id) {
	if(!mIsUDIMTexture) {
		LLOG_ERR << "Unable to set texture UDIM ID to Non-UDIM texture !";
		return;
	}
	mUDIM_ID = id;
}

void Texture::setVirtualID(uint32_t id) {
	if(!mIsSparse) {
		LLOG_ERR << "Unable to set texture virtual ID to Non-virtual texture !";
		return;
	}
	mVirtualID = id;
}

void Texture::addUDIMTileTexture(const UDIMTileInfo& udim_tile_info) {
	if(!mIsUDIMTexture) {
		LLOG_ERR << "Unable to add UDIM texture tile to Non-UDIM texture !";
		return;
	}

	assert((udim_tile_info.u + udim_tile_info.v * 10) < 100);
	mUDIMTileInfos[udim_tile_info.u + udim_tile_info.v * 10] = udim_tile_info;
}

bool Texture::addTexturePage(uint32_t index, int3 offset, uint3 extent, const uint64_t size, uint32_t memoryTypeBits, const uint32_t mipLevel, uint32_t layer) {
  auto pPage = VirtualTexturePage::create(ref<Texture>(this), offset, extent, mipLevel, layer);
  if (!pPage) return false;

  pPage->mMemoryTypeBits = memoryTypeBits;
  pPage->mDevMemSize = size;
  pPage->mIndex = index;
    
  mSparseDataPages.push_back(pPage);
  return true;
}

bool Texture::compareDesc(const Texture* pOther) const {
	return mWidth == pOther->mWidth &&
		mHeight == pOther->mHeight &&
		mDepth == pOther->mDepth &&
		mMipLevels == pOther->mMipLevels &&
		mSampleCount == pOther->mSampleCount &&
		mArraySize == pOther->mArraySize &&
		mFormat == pOther->mFormat &&
		mIsSparse == pOther->mIsSparse &&
		all(mSparsePageRes == pOther->mSparsePageRes) &&
		mIsUDIMTexture == pOther->mIsUDIMTexture;
}

Texture::~Texture() {
	if (mIsUDIMTexture) {
		for(auto& info: mUDIMTileInfos) {
			info.pTileTexture.reset();
			info.pTileTexture = nullptr;
		}
	} else {
		if(mIsSparse) {
			for(auto pPage: mSparseDataPages) {
				pPage->release();
				pPage.reset();
			}
			mSparseDataPages.clear();
			mpDevice->getGfxDevice()->releaseTailMemory(this);
		}

		mpDevice->releaseResource(mGfxTextureResource);
	}
}

#ifdef SCRIPTING
SCRIPT_BINDING(Texture) {
	pybind11::class_<Texture, Texture::SharedPtr> texture(m, "Texture");
	texture.def_property_readonly("width", &Texture::getWidth);
	texture.def_property_readonly("height", &Texture::getHeight);
	texture.def_property_readonly("depth", &Texture::getDepth);
	texture.def_property_readonly("mipCount", &Texture::getMipCount);
	texture.def_property_readonly("arraySize", &Texture::getArraySize);
	texture.def_property_readonly("samples", &Texture::getSampleCount);
	texture.def_property_readonly("format", &Texture::getFormat);

	auto data = [](Texture* pTexture, uint32_t subresource) {
		return pTexture->device()->getRenderContext()->readTextureSubresource(pTexture, subresource);
	};
	texture.def("data", data, "subresource"_a);
}
#endif

}  // namespace Falcor
