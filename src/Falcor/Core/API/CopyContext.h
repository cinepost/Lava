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
#ifndef SRC_FALCOR_CORE_API_COPYCONTEXT_H_
#define SRC_FALCOR_CORE_API_COPYCONTEXT_H_

#include "Falcor/Core/API/Handles.h"
#include "Falcor/Core/API/Resource.h"
#include "Falcor/Core/API/ResourceViews.h"
#include "Falcor/Core/API/Fence.h"
#include "Falcor/Core/API/LowLevelContextData.h"
#include "Falcor/Core/API/VirtualTexturePage.h"
#include "Falcor/Core/Macros.h"

#include <memory>
#include <string>
#include <vector>


namespace Falcor {

class Buffer;
class Texture;
class TextureManager;

uint32_t getMipLevelPackedDataSize(const Texture* pTexture, uint32_t w, uint32_t h, uint32_t d, ResourceFormat format);

class FALCOR_API CopyContext {
 public:
    class FALCOR_API ReadTextureTask {
     public:
        using SharedPtr = std::shared_ptr<ReadTextureTask>;
        static SharedPtr create(CopyContext* pCtx, const Texture* pTexture, uint32_t subresourceIndex);
        std::vector<uint8_t> getData() const;
        void getData(uint8_t* textureData) const;
        void getData(std::vector<uint8_t>& textureData) const;
        void getData(void* pData, size_t size) const;

     private:
        ReadTextureTask() = default;
        ref<Fence> mpFence;
        ref<Buffer> mpBuffer;
        CopyContext* mpContext;
        uint32_t mRowCount;
        uint32_t mRowSize;
        uint32_t mActualRowSize;
        uint32_t mDepth;
    };

    /**
     * Constructor.
     * Throws an exception if creation failed.
     * @param[in] pDevice Graphics device.
     * @param[in] pQueue Command queue.
     */
    CopyContext(Device* pDevice, gfx::ICommandQueue* pQueue);
    virtual ~CopyContext();

    ref<Device> getDevice() const;

    /** Flush the command list. This doesn't reset the command allocator, just submits the commands
        \param[in] wait If true, will block execution until the GPU finished processing the commands
    */
    virtual void submit(bool wait = false);

    /** Check if we have pending commands
    */
    bool hasPendingCommands() const { return mCommandsPending; }

    /** Signal the context that we have pending commands. Useful in case you make raw API calls
    */
    void setPendingCommands(bool commandsPending) { mCommandsPending = commandsPending; }

    /**
     * Signal a fence.
     * @param pFence The fence to signal.
     * @param value The value to signal. If Fence::kAuto, the signaled value will be auto-incremented.
     * @return Returns the signaled value.
     */
    uint64_t signal(Fence* pFence, uint64_t value = Fence::kAuto);

    /**
     * Wait for a fence to be signaled on the device.
     * Queues a device-side wait and returns immediately.
     * The device will wait until the fence reaches or exceeds the specified value.
     * @param pFence The fence to wait for.
     * @param value The value to wait for. If Fence::kAuto, wait for the last signaled value.
     */
    void wait(Fence* pFence, uint64_t value = Fence::kAuto);

    /** Insert a resource barrier
        if pViewInfo is nullptr, will transition the entire resource. Otherwise, it will only transition the subresource in the view
        \return true if a barrier commands were recorded for the entire resource-view, otherwise false (for example, when the current resource state is the same as the new state or when only some subresources were transitioned)
    */
    virtual bool resourceBarrier(const Resource* pResource, Resource::State newState, const ResourceViewInfo* pViewInfo = nullptr);

    /** Insert a UAV barrier
    */
    virtual void uavBarrier(const Resource* pResource);

    /** Copy an entire resource
    */
    void copyResource(const Resource* pDst, const Resource* pSrc);
    void copyResource(const Buffer* pDstBuf, const Resource* pSrc);

    /** Copy a subresource
    */
    void copySubresource(const Texture* pDst, uint32_t dstSubresourceIdx, const Texture* pSrc, uint32_t srcSubresourceIdx);

    /** Copy part of a buffer
    */
    void copyBufferRegion(const Buffer* pDst, uint64_t dstOffset, const Buffer* pSrc, uint64_t srcOffset, uint64_t numBytes);

