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

#include "Device.h"
#include "Falcor/Utils/Image/TextureManager.h"
#include "Falcor/Core/API/FBO.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Core/API/CopyContext.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/GpuMemoryHeap.h"
#include "Falcor/Core/API/RtAccelerationStructure.h"
#include "Falcor/Core/Program/ProgramManager.h"


namespace Falcor {

static const uint32_t kTransientHeapConstantBufferSize = 16 * 1024 * 1024;

static const size_t kConstantBufferDataPlacementAlignment = 256;
// This actually depends on the size of the index, but we can handle losing 2 bytes
static const size_t kIndexBufferDataPlacementAlignment = 4;

std::atomic<std::uint8_t> Device::UID = 0;

Device::Device(Window::SharedPtr pWindow, const Device::Desc& desc) : mDesc(desc), mpWindow(pWindow), mPhysicalDeviceName("Unknown") {
    mInitialized = false;
    mCurrentBackBufferIndex = 0;
    _uid = UID++;
    if(pWindow) { mHeadless = false; } else { mHeadless = true; };
}

Device::SharedPtr Device::create(const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(nullptr, desc));
    pDevice->mUseIDesc = false;
    
    if (!pDevice->apiInit(desc.validationLayerOuputFilename)) {
        return nullptr;
    }

    return pDevice;
}

Device::SharedPtr Device::create(Window::SharedPtr pWindow, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(pWindow, desc));
    pDevice->mUseIDesc = false;

    if (!pDevice->apiInit(desc.validationLayerOuputFilename)) {
        return nullptr;
    }

    return pDevice;
}

Device::SharedPtr Device::create(const Device::IDesc& idesc, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(nullptr, desc));
    pDevice->mIDesc = idesc;
    pDevice->mUseIDesc = true;

    if (!pDevice->apiInit(idesc.validationLayerOuputFilename)) {
        return nullptr;
    }

    return pDevice;
}

Device::SharedPtr Device::create(Window::SharedPtr pWindow, const Device::IDesc& idesc, const Device::Desc& desc) {
    auto pDevice = SharedPtr(new Device(pWindow, desc));
    pDevice->mIDesc = idesc;
    pDevice->mUseIDesc = true;

    if(!pDevice->apiInit(idesc.validationLayerOuputFilename)) {
        return nullptr;
    }

    return pDevice;
}

Buffer::SharedPtr Device::createBuffer(size_t size, ResourceBindFlags bindFlags, MemoryType memoryType, const void* pInitData) {
    return make_shared_ptr<Buffer>(Device::SharedPtr(this), size, bindFlags, memoryType, pInitData);
}

Texture::SharedPtr Device::createTexture1D(
    uint32_t width,
    ResourceFormat format,
    uint32_t arraySize,
    uint32_t mipLevels,
    const void* pInitData,
    ResourceBindFlags bindFlags)
{
    //return make_shared_ptr<Texture>(Device::SharedPtr(this), Resource::Type::Texture1D, format, width, 1, 1, arraySize, mipLevels, 1, bindFlags, pInitData);
    return Texture::create1D(Device::SharedPtr(this), width, format, arraySize, mipLevels, pInitData, bindFlags);
}

Texture::SharedPtr Device::createTexture2D(
    uint32_t width,
    uint32_t height,
    ResourceFormat format,
    uint32_t arraySize,
    uint32_t mipLevels,
    const void* pInitData,
    ResourceBindFlags bindFlags)
{
    //return make_shared_ptr<Texture>(Device::SharedPtr(this), Resource::Type::Texture2D, format, width, height, 1, arraySize, mipLevels, 1, bindFlags, pInitData);
    return Texture::create2D(Device::SharedPtr(this), width, height, format, arraySize, mipLevels, pInitData, bindFlags);
}

Texture::SharedPtr Device::createTexture3D(
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    ResourceFormat format,
    uint32_t mipLevels,
    const void* pInitData,
    ResourceBindFlags bindFlags,
    bool sparse)
{
    //return make_shared_ptr<Texture>(Device::SharedPtr(this), Resource::Type::Texture3D, format, width, height, depth, 1, mipLevels, 1, bindFlags, pInitData);
    return Texture::create3D(Device::SharedPtr(this), width, height, depth, format, mipLevels, pInitData, bindFlags, sparse);

}

