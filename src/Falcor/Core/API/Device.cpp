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

#include <thread>

#include "Falcor/Utils/Image/TextureManager.h"
#include "Falcor/Core/API/RenderContext.h"

#include "Device.h"
#include "Sampler.h"


namespace Falcor {
    
void createNullViews(Device::SharedPtr pDevice);
void releaseNullViews(Device::SharedPtr pDevice);

std::atomic<std::uint8_t> Device::UID = 0;

Device::Device(Window::SharedPtr pWindow, const Device::Desc& desc) : mpWindow(pWindow), mDesc(desc), mPhysicalDeviceName("Unknown") {
    _uid = UID++;
    if(pWindow) { mHeadless = false; } else { mHeadless = true; };
}

Device::SharedPtr Device::create(const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(nullptr, desc));
    pDevice->mUseIDesc = false;
    if (!pDevice->init())
        return nullptr;

    return pDevice;
}

Device::SharedPtr Device::create(Window::SharedPtr pWindow, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(pWindow, desc));
    pDevice->mUseIDesc = false;
    if (!pDevice->init())
        return nullptr;

    return pDevice;
}

Device::SharedPtr Device::create(const Device::IDesc& idesc, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(nullptr, desc));
    pDevice->mIDesc = idesc;
    pDevice->mUseIDesc = true;

    if (!pDevice->init())
        return nullptr;

    return pDevice;
}

Device::SharedPtr Device::create(Window::SharedPtr pWindow, const Device::IDesc& idesc, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(pWindow, desc));
    pDevice->mIDesc = idesc;
    pDevice->mUseIDesc = true;

    if (!pDevice->init())
        return nullptr;

    return pDevice;
}

/**
 * Initialize device
 */
