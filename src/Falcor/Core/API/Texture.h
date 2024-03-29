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
#ifndef SRC_FALCOR_CORE_API_TEXTURE_H_
#define SRC_FALCOR_CORE_API_TEXTURE_H_

#include <map>
#include <string>
#include <algorithm>
#include <memory>
#include <atomic>
#include <mutex>

#include "Falcor/Core/Framework.h"
#include "Resource.h"
#include "VirtualTexturePage.h"
#include "Falcor/Utils/Image/Bitmap.h"

#if defined(FALCOR_GFX_VK) 
namespace gfx {
	namespace vk {
		class DeviceImpl;
		class ResourceCommandEncoder;
	}
}
#endif

namespace Falcor {

class Engine;
class Sampler;
class Device;
class CopyContext;
class RenderContext;
class TextureManager;
class ResourceManager;
class VirtualTexturePage;

/** Abstracts the API texture objects
*/
class dlldecl Texture : public Resource, public inherit_shared_from_this<Resource, Texture> {
 public:
	using SharedPtr = std::shared_ptr<Texture>;
	using SharedConstPtr = std::shared_ptr<const Texture>;
	using WeakPtr = std::weak_ptr<Texture>;
	using WeakConstPtr = std::weak_ptr<const Texture>;
	using inherit_shared_from_this<Resource, Texture>::shared_from_this;

	struct MipTailInfo {
		bool singleMipTail;
		bool alignedMipSize;
	};

	struct UDIMTileInfo {
		Texture::SharedPtr pTileTexture = nullptr;
		uint32_t u = 0;
		uint32_t v = 0;

		bool operator==(const UDIMTileInfo& other) const { return ((u == other.u) && (v == other.v) && (pTileTexture == other.pTileTexture)); }
	};

	~Texture();

	/** Get a mip-level width
	*/
	inline uint32_t getWidth(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < mMipLevels) ? std::max(1U, mWidth >> mipLevel) : 0; }

