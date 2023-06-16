/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#include "stdafx.h"

#include <random>
#include <chrono>

#include "Falcor/Core/Framework.h"
#include "GFXLowLevelContextApiData.h"
#include "GFXFormats.h"
#include "GFXResource.h"

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include "gfx_lib/slang-gfx.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Texture.h"

#include "lava_utils_lib/logging.h"

#include "Falcor/Core/API/CopyContext.h"

namespace Falcor {

static inline std::string to_string(gfx::ITextureResource::Offset3D offset) {
	return std::to_string(offset.x) + " " + std::to_string(offset.y) + " " + std::to_string(offset.z);
}

static inline std::string to_string(gfx::ITextureResource::Extents extent) {
	return std::to_string(extent.width) + " " + std::to_string(extent.height) + " " + std::to_string(extent.depth);
}

uint32_t getMipLevelPackedDataSize(const Texture* pTexture, uint32_t w, uint32_t h, uint32_t d, ResourceFormat format) {
	uint32_t perW = getFormatWidthCompressionRatio(format);
	uint32_t bw = align_to(perW, w) / perW;

	uint32_t perH = getFormatHeightCompressionRatio(format);
	uint32_t bh = align_to(perH, h) / perH;

	uint32_t size = bh * bw * d * getFormatBytesPerBlock(format);
	return size;
}

void CopyContext::bindDescriptorHeaps() { }

void CopyContext::updateTextureSubresources(const Texture* pTexture, uint32_t firstSubresource, uint32_t subresourceCount, const void* pData, const uint3& offset, const uint3& size) {
	resourceBarrier(pTexture, Resource::State::CopyDest);

	bool copyRegion = (offset != uint3(0)) || (size != uint3(-1));
	FALCOR_ASSERT(subresourceCount == 1 || (copyRegion == false));
	uint8_t* dataPtr = (uint8_t*)pData;
	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
	gfx::ITextureResource::Offset3D gfxOffset = { (int)offset.x, (int)offset.y, (int)offset.z };
	gfx::ITextureResource::Extents gfxSize = { (int)size.x, (int)size.y, (int)size.z };
	gfx::FormatInfo formatInfo = {};
	gfx::gfxGetFormatInfo(getGFXFormat(pTexture->getFormat()), &formatInfo);

	for (uint32_t index = firstSubresource; index < firstSubresource + subresourceCount; index++) {
		gfx::SubresourceRange subresourceRange = {};
		subresourceRange.baseArrayLayer = pTexture->getSubresourceArraySlice(index);
		subresourceRange.mipLevel = pTexture->getSubresourceMipLevel(index);
		subresourceRange.layerCount = 1;
		subresourceRange.mipLevelCount = 1;

		if (!copyRegion) {
			gfxSize.width = align_to(formatInfo.blockWidth, pTexture->getWidth(subresourceRange.mipLevel));
			gfxSize.height = align_to(formatInfo.blockHeight, pTexture->getHeight(subresourceRange.mipLevel));
			gfxSize.depth = pTexture->getDepth(subresourceRange.mipLevel);
		}

		gfx::ITextureResource::SubresourceData data = {};
		data.data = dataPtr;
		data.strideY = (int64_t)(gfxSize.width) / formatInfo.blockWidth * formatInfo.blockSizeInBytes;
		data.strideZ = data.strideY * (gfxSize.height / formatInfo.blockHeight);
		dataPtr += data.strideZ * gfxSize.depth;
		resourceEncoder->uploadTextureData(static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get()), subresourceRange, gfxOffset, gfxSize, &data, 1);
	}
}