bool Device::init() {
    const uint32_t kDirectQueueIndex = (uint32_t)LowLevelContextData::CommandQueueType::Direct;
    FALCOR_ASSERT(mDesc.cmdQueues[kDirectQueueIndex] > 0);
    if (apiInit() == false) return false;

    mpFrameFence = GpuFence::create(shared_from_this());

#if FALCOR_D3D12_AVAILABLE
    // Create the descriptor pools
    D3D12DescriptorPool::Desc poolDesc;
    poolDesc.setDescCount(ShaderResourceType::TextureSrv, 1000000).setDescCount(ShaderResourceType::Sampler, 2048).setShaderVisible(true);
    mpD3D12GpuDescPool = D3D12DescriptorPool::create(poolDesc, mpFrameFence);
    poolDesc.setShaderVisible(false).setDescCount(ShaderResourceType::Rtv, 16 * 1024).setDescCount(ShaderResourceType::Dsv, 1024);
    mpD3D12CpuDescPool = D3D12DescriptorPool::create(poolDesc, mpFrameFence);
#endif // FALCOR_D3D12

    mpUploadHeap = GpuMemoryHeap::create(shared_from_this(), GpuMemoryHeap::Type::Upload, 1024 * 1024 * 2, mpFrameFence);

    createNullViews();

    size_t maxTextureCount = 1024 * 10;
    size_t threadCount = std::thread::hardware_concurrency();
    mpTextureManager = TextureManager::create(shared_from_this(), maxTextureCount, threadCount);
    assert(mpTextureManager);

    mpRenderContext = RenderContext::create(shared_from_this(), mCmdQueues[(uint32_t)LowLevelContextData::CommandQueueType::Direct][0]);

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
    ResourceHandle apiHandles[kSwapChainBuffersCount] = {};
    getApiFboData(width, height, colorFormat, depthFormat, apiHandles, mCurrentBackBufferIndex);

    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
    {
        // Create a texture object
        auto pColorTex = Texture::SharedPtr(new Texture(shared_from_this(), width, height, 1, 1, 1, 1, colorFormat, Texture::Type::Texture2D, Texture::BindFlags::RenderTarget));
        pColorTex->mApiHandle = apiHandles[i];
        // Create the FBO if it's required
        if (mpSwapChainFbos[i] == nullptr) mpSwapChainFbos[i] = Fbo::create(shared_from_this());
        mpSwapChainFbos[i]->attachColorTarget(pColorTex, 0);

        // Create a depth texture
        if (depthFormat != ResourceFormat::Unknown)
        {
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
    return true;
    //return is_set(mSupportedFeatures, flags);
}

void Device::executeDeferredReleases() {
mpUploadHeap->executeDeferredReleases();
        uint64_t gpuVal = mpFrameFence->getGpuValue();
        while (mDeferredReleases.size() && mDeferredReleases.front().frameID <= gpuVal)
        {
            mDeferredReleases.pop();
        }

#ifdef FALCOR_D3D12_AVAILABLE
        mpD3D12CpuDescPool->executeDeferredReleases();
        mpD3D12GpuDescPool->executeDeferredReleases();
#endif // FALCOR_D3D12_AVAILABLE
}

void Device::toggleVSync(bool enable) {
    mDesc.enableVsync = enable;
}

void Device::cleanup() {
    toggleFullScreen(false);
    mpRenderContext->flush(true);

    // Release all the bound resources. Need to do that before deleting the RenderContext
    for (uint32_t i = 0; i < arraysize(mCmdQueues); i++) {
        mCmdQueues[i].clear();
#if FALCOR_GFX_VK
        mCmdNativeQueues[i].clear();
#endif
    }

    if(mHeadless) {
        mpOffscreenFbo.reset();
    } else {
        for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) mpSwapChainFbos[i].reset();
    }

    mDeferredReleases = decltype(mDeferredReleases)();

    releaseNullViews();
    mpRenderContext.reset();
    mpUploadHeap.reset();

#ifdef FALCOR_D3D12
        mpD3D12CpuDescPool.reset();
        mpD3D12GpuDescPool.reset();
#endif // FALCOR_D3D12

    mpFrameFence.reset();

    for (auto& heap : mTimestampQueryHeaps) heap.reset();

    destroyApiObjects();
    if(mpWindow) {
        mpWindow.reset();
    }
}

void Device::flushAndSync() {
    mpRenderContext->flush(true);
    mpFrameFence->gpuSignal(mpRenderContext->getLowLevelData()->getCommandQueue());
    executeDeferredReleases();
}

bool Device::isShaderModelSupported(ShaderModel shaderModel) const {
    return ((uint32_t)shaderModel <= (uint32_t)mSupportedShaderModel);
}

Fbo::SharedPtr Device::resizeSwapChain(uint32_t width, uint32_t height)
{
    FALCOR_ASSERT(width > 0 && height > 0);

    mpRenderContext->flush(true);

    // Store the FBO parameters
    ResourceFormat colorFormat = mpSwapChainFbos[0]->getColorTexture(0)->getFormat();
    const auto& pDepth = mpSwapChainFbos[0]->getDepthStencilTexture();
    ResourceFormat depthFormat = pDepth ? pDepth->getFormat() : ResourceFormat::Unknown;

    // updateDefaultFBO() attaches the resized swapchain to new Texture objects, with Undefined resource state.
    // This is fine in Vulkan because a new swapchain is created, but D3D12 can resize without changing
    // internal resource state, so we must cache the Falcor resource state to track it correctly in the new Texture object.
    // #TODO Is there a better place to cache state within D3D12 implementation instead of #ifdef-ing here?

#ifdef FALCOR_D3D12
    // Save FBO resource states
    std::array<Resource::State, kSwapChainBuffersCount> fboColorStates;
    std::array<Resource::State, kSwapChainBuffersCount> fboDepthStates;
    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
    {
        FALCOR_ASSERT(mpSwapChainFbos[i]->getColorTexture(0)->isStateGlobal());
        fboColorStates[i] = mpSwapChainFbos[i]->getColorTexture(0)->getGlobalState();

        const auto& pSwapChainDepth = mpSwapChainFbos[i]->getDepthStencilTexture();
        if (pSwapChainDepth != nullptr)
        {
            FALCOR_ASSERT(pSwapChainDepth->isStateGlobal());
            fboDepthStates[i] = pSwapChainDepth->getGlobalState();
        }
    }
#endif

    FALCOR_ASSERT(mpSwapChainFbos[0]->getSampleCount() == 1);

    // Delete all the FBOs
    releaseFboData();
    apiResizeSwapChain(width, height, colorFormat);
    updateDefaultFBO(width, height, colorFormat, depthFormat);

#ifdef FALCOR_D3D12
    // Restore FBO resource states
    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
    {
        FALCOR_ASSERT(mpSwapChainFbos[i]->getColorTexture(0)->isStateGlobal());
        mpSwapChainFbos[i]->getColorTexture(0)->setGlobalState(fboColorStates[i]);
        const auto& pSwapChainDepth = mpSwapChainFbos[i]->getDepthStencilTexture();
        if (pSwapChainDepth != nullptr)
        {
            FALCOR_ASSERT(pSwapChainDepth->isStateGlobal());
            pSwapChainDepth->setGlobalState(fboDepthStates[i]);
        }
    }
#endif

#if !defined(FALCOR_D3D12) && !defined(FALCOR_GFX) && !defined(FALCOR_VK)
#error Verify state handling on swapchain resize for this API
#endif

    return getSwapChainFbo();
}

void Device::createNullViews() {
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Buffer] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Buffer);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture1D] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Texture1D);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture1DArray] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Texture1DArray);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2D] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Texture2D);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2DArray] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Texture2DArray);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2DMS] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Texture2DMS);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2DMSArray] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Texture2DMSArray);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture3D] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::Texture3D);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::TextureCube] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::TextureCube);
    mNullViews.srv[(size_t)ShaderResourceView::Dimension::TextureCubeArray] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::TextureCubeArray);

    if (isFeatureSupported(Device::SupportedFeatures::Raytracing))
    {
        mNullViews.srv[(size_t)ShaderResourceView::Dimension::AccelerationStructure] = ShaderResourceView::create(shared_from_this(), ShaderResourceView::Dimension::AccelerationStructure);
    }

    mNullViews.uav[(size_t)UnorderedAccessView::Dimension::Buffer] = UnorderedAccessView::create(shared_from_this(), UnorderedAccessView::Dimension::Buffer);
    mNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture1D] = UnorderedAccessView::create(shared_from_this(), UnorderedAccessView::Dimension::Texture1D);
    mNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture1DArray] = UnorderedAccessView::create(shared_from_this(), UnorderedAccessView::Dimension::Texture1DArray);
    mNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture2D] = UnorderedAccessView::create(shared_from_this(), UnorderedAccessView::Dimension::Texture2D);
    mNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture2DArray] = UnorderedAccessView::create(shared_from_this(), UnorderedAccessView::Dimension::Texture2DArray);
    mNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture3D] = UnorderedAccessView::create(shared_from_this(), UnorderedAccessView::Dimension::Texture3D);

    mNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture1D] = DepthStencilView::create(shared_from_this(), DepthStencilView::Dimension::Texture1D);
    mNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture1DArray] = DepthStencilView::create(shared_from_this(), DepthStencilView::Dimension::Texture1DArray);
    mNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2D] = DepthStencilView::create(shared_from_this(), DepthStencilView::Dimension::Texture2D);
    mNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2DArray] = DepthStencilView::create(shared_from_this(), DepthStencilView::Dimension::Texture2DArray);
    mNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2DMS] = DepthStencilView::create(shared_from_this(), DepthStencilView::Dimension::Texture2DMS);
    mNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2DMSArray] = DepthStencilView::create(shared_from_this(), DepthStencilView::Dimension::Texture2DMSArray);

    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Buffer] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Buffer);
    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture1D] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Texture1D);
    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture1DArray] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Texture1DArray);
    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2D] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Texture2D);
    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2DArray] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Texture2DArray);
    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2DMS] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Texture2DMS);
    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2DMSArray] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Texture2DMSArray);
    mNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture3D] = RenderTargetView::create(shared_from_this(), RenderTargetView::Dimension::Texture3D);

    mNullViews.cbv = ConstantBufferView::create(shared_from_this());
}

void Device::releaseNullViews() {
    mNullViews = {};
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
