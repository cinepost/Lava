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

using ResourceWeakPtr = std::weak_ptr<Resource>;
using ResourceSharedPtr = std::shared_ptr<Resource>;
using ConstTextureSharedPtrRef = const std::shared_ptr<Texture>&;
using ConstBufferSharedPtrRef = const std::shared_ptr<Buffer>&;

struct dlldecl ResourceViewInfo {
    ResourceViewInfo() = default;
    ResourceViewInfo(uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        : mostDetailedMip(mostDetailedMip), mipCount(mipCount), firstArraySlice(firstArraySlice), arraySize(arraySize) {}

    ResourceViewInfo(uint32_t firstElement, uint32_t elementCount)
        : firstElement(firstElement), elementCount(elementCount) {}

    static const uint32_t kMaxPossible = -1;

    // Textures
    uint32_t mostDetailedMip = 0;
    uint32_t mipCount = kMaxPossible;
    uint32_t firstArraySlice = 0;
    uint32_t arraySize = kMaxPossible;

    // Buffers
    uint32_t firstElement = 0;
    uint32_t elementCount = kMaxPossible;

    bool operator==(const ResourceViewInfo& other) const {
        return (firstArraySlice == other.firstArraySlice)
            && (arraySize == other.arraySize)
            && (mipCount == other.mipCount)
            && (mostDetailedMip == other.mostDetailedMip)
            && (firstElement == other.firstElement)
            && (elementCount == other.elementCount);
    }
};

/** Abstracts API resource views.
*/
template<typename ApiHandleType>
class dlldecl ResourceView {
 public:
    using ApiHandle = ApiHandleType;
    using Dimension = ReflectionResourceType::Dimensions;
    static const uint32_t kMaxPossible = -1;
    virtual ~ResourceView();

    ResourceView(std::shared_ptr<Device> pDevice, ResourceWeakPtr& pResource, ApiHandle handle, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        : mApiHandle(handle), mpDevice(pDevice), mpResource(pResource), mViewInfo(mostDetailedMip, mipCount, firstArraySlice, arraySize) {}

    ResourceView(std::shared_ptr<Device> pDevice, ResourceWeakPtr& pResource, ApiHandle handle, uint32_t firstElement, uint32_t elementCount)
        : mApiHandle(handle), mpDevice(pDevice), mpResource(pResource), mViewInfo(firstElement, elementCount) {}

    ResourceView(std::shared_ptr<Device> pDevice, ResourceWeakPtr& pResource, ApiHandle handle)
        : mApiHandle(handle), mpDevice(pDevice), mpResource(pResource) {}

    /** Get the raw API handle.
    */
    const ApiHandle& getApiHandle() const { return mApiHandle; }

    /** Get information about the view.
    */
    const ResourceViewInfo& getViewInfo() const { return mViewInfo; }

    /** Get the resource referenced by the view.
    */
    ResourceSharedPtr getResource() const { return mpResource.lock(); }

#if FALCOR_ENABLE_CUDA
    /** Get the CUDA device address for this view.
    */
    void* getCUDADeviceAddress() const
    {
        return mpResource.lock()->getCUDADeviceAddress(mViewInfo);
    }
#endif


 protected:
    ApiHandle mApiHandle;
    std::shared_ptr<Device> mpDevice;
    ResourceWeakPtr mpResource;
    ResourceViewInfo mViewInfo;
};

template<>
ResourceView<CbvHandle>::~ResourceView<CbvHandle>();

//template<>
//ResourceView<CbvHandle>::~ResourceView<CbvHandle>() {};

class dlldecl ShaderResourceView : public ResourceView<SrvHandle> {
  public:
    using SharedPtr = std::shared_ptr<ShaderResourceView>;
    using SharedConstPtr = std::shared_ptr<const ShaderResourceView>;

    static SharedPtr create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(std::shared_ptr<Device> pDevice, ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);

#ifdef FALCOR_D3D12
    static SharedPtr createViewForAccelerationStructure(ConstBufferSharedPtrRef pBuffer);
#endif

    static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);
    //static SharedPtr getNullBufferView(std::shared_ptr<Device> pDevice);
    //static SharedPtr getNullTypedBufferView(std::shared_ptr<Device> pDevice);

    /** Get the D3D12 CPU descriptor handle representing this resource view.
        Valid only when D3D12 is the underlying API.
    */
    D3D12DescriptorCpuHandle getD3D12CpuHeapHandle() const;

private:

    ShaderResourceView(std::shared_ptr<Device> pDevice, ResourceWeakPtr pResource, ApiHandle handle, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        : ResourceView(pDevice, pResource, handle, mostDetailedMip, mipCount, firstArraySlice, arraySize) {}

    ShaderResourceView(std::shared_ptr<Device> pDevice, ResourceWeakPtr pResource, ApiHandle handle, uint32_t firstElement, uint32_t elementCount)
        : ResourceView(pDevice, pResource, handle, firstElement, elementCount) {}

    ShaderResourceView(std::shared_ptr<Device> pDevice, ResourceWeakPtr pResource, ApiHandle handle)
        : ResourceView(pDevice, pResource, handle) {}
};

class dlldecl DepthStencilView : public ResourceView<DsvHandle> {
 public:
    using SharedPtr = std::shared_ptr<DepthStencilView>;
    using SharedConstPtr = std::shared_ptr<const DepthStencilView>;

    static SharedPtr create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);
    static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);
    
    /** Get the D3D12 CPU descriptor handle representing this resource view.
        Valid only when D3D12 is the underlying API.
    */
    D3D12DescriptorCpuHandle getD3D12CpuHeapHandle() const;
 private:
    DepthStencilView(std::shared_ptr<Device> pDevice,ResourceWeakPtr pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}
};

class dlldecl UnorderedAccessView : public ResourceView<UavHandle> {
 public:
    using SharedPtr = std::shared_ptr<UnorderedAccessView>;
    using SharedConstPtr = std::shared_ptr<const UnorderedAccessView>;

    static SharedPtr create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(std::shared_ptr<Device> pDevice, ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);

    static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);
    //static SharedPtr getNullBufferView(std::shared_ptr<Device> pDevice);
    //static SharedPtr getNullTypedBufferView(std::shared_ptr<Device> pDevice);

    /** Get the D3D12 CPU descriptor handle representing this resource view.
        Valid only when D3D12 is the underlying API.
    */
    D3D12DescriptorCpuHandle getD3D12CpuHeapHandle() const;

 private:
    UnorderedAccessView(std::shared_ptr<Device> pDevice, ResourceWeakPtr pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}

    UnorderedAccessView(std::shared_ptr<Device> pDevice, ResourceWeakPtr pResource, ApiHandle handle, uint32_t firstElement, uint32_t elementCount)
        : ResourceView(pDevice, pResource, handle, firstElement, elementCount) {}
};

class dlldecl RenderTargetView : public ResourceView<RtvHandle> {
 public:
    using SharedPtr = std::shared_ptr<RenderTargetView>;
    using SharedConstPtr = std::shared_ptr<const RenderTargetView>;
    static SharedPtr create(std::shared_ptr<Device> pDevice, ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(std::shared_ptr<Device> pDevice, Dimension dimension);

    static SharedPtr getNullView(std::shared_ptr<Device> pDevice, Dimension dimension);

    /** Get the D3D12 CPU descriptor handle representing this resource view.
        Valid only when D3D12 is the underlying API.
    */
    D3D12DescriptorCpuHandle getD3D12CpuHeapHandle() const;

    ~RenderTargetView();

 private:
    RenderTargetView(std::shared_ptr<Device> pDevice, ResourceWeakPtr pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}
};

class dlldecl ConstantBufferView : public ResourceView<CbvHandle> {
 public:
    using SharedPtr = std::shared_ptr<ConstantBufferView>;
    using SharedConstPtr = std::shared_ptr<const ConstantBufferView>;
    static SharedPtr create(std::shared_ptr<Device> pDevice, ConstBufferSharedPtrRef pBuffer);
    static SharedPtr create(std::shared_ptr<Device> pDevice);

    static SharedPtr getNullView(std::shared_ptr<Device> pDevice);

    /** Get the D3D12 CPU descriptor handle representing this resource view.
        Valid only when D3D12 is the underlying API.
    */
    D3D12DescriptorCpuHandle getD3D12CpuHeapHandle() const;

 private:
    ConstantBufferView(std::shared_ptr<Device> pDevice, ResourceWeakPtr pResource, ApiHandle handle) : ResourceView(pDevice, pResource, handle, 0, 1, 0, 1) {}
};

//template<>
//ResourceView<CbvHandle>::~ResourceView<CbvHandle>() {};

    //template<typename T>
    //ResourceView<T>::~ResourceView()
    //{
    //    mpDevice->releaseResource(mApiHandle);
    //}

/*
struct NullResourceViews {
    ShaderResourceView::SharedPtr  srv;
    ConstantBufferView::SharedPtr  cbv;
    RenderTargetView::SharedPtr    rtv;
    UnorderedAccessView::SharedPtr uav;
    DepthStencilView::SharedPtr    dsv;
};

struct NullResourceBufferViews {
    ShaderResourceView::SharedPtr  srv;
    UnorderedAccessView::SharedPtr uav;
};
*/

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_RESOURCEVIEWS_H_
