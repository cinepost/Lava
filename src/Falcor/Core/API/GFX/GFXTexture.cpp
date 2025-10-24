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

#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Formats.h"

namespace Falcor {

namespace {

inline gfx::IResource::Type getResourceType(Texture::Type type) {
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
			assert(false);
			return gfx::IResource::Type::Unknown;
	}
}

} // namespace

uint64_t Texture::getTextureSizeInBytes() const {

	if(mIsUDIMTexture) {
		// UDIM textures are just placeholders with no GPU data allocated.
		return 0;
	}

	// get allocation info for resource description
	size_t outSizeBytes = 0, outAlignment = 0;

	//Slang::ComPtr<gfx::IDevice> iDevicePtr = mpDevice->getApiHandle();
	gfx::ITextureResource* textureResource = getGfxTextureResource();
	FALCOR_ASSERT(textureResource);

	gfx::ITextureResource::Desc *desc = textureResource->getDesc();

	mpDevice->getGfxDevice()->getTextureAllocationInfo(*desc, &outSizeBytes, &outAlignment);
	FALCOR_ASSERT(outSizeBytes > 0);

	return outSizeBytes;

}

void Texture::apiInit(const void* pData, bool autoGenMips, bool sparse) {
	// create resource description
	gfx::ITextureResource::Desc desc = {};

	// base description
	desc.sparse = sparse;

	// type
	desc.type = getResourceType(mType); // same as resource dimension in D3D12

	// default state and allowed states
	gfx::ResourceState defaultState;
	getGFXResourceState(mBindFlags, defaultState, desc.allowedStates);

	// Always set texture to general(common) state upon creation.
	desc.defaultState = gfx::ResourceState::General;

	desc.memoryType = gfx::MemoryType::DeviceLocal;
	// texture resource specific description attributes

	// size
	desc.size.width = align_to(getFormatWidthCompressionRatio(mFormat), mWidth);
	desc.size.height = align_to(getFormatHeightCompressionRatio(mFormat), mHeight);
	desc.size.depth = mDepth; // relevant for Texture3D

	// array size
	if (mType == Texture::Type::TextureCube) {
		desc.arraySize = mArraySize * 6;
	} else {
		desc.arraySize = mArraySize;
	}

	// mip map levels
	desc.numMipLevels = mMipLevels;

	// format
	desc.format = getGFXFormat(mFormat); // lookup can result in Unknown / unsupported format

	// sample description
	desc.sampleDesc.numSamples = mSampleCount;
	desc.sampleDesc.quality = 0;

	// clear value
	gfx::ClearValue clearValue;
	if ((mBindFlags & (ResourceBindFlags::RenderTarget | ResourceBindFlags::DepthStencil)) != ResourceBindFlags::None) {
		if ((mBindFlags & ResourceBindFlags::DepthStencil) != ResourceBindFlags::None) {
			clearValue.depthStencil.depth = 1.0f;
		}
	}
	desc.optimalClearValue = &clearValue;

	// shared resource
	if (is_set(mBindFlags, ResourceBindFlags::Shared)) {
		desc.isShared = true;
	}

	// validate description
	assert(desc.size.width > 0 && desc.size.height > 0);
	assert(desc.numMipLevels > 0 && desc.size.depth > 0 && desc.arraySize > 0 && desc.sampleDesc.numSamples > 0);

	// create resource
	if(SLANG_FAILED(mpDevice->getGfxDevice()->createTextureResource(desc, nullptr, mGfxTextureResource.writeRef()))) {
		LLOG_FTL << "Error creating texture " << to_string(desc);
		return;
	}
	
	gfx::ITextureResource* ptx = getGfxTextureResource();
	gfx::vk::TextureResourceImpl* pTextureResourceImpl = static_cast<gfx::vk::TextureResourceImpl*>(ptx);

	if(sparse) {
		const VkSparseImageMemoryRequirements& sparseImageMemoryRequirements = pTextureResourceImpl->getSparseImageMemoryRequirements();

		const VkExtent3D& imageGranularity = sparseImageMemoryRequirements.formatProperties.imageGranularity;
		LLOG_DBG << "Sparse image granularity " << imageGranularity.width << " x " << imageGranularity.height << " x " << imageGranularity.depth;

		auto pTextureManager = mpDevice->getTextureManager();

		uint32_t pageIndex = 0;
		uint32_t sparseDataPagesCapacity = 0;
		// Sparse bindings for each mip level of all layers outside of the mip tail
		for (uint32_t layer = 0; layer < pTextureResourceImpl->getArraySize(); ++layer) {

			// sparseImageMemoryRequirements.imageMipTailFirstLod is the first mip level that's stored inside the mip tail
			uint32_t currentMipBase = 0;
			for (uint32_t mipLevel = 0; mipLevel < sparseImageMemoryRequirements.imageMipTailFirstLod; ++mipLevel) {
				VkExtent3D extent;
				extent.width = std::max(desc.size.width >> mipLevel, 1);
				extent.height = std::max(desc.size.height >> mipLevel, 1);
				extent.depth = std::max(desc.size.depth >> mipLevel, 1);

				LLOG_DBG << "Mip level " << mipLevel << " width " << extent.width << " height " << extent.height << " depth " << extent.depth;

				// Aligned sizes by image granularity
				Falcor::uint3 sparseBindCounts = alignedDivision(extent, imageGranularity);
				Falcor::uint3 lastBlockExtent = {
					(extent.width % imageGranularity.width) ? extent.width % imageGranularity.width : imageGranularity.width,
					(extent.height % imageGranularity.height) ? extent.height % imageGranularity.height : imageGranularity.height,
					(extent.depth % imageGranularity.depth) ? extent.depth % imageGranularity.depth : imageGranularity.depth
				};

				LLOG_DBG << "Mip level " << mipLevel << " sparse binds count: " <<  sparseBindCounts.x << " " << sparseBindCounts.y << " " << sparseBindCounts.z;

				// @todo: Comment
				for (uint32_t z = 0; z < sparseBindCounts.z; ++z) {
					for (uint32_t y = 0; y < sparseBindCounts.y; ++y) {
						for (uint32_t x = 0; x < sparseBindCounts.x; ++x) {
							// Offset
							int3 offset (
								x * imageGranularity.width,
								y * imageGranularity.height,
								z * imageGranularity.depth
							);

							// Size of the page
							uint3 extent(
								(x == sparseBindCounts.x - 1) ? lastBlockExtent.x : imageGranularity.width,
								(y == sparseBindCounts.y - 1) ? lastBlockExtent.y : imageGranularity.height,
								(z == sparseBindCounts.z - 1) ? lastBlockExtent.z : imageGranularity.depth
							);

							// Add new virtual page
							addTexturePage(offset, extent, mipLevel, layer, pageIndex++);
						}
					}
				}
				mMipBases[mipLevel] = currentMipBase;
				
				currentMipBase += sparseBindCounts.x * sparseBindCounts.y * sparseBindCounts.z;
			}

			sparseDataPagesCapacity += currentMipBase;

			// @todo: proper comment
			// @todo: store in mip tail and properly release
			// @todo: Only one block for single mip tail
			
		} // end layers and mips
	}

//#if defined(FALCOR_GFX_VK) || defined(FALCOR_VK)
//	mMipTailimageMemoryBind.memory = VK_NULL_HANDLE;
//#endif

	// upload init data through texture class
	if (pData) {
		uploadInitData(pData, autoGenMips);
	}
}

bool Texture::addTexturePage(int3 offset, uint3 extent, uint32_t mipLevel, uint32_t layer, uint32_t index) {
  
  const auto& memRequirements = getGfxVKTextureResource()->getMemoryRequirements();

  auto pPage = VirtualTexturePage::create(Texture::SharedPtr(this), offset, extent, mipLevel, layer, index, memRequirements.alignment, memRequirements.memoryTypeBits);
  if (!pPage) return false;

  //LLOG_DBG << "VirtualTexturePage id: " << std::to_string(index) << " offset: " << to_string(offset) << " extent: " << to_string(extent);
    
  mSparseDataPages.push_back(pPage);
  return true;
}


void Texture::updateSparseBindInfo() {
	std::vector<const gfx::IVirtualTexturePageResource*> gfxTexturePages(mSparseDataPages.size());
	for(size_t i = 0; i < mSparseDataPages.size(); ++i) {
		gfxTexturePages[i] = mSparseDataPages[i]->getGfxTexturePageResource();
	}

	mpDevice->getGfxDevice()->updateSparseBindInfo(getGfxTextureResource(), gfxTexturePages);
}

}  // namespace Falcor
