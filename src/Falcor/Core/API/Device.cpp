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
#include "Falcor/stdafx.h"
#include "Device.h"

#include "Falcor/Core/API/ResourceManager.h"
#include "Falcor/Utils/Debug/debug.h"


namespace Falcor {
    
void createNullViews(Device::SharedPtr pDevice);
void releaseNullViews(Device::SharedPtr pDevice);
void createNullBufferViews(Device::SharedPtr pDevice);
void releaseNullBufferViews(Device::SharedPtr pDevice);
void createNullTypedBufferViews(Device::SharedPtr pDevice);
void releaseNullTypedBufferViews(Device::SharedPtr pDevice);
void releaseStaticResources(Device::SharedPtr pDevice);

std::atomic<std::uint8_t> Device::UID = 0;

Device::Device(const Device::Desc& desc) : mDesc(desc), mGpuId(0), mPhysicalDeviceName("Unknown") {
    _uid = UID++;

#ifdef FALCOR_VK
    mVkSurface = mDesc.surface;
    if (mDesc.surface == VK_NULL_HANDLE) { mHeadless = true; } else { mHeadless = false; };
#endif
}

Device::Device(uint32_t gpuId, const Device::Desc& desc) : mDesc(desc), mGpuId(gpuId), mPhysicalDeviceName("Unknown") {
    _uid = UID++;
#ifdef FALCOR_VK
    mVkSurface = mDesc.surface;
    if (mDesc.surface == VK_NULL_HANDLE) { mHeadless = true; } else { mHeadless = false; };
#endif
}

Device::SharedPtr Device::create(std::shared_ptr<const DeviceManager> pDeviceManager, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(desc));
    if (!pDevice->init(pDeviceManager))
        return nullptr;

    return pDevice;
}

Device::SharedPtr Device::create(std::shared_ptr<const DeviceManager> pDeviceManager, uint32_t deviceId, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(deviceId, desc));  // headless device
    if (!pDevice->init(pDeviceManager))
        return nullptr;

    return pDevice;
}

/**
 * Initialize device
 */
bool Device::init(std::shared_ptr<const DeviceManager> pDeviceManager) {
    const uint32_t kDirectQueueIndex = (uint32_t)LowLevelContextData::CommandQueueType::Direct;
    assert(mDesc.cmdQueues[kDirectQueueIndex] > 0);
    if (apiInit(pDeviceManager) == false) return false;

    // Create the descriptor pools
    DescriptorPool::Desc poolDesc;
    // For DX12 there is no difference between the different SRV/UAV types. For Vulkan it matters, hence the #ifdef
    // DX12 guarantees at least 1,000,000 descriptors
    poolDesc.setDescCount(DescriptorPool::Type::TextureSrv, 1000000).setDescCount(DescriptorPool::Type::Sampler, 2048).setShaderVisible(true);

#ifndef FALCOR_D3D12
    poolDesc.setDescCount(DescriptorPool::Type::Cbv, 16 * 1024).setDescCount(DescriptorPool::Type::TextureUav, 16 * 1024);
    poolDesc.setDescCount(DescriptorPool::Type::StructuredBufferSrv, 2 * 1024)
        .setDescCount(DescriptorPool::Type::StructuredBufferUav, 2 * 1024)
        .setDescCount(DescriptorPool::Type::TypedBufferSrv, 2 * 1024)
        .setDescCount(DescriptorPool::Type::TypedBufferUav, 2 * 1024)
        .setDescCount(DescriptorPool::Type::RawBufferSrv, 2 * 1024)
        .setDescCount(DescriptorPool::Type::RawBufferUav, 2 * 1024);
#endif

    mpFrameFence = GpuFence::create(shared_from_this());
    mpGpuDescPool = DescriptorPool::create(shared_from_this(), poolDesc, mpFrameFence);
    poolDesc.setShaderVisible(false).setDescCount(DescriptorPool::Type::Rtv, 16 * 1024).setDescCount(DescriptorPool::Type::Dsv, 1024);
    mpCpuDescPool = DescriptorPool::create(shared_from_this(), poolDesc, mpFrameFence);
    mpUploadHeap = GpuMemoryHeap::create(shared_from_this(), GpuMemoryHeap::Type::Upload, 1024 * 1024 * 2, mpFrameFence);
    mpRenderContext = RenderContext::create(shared_from_this(), mCmdQueues[(uint32_t)LowLevelContextData::CommandQueueType::Direct][0]);
    assert(mpRenderContext);

    mpResourceManager = ResourceManager::create(shared_from_this());
    assert(mpResourceManager);


    // create static null views
    createNullViews(shared_from_this());
    createNullBufferViews(shared_from_this());
    createNullTypedBufferViews(shared_from_this());

    // create default sampler
    Sampler::Desc desc;
    desc.setMaxAnisotropy(16);
    desc.setLodParams(0.0f, 1000.0f, -0.0f);
    desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    desc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpDefaultSampler = Sampler::create(shared_from_this(), desc);

    mpRenderContext->flush();  // This will bind the descriptor heaps.
    // TODO: Do we need to flush here or should RenderContext::create() bind the descriptor heaps automatically without flush? See #749.

    // Update the FBOs or offscreen buffer
    if (!mHeadless) {
        if (updateDefaultFBO(mDesc.width, mDesc.height, mDesc.colorFormat, mDesc.depthFormat) == false) {
            return false;
        }
    } else {
        // Update offscreen buffer
        if (updateOffscreenFBO(mDesc.width, mDesc.height, mDesc.colorFormat, mDesc.depthFormat) == false) {
            return false;
        }
    }
    return true;
}

