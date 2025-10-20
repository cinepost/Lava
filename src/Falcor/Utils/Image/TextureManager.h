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
#ifndef SRC_FALCOR_UTILS_IMAGE_TEXTUREMANAGER_H_
#define SRC_FALCOR_UTILS_IMAGE_TEXTUREMANAGER_H_

#include "Falcor/Core/Object.h"
#include "Falcor/Core/Program/ShaderVar.h"
#include "Falcor/Utils/Image/LTX_Bitmap.h"
#include "Falcor/Utils/ThreadPool.h"

#include "TextureDataCacheLRU.h"
#include "AsyncTextureLoader.h"

#include "Scene/Material/VirtualTextureData.slang"

#include "lava_utils_lib/lru_cache.hpp"

#include <mutex>

namespace Falcor {

class Device;
class Texture;
class VirtualTexturePage;

/** Multi-threaded texture manager.

	This class manages a collection of textures and implements
	asynchronous texture loading. All operations are thread-safe.

	Each managed texture is assigned a unique handle upon loading.
	This handle is used in shader code to reference the given texture
	in the array of GPU texture descriptors.
*/
class FALCOR_API TextureManager {
public:
	using UniquePtr = std::unique_ptr<TextureManager>;
	using TileList = std::vector<std::pair<fs::path, Falcor::uint2>>;

	~TextureManager();

	/** State of a managed texture.
	*/
	enum class TextureState {
		Invalid,        ///< Invalid/unknown texture.
		Referenced,     ///< Texture is referenced, but not yet loaded.
		Converting,     ///< Virtual texture (.ltx) being referenced, but not yet converted. Ongoing on-line conversion.
		Loaded,         ///< Texture has finished loading.
	};

	/** Sparse (virtual) texture info structure.
	*/

//	struct VirtualTextureInfo {
//	};

	/** Handle to a managed texture.
	*/
	struct TextureHandle {

		enum class Mode {
			Uniform,
			Texture,       ///< Normal texture.
			Virtual,       ///< Virtual texture.
			UDIM_Texture,  ///< UDIM texture. No actual data/resource associated.
		};

		uint32_t id = kInvalidID;
		static const uint32_t kInvalidID = std::numeric_limits<uint32_t>::max();

		Mode mMode = Mode::Texture;
		
		inline uint32_t getID() const { return id; }
		inline bool isValid() const { return id != kInvalidID; }
		inline bool isUDIMTexture() const { return mMode == Mode::UDIM_Texture; }
		inline Mode mode() const { return mMode; }

		explicit operator bool() const { return isValid(); }
		bool operator==(const TextureHandle& other) const { return id == other.id; }
	};

	/** Struct describing a managed texture.
	*/
	struct TextureDesc {
		TextureState state = TextureState::Invalid;     ///< Current state of the texture.
		Falcor::SharedPtr<Texture> pTexture;            ///< Valid texture object when state is 'Loaded', or nullptr if loading failed.

		bool isValid() const { return state != TextureState::Invalid; }
	};

	/** Add a texture to the manager.
		If the texture is already managed, its existing handle is returned.
		\param[in] pTexture The texture resource.
		\return Unique handle to the texture.
	*/
	TextureHandle addTexture(const Falcor::SharedPtr<Texture>& pTexture);

	/** Requst loading a texture from file.
		This will add the texture to the set of managed textures. The function returns a handle immediately.
		If asynchronous loading is requested, the texture data will not be available until loading completes.
		The returned handle is valid for the entire lifetime of the texture, until removeTexture() is called.
		\param[in] path File path of the texture. This can be a full path or a relative path from a data directory.
		\param[in] generateMipLevels Whether the full mip-chain should be generated.
		\param[in] loadAsSRGB Load the texture as sRGB format if supported, otherwise linear color.
		\param[in] bindFlags The bind flags for the texture resource.
		\param[in] async Load asynchronously, otherwise the function blocks until the texture data is loaded.
		\return Unique handle to the texture, or an invalid handle if the texture can't be found.
	*/
	bool loadTexture(TextureHandle& handle, const fs::path& path, bool generateMipLevels, bool loadAsSRGB, ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource, bool async = true, const std::string& udimMask = "<UDIM>", bool loadAsSparse = false);

