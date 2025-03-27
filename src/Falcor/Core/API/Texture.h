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

#include "Falcor/Core/API/Handles.h"
#include "Falcor/Core/API/Formats.h"
#include "Falcor/Core/API/Resource.h"
#include "Falcor/Core/API/ResourceViews.h"
#include "Falcor/Core/API/VirtualTexturePage.h"
#include "Falcor/Core/Macros.h"
#include "Falcor/Utils/Image/Bitmap.h"

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;

#include <fstd/span.h>


namespace gfx {
	namespace vk {
		class DeviceImpl;
		class ResourceCommandEncoder;
	}
}

namespace Falcor {

class Sampler;
class Device;
class CopyContext;
class RenderContext;
class TextureManager;
class ResourceManager;
class VirtualTexturePage;

/** Abstracts the API texture objects
*/
class FALCOR_API Texture : public Resource {
    FALCOR_OBJECT(Texture)
	public:
		struct SubresourceLayout {
	        /// Size of a single row in bytes (unaligned).
	        size_t rowSize;
	        /// Size of a single row in bytes (aligned to device texture alignment).
	        size_t rowSizeAligned;
	        /// Number of rows.
	        size_t rowCount;
	        /// Number of depth slices.
	        size_t depth;

	        /// Get the total size of the subresource in bytes (unaligned).
	        size_t getTotalByteSize() const { return rowSize * rowCount * depth; }

	        /// Get the total size of the subresource in bytes (aligned to device texture alignment).
	        size_t getTotalByteSizeAligned() const { return rowSizeAligned * rowCount * depth; }
	    };

	    Texture(
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
	        const void* pInitData
	    );

	    Texture(
	        ref<Device> pDevice,
	        gfx::ITextureResource* pResource,
	        Type type,
	        ResourceFormat format,
	        uint32_t width,
	        uint32_t height,
	        uint32_t depth,
	        uint32_t arraySize,
	        uint32_t mipLevels,
	        uint32_t sampleCount,
	        ResourceBindFlags bindFlags,
	        Resource::State initState
	    );

		struct MipTailInfo {
			bool singleMipTail;
			bool alignedMipSize;
		};

		struct UDIMTileInfo {
			ref<Texture> pTileTexture = nullptr;
			uint32_t u = 0;
			uint32_t v = 0;

			bool operator==(const UDIMTileInfo& other) const { return ((u == other.u) && (v == other.v) && (pTileTexture == other.pTileTexture)); }
		};

		~Texture();

		/** Get a mip-level width
		*/
		uint32_t getWidth(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < mMipLevels) ? std::max(1U, mWidth >> mipLevel) : 0; }

