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
#include <boost/range/adaptors.hpp>

#include "stdafx.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/VirtualTexturePage.h"
#include "Falcor/Utils/StringUtils.h"
#include "Falcor/Utils/ConfigStore.h"
#include "Falcor/Utils/Image/LTX_Bitmap.h"

#include "TextureManager.h"


namespace ba = boost::adaptors;

// Temporarily disable asynchronous texture loader until Falcor supports parallel GPU work submission.
// Until then `TextureManager` should only called from the main thread.
#define DISABLE_ASYNC_TEXTURE_LOADER

namespace Falcor {

namespace {
	const size_t kMaxTextureHandleCount = std::numeric_limits<uint32_t>::max();
	static_assert(TextureManager::TextureHandle::kInvalidID >= kMaxTextureHandleCount);
}

TextureManager::SharedPtr TextureManager::create(Device::SharedPtr pDevice, size_t maxTextureCount, size_t threadCount) {
	return SharedPtr(new TextureManager(pDevice, maxTextureCount, threadCount));
}

TextureManager::TextureManager(Device::SharedPtr pDevice, size_t maxTextureCount, size_t threadCount)
	: mpDevice(pDevice)
	, mMaxTextureCount(std::min(maxTextureCount, kMaxTextureHandleCount))
	, mAsyncTextureLoader(mpDevice, threadCount)
{
	mUDIMTextureTilesCount = 0;
	mUDIMTexturesCount = 0;

	mSparseTexturesEnabled = true; // TODO: should be dependent on device features !! 
}

TextureManager::~TextureManager() {}

TextureManager::TextureHandle TextureManager::addTexture(const Texture::SharedPtr& pTexture) {
	assert(pTexture);
	if (pTexture->getType() != Resource::Type::Texture2D || pTexture->getSampleCount() != 1) {
		throw std::runtime_error("Only single-sample 2D textures can be added");
	}

	std::unique_lock<std::mutex> lock(mMutex);
	TextureHandle handle;

	if (auto it = mTextureToHandle.find(pTexture.get()); it != mTextureToHandle.end()) {
		// Texture is already managed. Return its handle.
		handle = it->second;
	} else {
		// Texture is not already managed. Add new texture desc.
		TextureDesc desc = { TextureState::Loaded, pTexture };
		handle = addDesc(desc);

		// Add to texture-to-handle map.
		mTextureToHandle[pTexture.get()] = handle;

		// If texture was originally loaded from disk, add to key-to-handle map to avoid loading it again later if requested in loadTexture().
		// It's possible the user-provided texture has already been loaded by us. In that case, log a warning as the redundant load should be fixed.
		if (!pTexture->getSourcePath().empty()) {
			bool hasMips = pTexture->getMipCount() > 1;
			bool isSrgb = isSrgbFormat(pTexture->getFormat());
			TextureKey textureKey(pTexture->getSourcePath().string(), hasMips, isSrgb, pTexture->getBindFlags());

			if (mKeyToHandle.find(textureKey) == mKeyToHandle.end()) {
				mKeyToHandle[textureKey] = handle;
			} else {
				LLOG_WRN << "TextureManager::addTexture() - Texture loaded from '" << pTexture->getSourcePath() << "' appears to be identical to an already loaded texture. This could be optimized by getting it from TextureManager.";
			}
		}
	}

	return handle;
}

Texture::SharedPtr TextureManager::loadSparseTexture(const fs::path& path, bool generateMipLevels, bool loadAsSRGB, Resource::BindFlags bindFlags) {
	std::string ext = path.extension().string();

	if (ext == ".dds") {
		LLOG_ERR << "Sparse texture handling for DDS format unimplemented !!!";
		return nullptr;
	}

	const auto& configStore = ConfigStore::instance();
	//bool vtoff = configStore.get<bool>("vtoff", true);
	//if (!mSparseTexturesEnabled || vtoff) {
	//	if (ext == ".ltx") {
	//		LLOG_ERR << "Virtual texturing disabled. Unable to use LTX texture " << path.string();
	//		return handle;
	//	}
	//	return loadTexture(path, generateMipLevels, loadAsSRGB, bindFlags);
	//}

	fs::path ltxPath = appendExtension(path, ".ltx");

	bool ltxMagicMatch = false;

	if(fs::exists(ltxPath) && LTX_Bitmap::checkFileMagic(ltxPath, true)) ltxMagicMatch = true;

	if(!fs::exists(ltxPath) || !ltxMagicMatch ) {
		if(!configStore.get<bool>("fconv", true)) {
			LLOG_WRN << "On-line sparse texture conversion disabled !!!";
			return nullptr;
		}

		LLOG_DBG << "Converting texture " << path << " to LTX format ...";
		LTX_Bitmap::TLCParms tlcParms;
		tlcParms.compressorName = configStore.get<std::string>("vtex_tlc", "none");
		tlcParms.compressionLevel = (uint8_t)configStore.get<int>("vtex_tlc_level", 0);
		if (!LTX_Bitmap::convertToLtxFile(mpDevice, path.string(), ltxPath.string(), tlcParms, true)) {
			LLOG_ERR << "Error converting texture to " << ltxPath;
			return nullptr;
		} else {
			LLOG_DBG << "Conversion done for " << ltxPath;
		}
	}
	
	auto pLtxBitmap = LTX_Bitmap::createFromFile(mpDevice, ltxPath, true);
  if (!pLtxBitmap) {
    LLOG_ERR << "Error loading converted ltx bitmap from " << ltxPath;
    return nullptr;
  }
  ResourceFormat texFormat = pLtxBitmap->getFormat();

  if (loadAsSRGB) {
    texFormat = linearToSrgbFormat(texFormat);
  }

  uint32_t arraySize = 1;
  Texture::SharedPtr pTexture = Texture::SharedPtr(
  	new Texture(mpDevice, pLtxBitmap->getWidth(), pLtxBitmap->getHeight(), 1, arraySize, pLtxBitmap->getMipLevelsCount(), 1, texFormat, Texture::Type::Texture2D, bindFlags)
  );

  if( !pTexture ) return nullptr;

  pTexture->setSourceFilename(ltxPath.string());
  pTexture->mIsSparse = true;
  
	try {
    pTexture->apiInit(nullptr, generateMipLevels);

    for(auto& pPage: pTexture->sparseDataPages()) {
    	pPage->mID = static_cast<uint32_t>(mSparseDataPages.size());
    	mSparseDataPages.push_back(pPage);
    }

  } catch (const std::runtime_error& e) {
    LLOG_ERR << "Error initializing sparse texture " << ltxPath << "'\nError details:";
    LLOG_ERR << e.what();
    return nullptr;
  } catch (...) {
    LLOG_ERR <<  "Error initializing sparse texture " << ltxPath;
    return nullptr;
  }

  uint32_t deviceMemRequiredSize = pTexture->getTextureSizeInBytes();
  LLOG_DBG << "Texture requires " << std::to_string(deviceMemRequiredSize) << " bytes of device memory";
  //if(deviceMemRequiredSize <= deviceCacheMemSizeLeft) {
  //  deviceCacheMemSizeLeft = deviceCacheMemSize - deviceMemRequiredSize;
  //} else {
  //  LLOG_ERR << "No texture memory left for texture " <<  ltxPath;
  //  return handle;
  //}
  
  // Sparse bitmaps tracking
  auto it = mTextureLTXBitmapsMap.find(pTexture->id());
  if (it == mTextureLTXBitmapsMap.end()) {
    mTextureLTXBitmapsMap[pTexture->id()] = std::move(pLtxBitmap);
  }
  //mTextureLTXBitmapsMap[pTexture->id()] = std::move(pLtxBitmap);
  
	return pTexture;
}

void TextureManager::loadPages(const Texture::SharedPtr& pTexture, const std::vector<uint32_t>& pageIds) {
	if(!mHasSparseTextures || !pTexture || !pTexture->isSparse()) return;

  assert(pTexture.get());

  uint32_t textureID = pTexture->id();

  auto it = mTextureLTXBitmapsMap.find(textureID);
  if (it == mTextureLTXBitmapsMap.end()) {
    LLOG_ERR << "No LTX_Bitmap stored for texture " <<  pTexture->getSourceFilename();
    return;
  }

  std::vector<uint32_t> _pageIds = pageIds;
  std::sort(_pageIds.begin(), _pageIds.end());

  // allocate pages
  uint32_t data_pages_count = (uint32_t)mSparseDataPages.size();
  for( uint32_t page_id: _pageIds ) {
  	if (page_id >= data_pages_count) {
  		LLOG_ERR << "Error sparse data pages indexing! Page with id " << page_id << " requested while " << data_pages_count << " pages exist !!!";
  		continue;
  	}
    mSparseDataPages[page_id]->allocate();
  }

  //pTexture->updateSparseBindInfo();
  //fillMipTail(pTexture);

  // read data and fill pages
  auto pLtxBitmap = mTextureLTXBitmapsMap[textureID];
  std::string ltxFilename = pLtxBitmap->getFileName();
  auto pFile = fopen(ltxFilename.c_str(), "rb");
  std::vector<uint8_t> tmpPage(65536);
  auto pTmpPageData = tmpPage.data();
  
  for( uint32_t page_id: _pageIds ) {
    const auto& pPage = mSparseDataPages[page_id];
    if(pLtxBitmap->readPageData(pPage->index(), pTmpPageData, pFile)) {
    	mpDevice->getRenderContext()->updateTexturePage(pPage.get(), pTmpPageData);
    	LLOG_DBG << "Loaded page mip level " << std::to_string(pPage->mipLevel());
  	} else {
  		pPage->release();
  	}
  }
  //mpDevice->getRenderContext()->flush(true);

  mpDevice->getRenderContext()->fillMipTail(pTexture, nullptr);

  fclose(pFile);

  pTexture->updateSparseBindInfo();
}

TextureManager::TextureHandle TextureManager::loadTexture(const fs::path& path, bool generateMipLevels, bool loadAsSRGB, Resource::BindFlags bindFlags, bool async, std::string udim_mask, bool loadAsSparse) {
	TextureHandle handle;

	// Is UDIM file path ?
	bool is_udim_texture = false;
	std::vector<std::pair<fs::path, Falcor::uint2>> udim_tile_fileinfos;

	if (udim_mask != "") {
		std::string path_filename =  path.filename().string();
		size_t udim_mask_found = path_filename.find(udim_mask);
		
		if (udim_mask_found != std::string::npos) {
			is_udim_texture = true;
			LLOG_DBG << "UDIM texture requested: " << path.string();

			// Now get the list of available tiles
			const boost::regex udim_tile_filter(path_filename.replace(udim_mask_found, udim_mask.size(), "\\d{4}\\").c_str());
			boost::smatch what;

			for (auto &entry: boost::make_iterator_range(fs::directory_iterator(path.parent_path()), {})
				| ba::filtered(static_cast<bool (*)(const fs::path &)>(&fs::is_regular_file))
				| ba::filtered([&](const fs::path &path){ return boost::regex_match(path.filename().string(), what, udim_tile_filter); })
			) {
				std::string udim_tile_number_str = entry.path().filename().string().substr(udim_mask_found, 4);
				size_t udim_tile_number = static_cast<size_t>(std::stoul(udim_tile_number_str)) - 1001;

				if( udim_tile_number < 100) {
					std::ldiv_t ldivresult;
					ldivresult = ldiv(udim_tile_number,10);

					size_t udim_tile_u_number = ldivresult.rem;
					size_t udim_tile_v_number = ldivresult.quot;

					udim_tile_fileinfos.push_back(std::make_pair(entry.path(), Falcor::uint2({udim_tile_u_number, udim_tile_v_number})));
				} else {
					LLOG_ERR << "Wrong UDIM filename: " << entry.path().string();
				}
			}

			for (auto const& i: udim_tile_fileinfos) {
				LLOG_DBG << "Found UDIM tile " << to_string(i.second) << " : " << i.first.string();
			}
		}
	}

	// Find the full path to the texture if it's not a UDIM.
	fs::path fullPath;

	if (is_udim_texture) {
		// If UDIM texture requested we have store handle with no actual texture loaded that is referenced by actual tiles textures.
		// So we use UDIM texture path as fullpath key for map storage and access.
		fullPath = path;
	} else {
		if (!findFileInDataDirectories(path, fullPath)) {
			LLOG_WRN << "Can't find texture file " << path;
			return handle;
		}
	}

	std::unique_lock<std::mutex> lock(mMutex);
	const TextureKey textureKey(fullPath, generateMipLevels, loadAsSRGB, bindFlags);

	if (auto it = mKeyToHandle.find(textureKey); it != mKeyToHandle.end()) {
		// Texture is already managed. Return its handle.
		LLOG_INF << "Texture " << textureKey.fullPath.string() << " is already managed !";
		handle = it->second;
	} else {

	// Check if UDIM texture requested...

#ifndef DISABLE_ASYNC_TEXTURE_LOADER
		mLoadRequestsInProgress++;

		// Texture is not already managed. Add new texture desc.
		TextureDesc desc = { TextureState::Referenced, nullptr };
		handle = addDesc(desc);

		// Add to key-to-handle map.
		mKeyToHandle[textureKey] = handle;

		// Function called by the async texture loader when loading finishes.
		// It's called by a worker thread so needs to acquire the mutex before changing any state.
		auto callback = [=](Texture::SharedPtr pTexture)
		{
			std::unique_lock<std::mutex> lock(mMutex);

			// Mark texture as loaded.
			auto& desc = getDesc(handle);
			desc.state = TextureState::Loaded;
			desc.pTexture = pTexture;

			// Add to texture-to-handle map.
			if (pTexture) mTextureToHandle[pTexture.get()] = handle;

			mLoadRequestsInProgress--;
			mCondition.notify_all();
		};

		// Issue load request to texture loader.
		mAsyncTextureLoader.loadFromFile(fullPath, generateMipLevels, loadAsSRGB, bindFlags, callback);
#else
		// Load texture from main thread.
		
		if(!is_udim_texture) {
			// Load single texture
			Texture::SharedPtr pTexture = nullptr;

			if(!loadAsSparse) {
				pTexture = Texture::createFromFile(mpDevice, fullPath, generateMipLevels, loadAsSRGB, bindFlags);
			} else {
				pTexture = loadSparseTexture(fullPath, generateMipLevels, loadAsSRGB, bindFlags);
				if(!pTexture) {
					LLOG_ERR << "Error loading sparse texture !!!";
				}
			}

			// Add new texture desc.
			TextureDesc desc = { TextureState::Loaded, pTexture };
			handle = addDesc(desc);

			// Add to key-to-handle map.
			mKeyToHandle[textureKey] = handle;

			// Add to texture-to-handle map.
			if (pTexture) {
				mTextureToHandle[pTexture.get()] = handle;
				if (pTexture->isSparse()) {
					mHasSparseTextures = true;
				}
			}

		} else {

			// Load UDIM texture tiles
			Texture::SharedPtr pUDIMTexture = Texture::createUDIMFromFile(mpDevice, fullPath);

			// Add epmty texture tileset desc.
			TextureDesc desc = { TextureState::Loaded, pUDIMTexture };
			handle = addDesc(desc, TextureHandle::Mode::UDIM_Texture);
		
			// Add UDIM tileset to key-to-handle map.
			mKeyToHandle[textureKey] = handle;

			// Add to texture-to-handle map.
			if (pUDIMTexture) {
				mTextureToHandle[pUDIMTexture.get()] = handle;
				mUDIMTexturesCount++;
			}

			for( const auto& i: udim_tile_fileinfos) {
				const auto& udim_tile_fullpath = i.first;
				const Falcor::uint2& udim_tile_pos = i.second;

				const TextureKey udimTileTextureKey(udim_tile_fullpath, generateMipLevels, loadAsSRGB, bindFlags);

				TextureHandle udim_tile_handle;

				if (auto it = mKeyToHandle.find(udimTileTextureKey); it != mKeyToHandle.end()) {
					// Texture tile is already managed. Return its handle.
					udim_tile_handle = it->second;
				} else {

					Texture::SharedPtr pUdimTileTex = nullptr;

					if(!loadAsSparse) {
						pUdimTileTex = Texture::createFromFile(mpDevice, udim_tile_fullpath, generateMipLevels, loadAsSRGB, bindFlags);
					} else {
						pUdimTileTex = loadSparseTexture(udim_tile_fullpath, generateMipLevels, loadAsSRGB, bindFlags);
						if(pUdimTileTex) {
							mHasSparseTextures = true;
						}
					}

					LLOG_DBG << "Loaded " << (loadAsSparse ? "sparse": "" ) << " UDIM tile texture: " << udim_tile_fullpath << " pos: " << std::to_string(udim_tile_pos[0]) << "x" << std::to_string(udim_tile_pos[1]);
					
					TextureDesc udim_tile_desc = { TextureState::Loaded, pUdimTileTex };
					udim_tile_handle = addDesc(udim_tile_desc, TextureHandle::Mode::UDIM_Tile);
		
					// Add tile handle to key-to-handle map.
					mKeyToHandle[udimTileTextureKey] = udim_tile_handle;

					// Add tile to texture-to-handle map.
					if (pUdimTileTex) {
						mHasUDIMTextures = true;
						mTextureToHandle[pUdimTileTex.get()] = udim_tile_handle;
						
						if (pUDIMTexture) {
							pUDIMTexture->addUDIMTileTexture({pUdimTileTex, udim_tile_pos[0], udim_tile_pos[1]});
							mUDIMTextureTilesCount ++;
						}
					}
				}
			}
		}

		mCondition.notify_all();
#endif
	}

	lock.unlock();

	if (!async) {
		waitForTextureLoading(handle);
	}

	return handle;
}

void TextureManager::waitForTextureLoading(const TextureHandle& handle) {
	if (!handle) return;

	// Acquire mutex and wait for texture state to change.
	std::unique_lock<std::mutex> lock(mMutex);
	mCondition.wait(lock, [&]() { return getDesc(handle).state == TextureState::Loaded; });

	mpDevice->flushAndSync();
}

void TextureManager::waitForAllTexturesLoading() {
	// Acquire mutex and wait for all in-progress requests to finish.
	std::unique_lock<std::mutex> lock(mMutex);
	mCondition.wait(lock, [&]() { return mLoadRequestsInProgress == 0; });

	mpDevice->flushAndSync();
}

void TextureManager::removeTexture(const TextureHandle& handle) {
	if (!handle) return;

	waitForTextureLoading(handle);

	std::lock_guard<std::mutex> lock(mMutex);

	// Get texture desc. If it's already cleared, we're done.
	auto& desc = getDesc(handle);
	if (!desc.isValid()) return;

	// Remove handle from maps.
	// Note not all handles exist in key-to-handle map so search for it. This can be optimized if needed.
	auto it = std::find_if(mKeyToHandle.begin(), mKeyToHandle.end(), [handle](const auto& keyVal) { return keyVal.second == handle; });
	if (it != mKeyToHandle.end()) mKeyToHandle.erase(it);

	if (desc.pTexture) {
		assert(mTextureToHandle.find(desc.pTexture.get()) != mTextureToHandle.end());
		mTextureToHandle.erase(desc.pTexture.get());
	}

	// Clear texture desc.
	desc = {};

	// Return handle to the free list.
	mFreeList.push_back(handle);
}

TextureManager::TextureDesc TextureManager::getTextureDesc(const TextureHandle& handle) const {
	if (!handle) return {};

	std::lock_guard<std::mutex> lock(mMutex);
	assert(handle && handle.id < mTextureDescs.size());
	return mTextureDescs[handle.id];
}

size_t TextureManager::getTextureDescCount() const {
	std::lock_guard<std::mutex> lock(mMutex);
	return mTextureDescs.size();
}

void TextureManager::finalize() {
	if (!mDirty) return;

	for (size_t i = 0; i < mTextureDescs.size(); i++) {
		const auto& pTex = mTextureDescs[i].pTexture;
		if(pTex && pTex->isUDIMTexture()) {
			pTex->setUDIM_ID(i);
		}
	}

	mDirty = false;
}

void TextureManager::setShaderData(const ShaderVar& var, const size_t descCount) const {
	LLOG_DBG << "Setting shader data for " << to_string(mTextureDescs.size()) << " texture descs";
	std::lock_guard<std::mutex> lock(mMutex);

	if (mTextureDescs.size() > descCount) {
		throw std::runtime_error("Textures descriptor array is too small");
	}

	Texture::SharedPtr nullTexture;

	size_t ii = 0; // Current material system textures index

	// Fill in textures
	for (size_t i = 0; i < mTextureDescs.size(); i++) {
		const auto& pTex = mTextureDescs[i].pTexture;
		if(pTex && !pTex->isUDIMTexture()) {
			var[ii] = pTex;
		} else {
			var[ii] = nullTexture;
		}
		ii++;
	}

	// Fill the array tail
	for (size_t i = ii; i < descCount; i++) {	
		var[i] = nullTexture;
	}
}

void TextureManager::setUDIMTableShaderData(const ShaderVar& var, const size_t descCount) const {
	LLOG_DBG << "Setting UDIM table shader data " << to_string(descCount) << " descriptors";

	for (size_t i = 0; i < mTextureDescs.size(); i++) {
		const auto& pTex = mTextureDescs[i].pTexture;
		if(pTex && pTex->isUDIMTexture()) {
			for( const auto& tileInfo: pTex->getUDIMTileInfos()) {
				if( tileInfo.pTileTexture ) {
					auto const& it = mTextureToHandle.find(tileInfo.pTileTexture.get());
					if(it != mTextureToHandle.end()) {
						size_t udim_tile_idx = static_cast<size_t>(pTex->getUDIM_ID()) * 100 + tileInfo.u + tileInfo.v * 10;
						LLOG_DBG << "Tile idx: " << std::to_string(udim_tile_idx);
						var[udim_tile_idx] = it->second.id;
					}
				}
			}
		}
	}
}

TextureManager::TextureHandle TextureManager::addDesc(const TextureDesc& desc, TextureHandle::Mode mode) {
	TextureHandle handle;
	handle.mMode = mode;

	// Allocate new texture handle and insert desc.
	if (!mFreeList.empty()) {
		handle = mFreeList.back();
		mFreeList.pop_back();
		getDesc(handle) = desc;
	} else {
		if (mTextureDescs.size() >= mMaxTextureCount) {
			throw std::runtime_error("Out of texture handles");
		}

		handle.id = static_cast<uint32_t>(mTextureDescs.size());
		handle.mMode = mode;
		mTextureDescs.emplace_back(desc);
		
	}

	return handle;
}

TextureManager::TextureDesc& TextureManager::getDesc(const TextureHandle& handle) {
	assert(handle && handle.id < mTextureDescs.size());
	return mTextureDescs[handle.id];
}

}  // namespace Falcor