	Falcor::SharedPtr<Texture> loadTexture(const fs::path& path, bool generateMipLevels, bool loadAsSRGB, ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource, const std::string& udimMask = "<UDIM>", bool loadAsSparse = false);

	Falcor::SharedPtr<Texture> loadSparseTexture(const fs::path& path, bool generateMipLevels, bool loadAsSRGB, ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource);

	/** Wait for a requested texture to load.
		If the handle is valid, the call blocks until the texture is loaded (or failed to load).
		\param[in] handle Texture handle.
	*/
	void waitForTextureLoading(const TextureHandle& handle);

	/** Waits for all currently requested textures to be loaded.
	*/
	void waitForAllTexturesLoading();

	/** Remove a texture.
		\param[in] handle Texture handle.
	*/
	void removeTexture(const TextureHandle& handle);

	/** Get a loaded texture. Call getTextureDesc() for more info.
		\param[in] handle Texture handle.
		\return Texture if loaded, or nullptr if handle doesn't exist or texture isn't yet loaded.
	*/
	Falcor::SharedPtr<Texture> getTexture(const TextureHandle& handle) const { return getTextureDesc(handle).pTexture; }

	/** Get a texture desc.
		\param[in] handle Texture handle.
		\return Texture desc, or invalid desc if handle is invalid.
	*/
	TextureDesc getTextureDesc(const TextureHandle& handle) const;

	/** Get texture desc count.
		\return Number of texture descs.
	*/
	size_t getTextureDescCount() const;


	/** Get UDIM texture desc count.
	  \return Number of UDIM texture descs.
	*/

	size_t getUDIMTextureTilesCount() const { return mUDIMTextureTilesCount; }

	size_t getUDIMTexturesCount() const { return mUDIMTexturesCount; }

	bool hasUDIMTextures() const { return mHasUDIMTextures; };

	bool hasSparseTextures() const { return mHasSparseTextures; };

	/** Bind all textures into a shader var.
		The shader var should refer to a Texture2D descriptor array of fixed size.
		The array must be large enough, otherwise an exception is thrown.
		This restriction will go away when unbounded descriptor arrays are supported (see #1321).
		\param[in] var Shader var for descriptor array.
		\param[in] descCount Size of descriptor array.
	*/
	void setShaderData(const ShaderVar& var, const size_t descCount) const;
	void setShaderData(const ShaderVar& var, const std::vector<Falcor::SharedPtr<Texture>>& textures) const;

	void setExtendedTexturesShaderData(const ShaderVar& var, const size_t descCount);

	void setVirtualTexturesShaderData(const ShaderVar& var, const ShaderVar& pagesBufferVar, const size_t descCount);

	void setUDIMTableShaderData(const ShaderVar& var, size_t descCount);

	void finalize();

	void loadPages(const Falcor::SharedPtr<Texture>& pTexture, const std::vector<uint32_t>& pageIds);
	void loadPagesAsync(const std::vector<std::pair<Falcor::SharedPtr<Texture>, std::vector<uint32_t>>>& texturesToPageIDsList);

	void updateSparseBindInfo();

	bool getTextureHandle(const Texture* pTexture, TextureHandle& handle) const;

	Falcor::SharedPtr<Buffer> getPagesResidencyBuffer() { return mpVirtualPagesResidencyDataBuffer; }
	Falcor::SharedPtr<const Buffer> getPagesResidencyBuffer() const { return mpVirtualPagesResidencyDataBuffer; }

	Falcor::SharedPtr<Buffer> getVirtualTexturesDataBuffer() { return mpVirtualTexturesDataBuffer; }

	size_t getVirtualTexturePagesStartIndex(const Texture* pTexture);

	const std::map<const Texture*, size_t>& getVirtualPagesStartMap() const { return mVirtualPagesStartMap;}

private:
	TextureManager(Falcor::SharedPtr<Device> pDevice, size_t maxTextureCount, size_t threadCount);

	/** Builds data structures needed for sparse residency management.
	*/
	void buildSparseResidencyData();

	/** Key to uniquely identify a managed texture.
	*/
	struct TextureKey {
		fs::path fullPath;
		bool generateMipLevels;
		bool loadAsSRGB;
		ResourceBindFlags bindFlags;

		TextureKey(const fs::path& path, bool mips, bool srgb, ResourceBindFlags flags)
			: fullPath(path), generateMipLevels(mips), loadAsSRGB(srgb), bindFlags(flags)
		{}

