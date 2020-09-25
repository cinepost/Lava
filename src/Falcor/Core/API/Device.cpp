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

#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {
    
void createNullViews(Device::SharedPtr device);
void releaseNullViews();
void createNullBufferViews(Device::SharedPtr device);
void releaseNullBufferViews();
void createNullTypedBufferViews(Device::SharedPtr device);
void releaseNullTypedBufferViews();

Device::SharedPtr _gpDevice;
Device::SharedPtr _gpDeviceHeadless;

std::vector<Device::SharedPtr> _gDevices;
std::vector<Device::SharedPtr> _gDevicesHeadless;

std::atomic<std::uint8_t> Device::UID = 0;

Device::Device(Window::SharedPtr pWindow, const Device::Desc& desc) : mpWindow(pWindow), mDesc(desc), mPhysicalDeviceName("Unknown") {
    _uid = UID++;
    if (!pWindow){
        headless = true;
        LOG_DBG("Device uid: %u type headless", _uid);
    } else {
        headless = false;
        LOG_DBG("Device uid: %u", _uid);
    }
}

Device::~Device() {}

Device::SharedPtr Device::create(const Device::Desc& desc) {
    if (_gpDeviceHeadless) {
        logError("Falcor only supports a single headless device");
        return nullptr;
    }

    _gpDeviceHeadless = SharedPtr(new Device(nullptr, desc));  // headless device
    if (_gpDeviceHeadless->init() == false) { _gpDeviceHeadless = nullptr;}
    return _gpDeviceHeadless;
}

Device::SharedPtr Device::create(Window::SharedPtr& pWindow, const Device::Desc& desc) {
    if(pWindow) {
        // Swapchain enabled device
        if (_gpDevice) {
            logError("Falcor only supports a single device");
            return nullptr;
        }

        _gpDevice = SharedPtr(new Device(pWindow, desc));
        if (_gpDevice->init() == false) { _gpDevice = nullptr;}
        return _gpDevice;
    } else {
        // Headless device
        return create(desc);
    }
}

/**
 * Initialize device
 */
bool Device::init() {
    const uint32_t kDirectQueueIndex = (uint32_t)LowLevelContextData::CommandQueueType::Direct;
    assert(mDesc.cmdQueues[kDirectQueueIndex] > 0);
    if (apiInit() == false) return false;

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
    
    createNullViews(shared_from_this());
    createNullBufferViews(shared_from_this());
    createNullTypedBufferViews(shared_from_this());

    mpRenderContext = RenderContext::create(shared_from_this(), mCmdQueues[(uint32_t)LowLevelContextData::CommandQueueType::Direct][0]);
    
    assert(mpRenderContext);
    mpRenderContext->flush();  // This will bind the descriptor heaps.
    // TODO: Do we need to flush here or should RenderContext::create() bind the descriptor heaps automatically without flush? See #749.

    // Update the FBOs or offscreen buffer
    if (!headless) {
        // Update FBOs
        if (updateDefaultFBO(mpWindow->getClientAreaSize().x, mpWindow->getClientAreaSize().y, mDesc.colorFormat, mDesc.depthFormat) == false) {
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
    if (!headless) {
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
    assert(!headless);
    return mpSwapChainFbos[mCurrentBackBufferIndex];
}

Fbo::SharedPtr Device::getOffscreenFbo() const {
    if(!mpOffscreenFbo) {
        LOG_ERR("No mpOffscreenFbo!!!");
    }
    assert(headless);
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
    toggleFullScreen(false);
    mpRenderContext->flush(true);
    // Release all the bound resources. Need to do that before deleting the RenderContext
    for (uint32_t i = 0; i < arraysize(mCmdQueues); i++) mCmdQueues[i].clear();
    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) mpSwapChainFbos[i].reset();
    mDeferredReleases = decltype(mDeferredReleases)();
    releaseNullViews();
    releaseNullBufferViews();
    releaseNullTypedBufferViews();
    mpRenderContext.reset();
    mpUploadHeap.reset();
    mpCpuDescPool.reset();
    mpGpuDescPool.reset();
    mpFrameFence.reset();
    for (auto& heap : mTimestampQueryHeaps) heap.reset();

    destroyApiObjects();
    mpWindow.reset();
}

void Device::present() {
    assert(!headless);
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

Fbo::SharedPtr Device::resizeSwapChain(uint32_t width, uint32_t height) {
    assert(width > 0 && height > 0);

    mpRenderContext->flush(true);

    // resize offscreen fbo
    if(headless) {
        LOG_DBG("Device::resizeSwapChain headless");
        // Store the FBO parameters
        ResourceFormat colorFormat = mpOffscreenFbo->getColorTexture(0)->getFormat();
        const auto& pDepth = mpOffscreenFbo->getDepthStencilTexture();
        ResourceFormat depthFormat = pDepth ? pDepth->getFormat() : ResourceFormat::Unknown;

        assert(mpOffscreenFbo->getSampleCount() == 1);

        // Delete all the FBOs
        LOG_DBG("Device::resizeSwapChain releaseFboData");
        releaseFboData();
        LOG_DBG("Device::resizeSwapChain apiResizeOffscreenFBO");
        apiResizeOffscreenFBO(width, height, colorFormat);
        LOG_DBG("Device::resizeSwapChain updateOffscreenFBO");
        updateOffscreenFBO(width, height, colorFormat, depthFormat);

        LOG_DBG("Device::resizeSwapChain headless done");
        return getOffscreenFbo();
    }

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
    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) {
        assert(mpSwapChainFbos[i]->getColorTexture(0)->isStateGlobal());
        fboColorStates[i] = mpSwapChainFbos[i]->getColorTexture(0)->getGlobalState();

        const auto& pSwapChainDepth = mpSwapChainFbos[i]->getDepthStencilTexture();
        if (pSwapChainDepth != nullptr) {
            assert(pSwapChainDepth->isStateGlobal());
            fboDepthStates[i] = pSwapChainDepth->getGlobalState();
        }
    }
#endif

    assert(mpSwapChainFbos[0]->getSampleCount() == 1);

    // Delete all the FBOs
    releaseFboData();
    apiResizeSwapChain(width, height, colorFormat);
    updateDefaultFBO(width, height, colorFormat, depthFormat);

#ifdef FALCOR_D3D12
    // Restore FBO resource states
    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) {
        assert(mpSwapChainFbos[i]->getColorTexture(0)->isStateGlobal());
        mpSwapChainFbos[i]->getColorTexture(0)->setGlobalState(fboColorStates[i]);
        const auto& pSwapChainDepth = mpSwapChainFbos[i]->getDepthStencilTexture();
        if (pSwapChainDepth != nullptr) {
            assert(pSwapChainDepth->isStateGlobal());
            pSwapChainDepth->setGlobalState(fboDepthStates[i]);
        }
    }
#endif

#if !defined(FALCOR_D3D12) && !defined(FALCOR_VK)
#error Verify state handling on swapchain resize for this API
#endif

    return getSwapChainFbo();
}

SCRIPT_BINDING(Device) {
    auto deviceDesc = m.class_<Device::Desc>("DeviceDesc");
#define desc_field(f_) rwField(#f_, &Device::Desc::f_)
    deviceDesc.desc_field(colorFormat).desc_field(depthFormat).desc_field(apiMajorVersion).desc_field(apiMinorVersion);
    deviceDesc.desc_field(enableVsync).desc_field(enableDebugLayer).desc_field(cmdQueues);
#undef desc_field

    // Device
    Device::SharedPtr (&create_headless)(const Device::Desc&) = Device::create;

    auto deviceClass = m.regClass(Device);
    //deviceClass.ctor(&Device::create);
    deviceClass.ctor(&create_headless);
}

}  // namespace Falcor

