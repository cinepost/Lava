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
#include "Core/API/Texture.h"
#include "Core/API/Device.h"
#include "Core/API/Formats.h"

#include "GFXFormats.h"
#include "GFXResource.h"

namespace Falcor
{

    gfx::IResource::Type getResourceType(Texture::Type type)
    {
        switch (type)
        {
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
            FALCOR_UNREACHABLE();
            return gfx::IResource::Type::Unknown;
        }
    }

    uint64_t Texture::getTextureSizeInBytes() const
    {
        // get allocation info for resource description
        size_t outSizeBytes = 0, outAlignment = 0;

        Slang::ComPtr<gfx::IDevice> iDevicePtr = mpDevice->getApiHandle();
        gfx::ITextureResource* textureResource = static_cast<gfx::ITextureResource*>(getApiHandle().get());
        FALCOR_ASSERT(textureResource);

        gfx::ITextureResource::Desc *desc = textureResource->getDesc();

        mpDevice->getApiHandle()->getTextureAllocationInfo(*desc, &outSizeBytes, &outAlignment);
        FALCOR_ASSERT(outSizeBytes > 0);

        return outSizeBytes;

    }

    void Texture::apiInit(const void* pData, bool autoGenMips)
    {
        // create resource description
        gfx::ITextureResource::Desc desc = {};

        // base description

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
        if (mType == Texture::Type::TextureCube)
        {
            desc.arraySize = mArraySize * 6;
        }
        else
        {
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
        if ((mBindFlags & (Texture::BindFlags::RenderTarget | Texture::BindFlags::DepthStencil)) != Texture::BindFlags::None)
        {
            if ((mBindFlags & Texture::BindFlags::DepthStencil) != Texture::BindFlags::None)
            {
                clearValue.depthStencil.depth = 1.0f;
            }
        }
        desc.optimalClearValue = clearValue;

        // shared resource
        if (is_set(mBindFlags, Resource::BindFlags::Shared))
        {
            desc.isShared = true;
        }

        // validate description
        FALCOR_ASSERT(desc.size.width > 0 && desc.size.height > 0);
        FALCOR_ASSERT(desc.numMipLevels > 0 && desc.size.depth > 0 && desc.arraySize > 0 && desc.sampleDesc.numSamples > 0);

        // create resource
        Slang::ComPtr<gfx::ITextureResource> textureResource =
            mpDevice->getApiHandle()->createTextureResource(desc, nullptr);
        FALCOR_ASSERT(textureResource);

        mApiHandle = textureResource;

        // upload init data through texture class
        if (pData)
        {
            uploadInitData(pData, autoGenMips);
        }

#if FALCOR_D3D12_AVAILABLE
        gfx::InteropHandle handle = {};
        FALCOR_GFX_CALL(mApiHandle->getNativeResourceHandle(&handle));
        FALCOR_ASSERT(handle.api == gfx::InteropHandleAPI::D3D12);
        mpD3D12Handle = reinterpret_cast<ID3D12Resource*>(handle.handleValue);
#endif
    }

    Texture::~Texture()
    {
        if (mApiHandle)
        {
            ApiObjectHandle objectHandle;
            mApiHandle->queryInterface(SLANG_UUID_ISlangUnknown, (void**)objectHandle.writeRef());
            mpDevice->releaseResource(objectHandle);
        }
    }

    void Texture::updateSparseBindInfo() {
        /*
        assert(mImage != VK_NULL_HANDLE);

        if (!mIsSparse) {
            LOG_ERR("Unable to sparse bind non sparse texture !!!");
            return;
        }
        
        // Update list of memory-backed sparse image memory binds
        mSparseImageMemoryBinds.clear();
        for (auto pPage : mPages) {
            //if ( pPage->isResident())
                mSparseImageMemoryBinds.push_back(pPage->mImageMemoryBind);
        }

        // Update sparse bind info
        mBindSparseInfo = {};
        mBindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        // todo: Semaphore for queue submission
        // bindSparseInfo.signalSemaphoreCount = 1;
        // bindSparseInfo.pSignalSemaphores = &bindSparseSemaphore;

        // Image memory binds
        mImageMemoryBindInfo = {};
        mImageMemoryBindInfo.image = mImage;
        mImageMemoryBindInfo.bindCount = static_cast<uint32_t>(mSparseImageMemoryBinds.size());
        mImageMemoryBindInfo.pBinds = mSparseImageMemoryBinds.data();

        mBindSparseInfo.imageBindCount = (mImageMemoryBindInfo.bindCount > 0) ? 1 : 0;
        mBindSparseInfo.pImageBinds = &mImageMemoryBindInfo;

        // Opaque image memory binds for the mip tail
        mOpaqueMemoryBindInfo.image = mImage;
        mOpaqueMemoryBindInfo.bindCount = static_cast<uint32_t>(mOpaqueMemoryBinds.size());
        mOpaqueMemoryBindInfo.pBinds = mOpaqueMemoryBinds.data();
        mBindSparseInfo.imageOpaqueBindCount = (mOpaqueMemoryBindInfo.bindCount > 0) ? 1 : 0;
        mBindSparseInfo.pImageOpaqueBinds = &mOpaqueMemoryBindInfo;


        // --------------------
        // Bind to queue
        auto queue = mpDevice->getCommandQueueHandle(LowLevelContextData::CommandQueueType::Direct, 0);

        // todo: in draw?
        vkQueueBindSparse(queue, 1, &mBindSparseInfo, VK_NULL_HANDLE);
        
        //todo: use sparse bind semaphore
        vkQueueWaitIdle(queue);
        */
    }

}  // namespace Falcor