		bool operator<(const TextureKey& rhs) const {
			if (fullPath != rhs.fullPath) return fullPath < rhs.fullPath;
			else if (generateMipLevels != rhs.generateMipLevels) return generateMipLevels < rhs.generateMipLevels;
			else if (loadAsSRGB != rhs.loadAsSRGB) return loadAsSRGB < rhs.loadAsSRGB;
			else return bindFlags < rhs.bindFlags;
		}
	};

	TextureHandle addDesc(const TextureDesc& desc, TextureHandle::Mode mode = TextureHandle::Mode::Texture);
	TextureDesc& getDesc(const TextureHandle& handle);

	Device* mpDevice;

	lava::ut::data::LRUCache<uint32_t, VirtualTexturePage::PageData>::UniquePtr mpPageDataCache = nullptr;

	std::vector<std::pair<VirtualTexturePage*, VirtualTexturePage::PageData>> mSimplePagesDataCache;
	std::vector<std::pair<Texture*, VirtualTexturePage::PageData>> mSimpleTextureTailDataCache;

	mutable std::mutex mMutex;                                  ///< Mutex for synchronizing access to shared resources.
	mutable std::mutex mPageMutex;                              ///< Mutex for synchronizing texture page updates.
	std::condition_variable mCondition;                         ///< Condition variable to wait on for loading to finish.

	// Internal state. Do not access outside of critical section.
	BS::multi_future<Texture*> mTextureLoadingTasks;

	std::vector<TextureDesc> mTextureDescs;                     ///< Array of all texture descs, indexed by handle ID.
	std::vector<TextureHandle> mFreeList;                       ///< List of unused handles.
	std::map<TextureKey, TextureHandle> mKeyToHandle;           ///< Map from texture key to handle.
	std::map<const Texture*, TextureHandle> mTextureToHandle;   ///< Map from texture ptr to handle.

	Falcor::SharedPtr<Buffer> mpExtendedTexturesDataBuffer;

	std::vector<VirtualTextureData> mVirtualTexturesData;
	std::vector<uint8_t> mVirtualPagesData;
	std::map<const Texture*, size_t> mVirtualPagesStartMap;

	Falcor::SharedPtr<Buffer> mpVirtualTexturesDataBuffer;
	Falcor::SharedPtr<Buffer> mpVirtualPagesResidencyDataBuffer;

	Falcor::SharedPtr<Buffer> mpUDIMTextureTilesTableBuffer;

	bool mSparseTexturesEnabled = false;
	bool mHasSparseTextures = false;
	bool mHasUDIMTextures = false;
	bool mDirty = true;
	bool mDirtySparseResidency = true;

	AsyncTextureLoader mAsyncTextureLoader;                     ///< Utility for asynchronous texture loading.
	size_t mLoadRequestsInProgress = 0;                         ///< Number of load requests currently in progress.
	size_t mUDIMTextureTilesCount = 0;                          ///< Number of managed UDIM tile textures
	size_t mUDIMTexturesCount = 0;

	std::atomic<uint32_t> mSparseTexturesCount = 0;

	const size_t mMaxTextureCount;                              ///< Maximum number of textures that can be simultaneously managed.

	uint32_t mMaxCPUSparseTexturesMemoryCap = 0;                ///< Maximum memory cap used for storing virtual textures page data
	uint32_t mMaxGPUSparseTexturesMemoryCap = 0;								///< Maximum memory cap used for storing virtual textures page data

	Falcor::SharedPtr<Texture> mNullTexture;

	std::map<uint32_t, LTX_Bitmap::SharedConstPtr> 	  mTextureLTXBitmapsMap;
	std::vector<std::shared_ptr<VirtualTexturePage>>  mSparseDataPages;

	friend class Device;
};

inline std::string to_string(TextureManager::TextureHandle::Mode mode) {
#define mode_2_string(a) case TextureManager::TextureHandle::Mode::a: return #a;
  switch (mode) {
  		mode_2_string(Uniform);
      mode_2_string(Texture);
      mode_2_string(Virtual);
      mode_2_string(UDIM_Texture);
    default:
      assert(false);
      return "Unknown TextureHandle::Mode";
  }
#undef mode_2_string
}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_IMAGE_TEXTUREMANAGER_H_
