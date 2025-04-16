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
#ifndef SRC_FALCOR_CORE_API_RESOURCEVIEWS_H_
#define SRC_FALCOR_CORE_API_RESOURCEVIEWS_H_

#include <vector>
#include <memory>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/Program/ProgramReflection.h"


namespace Falcor {

class Device;
class Resource;
class Texture;
class Buffer;

using ResourceSharedPtr = std::shared_ptr<Resource>;

struct FALCOR_API ResourceViewInfo {
    ResourceViewInfo() = default;
    ResourceViewInfo(uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        : mostDetailedMip(mostDetailedMip), mipCount(mipCount), firstArraySlice(firstArraySlice), arraySize(arraySize) {}

    ResourceViewInfo(uint64_t offset, uint64_t size) : offset(offset), size(size) {}

    static constexpr uint32_t kMaxPossible = -1;
    static constexpr uint64_t kEntireBuffer = -1;

    // Textures
    uint32_t mostDetailedMip = 0;
    uint32_t mipCount = kMaxPossible;
    uint32_t firstArraySlice = 0;
    uint32_t arraySize = kMaxPossible;

    // Buffers
    uint64_t offset = 0;
    uint64_t size = kEntireBuffer;

    bool operator==(const ResourceViewInfo& other) const {
        return (firstArraySlice == other.firstArraySlice) && (arraySize == other.arraySize) && (mipCount == other.mipCount) &&
               (mostDetailedMip == other.mostDetailedMip) && (offset == other.offset) && (size == other.size);
    }
};

/** Abstracts API resource views.
*/
template<typename ApiHandleType>
class FALCOR_API ResourceView: public std::enable_shared_from_this<ResourceView<ApiHandleType>> {
 public:
    using ApiHandle = ApiHandleType;
    using Dimension = ReflectionResourceType::Dimensions;
    static const uint32_t kMaxPossible = -1;
    virtual ~ResourceView();

    ResourceView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        : mApiHandle(handle), mpDevice(pDevice), mpResource(pResource), mViewInfo(mostDetailedMip, mipCount, firstArraySlice, arraySize) {}

    ResourceView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle, uint64_t offset, uint64_t size)
        : mApiHandle(handle), mpDevice(pDevice), mpResource(pResource), mViewInfo(offset, size) {}

    ResourceView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle)
        : mApiHandle(handle), mpDevice(pDevice), mpResource(pResource) {}

    gfx::IResourceView* getGfxResourceView() const { return mApiHandle; }

    /** Get the raw API handle.
    */
    const ApiHandle& getApiHandle() const { return mApiHandle; }

    /** Get information about the view.
    */
    const ResourceViewInfo& getViewInfo() const { return mViewInfo; }

    /** Get the resource referenced by the view.
    */
    Resource* getResource() const { return mpResource; }
    //ResourceSharedPtr getResource() const { 
    //    assert(!mpResource.expired());
    //    return mpResource.lock(); 
    //}
    // Resource* getResource() const { return mpResource.lock().get(); }

 protected:
    friend class Resource;

    ApiHandle mApiHandle;
    std::shared_ptr<Device> mpDevice;
    Resource* mpResource;
    ResourceViewInfo mViewInfo;
};

template<>
ResourceView<CbvHandle>::~ResourceView<CbvHandle>();

class FALCOR_API ShaderResourceView : public ResourceView<SrvHandle>, public inherit_shared_from_this<ResourceView<SrvHandle>, ReflectionArrayType> {
    public:
        using SharedPtr = std::shared_ptr<ShaderResourceView>;
        using SharedConstPtr = std::shared_ptr<const ShaderResourceView>;

        static SharedPtr create(std::shared_ptr<Device> pDevice, Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize);
        static SharedPtr create(std::shared_ptr<Device> pDevice, Buffer* pBuffer, uint64_t offset, uint64_t size);
        static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);
        static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);

        ShaderResourceView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
            : ResourceView(pDevice, pResource, handle, mostDetailedMip, mipCount, firstArraySlice, arraySize) {}

        ShaderResourceView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle, uint64_t offset, uint64_t size)
            : ResourceView(pDevice, pResource, handle, offset, size) {}

        ShaderResourceView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle)
            : ResourceView(pDevice, pResource, handle) {}
};

class FALCOR_API DepthStencilView : public ResourceView<DsvHandle>, public inherit_shared_from_this<ResourceView<DsvHandle>, ReflectionArrayType> {
 public:
    using SharedPtr = std::shared_ptr<DepthStencilView>;
    using SharedConstPtr = std::shared_ptr<const DepthStencilView>;

    static SharedPtr create(std::shared_ptr<Device> pDevice, Texture* pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);
    static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);

    DepthStencilView(std::shared_ptr<Device> pDevice,Resource* pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}
};

class FALCOR_API UnorderedAccessView : public ResourceView<UavHandle>, public inherit_shared_from_this<ResourceView<UavHandle>, ReflectionArrayType> {
 public:
    using SharedPtr = std::shared_ptr<UnorderedAccessView>;
    using SharedConstPtr = std::shared_ptr<const UnorderedAccessView>;

    static SharedPtr create(std::shared_ptr<Device> pDevice, Texture* pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Buffer* pBuffer, uint64_t offset, uint64_t size);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);

    static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);

    UnorderedAccessView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}

    UnorderedAccessView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle, uint64_t offset, uint64_t size)
        : ResourceView(pDevice, pResource, handle, offset, size) {}
};

class FALCOR_API RenderTargetView : public ResourceView<RtvHandle>, public inherit_shared_from_this<ResourceView<RtvHandle>, ReflectionArrayType> {
 public:
    using SharedPtr = std::shared_ptr<RenderTargetView>;
    using SharedConstPtr = std::shared_ptr<const RenderTargetView>;
    static SharedPtr create(std::shared_ptr<Device> pDevice, Texture* pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);

    static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);

    ~RenderTargetView();

    RenderTargetView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}
};

class FALCOR_API ConstantBufferView : public ResourceView<CbvHandle>, public inherit_shared_from_this<ResourceView<CbvHandle>, ReflectionArrayType> {
 public:
    using SharedPtr = std::shared_ptr<ConstantBufferView>;
    using SharedConstPtr = std::shared_ptr<const ConstantBufferView>;
    static SharedPtr create(std::shared_ptr<Device> pDevice, Buffer* pBuffer);
    static SharedPtr create(std::shared_ptr<Device> pDevice);

    static SharedPtr getNullView(std::shared_ptr<Device> pDevice);

    ConstantBufferView(std::shared_ptr<Device> pDevice, Resource* pResource, ApiHandle handle) : ResourceView(pDevice, pResource, handle, 0, 1, 0, 1) {}
};

struct NullResourceViews {
    std::array<ShaderResourceView::SharedPtr, (size_t)ShaderResourceView::Dimension::Count> srv;
    std::array<UnorderedAccessView::SharedPtr, (size_t)UnorderedAccessView::Dimension::Count> uav;
    std::array<DepthStencilView::SharedPtr, (size_t)DepthStencilView::Dimension::Count> dsv;
    std::array<RenderTargetView::SharedPtr, (size_t)RenderTargetView::Dimension::Count> rtv;
    ConstantBufferView::SharedPtr cbv;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_RESOURCEVIEWS_H_