		/** Get a mip-level height
		*/
		uint32_t getHeight(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < mMipLevels) ? std::max(1U, mHeight >> mipLevel) : 0; }

		/** Get a mip-level depth
		*/
		uint32_t getDepth(uint32_t mipLevel = 0) const { return (mipLevel == 0) || (mipLevel < mMipLevels) ? std::max(1U, mDepth >> mipLevel) : 0; }

		/** Get the number of mip-levels
		*/
		uint32_t getMipCount() const { return mMipLevels; }

		/** Get the sample count
		*/
		uint32_t getSampleCount() const { return mSampleCount; }

		/** Get the array size
		*/
		uint32_t getArraySize() const { return mArraySize; }

		/** Get the array index of a subresource
		*/
		uint32_t getSubresourceArraySlice(uint32_t subresource) const { return subresource / mMipLevels; }

		/** Get the mip-level of a subresource
		*/
		uint32_t getSubresourceMipLevel(uint32_t subresource) const { return subresource % mMipLevels; }

		/** Get the subresource index
		*/
		uint32_t getSubresourceIndex(uint32_t arraySlice, uint32_t mipLevel) const { return mipLevel + arraySlice * mMipLevels; }

		/** Get the resource format
		*/
		ResourceFormat getFormat() const { return mFormat; }

	    /**
	     * Create a new texture object with mips specified explicitly from individual files.
	     * @param[in] paths List of full paths of all mips, starting from mip0.
	     * @param[in] loadAsSrgb Load the texture using sRGB format. Only valid for 3 or 4 component textures.
	     * @param[in] bindFlags The bind flags to create the texture with.
	     * @param[in] importFlags Optional flags for the file import.
	     * @return A new texture, or nullptr if the texture failed to load.
	     */
	    static ref<Texture> createMippedFromFiles(
	      ref<Device> pDevice,
	      fstd::span<const fs::path> paths,
	      bool loadAsSrgb,
	      ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource,
	      Bitmap::ImportFlags importFlags = Bitmap::ImportFlags::None
	    );

	    /**
	     * Create a new texture object from a file.
	     * @param[in] path File path of the image (absolute or relative to working directory).
	     * @param[in] generateMipLevels Whether the mip-chain should be generated.
	     * @param[in] loadAsSrgb Load the texture using sRGB format. Only valid for 3 or 4 component textures.
	     * @param[in] bindFlags The bind flags to create the texture with.
	     * @param[in] importFlags Optional flags for the file import.
	     * @return A new texture, or nullptr if the texture failed to load.
	     */
	    static ref<Texture> createFromFile(
	      ref<Device> pDevice,
	      const fs::path& path,
	      bool generateMipLevels,
	      bool loadAsSrgb,
	      ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource,
	      Bitmap::ImportFlags importFlags = Bitmap::ImportFlags::None
	    );

		/** Create a 1D texture.
			\param[in] width The width of the texture.
			\param[in] format The format of the texture.
			\param[in] arraySize The array size of the texture.
			\param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
			\param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
			\param[in] bindFlags The requested bind flags for the resource.
			\return A pointer to a new texture, or throws an exception if creation failed.
		*/
		static ref<Texture> create1D(
			ref<Device> pDevice, 
			uint32_t width, 
			ResourceFormat format, 
			uint32_t arraySize = 1, 
			uint32_t mipLevels = kMaxPossible, 
			const void* pInitData = nullptr, 
			ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource);

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
		static ref<Texture> create2D(
			ref<Device> pDevice, 
			uint32_t width, 
			uint32_t height, 
			ResourceFormat format, 
			uint32_t arraySize = 1, 
			uint32_t mipLevels = kMaxPossible, 
			const void* pInitData = nullptr, 
			ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource);

		/** Create a 3D texture.
			\param[in] width The width of the texture.
			\param[in] height The height of the texture.
			\param[in] depth The depth of the texture.
			\param[in] format The format of the texture.
			\param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
			\param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
			\param[in] bindFlags The requested bind flags for the resource.
			\return A pointer to a new texture, or throws an exception if creation failed.
		*/
		static ref<Texture> create3D(
			ref<Device> pDevice, 
			uint32_t width, 
			uint32_t height, 
			uint32_t depth, 
			ResourceFormat format, 
			uint32_t mipLevels = kMaxPossible, 
			const void* pInitData = nullptr, 
			ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource);

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
		static ref<Texture> createCube(
			ref<Device> pDevice, 
			uint32_t width, 
			uint32_t height, 
			ResourceFormat format, 
			uint32_t arraySize = 1, 
			uint32_t mipLevels = kMaxPossible, 
			const void* pInitData = nullptr, 
			ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource);

		/** Create a multi-sampled 2D texture.
			\param[in] width The width of the texture.
			\param[in] height The height of the texture.
			\param[in] format The format of the texture.
			\param[in] sampleCount The sample count of the texture.
			\param[in] arraySize The array size of the texture.
			\param[in] bindFlags The requested bind flags for the resource.
			\return A pointer to a new texture, or throws an exception if creation failed.
		*/
		static ref<Texture> create2DMS(
			ref<Device> pDevice, 
			uint32_t width, 
			uint32_t height, 
			ResourceFormat format, 
			uint32_t sampleCount, 
			uint32_t arraySize = 1, 
			ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource);

		/**
	     * Create a new texture from an resource.
	     * @param[in] pResource Already allocated resource.
	     * @param[in] type The type of texture.
	     * @param[in] format The format of the texture.
	     * @param[in] width The width of the texture.
	     * @param[in] height The height of the texture.
	     * @param[in] depth The depth of the texture.
	     * @param[in] arraySize The array size of the texture.
	     * @param[in] mipLevels The number of mip levels.
	     * @param[in] sampleCount The sample count of the texture.
	     * @param[in] bindFlags Texture bind flags. Flags must match the bind flags of the original resource.
	     * @param[in] initState The initial resource state.
	     * @return A pointer to a new texture, or throws an exception if creation failed.
	     */
	    static ref<Texture> createFromResource(
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
	        Resource::State initState
	    );

		/** Create UDIM pseudo texture.
			This is just a placeholder. No actual data uploaded and no graphics API code executed.
		*/ 
		static ref<Texture> createUDIMFromFile(ref<Device> pDevice, const fs::path& path);

		gfx::ITextureResource* getGfxTextureResource() const { return mGfxTextureResource; }

    	virtual gfx::IResource* getGfxResource() const override;

		/** Get a shader-resource view for the entire resource
		*/
		virtual ref<ShaderResourceView> getSRV() override;

		/** Get an unordered access view for the entire resource
		*/
		virtual ref<UnorderedAccessView> getUAV() override;

		/** Get a shader-resource view.
			\param[in] mostDetailedMip The most detailed mip level of the view
			\param[in] mipCount The number of mip-levels to bind. If this is equal to Texture#kMaxPossible, will create a view ranging from mostDetailedMip to the texture's mip levels count
			\param[in] firstArraySlice The first array slice of the view
			\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
		*/
		ref<ShaderResourceView> getSRV(uint32_t mostDetailedMip, uint32_t mipCount = kMaxPossible, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);
		
		/** Get a render-target view.
			\param[in] mipLevel The requested mip-level
			\param[in] firstArraySlice The first array slice of the view
			\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
		*/
		ref<RenderTargetView> getRTV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

		/** Get a depth stencil view.
			\param[in] mipLevel The requested mip-level
			\param[in] firstArraySlice The first array slice of the view
			\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
		*/
		ref<DepthStencilView> getDSV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

		/** Get an unordered access view.
			\param[in] mipLevel The requested mip-level
			\param[in] firstArraySlice The first array slice of the view
			\param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
		*/
		ref<UnorderedAccessView> getUAV(uint32_t mipLevel, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

	    /**
	     * Get the data layout of a subresource.
	     * @param[in] subresource The subresource index.
	     */
	    SubresourceLayout getSubresourceLayout(uint32_t subresource) const;

	    /**
	     * Set the data of a subresource.
	     * @param[in] subresource The subresource index.
	     * @param[in] pData The data to write.
	     * @param[in] size The size of the data (must match the actual subresource size).
	     */
	    void setSubresourceBlob(uint32_t subresource, const void* pData, size_t size);

	    /**
	     * Get the data of a subresource.
	     * @param[in] subresource The subresource index.
	     * @param[in] pData The data buffer to read to.
	     * @param[in] size The size of the data (must match the actual subresource size).
	     */
	    void getSubresourceBlob(uint32_t subresource, void* pData, size_t size) const;

		/** Capture the texture to an image file in asynchronous manner (using Falcor::Threading).
			\param[in] mipLevel Requested mip-level
			\param[in] arraySlice Requested array-slice
			\param[in] filename Name of the file to save.
			\param[in] fileFormat Destination image file format (e.g., PNG, PFM, etc.)
			\param[in] exportFlags Save flags, see Bitmap::ExportFlags
		*/
		void captureToFile(uint32_t mipLevel,
	        uint32_t arraySlice,
	        const fs::path& path,
	        Bitmap::FileFormat format = Bitmap::FileFormat::PngFile,
	        Bitmap::ExportFlags exportFlags = Bitmap::ExportFlags::None,
	        bool async = true
	    );

	    void captureToFileBlocking(uint32_t mipLevel,
	        uint32_t arraySlice,
	        const fs::path& path,
	        Bitmap::FileFormat format = Bitmap::FileFormat::PngFile,
	        Bitmap::ExportFlags exportFlags = Bitmap::ExportFlags::None
	    );

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
		void setSourcePath(const fs::path& path) { mSourcePath = path; }

		/** In case the texture was loaded from a file, get the source file path
		*/
		fs::path getSourcePath() const { return mSourcePath; }

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

		const std::vector<ref<VirtualTexturePage>>& sparseDataPages() { return mSparseDataPages; };

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

  	protected:
		void uploadInitData(RenderContext* pRenderContext, const void* pData, bool autoGenMips);
		void setMipTailFilled(bool state) { mMipTailFilled = state; }

		Slang::ComPtr<gfx::ITextureResource> mGfxTextureResource;

		bool mReleaseRtvsAfterGenMips = true;
		fs::path mSourcePath;

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

		VkImage mImage = VK_NULL_HANDLE;
		VkMemoryRequirements mMemRequirements;

		VkBindSparseInfo mBindSparseInfo;                               // Sparse queue binding information
		std::vector<ref<VirtualTexturePage>> mSparseDataPages;    		// Contains all virtual pages of the texture
		std::vector<VkSparseImageMemoryBind> mSparseImageMemoryBinds;   // Sparse image memory bindings of all memory-backed virtual tables
		std::vector<VkSparseMemoryBind> mOpaqueMemoryBinds;             // Sparse ópaque memory bindings for the mip tail (if present)
		VkSparseImageMemoryBindInfo mImageMemoryBindInfo;               // Sparse image memory bind info
		VkSparseImageOpaqueMemoryBindInfo mOpaqueMemoryBindInfo;        // Sparse image opaque memory bind info (mip tail)
		VkSparseImageMemoryRequirements mSparseImageMemoryRequirements; // @todo: Comment

		VkSparseImageMemoryBind 				mMipTailimageMemoryBind{};

		//VkSemaphore mBindSparseSemaphore = VK_NULL_HANDLE;
	
		bool mSparseBindDirty = true;

		friend class Device;
		friend class ResourceManager;
		friend class TextureManager;
		friend class CopyContext;
		friend class VirtualTexturePage;
		friend class gfx::vk::DeviceImpl;
		friend class gfx::vk::ResourceCommandEncoder;

};

inline std::string to_string(const ref<Texture>& pTex) {
	std::string s = "Texture: " + std::to_string(pTex->getWidth()) + "x" + std::to_string(pTex->getHeight());
	s += " source " + pTex->getSourcePath().string();
	return s;
}

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_TEXTURE_H_