    /** Copy a region of a subresource from one texture to another
        `srcOffset`, `dstOffset` and `size` describe the source and destination regions. For any channel of `extent` that is -1, the source texture dimension will be used
    */
    void copySubresourceRegion(const Texture* pDst, uint32_t dstSubresource, const Texture* pSrc, uint32_t srcSubresource, const uint3& dstOffset = uint3(0), const uint3& srcOffset = uint3(0), const uint3& size = uint3(-1));

    /** Update a texture's subresource data
        `offset` and `size` describe a region to update. For any channel of `extent` that is -1, the texture dimension will be used.
        pData can't be null. The size of the pointed buffer must be equal to a single texel size times the size of the region we are updating
    */
    void updateSubresourceData(const Texture* pDst, uint32_t subresource, const void* pData, const uint3& offset = uint3(0), const uint3& size = uint3(-1));

    /** Update an entire texture
    */
    void updateTextureData(const Texture* pTexture, const void* pData);

    /** Update texture page data
    */
    void updateTexturePage(const VirtualTexturePage* pPage, const void* pData);

    /** Fill sparse texture mip tail data
    */
    void fillMipTail(Texture* pTexture, const void* pData, bool tailDataInOnePage);

    /** Uodate texture image data (used to fill mip tail data)
    */
    void updateMipTailData(const Texture* pTexture, const int3& offset, const uint3& extent, uint8_t mipLevel, const void* pData);
    
    /** Update a buffer
    */
    void updateBuffer(const Buffer* pBuffer, const void* pData, size_t offset = 0, size_t numBytes = 0);

    void readBuffer(const Buffer* pBuffer, void* pData, size_t offset = 0, size_t numBytes = 0);

    template<typename T>
    std::vector<T> readBuffer(const Buffer* pBuffer, size_t firstElement = 0, size_t elementCount = 0);

    /** Read texture data synchronously. Calling this command will flush the pipeline and wait for the GPU to finish execution
    */
    std::vector<uint8_t> readTextureSubresource(const Texture* pTexture, uint32_t subresourceIndex);

    /** Read texture data synchronously. Calling this command will flush the pipeline and wait for the GPU to finish execution
    */
    void readTextureSubresource(const Texture* pTexture, uint32_t subresourceIndex, uint8_t* textureData);
    void readTextureSubresource(const Texture* pTexture, uint32_t subresourceIndex, std::vector<uint8_t>& textureData);

    /** Read texture data Asynchronously
    */
    ReadTextureTask::SharedPtr asyncReadTextureSubresource(const Texture* pTexture, uint32_t subresourceIndex);

    /** Get the low-level context data
    */
    LowLevelContextData* getLowLevelData() const { return mpLowLevelData.get(); }

    /** Bind the descriptor heaps from the device into the command list.
    */
    void bindDescriptorHeaps();

    /**
     * Binds the GPU descriptor pool for passes that need to access descriptors directly from root signatures.
     */
    void bindCustomGPUDescriptorPool();

    /**
     * Unbinds the GPU descriptor pool for passes that need to access descriptors directly from root signatures.
     */
    void unbindCustomGPUDescriptorPool();

 protected:
    bool textureBarrier(const Texture* pTexture, Resource::State newState);
    bool bufferBarrier(const Buffer* pBuffer, Resource::State newState);
    bool subresourceBarriers(const Texture* pTexture, Resource::State newState, const ResourceViewInfo* pViewInfo);
    void apiSubresourceBarrier(const Texture* pTexture, Resource::State newState, Resource::State oldState, uint32_t arraySlice, uint32_t mipLevel);
    void updateTextureSubresources(const Texture* pTexture, uint32_t firstSubresource, uint32_t subresourceCount, const void* pData, const uint3& offset = uint3(0), const uint3& size = uint3(-1));

    /** Update texture page data
    */
    void updateTexturePage(const VirtualTexturePage* pPage, ref<Buffer> pStagingBuffer);

    Device* mpDevice;
    std::unique_ptr<LowLevelContextData> mpLowLevelData;
    bool mCommandsPending = false;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_COPYCONTEXT_H_