std::string& Device::getPhysicalDeviceName() {
    return mPhysicalDeviceName;
}

void Device::releaseFboData() {
    // First, delete all FBOs
    if (!mHeadless) {
        // Delete swapchain FBOs
        for (auto& pFbo : mpSwapChainFbos) {
            pFbo->attachColorTarget(nullptr, 0);
            pFbo->attachDepthStencilTarget(nullptr);
        }
    } else {
        // Delete headless FBO
        mpOffscreenFbo->attachColorTarget(nullptr, 0);
        mpOffscreenFbo->attachDepthStencilTarget(nullptr);
    }

    // Now execute all deferred releases
    release();
}

void Device::release() {
    decltype(mDeferredReleases)().swap(mDeferredReleases);  
}


bool Device::updateOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat) {
    ResourceHandle apiHandle;
    getApiFboData(width, height, colorFormat, depthFormat, apiHandle);

    // Create a texture object
    auto pColorTex = Texture::SharedPtr(new Texture(shared_from_this(), width, height, 1, 1, 1, 1, colorFormat, Texture::Type::Texture2D, Texture::BindFlags::RenderTarget));
    pColorTex->mApiHandle = apiHandle;

    // Create the FBO if it's required
    if (mpOffscreenFbo == nullptr) mpOffscreenFbo = Fbo::create(shared_from_this());
    mpOffscreenFbo->attachColorTarget(pColorTex, 0);

    // Create a depth texture
    if (depthFormat != ResourceFormat::Unknown) {
        auto pDepth = Texture::create2D(shared_from_this(), width, height, depthFormat, 1, 1, nullptr, Texture::BindFlags::DepthStencil);
        mpOffscreenFbo->attachDepthStencilTarget(pDepth);
    }

    return true;
}

bool Device::updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat) {
    if(mVkSurface == VK_NULL_HANDLE) return false;

    ResourceHandle apiHandles[kSwapChainBuffersCount] = {};
    if(!getApiFboData(width, height, colorFormat, depthFormat, apiHandles, mCurrentBackBufferIndex)) return false;

    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) {
        // Create a texture object
        auto pColorTex = Texture::SharedPtr(new Texture(shared_from_this(), width, height, 1, 1, 1, 1, colorFormat, Texture::Type::Texture2D, Texture::BindFlags::RenderTarget));
        pColorTex->mApiHandle = apiHandles[i];
        // Create the FBO if it's required
        if (mpSwapChainFbos[i] == nullptr) mpSwapChainFbos[i] = Fbo::create(shared_from_this());
        mpSwapChainFbos[i]->attachColorTarget(pColorTex, 0);

        // Create a depth texture
        if (depthFormat != ResourceFormat::Unknown) {
            auto pDepth = Texture::create2D(shared_from_this(), width, height, depthFormat, 1, 1, nullptr, Texture::BindFlags::DepthStencil);
            mpSwapChainFbos[i]->attachDepthStencilTarget(pDepth);
        }
    }
    return true;
}

