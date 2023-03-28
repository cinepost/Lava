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
#include "lava_utils_lib/logging.h"


namespace Falcor {

namespace {

static const bool kTopDown = true; // Memory layout when loading from file

Texture::BindFlags updateBindFlags(Device::SharedPtr pDevice, Texture::BindFlags flags, bool hasInitData, uint32_t mipLevels, ResourceFormat format, const std::string& texType) {
	if ((mipLevels == Texture::kMaxPossible) && hasInitData) {
		flags |= Texture::BindFlags::RenderTarget;
	}

	Texture::BindFlags supported = getFormatBindFlags(pDevice, format);
	supported |= ResourceBindFlags::Shared;
	if ((flags & supported) != flags) {
		throw std::runtime_error("Error when creating " + texType + " of format " + to_string(format) + ". The requested bind-flags are not supported. Requested = (" 
									+ to_string(flags) + "), supported = (" +to_string(supported) + ").");
		flags = flags & supported;
	}

	return flags;
}

}  // namespace


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

	Texture::SharedPtr pTexture = std::make_shared<Texture>(device, width, height, depth, arraySize, mipLevels, sampleCount, format, type, bindFlags);
	pTexture->mApiHandle = handle;
	pTexture->mState.global = initState;
	pTexture->mState.isGlobal = true;
	return std::move(pTexture);
}

Texture::SharedPtr Texture::create1D(std::shared_ptr<Device> device, uint32_t width, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags) {
	bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "Texture1D");
	Texture::SharedPtr pTexture = std::make_shared<Texture>(device, width, 1, 1, arraySize, mipLevels, 1, format, Type::Texture1D, bindFlags);
	pTexture->apiInit(pData, (mipLevels == kMaxPossible));
	return std::move(pTexture);
}

Texture::SharedPtr Texture::create2D(std::shared_ptr<Device> device, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags) {
	bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "Texture2D");
	Texture::SharedPtr pTexture = std::make_shared<Texture>(device, width, height, 1, arraySize, mipLevels, 1, format, Type::Texture2D, bindFlags);
	pTexture->apiInit(pData, (mipLevels == kMaxPossible));
	return std::move(pTexture);
}

Texture::SharedPtr Texture::create3D(std::shared_ptr<Device> device, uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t mipLevels, const void* pData, BindFlags bindFlags, bool sparse) {
	bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "Texture3D");
	Texture::SharedPtr pTexture = std::make_shared<Texture>(device, width, height, depth, 1, mipLevels, 1, format, Type::Texture3D, bindFlags);
	pTexture->apiInit(pData, (mipLevels == kMaxPossible));
	return std::move(pTexture);
}

Texture::SharedPtr Texture::createCube(std::shared_ptr<Device> device, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags) {
	bindFlags = updateBindFlags(device, bindFlags, pData != nullptr, mipLevels, format, "TextureCube");
	Texture::SharedPtr pTexture = std::make_shared<Texture>(device, width, height, 1, arraySize, mipLevels, 1, format, Type::TextureCube, bindFlags);
	pTexture->apiInit(pData, (mipLevels == kMaxPossible));
	return std::move(pTexture);
}

Texture::SharedPtr Texture::create2DMS(std::shared_ptr<Device> device, uint32_t width, uint32_t height, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, BindFlags bindFlags) {
	bindFlags = updateBindFlags(device, bindFlags, false, 1, format, "Texture2DMultisample");
	Texture::SharedPtr pTexture = std::make_shared<Texture>(device, width, height, 1, arraySize, 1, sampleCount, format, Type::Texture2DMultisample, bindFlags);
	pTexture->apiInit(nullptr, false);
	return std::move(pTexture);
}

Texture::SharedPtr Texture::createUDIMFromFile(std::shared_ptr<Device> pDevice, const std::string& filename) {
	fs::path fullPath(filename);
	return createUDIMFromFile(pDevice, fullPath);
}

Texture::SharedPtr Texture::createUDIMFromFile(std::shared_ptr<Device> pDevice, const fs::path& path) {
	Texture::SharedPtr pTexture = std::make_shared<Texture>(pDevice, 1, 1, 1, 1, 1, 1, ResourceFormat::R8Unorm, Type::Texture2D, BindFlags::None);
	pTexture->mIsUDIMTexture = true;
	pTexture->mSourceFilename = path.string();

	for(uint i = 0; i < 100; i++) {
		pTexture->mUDIMTileInfos[i].pTileTexture = nullptr;
	}

	return std::move(pTexture);
}

Texture::SharedPtr Texture::createFromFile(Device::SharedPtr pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags) {
	fs::path fullPath(filename);
	return createFromFile(pDevice, fullPath, generateMipLevels, loadAsSrgb, bindFlags);
}