/*
SCRIPT_BINDING(RenderGraph) {
    auto graphClass = m.regClass(RenderGraph);
    graphClass.ctor(&RenderGraph::create);
    graphClass.property("name", &RenderGraph::getName, &RenderGraph::setName);
    graphClass.func_(RenderGraphIR::kAddPass, &RenderGraph::addPass, "pass"_a, "name"_a);
    graphClass.func_(RenderGraphIR::kRemovePass, &RenderGraph::removePass, "name"_a);
    graphClass.func_(RenderGraphIR::kAddEdge, &RenderGraph::addEdge, "src"_a, "dst"_a);
    graphClass.func_(RenderGraphIR::kRemoveEdge, ScriptBindings::overload_cast<const std::string&, const std::string&>(&RenderGraph::removeEdge), "src"_a, "src"_a);
    graphClass.func_(RenderGraphIR::kMarkOutput, &RenderGraph::markOutput, "name"_a);
    graphClass.func_(RenderGraphIR::kUnmarkOutput, &RenderGraph::unmarkOutput, "name"_a);
    graphClass.func_(RenderGraphIR::kAutoGenEdges, &RenderGraph::autoGenEdges, "executionOrder"_a);
    graphClass.func_("getPass", &RenderGraph::getPass, "name"_a);
    graphClass.func_("getOutput", ScriptBindings::overload_cast<const std::string&>(&RenderGraph::getOutput), "name"_a);
    auto printGraph = [](RenderGraph::SharedPtr pGraph) { pybind11::print(RenderGraphExporter::getIR(pGraph)); };
    graphClass.func_("print", printGraph);

    // RenderPass
    auto passClass = m.regClass(RenderPass);

    // RenderPassLibrary
    const auto& createRenderPass = [](std::shared_ptr<Device> device, const std::string& passName, pybind11::dict d = {}) {
        auto pPass = RenderPassLibrary::instance().createPass(device->getRenderContext(), passName.c_str(), Dictionary(d));
        if (!pPass) { 
            throw std::runtime_error(("Can't create a render pass named `" + passName + "`. Make sure the required library was loaded.").c_str());
        }
        return pPass;
    };
    passClass.ctor(createRenderPass, "device"_a, "name"_a, "dict"_a = pybind11::dict());

    const auto& loadPassLibrary = [](const std::string& library) {
        return RenderPassLibrary::instance().loadLibrary(library);
    };

    m.func_(RenderGraphIR::kLoadPassLibrary, loadPassLibrary, "name"_a);

    const auto& updateRenderPass = [](std::shared_ptr<Device> device, const RenderGraph::SharedPtr& pGraph, const std::string& passName, pybind11::dict d) {
        pGraph->updatePass(device->getRenderContext(), passName, Dictionary(d));
    };

    graphClass.func_(RenderGraphIR::kUpdatePass, updateRenderPass, "device"_a, "name"_a, "dict"_a);
}
*/