CopyContext::ReadTextureTask::SharedPtr CopyContext::ReadTextureTask::create(CopyContext* pCtx, const Texture* pTexture, uint32_t subresourceIndex) {
	auto start = std::chrono::high_resolution_clock::now();

	Device::SharedPtr pDevice = pCtx->device();
	SharedPtr pThis = SharedPtr(new ReadTextureTask);
	pThis->mpContext = pCtx;
	
	//Get footprint
	gfx::ITextureResource* srcTexture = static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get());
	gfx::FormatInfo formatInfo;
	gfx::gfxGetFormatInfo(srcTexture->getDesc()->format, &formatInfo);

	auto mipLevel = pTexture->getSubresourceMipLevel(subresourceIndex);
	pThis->mActualRowSize = (pTexture->getWidth(mipLevel) + formatInfo.blockWidth - 1) / formatInfo.blockWidth * formatInfo.blockSizeInBytes;
	size_t rowAlignment = 1;
	pDevice->getApiHandle()->getTextureRowAlignment(&rowAlignment);
	pThis->mRowSize = align_to(static_cast<uint32_t>(rowAlignment), pThis->mActualRowSize);
	uint64_t rowCount =  (pTexture->getHeight(mipLevel) + formatInfo.blockHeight - 1) / formatInfo.blockHeight;
	uint64_t size = pTexture->getDepth(mipLevel) * rowCount * pThis->mRowSize;

	//Create buffer
	pThis->mpBuffer = Buffer::create(pDevice, size, Buffer::BindFlags::None, Buffer::CpuAccess::Read, nullptr);

	//Copy from texture to buffer
	pCtx->resourceBarrier(pTexture, Resource::State::CopySource);
	auto encoder = pCtx->getLowLevelData()->getApiData()->getResourceCommandEncoder();
	gfx::SubresourceRange srcSubresource = {};
	srcSubresource.baseArrayLayer = pTexture->getSubresourceArraySlice(subresourceIndex);
	srcSubresource.mipLevel = mipLevel;
	srcSubresource.layerCount = 1;
	srcSubresource.mipLevelCount = 1;

	encoder->copyTextureToBuffer(
		static_cast<gfx::IBufferResource*>(pThis->mpBuffer->getApiHandle().get()),
		pThis->mpBuffer->getGpuAddressOffset(),
		size,
		pThis->mRowSize,
		srcTexture,
		gfx::ResourceState::CopySource,
		srcSubresource,
		gfx::ITextureResource::Offset3D(0, 0, 0),
		gfx::ITextureResource::Extents{ (int)pTexture->getWidth(mipLevel), (int)pTexture->getHeight(mipLevel), (int)pTexture->getDepth(mipLevel) });
	pCtx->setPendingCommands(true);

	// Create a fence and signal
	pThis->mpFence = GpuFence::create(pDevice);
	pCtx->flush(false);
	pThis->mpFence->gpuSignal(pCtx->getLowLevelData()->getCommandQueue());
	pThis->mRowCount = (uint32_t)rowCount;
	pThis->mDepth = pTexture->getDepth(mipLevel);

	auto stop = std::chrono::high_resolution_clock::now();
  //std::cout << "CopyContext::ReadTextureTask::create time: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms." << std::endl;

	return pThis;
}

std::vector<uint8_t> CopyContext::ReadTextureTask::getData() {
	mpFence->syncCpu();
	// Get buffer data
	std::vector<uint8_t> result;
	result.resize((size_t)mRowCount * mActualRowSize);
	uint8_t* pData = reinterpret_cast<uint8_t*>(mpBuffer->map(Buffer::MapType::Read));

	for (uint32_t z = 0; z < mDepth; z++) {
		const uint8_t* pSrcZ = pData + z * (size_t)mRowSize * mRowCount;
		uint8_t* pDstZ = result.data() + z * (size_t)mActualRowSize * mRowCount;
		
		for (uint32_t y = 0; y < mRowCount; y++) {
			const uint8_t* pSrc = pSrcZ + y * (size_t)mRowSize;
			uint8_t* pDst = pDstZ + y * (size_t)mActualRowSize;
			memcpy(pDst, pSrc, mActualRowSize);
		}
	}

	mpBuffer->unmap();
	return result;
}

void CopyContext::ReadTextureTask::getData(uint8_t* textureData) {
	mpFence->syncCpu();
	
	// Get buffer data
	uint8_t* pData = reinterpret_cast<uint8_t*>(mpBuffer->map(Buffer::MapType::Read));

	memcpy(textureData, pData, (size_t)mRowCount * mActualRowSize);
	//memmove(textureData, pData, (size_t)mRowCount * mActualRowSize);

	mpBuffer->unmap();
}

void CopyContext::ReadTextureTask::getData(std::vector<uint8_t>& textureData) {
	textureData.resize((size_t)mRowCount * mActualRowSize);
	getData(textureData.data());
}