Texture::SharedPtr Texture::createFromFile(Device::SharedPtr pDevice, const fs::path& path, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags) {
	fs::path fullPath;
	if (!findFileInDataDirectories(path, fullPath)) {
		LLOG_WRN << "Error when loading texture. Can't find file " << path;
		return nullptr;
	}

	Texture::SharedPtr pTex;
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
		Bitmap::UniqueConstPtr pBitmap = Bitmap::createFromFile(pDevice, fullPath, kTopDown);
		if (pBitmap) {
			ResourceFormat texFormat = pBitmap->getFormat();
			if (loadAsSrgb) {
				texFormat = linearToSrgbFormat(texFormat);
			}

			pTex = Texture::create2D(pDevice, pBitmap->getWidth(), pBitmap->getHeight(), texFormat, 1, generateMipLevels ? Texture::kMaxPossible : 1, pBitmap->getData(), bindFlags);
		}
	}

	if (pTex != nullptr) {
		pTex->setSourcePath(fullPath);
	}

	return pTex;
}


Texture::Texture(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels, uint32_t sampleCount, ResourceFormat format, Type type, BindFlags bindFlags)
	: Resource(pDevice, type, bindFlags, 0), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(mipLevels), mSampleCount(sampleCount), mArraySize(arraySize), mFormat(format), mIsSparse(false), 
	  mIsSolid(false) {
	
	LLOG_DBG << "Create texture " << std::to_string(id()) << " width " << std::to_string(width) << " height " << std::to_string(height) 
		<< " format " << to_string(format) << " bindFlags " << to_string(bindFlags);

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

	if (arraySize == Resource::kMaxPossible) {
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

DepthStencilView::SharedPtr Texture::getDSV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
	assert(!mIsUDIMTexture && "UDIM texture placeholder !");

	auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
		return DepthStencilView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, firstArraySlice, arraySize);
	};

	return findViewCommon<DepthStencilView>(this, mipLevel, 1, firstArraySlice, arraySize, mDsvs, createFunc);
}

UnorderedAccessView::SharedPtr Texture::getUAV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
	assert(!mIsUDIMTexture && "UDIM texture placeholder !");

	auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
		return UnorderedAccessView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, firstArraySlice, arraySize);
	};

	return findViewCommon<UnorderedAccessView>(this, mipLevel, 1, firstArraySlice, arraySize, mUavs, createFunc);
}

ShaderResourceView::SharedPtr Texture::getSRV() {
	assert(!mIsUDIMTexture && "UDIM texture placeholder !");
	return getSRV(0);
}

UnorderedAccessView::SharedPtr Texture::getUAV() {
	assert(!mIsUDIMTexture && "UDIM texture placeholder !");
	return getUAV(0);
}

RenderTargetView::SharedPtr Texture::getRTV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) {
	assert(!mIsUDIMTexture && "UDIM texture placeholder !");
	auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
		assert(pTexture->device());
		return RenderTargetView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, firstArraySlice, arraySize);
	};

	auto result = findViewCommon<RenderTargetView>(this, mipLevel, 1, firstArraySlice, arraySize, mRtvs, createFunc);
	if (!result) {
		LLOG_ERR << "ERROR findViewCommon<RenderTargetView> returned NULL";
	}

	return result;
}

ShaderResourceView::SharedPtr Texture::getSRV(uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
	assert(!mIsUDIMTexture && "UDIM texture placeholder !");
	auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize) {
		return ShaderResourceView::create(pTexture->device(), pTexture->shared_from_this(), mostDetailedMip, mipCount, firstArraySlice, arraySize);
	};

	if(mIsSparse) {
		updateSparseBindInfo();
	}

	return findViewCommon<ShaderResourceView>(this, mostDetailedMip, mipCount, firstArraySlice, arraySize, mSrvs, createFunc);
}

