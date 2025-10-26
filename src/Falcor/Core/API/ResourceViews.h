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
#include "Falcor/Core/Object.h"
#include "Falcor/Core/Program/ProgramReflection.h"

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include "gfx_lib/slang-gfx.h"


namespace Falcor {

class Device;
class Resource;
class Texture;
class Buffer;

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
class FALCOR_API ResourceView: public Object {
    FALCOR_OBJECT(ResourceView)
 public:
    using Dimension = ReflectionResourceType::Dimensions;
    static const uint32_t kMaxPossible = -1;
    static constexpr uint64_t kEntireBuffer = -1;
    virtual ~ResourceView();

    ResourceView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        : mGfxResourceView(gfxResourceView), mpDevice(pDevice), mpResource(pResource), mViewInfo(mostDetailedMip, mipCount, firstArraySlice, arraySize) {}

    ResourceView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint64_t offset, uint64_t size)
        : mGfxResourceView(gfxResourceView), mpDevice(pDevice), mpResource(pResource), mViewInfo(offset, size) {}

    ResourceView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView)
        : mGfxResourceView(gfxResourceView), mpDevice(pDevice), mpResource(pResource) {}

    gfx::IResourceView* getGfxResourceView() const { return mGfxResourceView; }

    /** Get information about the view.
    */
    const ResourceViewInfo& getViewInfo() const { return mViewInfo; }

    /** Get the resource referenced by the view.
    */
    Resource* getResource() const { return mpResource; }


 protected:
    friend class Resource;

    void invalidate();

    Device* mpDevice;
    Slang::ComPtr<gfx::IResourceView> mGfxResourceView;
    ResourceViewInfo mViewInfo;
    Resource* mpResource;
};

class FALCOR_API ShaderResourceView : public ResourceView {
        FALCOR_OBJECT(ShaderResourceView)
    public:
        static SharedPtr create(Device* pDevice, Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize);
        static SharedPtr create(Device* pDevice, Buffer* pBuffer, uint64_t offset, uint64_t size);
        static SharedPtr create(Device* pDevice, Dimension dimension);

        ShaderResourceView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
            : ResourceView(pDevice, pResource, gfxResourceView, mostDetailedMip, mipCount, firstArraySlice, arraySize) {}

        ShaderResourceView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint64_t offset, uint64_t size)
            : ResourceView(pDevice, pResource, gfxResourceView, offset, size) {}

        ShaderResourceView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView)
            : ResourceView(pDevice, pResource, gfxResourceView) {}
};

class FALCOR_API DepthStencilView : public ResourceView {
    FALCOR_OBJECT(DepthStencilView)
 public:
    static SharedPtr create(Device* pDevice, Texture* pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(Device* pDevice, Dimension dimension);

    DepthStencilView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, gfxResourceView, mipLevel, 1, firstArraySlice, arraySize) {}
};

class FALCOR_API UnorderedAccessView : public ResourceView {
    FALCOR_OBJECT(UnorderedAccessView)
 public:
    static SharedPtr create(Device* pDevice, Texture* pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(Device* pDevice, Buffer* pBuffer, uint64_t offset, uint64_t size);
    static SharedPtr create(Device* pDevice, Dimension dimension);

    UnorderedAccessView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, gfxResourceView, mipLevel, 1, firstArraySlice, arraySize) {}

    UnorderedAccessView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint64_t offset, uint64_t size)
        : ResourceView(pDevice, pResource, gfxResourceView, offset, size) {}
};

class FALCOR_API RenderTargetView : public ResourceView {
    FALCOR_OBJECT(RenderTargetView)
 public:
    static SharedPtr create(Device* pDevice, Texture* pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
    static SharedPtr create(Device* pDevice, Dimension dimension);

    RenderTargetView(Device* pDevice, Resource* pResource, Slang::ComPtr<gfx::IResourceView> gfxResourceView, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
        ResourceView(pDevice, pResource, gfxResourceView, mipLevel, 1, firstArraySlice, arraySize) {}
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_RESOURCEVIEWS_H_