bool CopyContext::textureBarrier(const Texture* pTexture, Resource::State newState) {
	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
	bool recorded = false;

	if (pTexture->getGlobalState() != newState) {
		gfx::ITextureResource* textureResource = static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get());
		resourceEncoder->textureBarrier(1, &textureResource, getGFXResourceState(pTexture->getGlobalState()), getGFXResourceState(newState));
		mCommandsPending = true;
		recorded = true;
	}
	
	pTexture->setGlobalState(newState);
	return recorded;
}

bool CopyContext::bufferBarrier(const Buffer* pBuffer, Resource::State newState) {
	FALCOR_ASSERT(pBuffer);
	if (pBuffer->getCpuAccess() != Buffer::CpuAccess::None) return false;
	bool recorded = false;

	if (pBuffer->getGlobalState() != newState) {
		auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
		gfx::IBufferResource* bufferResource = static_cast<gfx::IBufferResource*>(pBuffer->getApiHandle().get());
		resourceEncoder->bufferBarrier(1, &bufferResource, getGFXResourceState(pBuffer->getGlobalState()), getGFXResourceState(newState));
		pBuffer->setGlobalState(newState);
		mCommandsPending = true;
		recorded = true;
	}
	return recorded;
}

void CopyContext::apiSubresourceBarrier(const Texture* pTexture, Resource::State newState, Resource::State oldState, uint32_t arraySlice, uint32_t mipLevel) {
	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
	auto subresourceState = pTexture->getSubresourceState(arraySlice, mipLevel);
	
	if (subresourceState != newState) {
		gfx::ITextureResource* textureResource = static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get());
		gfx::SubresourceRange subresourceRange = {};
		subresourceRange.baseArrayLayer = arraySlice;
		subresourceRange.mipLevel = mipLevel;
		subresourceRange.layerCount = 1;
		subresourceRange.mipLevelCount = 1;
		resourceEncoder->textureSubresourceBarrier(textureResource, subresourceRange, getGFXResourceState(subresourceState), getGFXResourceState(newState));
		mCommandsPending = true;
	}
}

void CopyContext::uavBarrier(const Resource* pResource) {
	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();

	if (pResource->getType() == Resource::Type::Buffer) {
		gfx::IBufferResource* bufferResource = static_cast<gfx::IBufferResource*>(pResource->getApiHandle().get());
		resourceEncoder->bufferBarrier(1, &bufferResource, gfx::ResourceState::UnorderedAccess, gfx::ResourceState::UnorderedAccess);
	} else {
		gfx::ITextureResource* textureResource = static_cast<gfx::ITextureResource*>(pResource->getApiHandle().get());
		resourceEncoder->textureBarrier(1, &textureResource, gfx::ResourceState::UnorderedAccess, gfx::ResourceState::UnorderedAccess);
	}
	mCommandsPending = true;
}

void CopyContext::copyResource(const Resource* pDst, const Resource* pSrc) {
	// Copy from texture to texture or from buffer to buffer.
	FALCOR_ASSERT(pDst->getType() == pSrc->getType());

	resourceBarrier(pDst, Resource::State::CopyDest);
	resourceBarrier(pSrc, Resource::State::CopySource);

	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();

	if (pDst->getType() == Resource::Type::Buffer) {
		FALCOR_ASSERT(pSrc->getSize() <= pDst->getSize());

		gfx::IBufferResource* srcBuffer = static_cast<gfx::IBufferResource*>(pSrc->getApiHandle().get());
		gfx::IBufferResource* dstBuffer = static_cast<gfx::IBufferResource*>(pDst->getApiHandle().get());

		resourceEncoder->copyBuffer(
			dstBuffer,
			static_cast<const Buffer*>(pDst)->getGpuAddressOffset(),
			srcBuffer,
			static_cast<const Buffer*>(pSrc)->getGpuAddressOffset(),
			pSrc->getSize());
	} else {
		gfx::ITextureResource* dstTexture = static_cast<gfx::ITextureResource*>(pDst->getApiHandle().get());
		gfx::ITextureResource* srcTexture = static_cast<gfx::ITextureResource*>(pSrc->getApiHandle().get());
		gfx::SubresourceRange subresourceRange = {};
		resourceEncoder->copyTexture(dstTexture, gfx::ResourceState::CopyDestination, subresourceRange, gfx::ITextureResource::Offset3D(0, 0, 0),
			srcTexture, gfx::ResourceState::CopySource, subresourceRange, gfx::ITextureResource::Offset3D(0, 0, 0), gfx::ITextureResource::Extents{ 0,0,0 });
	}
	mCommandsPending = true;
}