void Texture::captureToFile(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename, Bitmap::FileFormat format, Bitmap::ExportFlags exportFlags) {
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to capture UDIM texture !";
		return;
	}

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
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to capture UDIM texture !";
		return;
	}

	uint32_t channels;
	ResourceFormat resourceFormat;
	std::vector<uint8_t> textureData;
	
	readTextureData(mipLevel, arraySlice, textureData, resourceFormat, channels);
	Bitmap::saveImage(filename, getWidth(mipLevel), getHeight(mipLevel), format, exportFlags, resourceFormat, true, (void*)(textureData.data()));
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
	assert(mType == Type::Texture2D);
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to read UDIM texture data !";
		return;
	}

	RenderContext* pContext = mpDevice->getRenderContext();

	// Handle the special case where we have an HDR texture with less then 3 channels
	FormatType type = getFormatType(mFormat);
	uint32_t channels = getFormatChannelCount(mFormat);
	
	if (mFormat != dstResourceFormat) {
		uint32_t elementCount = getWidth(0) * getHeight(0);

		std::vector<float> testData(getWidth(0) * getHeight(0) *3);
		for (size_t i = 0; i < testData.size(); i+=3) testData[i]=1.0f;

		Buffer::SharedPtr pBuffer = Buffer::create(mpDevice, elementCount * getFormatBytesPerBlock(dstResourceFormat), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::Read);
	
		uint4 srcRect = {0, 0, getWidth(0), getHeight(0)};
		uint4 dstRect = {0, 0, getWidth(0), getHeight(0)};
		uint32_t bufferWidthPixels = getWidth(0);
		const Sampler::ReductionMode componentsReduction[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard };
    const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f) };
		pContext->blitToBuffer(getSRV(mipLevel, 1, arraySlice, 1), pBuffer, bufferWidthPixels, dstResourceFormat, srcRect, dstRect, Sampler::Filter::Linear, componentsReduction, componentsTransform);
		pContext->flush(true);
		const uint8_t* pBuf = reinterpret_cast<const uint8_t*>(pBuffer->map(Buffer::MapType::Read));

		LLOG_TRC << "blitToBuffer dst buffer read size " << std::to_string(pBuffer->getSize());

		::memcpy(reinterpret_cast<void*>(pTextureData), reinterpret_cast<const void*>(pBuf), pBuffer->getSize());
		pBuffer->unmap();
	} else {
		uint32_t subresource = getSubresourceIndex(arraySlice, mipLevel);
		pContext->readTextureSubresource(this, subresource, pTextureData);
		pContext->flush(true);
	}

}

void Texture::readTextureData(uint32_t mipLevel, uint32_t arraySlice, uint8_t* textureData, ResourceFormat& resourceFormat, uint32_t& channels) {
	assert(mType == Type::Texture2D);
	if(mIsUDIMTexture) {
		LLOG_WRN << "Unable to read UDIM texture data !";
		return;
	}

	RenderContext* pContext = mpDevice->getRenderContext();

	// Handle the special case where we have an HDR texture with less then 3 channels
	FormatType type = getFormatType(mFormat);
	channels = getFormatChannelCount(mFormat);
	resourceFormat = mFormat;

	//if (type == FormatType::Float && channels < 3) {
	//	Texture::SharedPtr pOther = Texture::create2D(mpDevice, getWidth(mipLevel), getHeight(mipLevel), ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);
	//	pContext->blit(getSRV(mipLevel, 1, arraySlice, 1), pOther->getRTV(0, 0, 1));
	//	pContext->readTextureSubresource(pOther.get(), 0, textureData);
	//	resourceFormat = ResourceFormat::RGBA32Float;
	//} else {
		uint32_t subresource = getSubresourceIndex(arraySlice, mipLevel);
		pContext->readTextureSubresource(this, subresource, textureData);
	//}

	pContext->flush(true);
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

void Texture::uploadInitData(const void* pData, bool autoGenMips) {
	assert(mpDevice);
	assert(!mIsUDIMTexture && "UDIM texture placeholder. Unable to upload data !!!");

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
		for (uint32_t a = 0 ; a < mArraySize ; a++) {
			auto srv = getSRV(m, 1, a, 1);
			auto rtv = getRTV(m + 1, a, 1);
			if (!minMaxMips) {
				pContext->blit(srv, rtv, RenderContext::kMaxRect, RenderContext::kMaxRect, Sampler::Filter::Linear);
			} else {
				assert(false && "unimplemented");
				//const Sampler::ReductionMode redModes[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Min, Sampler::ReductionMode::Max, Sampler::ReductionMode::Standard };
				//const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f) };
				//pContext->blit(srv, rtv, RenderContext::kMaxRect, RenderContext::kMaxRect, Sampler::Filter::Linear, redModes, componentsTransform);
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
	assert(!mIsUDIMTexture);

	uint64_t count = 0;
	for (uint32_t i = 0; i < getMipCount(); i++) {
		uint64_t texelsInMip = (uint64_t)getWidth(i) * getHeight(i) * getDepth(i);
		assert(texelsInMip > 0);
		count += texelsInMip;
	}
	count *= getArraySize();
	assert(count > 0);
	return count;
}

void Texture::setUDIM_ID(uint16_t id) {
	if(!mIsUDIMTexture) {
		LLOG_ERR << "Unable to set texture UDIM ID to Non-UDIM texture !";
		return;
	}
	mUDIM_ID = id;
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
  auto pPage = VirtualTexturePage::create(shared_from_this(), offset, extent, mipLevel, layer);
  if (!pPage) return false;

  //LLOG_DBG << "VirtualTexturePage id: " << std::to_string(index) << " offset: " << to_string(offset) << " extent: " << to_string(extent);

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
		mSparsePageRes == pOther->mSparsePageRes;
		mIsUDIMTexture == pOther->mIsUDIMTexture;
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