	/** Get a mip-level height
	*/
	inline uint32_t getHeight(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < mMipLevels) ? std::max(1U, mHeight >> mipLevel) : 0; }

	/** Get a mip-level depth
	*/
	inline uint32_t getDepth(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < mMipLevels) ? std::max(1U, mDepth >> mipLevel) : 0; }

	/** Get the number of mip-levels
	*/
	inline uint32_t getMipCount() const { return mMipLevels; }

	/** Get the sample count
	*/
	inline uint32_t getSampleCount() const { return mSampleCount; }

	/** Get the array size
	*/
	inline uint32_t getArraySize() const { return mArraySize; }

	/** Get the array index of a subresource
	*/
	inline uint32_t getSubresourceArraySlice(uint32_t subresource) const { return subresource / mMipLevels; }

	/** Get the mip-level of a subresource
	*/
	inline uint32_t getSubresourceMipLevel(uint32_t subresource) const { return subresource % mMipLevels; }

	/** Get the subresource index
	*/
	inline uint32_t getSubresourceIndex(uint32_t arraySlice, uint32_t mipLevel) const { return mipLevel + arraySlice * mMipLevels; }

	/** Get the resource format
	*/
	inline ResourceFormat getFormat() const { return mFormat; }

	/** Create a new texture from an existing API handle.
		\param[in] handle Handle of already allocated resource.
		\param[in] type The type of texture.
		\param[in] width The width of the texture.
		\param[in] height The height of the texture.
		\param[in] depth The depth of the texture.
		\param[in] format The format of the texture.
		\param[in] sampleCount The sample count of the texture.
		\param[in] arraySize The array size of the texture.
		\param[in] mipLevels The number of mip levels.
		\param[in] initState The initial resource state.
		\param[in] bindFlags Texture bind flags. Flags must match the bind flags of the original resource.
		\return A pointer to a new texture, or throws an exception if creation failed.
	*/
	static SharedPtr createFromApiHandle(std::shared_ptr<Device> pDevice, ApiHandle handle, Type type, uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels, State initState, BindFlags bindFlags);

	/** Create a 1D texture.
		\param[in] width The width of the texture.
		\param[in] format The format of the texture.
		\param[in] arraySize The array size of the texture.
		\param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
		\param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
		\param[in] bindFlags The requested bind flags for the resource.
		\return A pointer to a new texture, or throws an exception if creation failed.
	*/
	static SharedPtr create1D(std::shared_ptr<Device> pDevice, uint32_t width, ResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource);

	/** Create a 2D texture.
		\param[in] width The width of the texture.
		\param[in] height The height of the texture.
		\param[in] format The format of the texture.
		\param[in] arraySize The array size of the texture.
		\param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
		\param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
		\param[in] bindFlags The requested bind flags for the resource.
		\return A pointer to a new texture, or throws an exception if creation failed.
	*/
	static SharedPtr create2D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource);

	/** Create a 3D texture.
		\param[in] width The width of the texture.
		\param[in] height The height of the texture.
		\param[in] depth The depth of the texture.
		\param[in] format The format of the texture.
		\param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
		\param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
		\param[in] bindFlags The requested bind flags for the resource.
		\param[in] isSparse If true, the texture is created using sparse texture options supported by the API.
		\return A pointer to a new texture, or throws an exception if creation failed.
	*/
	static SharedPtr create3D(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource, bool sparse = false);

	/** Create a cube texture.
		\param[in] width The width of the texture.
		\param[in] height The height of the texture.
		\param[in] format The format of the texture.
		\param[in] arraySize The array size of the texture.
		\param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
		\param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
		\param[in] bindFlags The requested bind flags for the resource.
		\return A pointer to a new texture, or throws an exception if creation failed.
	*/
	static SharedPtr createCube(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource);

	/** Create a multi-sampled 2D texture.
		\param[in] width The width of the texture.
		\param[in] height The height of the texture.
		\param[in] format The format of the texture.
		\param[in] sampleCount The sample count of the texture.
		\param[in] arraySize The array size of the texture.
		\param[in] bindFlags The requested bind flags for the resource.
		\return A pointer to a new texture, or throws an exception if creation failed.
	*/
	static SharedPtr create2DMS(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize = 1, BindFlags bindFlags = BindFlags::ShaderResource);

	/** Create UDIM pseudo texture.
		This is just a placeholder. No actual data uploaded and no graphics API code executed.
	*/ 
	static SharedPtr createUDIMFromFile(std::shared_ptr<Device> pDevice, const std::string& filename);
	static SharedPtr createUDIMFromFile(std::shared_ptr<Device> pDevice, const fs::path& path);

	/** Create a new texture object from a file.
		\param[in] filename Filename of the image. Can also include a full path or relative path from a data directory.
		\param[in] generateMipLevels Whether the mip-chain should be generated.
		\param[in] loadAsSrgb Load the texture using sRGB format. Only valid for 3 or 4 component textures.
		\param[in] bindFlags The bind flags to create the texture with.
		\return A new texture, or nullptr if the texture failed to load.
	*/
	static SharedPtr createFromFile(std::shared_ptr<Device> pDevice, const std::string& filename, bool generateMipLevels, bool loadAsSrgb, BindFlags bindFlags = BindFlags::ShaderResource);
	static SharedPtr createFromFile(std::shared_ptr<Device> pDevice, const fs::path& path, bool generateMipLevels, bool loadAsSrgb, BindFlags bindFlags = BindFlags::ShaderResource);

	/** Get a shader-resource view for the entire resource
	*/
	ShaderResourceView::SharedPtr getSRV() override;

	/** Get an unordered access view for the entire resource
	*/
	UnorderedAccessView::SharedPtr getUAV() override;

#if FALCOR_ENABLE_CUDA
	/** Get the CUDA device address for this resource.
		\return CUDA device address.
		Throws an exception if the resource is not (or cannot be) shared with CUDA.
	*/
	virtual void* getCUDADeviceAddress() const override;

	/** Get the CUDA device address for a view of this resource.
		Throws an exception if the resource is not (or cannot be) shared with CUDA.
	*/
	virtual void* getCUDADeviceAddress(ResourceViewInfo const& viewInfo) const override;