Texture::SharedPtr Device::createTextureCube(
    uint32_t width,
    uint32_t height,
    ResourceFormat format,
    uint32_t arraySize,
    uint32_t mipLevels,
    const void* pInitData,
    ResourceBindFlags bindFlags)
{
    //return make_shared_ptr<Texture>(Device::SharedPtr(this), Resource::Type::TextureCube, format, width, height, 1, arraySize, mipLevels, 1, bindFlags, pInitData);
    return Texture::createCube(Device::SharedPtr(this), width, height, format, arraySize, mipLevels, pInitData, bindFlags);
}

Texture::SharedPtr Device::createTexture2DMS(
    uint32_t width,
    uint32_t height,
    ResourceFormat format,
    uint32_t sampleCount,
    uint32_t arraySize,
    ResourceBindFlags bindFlags)
{
    //return make_shared_ptr<Texture>(Device::SharedPtr(this), Resource::Type::Texture2DMultisample, format, width, height, 1, arraySize, 1, sampleCount, bindFlags, nullptr);
    return Texture::create2DMS(Device::SharedPtr(this), width, height, format, sampleCount, arraySize, bindFlags);
}

Texture::SharedPtr Device::createTextureFromResource(
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
    Resource::State initState)
{
    return make_shared_ptr<Texture>(Device::SharedPtr(this), pResource, type, format, width, height, depth, arraySize, mipLevels, sampleCount, bindFlags, initState);
}

Sampler::SharedPtr Device::createSampler(const Sampler::Desc& desc) {
    return make_shared_ptr<Sampler>(Device::SharedPtr(this), desc);
}

Fence::SharedPtr Device::createFence(const FenceDesc& desc) {
    return make_shared_ptr<Fence>(Device::SharedPtr(this), desc);
}

Fence::SharedPtr Device::createFence(bool shared) {
    FenceDesc desc;
    desc.shared = shared;
    return createFence(desc);
}

Buffer::SharedPtr Device::createTypedBuffer(
    ResourceFormat format,
    uint32_t elementCount,
    ResourceBindFlags bindFlags,
    MemoryType memoryType,
    const void* pInitData
)
{
    return make_shared_ptr<Buffer>(Device::SharedPtr(this), format, elementCount, bindFlags, memoryType, pInitData);
}

Buffer::SharedPtr Device::createStructuredBuffer(
    uint32_t structSize,
    uint32_t elementCount,
    ResourceBindFlags bindFlags,
    MemoryType memoryType,
    const void* pInitData,
    bool createCounter
){
    return make_shared_ptr<Buffer>(Device::SharedPtr(this), structSize, elementCount, bindFlags, memoryType, pInitData, createCounter);
}

Buffer::SharedPtr Device::createStructuredBuffer(
    const ReflectionType* pType,
    uint32_t elementCount,
    ResourceBindFlags bindFlags,
    MemoryType memoryType,
    const void* pInitData,
    bool createCounter
) {
    FALCOR_CHECK(pType != nullptr, "Can't create a structured buffer from a nullptr type.");
    const ReflectionResourceType* pResourceType = pType->unwrapArray()->asResourceType();
    if (!pResourceType || pResourceType->getType() != ReflectionResourceType::Type::StructuredBuffer) {
        FALCOR_THROW("Can't create a structured buffer from type '{}'.", pType->getClassName());
    }

    // Read the stride directly from the slang type layout, as the stored 'byte size' may not be the same
    auto structStride = pResourceType->getStructType()->getSlangTypeLayout()->getStride();

    FALCOR_ASSERT(structStride <= std::numeric_limits<uint32_t>::max());
    return make_shared_ptr<Buffer>(Device::SharedPtr(this), (uint32_t)structStride, elementCount, bindFlags, memoryType, pInitData, createCounter);
}

Buffer::SharedPtr Device::createStructuredBuffer(
    const ShaderVar& shaderVar,
    uint32_t elementCount,
    ResourceBindFlags bindFlags,
    MemoryType memoryType,
    const void* pInitData,
    bool createCounter
){
    return createStructuredBuffer(shaderVar.getType(), elementCount, bindFlags, memoryType, pInitData, createCounter);
}

Buffer::SharedPtr Device::createBufferFromResource(
    gfx::IBufferResource* pResource,
    size_t size,
    ResourceBindFlags bindFlags,
    MemoryType memoryType
){
    return make_shared_ptr<Buffer>(Device::SharedPtr(this), pResource, size, bindFlags, memoryType);
}

Buffer::SharedPtr Device::createBufferFromNativeHandle(VkBuffer handle, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType){
    return make_shared_ptr<Buffer>(Device::SharedPtr(this), handle, size, bindFlags, memoryType);
}