void CopyContext::copySubresource(const Texture* pDst, uint32_t dstSubresourceIdx, const Texture* pSrc, uint32_t srcSubresourceIdx) {
	copySubresourceRegion(pDst, dstSubresourceIdx, pSrc, srcSubresourceIdx, uint3(0), uint3(0), uint3(-1));
}

void CopyContext::updateBuffer(const Buffer* pBuffer, const void* pData, size_t offset, size_t numBytes) {
	if (numBytes == 0) {
		numBytes = pBuffer->getSize() - offset;
	}

	if (pBuffer->adjustSizeOffsetParams(numBytes, offset) == false) {
		LLOG_WRN << "CopyContext::updateBuffer() - size and offset are invalid. Nothing to update.";
		return;
	}

	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
	resourceEncoder->uploadBufferData(
		static_cast<gfx::IBufferResource*>(pBuffer->getApiHandle().get()),
		pBuffer->getGpuAddressOffset() + offset,
		numBytes,
		(void*)pData);

	mCommandsPending = true;
}

void CopyContext::copyBufferRegion(const Buffer* pDst, uint64_t dstOffset, const Buffer* pSrc, uint64_t srcOffset, uint64_t numBytes) {
	resourceBarrier(pDst, Resource::State::CopyDest);
	resourceBarrier(pSrc, Resource::State::CopySource);

	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
	gfx::IBufferResource* dstBuffer = static_cast<gfx::IBufferResource*>(pDst->getApiHandle().get());
	gfx::IBufferResource* srcBuffer = static_cast<gfx::IBufferResource*>(pSrc->getApiHandle().get());

	resourceEncoder->copyBuffer(dstBuffer, pDst->getGpuAddressOffset() + dstOffset, srcBuffer, pSrc->getGpuAddressOffset() + srcOffset, numBytes);
	mCommandsPending = true;
}

void CopyContext::copySubresourceRegion(const Texture* pDst, uint32_t dstSubresourceIdx, const Texture* pSrc, uint32_t srcSubresourceIdx, const uint3& dstOffset, const uint3& srcOffset, const uint3& size) {
	resourceBarrier(pDst, Resource::State::CopyDest);
	resourceBarrier(pSrc, Resource::State::CopySource);

	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
	gfx::ITextureResource* dstTexture = static_cast<gfx::ITextureResource*>(pDst->getApiHandle().get());
	gfx::ITextureResource* srcTexture = static_cast<gfx::ITextureResource*>(pSrc->getApiHandle().get());

	gfx::SubresourceRange dstSubresource = {};
	dstSubresource.baseArrayLayer = pDst->getSubresourceArraySlice(dstSubresourceIdx);
	dstSubresource.layerCount = 1;
	dstSubresource.mipLevel = pDst->getSubresourceMipLevel(dstSubresourceIdx);
	dstSubresource.mipLevelCount = 1;

	gfx::SubresourceRange srcSubresource = {};
	srcSubresource.baseArrayLayer = pSrc->getSubresourceArraySlice(srcSubresourceIdx);
	srcSubresource.layerCount = 1;
	srcSubresource.mipLevel = pSrc->getSubresourceMipLevel(srcSubresourceIdx);
	srcSubresource.mipLevelCount = 1;

	gfx::ITextureResource::Extents copySize = { (int)size.x, (int)size.y, (int)size.z };

	if (size.x == glm::uint(-1)) {
		copySize.width = pSrc->getWidth(srcSubresource.mipLevel) - srcOffset.x;
		copySize.height = pSrc->getHeight(srcSubresource.mipLevel) - srcOffset.y;
		copySize.depth = pSrc->getDepth(srcSubresource.mipLevel) - srcOffset.z;
	}

	resourceEncoder->copyTexture(
		dstTexture,
		gfx::ResourceState::CopyDestination,
		dstSubresource,
		gfx::ITextureResource::Offset3D(dstOffset.x, dstOffset.y, dstOffset.z),
		srcTexture,
		gfx::ResourceState::CopySource,
		srcSubresource,
		gfx::ITextureResource::Offset3D(srcOffset.x, srcOffset.y, srcOffset.z),
		copySize);
	mCommandsPending = true;
}