#endif

	/** Get a shader-resource view.
		\param[in] mostDetailedMip The most detailed mip level of the view
		\param[in] mipCount The number of mip-levels to bind. If this is equal to Texture#kMaxPossible, will create a view ranging from mostDetailedMip to the texture's mip levels count
		\param[in] firstArraySlice The first array slice of the view
		\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
	*/
	ShaderResourceView::SharedPtr getSRV(uint32_t mostDetailedMip, uint32_t mipCount = kMaxPossible, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);
	ShaderResourceView::SharedConstPtr getSRV(uint32_t mostDetailedMip, uint32_t mipCount = kMaxPossible, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible) const;

	/** Get a render-target view.
		\param[in] mipLevel The requested mip-level
		\param[in] firstArraySlice The first array slice of the view
		\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
	*/
	RenderTargetView::SharedPtr getRTV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

	/** Get a depth stencil view.
		\param[in] mipLevel The requested mip-level
		\param[in] firstArraySlice The first array slice of the view
		\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
	*/
	DepthStencilView::SharedPtr getDSV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

	/** Get an unordered access view.
		\param[in] mipLevel The requested mip-level
		\param[in] firstArraySlice The first array slice of the view
		\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
	*/
	UnorderedAccessView::SharedPtr getUAV(uint32_t mipLevel, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

	/** Capture the texture to an image file in asynchronous manner (using Falcor::Threading).
		\param[in] mipLevel Requested mip-level
		\param[in] arraySlice Requested array-slice
		\param[in] filename Name of the file to save.
		\param[in] fileFormat Destination image file format (e.g., PNG, PFM, etc.)
		\param[in] exportFlags Save flags, see Bitmap::ExportFlags
	*/
	void captureToFile(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename, Bitmap::FileFormat format = Bitmap::FileFormat::PngFile, Bitmap::ExportFlags exportFlags = Bitmap::ExportFlags::None);

	/** Capture the texture to an image file in a blocking manner.
		\param[in] mipLevel Requested mip-level
		\param[in] arraySlice Requested array-slice
		\param[in] filename Name of the file to save.
		\param[in] fileFormat Destination image file format (e.g., PNG, PFM, etc.)
		\param[in] exportFlags Save flags, see Bitmap::ExportFlags
	*/
	void captureToFileBlocking(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename, Bitmap::FileFormat format = Bitmap::FileFormat::PngFile, Bitmap::ExportFlags exportFlags = Bitmap::ExportFlags::None);


	/** Read the texture to an array.
		\param[in] mipLevel Requested mip-level
		\param[in] arraySlice Requested array-slice
		\param[out] textureData
		\param[out] resourceFormat Texture data format
		\param[out] channels Texture data channels number
	*/
	void readTextureData(uint32_t mipLevel, uint32_t arraySlice, uint8_t* textureData, ResourceFormat& resourceFormat, uint32_t& channels);
	void readTextureData(uint32_t mipLevel, uint32_t arraySlice, std::vector<uint8_t>& textureData, ResourceFormat& resourceFormat, uint32_t& channels);
	void readTextureData(uint32_t mipLevel, uint32_t arraySlice, uint8_t* textureData);

	void readConvertedTextureData(uint32_t mipLevel, uint32_t arraySlice, uint8_t* textureData, ResourceFormat resourceFormat);

	/** Generates mipmaps for a specified texture object.
	*/
	void generateMips(RenderContext* pContext, bool minMaxMips = false);

	/** In case the texture was loaded from a file, use this to set the file path
	*/
	void setSourceFilename(const std::string& filename) { mSourceFilename = filename; }

	/** In case the texture was loaded from a file, get the source file path
	*/
	const std::string& getSourceFilename() const { return mSourceFilename; }

	/** In case the texture was loaded from a file, use this to set the file path
	*/
	void setSourcePath(const fs::path& path) { mSourceFilename = path.string(); }

	/** In case the texture was loaded from a file, get the source file path
	*/
	fs::path getSourcePath() const { return fs::path(mSourceFilename); }

	/** Returns the total number of texels across all mip levels and array slices.
	*/
	uint64_t getTexelCount() const;

	/** Returns the size of the texture in bytes as allocated in GPU memory.
	*/
	uint64_t getTextureSizeInBytes() const;

	/** Compares the texture description to another texture.
		\return True if all fields (size/format/etc) are identical.
	*/
	bool compareDesc(const Texture* pOther) const;

	// Call before sparse binding to update memory bind list etc.
	void updateSparseBindInfo();

	bool isSparse() const { return mIsSparse; };

	const std::vector<VirtualTexturePage::SharedPtr>& sparseDataPages() { return mSparseDataPages; };

	uint32_t memoryTypeIndex() const { return mMemoryTypeIndex; }

	static uint8_t getMaxMipCount(const uint3& size);

	uint3 sparseDataPageRes() const { return mSparsePageRes; }

	uint32_t sparseDataPagesCount() const { return static_cast<uint32_t>(mSparseDataPages.size()); }

	uint32_t sparseDataBindsCount() const { return mSparseBindsCount; }

	uint32_t getMipTailStart() const;

	const std::array<uint32_t, 16>& getMipBases() const { return mMipBases; }

	bool isUDIMTexture() const { return mIsUDIMTexture; }

	const std::array<UDIMTileInfo, 100>& getUDIMTileInfos() const { return mUDIMTileInfos; }

	void setUDIM_ID(uint16_t id);

	void setVirtualID(uint32_t id);

	uint16_t getUDIM_ID() const { return mUDIM_ID; }

	uint32_t getVirtualID() const { return mVirtualID; }

	bool isSolid() const { return mIsSolid; }

	std::mutex& getMutex() { return mMutex; }

	bool isMipTailFilled() const { return (mIsSparse && mMipTailFilled); }

  private:
  	void addUDIMTileTexture(const UDIMTileInfo& udim_tile_info);
  	bool addTexturePage(uint32_t index, int3 offset, uint3 extent, const uint64_t size, uint32_t memoryTypeBits, const uint32_t mipLevel, uint32_t layer);

  public:
  	Texture(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels, uint32_t sampleCount, ResourceFormat format, Type Type, BindFlags bindFlags);

  protected:
		void apiInit(const void* pData, bool autoGenMips);
		void uploadInitData(const void* pData, bool autoGenMips);
		void setMipTailFilled(bool state) { mMipTailFilled = state; }

		bool mReleaseRtvsAfterGenMips = true;
		std::string mSourceFilename;

		uint32_t mWidth = 0;
		uint32_t mHeight = 0;
		uint32_t mDepth = 0;
		uint32_t mMipLevels = 0;
		uint32_t mSampleCount = 0;
		uint32_t mArraySize = 0;
		ResourceFormat mFormat = ResourceFormat::Unknown;

		mutable std::mutex mMutex;

		std::array<UDIMTileInfo, 100> mUDIMTileInfos;
		bool mIsUDIMTexture = false;
		bool mIsSparse = false;
		bool mIsSolid = false;
		bool mMipTailFilled = false;
		uint16_t mUDIM_ID = 0;
		uint32_t mVirtualID = 0; // Should always start with 1. 0 means non virtual texture.

		uint3 mSparsePageRes = int3(0);
		uint32_t mSparseBindsCount = 0;
		std::atomic<size_t> mSparseResidentMemSize = 0;
		std::array<uint32_t, 16> mMipBases = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

		MipTailInfo mMipTailInfo;
		uint32_t mMipTailStart;                                          // First mip level in mip tail
		uint32_t mMemoryTypeIndex;                                       // @todo: Comment

#if FALCOR_GFX_VK || defined(FALCOR_VK)
		// Vulkan
		VkImage mImage = VK_NULL_HANDLE;
		VkMemoryRequirements mMemRequirements;

		VkBindSparseInfo mBindSparseInfo;                               // Sparse queue binding information
		std::vector<VirtualTexturePage::SharedPtr> mSparseDataPages;    // Contains all virtual pages of the texture
		std::vector<VkSparseImageMemoryBind> mSparseImageMemoryBinds;   // Sparse image memory bindings of all memory-backed virtual tables
		std::vector<VkSparseMemoryBind> mOpaqueMemoryBinds;             // Sparse ópaque memory bindings for the mip tail (if present)
		VkSparseImageMemoryBindInfo mImageMemoryBindInfo;               // Sparse image memory bind info
		VkSparseImageOpaqueMemoryBindInfo mOpaqueMemoryBindInfo;        // Sparse image opaque memory bind info (mip tail)
		VkSparseImageMemoryRequirements mSparseImageMemoryRequirements; // @todo: Comment

		VkSparseImageMemoryBind 				mMipTailimageMemoryBind{};

		//VkSemaphore mBindSparseSemaphore = VK_NULL_HANDLE;
#endif  // FALCOR_GFX_VK
	
		bool mSparseBindDirty = true;

		friend class Device;
		friend class Engine;
		friend class ResourceManager;
		friend class TextureManager;
		friend class CopyContext;
		friend class VirtualTexturePage;
#if defined(FALCOR_GFX_VK)
		friend class gfx::vk::DeviceImpl;
		friend class gfx::vk::ResourceCommandEncoder;
#endif

};

inline std::string to_string(const std::shared_ptr<Texture>& tex) {
	std::string s = "Texture: " + std::to_string(tex->getWidth()) + "x" + std::to_string(tex->getHeight());
	s += " source " + tex->getSourceFilename();
	//s += ",map type " + to_string(buff->getType());
	return s;
}

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_TEXTURE_H_