/**
 * Initialize device
 */
bool Device::init() {
    this->incRef();

#if FALCOR_ENABLE_REF_TRACKING
    this->setEnableRefTracking(true);
#endif

    mpFrameFence = createFence();
    mpFrameFence->breakStrongReferenceToDevice();
    
    mpUploadHeap = GpuMemoryHeap::create(Device::SharedPtr(this), MemoryType::Upload, 1024 * 1024 * 2, mpFrameFence);
    mpUploadHeap->breakStrongReferenceToDevice();
    
    mpReadBackHeap = GpuMemoryHeap::create(Device::SharedPtr(this), MemoryType::ReadBack, 1024 * 1024 * 2, mpFrameFence);
    mpReadBackHeap->breakStrongReferenceToDevice();

    // create default sampler
    Sampler::Desc desc;
    desc.setMaxAnisotropy(16);
    desc.setLodParams(0.0f, 1000.0f, -0.0f);
    desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    desc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    
    mpDefaultSampler = createSampler(desc);
    mpDefaultSampler->breakStrongReferenceToDevice();

    mpTimestampQueryHeap = QueryHeap::create(Device::SharedPtr(this), QueryHeap::Type::Timestamp, 1024 * 1024);
    mpTimestampQueryHeap->breakStrongReferenceToDevice();

    size_t maxTextureCount = 1024 * 10;
    size_t threadCount = std::max(1u, std::thread::hardware_concurrency());
    
    mpTextureManager = std::make_unique<TextureManager>(this, maxTextureCount, threadCount);
    
    mpProgramManager = std::make_unique<ProgramManager>(this);
    
    mpRenderContext = std::make_unique<RenderContext>(this, mGfxCommandQueue);

    mpRenderContext->submit();  // This will bind the descriptor heaps.
    // TODO: Do we need to submit here or should RenderContext::create() bind the descriptor heaps automatically without submit? See #749

    mInitialized = true;

    this->decRef(false);

    return true;
}

const Falcor::SharedPtr<Sampler>& Device::getDefaultSampler() const { 
    FALCOR_ASSERT(mpDefaultSampler);
    return mpDefaultSampler; 
}

void Device::release() {
    decltype(mDeferredReleases)().swap(mDeferredReleases);  
}

gfx::ITransientResourceHeap* Device::getCurrentTransientResourceHeap() {
    return mpTransientResourceHeaps[mCurrentTransientResourceHeapIndex].get();
}

uint64_t Device::getMinAccelerationStructureScratchOffsetAlignment() const {
    size_t alignment = kAccelerationStructureScratchOffsetAlignment;
    mGfxDevice->getMinAccelerationStructureScratchOffsetAlignment(&alignment);
    return alignment;
}

void Device::releaseResource(ISlangUnknown* pResource) {
    if (pResource) {
        // Some static objects get here when the application exits

#if FALCOR_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
        if(this) {
            mDeferredReleases.push({mpFrameFence ? mpFrameFence->getSignaledValue() : 0, Slang::ComPtr<ISlangUnknown>(pResource)});
        }
#if FALCOR_GCC
#pragma GCC diagnostic pop
#endif
    }
}

bool Device::isFeatureSupported(SupportedFeatures flags) const {
    return true;
    //return is_set(mSupportedFeatures, flags);
}

void Device::executeDeferredReleases() {
    mpUploadHeap->executeDeferredReleases();
    mpReadBackHeap->executeDeferredReleases();

    uint64_t currentValue = mpFrameFence->getCurrentValue();
    while (mDeferredReleases.size() && mDeferredReleases.front().fenceValue <= currentValue) {
        mDeferredReleases.pop();
    }
}

void Device::toggleVSync(bool enable) {
    mDesc.enableVsync = enable;
}

void Device::wait() {
    assert(mpRenderContext); 
    mpRenderContext->submit(true);
    mpRenderContext->signal(mpFrameFence.get());
    executeDeferredReleases();
}

size_t Device::getBufferDataAlignment(ResourceBindFlags bindFlags) {
    if (is_set(bindFlags, ResourceBindFlags::Constant))
        return kConstantBufferDataPlacementAlignment;
    if (is_set(bindFlags, ResourceBindFlags::Index))
        return kIndexBufferDataPlacementAlignment;
    return 1;
}

bool Device::isShaderModelSupported(ShaderModel shaderModel) const {
    return ((uint32_t)shaderModel <= (uint32_t)mSupportedShaderModel);
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