static void randomPattern(uint8_t* buffer, uint32_t width, uint32_t height) {
	std::random_device rd;
	std::mt19937 rndEngine(rd());
	std::uniform_int_distribution<uint32_t> rndDist(0, 255);
	uint8_t rndVal[4] = { 0, 0, 0, 0 };
	while (rndVal[0] + rndVal[1] + rndVal[2] < 10) {
		rndVal[0] = (uint8_t)rndDist(rndEngine);
		rndVal[1] = (uint8_t)rndDist(rndEngine);
		rndVal[2] = (uint8_t)rndDist(rndEngine);
	}
	rndVal[3] = 255;
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			for (uint32_t c = 0; c < 4; c++, ++buffer) {
				*buffer = rndVal[c];
			}
		}
	}
}

void CopyContext::fillMipTail(Texture* pTexture, const void* pData, bool tailDataInOnePage) {
	assert(pTexture);
	if(!pData) return;

	mpDevice->getApiHandle()->allocateTailMemory(pTexture);

	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();

	gfx::FormatInfo formatInfo = {};
	gfx::gfxGetFormatInfo(getGFXFormat(pTexture->getFormat()), &formatInfo);

	uint8_t *dataPtr = (uint8_t*)pData;

	for (uint32_t mipLevel = pTexture->getMipTailStart(); mipLevel < pTexture->getMipCount(); mipLevel++) {

		const uint32_t width = std::max(pTexture->getWidth(mipLevel), 1u);
		const uint32_t height = std::max(pTexture->getHeight(mipLevel) , 1u);
		const uint32_t depth = 1u;

		gfx::SubresourceRange subresourceRange = {};
		subresourceRange.baseArrayLayer = 0; //pTexture->getSubresourceArraySlice(0);
		subresourceRange.mipLevel = mipLevel;
		subresourceRange.layerCount = 1;
		subresourceRange.mipLevelCount = 1;

		std::vector<uint8_t> tmpData(width * height * 4);
		randomPattern(tmpData.data(), width, height);
		

		gfx::ITextureResource::SubresourceData data = {};
		data.data = dataPtr;
		data.strideY = (int64_t)(width) / formatInfo.blockWidth * formatInfo.blockSizeInBytes;
		data.strideZ = data.strideY * (height / formatInfo.blockHeight);
		dataPtr += tailDataInOnePage ? (data.strideZ * depth) : 65536;

		resourceEncoder->uploadTextureData(static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get()), subresourceRange, {0, 0, 0}, {static_cast<gfx::GfxCount>(width), static_cast<gfx::GfxCount>(height), 1}, &data, 1);
	}
	mCommandsPending = true;
}

void CopyContext::updateTexturePage(const VirtualTexturePage* pPage, const void* pData) {
  assert(pPage);
  
  if(!pData) return;

  if(!pPage->isResident()) {
    LLOG_ERR << "Unable to update non-resident texture page !!!";
    return;
  }

  const Texture* pTexture = pPage->texture().get();
  
	uint8_t* dataPtr = (uint8_t*)pData;
	auto resourceEncoder = getLowLevelData()->getApiData()->getResourceCommandEncoder();
	gfx::ITextureResource::Offset3D gfxOffset = pPage->offsetGFX();
	gfx::ITextureResource::Extents gfxSize = pPage->extentGFX();
	gfx::FormatInfo formatInfo = {};
	gfx::gfxGetFormatInfo(getGFXFormat(pTexture->getFormat()), &formatInfo);

	gfx::SubresourceRange subresourceRange = {};
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.mipLevel = pPage->mipLevel();
	subresourceRange.layerCount = 1;
	subresourceRange.mipLevelCount = 1;

	gfx::ITextureResource::SubresourceData data = {};
	data.data = (uint8_t*)pData;
	data.strideY = (int64_t)(gfxSize.width) / formatInfo.blockWidth * formatInfo.blockSizeInBytes;
	data.strideZ = data.strideY * (gfxSize.height / formatInfo.blockHeight);

	resourceEncoder->uploadTexturePageData(static_cast<gfx::ITextureResource*>(pTexture->getApiHandle().get()), gfxOffset, gfxSize, pPage->mipLevel(), &data);
	mCommandsPending = true;
}

void CopyContext::updateTexturePage(const VirtualTexturePage* pPage, Buffer::SharedPtr pStagingBuffer) {
	return;
}

}  // namespace Falcor