Fbo::SharedPtr Device::getSwapChainFbo() const {
    assert(!mHeadless);
    return mpSwapChainFbos[mCurrentBackBufferIndex];
}

Fbo::SharedPtr Device::getOffscreenFbo() const {
    assert(mHeadless);
    assert(mpOffscreenFbo);
    return mpOffscreenFbo;
}

std::weak_ptr<QueryHeap> Device::createQueryHeap(QueryHeap::Type type, uint32_t count) {
    QueryHeap::SharedPtr pHeap = QueryHeap::create(shared_from_this(), type, count);
    mTimestampQueryHeaps.push_back(pHeap);
    return pHeap;
}

void Device::releaseResource(ApiObjectHandle pResource) {
    if (pResource) {
        // Some static objects get here when the application exits
        if(this) {
            mDeferredReleases.push({ mpFrameFence->getCpuValue(), pResource });
        }
    }
}

bool Device::isFeatureSupported(SupportedFeatures flags) const {
    return is_set(mSupportedFeatures, flags);
}

void Device::executeDeferredReleases() {
    mpUploadHeap->executeDeferredReleases();
    uint64_t gpuVal = mpFrameFence->getGpuValue();

    while (mDeferredReleases.size() && mDeferredReleases.front().frameID <= gpuVal) {
        mDeferredReleases.pop();
    }

    mpCpuDescPool->executeDeferredReleases();
    mpGpuDescPool->executeDeferredReleases();
}

void Device::toggleVSync(bool enable) {
    mDesc.enableVsync = enable;
}

void Device::cleanup() {
    if (!mpApiData) {
        std::cout << "Device cleanup (skip), Device mpApiData False \n";
        return;
    }

    std::cout << "Device cleanup \n";
    toggleFullScreen(false);

    mpRenderContext->flush(true);

    // Release all the bound resources. Need to do that before deleting the RenderContext
    for (uint32_t i = 0; i < arraysize(mCmdQueues); i++) mCmdQueues[i].clear();

    if(mHeadless) {
        mpOffscreenFbo.reset();
    } else {
        for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) mpSwapChainFbos[i].reset();
    }

    mDeferredReleases = decltype(mDeferredReleases)();

    releaseNullViews(shared_from_this());
    releaseNullBufferViews(shared_from_this());
    releaseNullTypedBufferViews(shared_from_this());
    releaseStaticResources(shared_from_this());

    mpRenderContext.reset();
    mpUploadHeap.reset();
    mpCpuDescPool.reset();
    mpGpuDescPool.reset();
    mpFrameFence.reset();

    for (auto& heap : mTimestampQueryHeaps) heap.reset();

    destroyApiObjects();
}

void Device::present() {
    assert(!mHeadless);
    mpRenderContext->resourceBarrier(mpSwapChainFbos[mCurrentBackBufferIndex]->getColorTexture(0).get(), Resource::State::Present);
    mpRenderContext->flush();
    apiPresent();
    mpFrameFence->gpuSignal(mpRenderContext->getLowLevelData()->getCommandQueue());
    if (mpFrameFence->getCpuValue() >= kSwapChainBuffersCount) {
        mpFrameFence->syncCpu(mpFrameFence->getCpuValue() - kSwapChainBuffersCount);
    }
    executeDeferredReleases();
    mFrameID++;
}

void Device::flushAndSync() {
    mpRenderContext->flush(true);
    mpFrameFence->gpuSignal(mpRenderContext->getLowLevelData()->getCommandQueue());
    executeDeferredReleases();
}

#ifdef SCRIPTING
SCRIPT_BINDING(Device) {
    ScriptBindings::SerializableStruct<Device::Desc> deviceDesc(m, "DeviceDesc");
#define field(f_) field(#f_, &Device::Desc::f_)
    deviceDesc.field(colorFormat);
    deviceDesc.field(depthFormat);
    deviceDesc.field(apiMajorVersion);
    deviceDesc.field(apiMinorVersion);
    deviceDesc.field(enableVsync);
    deviceDesc.field(enableDebugLayer);
    deviceDesc.field(cmdQueues);
#undef field

    // Device
    pybind11::class_<Device, Device::SharedPtr> device(m, "Device");
}
#endif

}  // namespace Falcor
