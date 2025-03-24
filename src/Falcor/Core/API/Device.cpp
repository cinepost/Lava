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
#include "Device.h"
#include "Raytracing.h"
#include "GFXHelpers.h"
#include "GFXAPI.h"
#include "ComputeStateObject.h"
#include "GraphicsStateObject.h"
#include "RtStateObject.h"
#include "NativeHandleTraits.h"

#include "Sampler.h"

#include "Core/Macros.h"
#include "Core/Error.h"
#include "Core/ObjectPython.h"
#include "Core/Program/Program.h"
#include "Core/Program/ProgramManager.h"
#include "Core/Program/ShaderVar.h"

#include "Falcor/Utils/StringUtils.h"
#include "Falcor/Utils/Timing/Profiler.h"
#include "Falcor/Utils/Image/TextureManager.h"
#include "Falcor/Core/API/CopyContext.h"
#include "Falcor/Core/API/RenderContext.h"

#include <thread>


namespace Falcor {

static_assert((uint32_t)RayFlags::None == 0);
static_assert((uint32_t)RayFlags::ForceOpaque == 0x1);
static_assert((uint32_t)RayFlags::ForceNonOpaque == 0x2);
static_assert((uint32_t)RayFlags::AcceptFirstHitAndEndSearch == 0x4);
static_assert((uint32_t)RayFlags::SkipClosestHitShader == 0x8);
static_assert((uint32_t)RayFlags::CullBackFacingTriangles == 0x10);
static_assert((uint32_t)RayFlags::CullFrontFacingTriangles == 0x20);
static_assert((uint32_t)RayFlags::CullOpaque == 0x40);
static_assert((uint32_t)RayFlags::CullNonOpaque == 0x80);
static_assert((uint32_t)RayFlags::SkipTriangles == 0x100);
static_assert((uint32_t)RayFlags::SkipProceduralPrimitives == 0x200);

static_assert(getMaxViewportCount() <= 8);
    
static const uint32_t kTransientHeapConstantBufferSize = 16 * 1024 * 1024;

static const size_t kConstantBufferDataPlacementAlignment = 256;
// This actually depends on the size of the index, but we can handle losing 2 bytes
static const size_t kIndexBufferDataPlacementAlignment = 4;

/// The default Shader Model to use when compiling programs.
/// If not supported, the highest supported shader model will be used instead.
static const ShaderModel kDefaultShaderModel = ShaderModel::SM6_6;

class GFXDebugCallBack : public gfx::IDebugCallback {
    virtual SLANG_NO_THROW void SLANG_MCALL
    handleMessage(gfx::DebugMessageType type, gfx::DebugMessageSource source, const char* message) override {
        if (type == gfx::DebugMessageType::Error) {
            LLOG_ERR << "GFX Error: " << message;
        } else if (type == gfx::DebugMessageType::Warning) {
            LLOG_WRN << "GFX Warning: " << message;
        } else {
            LLOG_DBG << "GFX Info: " << message;
        }
    }
};

GFXDebugCallBack gGFXDebugCallBack; // TODO: REMOVEGLOBAL

inline Device::Limits queryLimits(gfx::IDevice* pDevice) {
    const auto& deviceLimits = pDevice->getDeviceInfo().limits;

    auto toUint3 = [](const uint32_t value[]) { return uint3(value[0], value[1], value[2]); };

    Device::Limits limits = {};
    limits.maxComputeDispatchThreadGroups = toUint3(deviceLimits.maxComputeDispatchThreadGroups);
    limits.maxShaderVisibleSamplers = deviceLimits.maxShaderVisibleSamplers;
    return limits;
}

inline Device::SupportedFeatures querySupportedFeatures(gfx::IDevice* pDevice) {
    Device::SupportedFeatures result = Device::SupportedFeatures::None;
    
    if (pDevice->hasFeature("ray-tracing")) {
        result |= Device::SupportedFeatures::Raytracing;
    }

    if (pDevice->hasFeature("ray-query")) {
        result |= Device::SupportedFeatures::RaytracingTier1_1;
    }

    if (pDevice->hasFeature("conservative-rasterization-3")) {
        result |= Device::SupportedFeatures::ConservativeRasterizationTier3;
    }

    if (pDevice->hasFeature("conservative-rasterization-2")) {
        result |= Device::SupportedFeatures::ConservativeRasterizationTier2;
    }

    if (pDevice->hasFeature("conservative-rasterization-1")) {
        result |= Device::SupportedFeatures::ConservativeRasterizationTier1;
    }

    if (pDevice->hasFeature("rasterizer-ordered-views")) {
        result |= Device::SupportedFeatures::RasterizerOrderedViews;
    }

    if (pDevice->hasFeature("programmable-sample-positions-2")) {
        result |= Device::SupportedFeatures::ProgrammableSamplePositionsFull;
    } else if (pDevice->hasFeature("programmable-sample-positions-1")) {
        result |= Device::SupportedFeatures::ProgrammableSamplePositionsPartialOnly;
    }

    if (pDevice->hasFeature("barycentrics")) {
        result |= Device::SupportedFeatures::Barycentrics;
    }

    if (pDevice->hasFeature("wave-ops")) {
        result |= Device::SupportedFeatures::WaveOperations;
    }

    return result;
}

inline ShaderModel querySupportedShaderModel(gfx::IDevice* pDevice) {
    struct SMLevel {
        const char* name;
        ShaderModel level;
    };

    const SMLevel levels[] = {
        {"sm_6_7", ShaderModel::SM6_7},
        {"sm_6_6", ShaderModel::SM6_6},
        {"sm_6_5", ShaderModel::SM6_5},
        {"sm_6_4", ShaderModel::SM6_4},
        {"sm_6_3", ShaderModel::SM6_3},
        {"sm_6_2", ShaderModel::SM6_2},
        {"sm_6_1", ShaderModel::SM6_1},
        {"sm_6_0", ShaderModel::SM6_0},
    };
    
    for (auto level : levels) {
        if (pDevice->hasFeature(level.name)) {
            return level.level;
        }
    }
    return ShaderModel::Unknown;
}

Device::Device(const Device::Desc& desc) : mDesc(desc), mPhysicalDeviceName("Unknown") {
    _uid = UID++;

    // Create a global slang session passed to GFX and used for compiling programs in ProgramManager.
    slang::createGlobalSession(mSlangGlobalSession.writeRef());

    const uint32_t kTransientHeapConstantBufferSize = 16 * 1024 * 1024;

    gfx::IDevice::Desc gfxDesc = {};
    gfxDesc.deviceType = gfx::DeviceType::Vulkan;    
    gfxDesc.slang.slangGlobalSession = mSlangGlobalSession;

    // Setup shader cache.
    gfxDesc.shaderCache.maxEntryCount = mDesc.maxShaderCacheEntryCount;
    if (mDesc.shaderCachePath == "") {
        gfxDesc.shaderCache.shaderCachePath = nullptr;
    } else {
        gfxDesc.shaderCache.shaderCachePath = mDesc.shaderCachePath.c_str();
        // If the supplied shader cache path does not exist, we will need to create it before creating the device.
        if (fs::exists(mDesc.shaderCachePath)) {
            if (!fs::is_directory(mDesc.shaderCachePath))
                FALCOR_THROW("Shader cache path {} exists and is not a directory", mDesc.shaderCachePath);
        } else {
            fs::create_directories(mDesc.shaderCachePath);
        }
    }

    std::vector<void*> extendedDescs;
    // Add extended desc for root parameter attribute.
    gfx::D3D12DeviceExtendedDesc extDesc = {};
    extDesc.rootParameterShaderAttributeName = "root";
    extendedDescs.push_back(&extDesc);

    gfxDesc.extendedDescCount = extendedDescs.size();
    gfxDesc.extendedDescs = extendedDescs.data();

    // Setup debug layer.
    FALCOR_GFX_CALL(gfxSetDebugCallback(&gGFXDebugCallBack));
    if (mDesc.enableDebugLayer) {
        gfx::gfxEnableDebugLayer();
    }

    // Get list of available GPUs.
    const auto gpus = getGPUs();

    if (gpus.size() == 0) {
        FALCOR_THROW("Did not find any Vulkan GPUs !!!");
    }

    if (mDesc.gpu >= gpus.size()) {
        LLOG_WRN << "GPU index " << mDesc.gpu << " is out of range, using first GPU instead.";
        mDesc.gpu = 0;
    }


    gfxDesc.validationLayerOuputFilename = mDesc.validationLayerOuputFilename;

    // Try to create device on specific GPU.
    {
        gfxDesc.adapterLUID = reinterpret_cast<const gfx::AdapterLUID*>(&gpus[mDesc.gpu].luid);
        if (SLANG_FAILED(gfxCreateDevice(&gfxDesc, mGfxDevice.writeRef()))) {
            LLOG_ERR << "Failed to create rendering device on GPU " << mDesc.gpu << " (" <<gpus[mDesc.gpu].name  << ") !";
        }
    }

    // Otherwise try create device on any available GPU.
    if (!mGfxDevice) {
        gfxDesc.adapterLUID = nullptr;
        if (SLANG_FAILED(gfxCreateDevice(&gfxDesc, mGfxDevice.writeRef())))
            FALCOR_THROW("Failed to create rendering device !!!");
    }

    const auto& deviceInfo = mGfxDevice->getDeviceInfo();
    mInfo.adapterName = deviceInfo.adapterName;
    mInfo.adapterLUID = gfxDesc.adapterLUID ? gpus[mDesc.gpu].luid : AdapterLUID();
    mInfo.apiName = deviceInfo.apiName;
    mLimits = queryLimits(mGfxDevice);
    mSupportedFeatures = querySupportedFeatures(mGfxDevice);

    // Attempt to enable ray tracing validation if requested
    if (mDesc.enableRaytracingValidation) {
        enableRaytracingValidation();
    }

    // Vulkan always supports SER.
    mSupportedFeatures |= SupportedFeatures::ShaderExecutionReorderingAPI;

    mGfxDevice->getNativeDeviceHandles(&mInteropHandles);

    //mVkInstance = reinterpret_cast<VkInstance>(interopHandles.handles[0].handleValue);
    //mVkPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(interopHandles.handles[1].handleValue);
    //mVkDevice = reinterpret_cast<VkDevice>(interopHandles.handles[2].handleValue);

    mSupportedFeatures = querySupportedFeatures(mGfxDevice);
    mSupportedShaderModel = querySupportedShaderModel(mGfxDevice);
    mDefaultShaderModel = std::min(kDefaultShaderModel, mSupportedShaderModel);
    mGpuTimestampFrequency = 1000.0 / (double)mGfxDevice->getDeviceInfo().timestampFrequency;

    for (uint32_t i = 0; i < kInFlightFrameCount; ++i) {
        gfx::ITransientResourceHeap::Desc transientHeapDesc = {};
        transientHeapDesc.flags = gfx::ITransientResourceHeap::Flags::AllowResizing;
        transientHeapDesc.constantBufferSize = kTransientHeapConstantBufferSize;
        transientHeapDesc.samplerDescriptorCount = 2048;
        transientHeapDesc.uavDescriptorCount = 1000000;
        transientHeapDesc.srvDescriptorCount = 1000000;
        transientHeapDesc.constantBufferDescriptorCount = 1000000;
        transientHeapDesc.accelerationStructureDescriptorCount = 1000000;
        if (SLANG_FAILED(mGfxDevice->createTransientResourceHeap(transientHeapDesc, mpTransientResourceHeaps[i].writeRef())))
            FALCOR_THROW("Failed to create transient resource heap");
    }

    gfx::ICommandQueue::Desc queueDesc = {};
    queueDesc.type = gfx::ICommandQueue::QueueType::Graphics;
    if (SLANG_FAILED(mGfxDevice->createCommandQueue(queueDesc, mGfxCommandQueue.writeRef()))) {
        FALCOR_THROW("Failed to create command queue");
    }

    // The Device class contains a bunch of nested resource objects that have strong references to the device.
    // This is because we want a strong reference to the device when those objects are returned to the user.
    // However, here it immediately creates cyclic references device->resource->device upon creation of the device.
    // To break the cycles, we break the strong reference to the device for the resources that it owns.

    // Here, we temporarily increase the refcount of the device, so it won't be destroyed upon breaking the
    // nested strong references to it.
    this->incRef();

#if FALCOR_ENABLE_REF_TRACKING
    this->setEnableRefTracking(true);
#endif

    mpFrameFence = createFence();
    mpFrameFence->breakStrongReferenceToDevice();

    mpProgramManager = std::make_unique<ProgramManager>(this);

    mpProfiler = std::make_unique<Profiler>(ref<Device>(this));
    mpProfiler->breakStrongReferenceToDevice();

    mpDefaultSampler = createSampler(Sampler::Desc());
    mpDefaultSampler->breakStrongReferenceToDevice();

    mpUploadHeap = GpuMemoryHeap::create(ref<Device>(this), MemoryType::Upload, 1024 * 1024 * 2, mpFrameFence);
    mpUploadHeap->breakStrongReferenceToDevice();

    mpReadBackHeap = GpuMemoryHeap::create(ref<Device>(this), MemoryType::ReadBack, 1024 * 1024 * 2, mpFrameFence);
    mpReadBackHeap->breakStrongReferenceToDevice();

    mpTimestampQueryHeap = QueryHeap::create(ref<Device>(this), QueryHeap::Type::Timestamp, 1024 * 1024);
    mpTimestampQueryHeap->breakStrongReferenceToDevice();

    static const size_t maxTextureCount = 1024 * 10;
    const size_t threadCount = std::max(1u, std::thread::hardware_concurrency());
    mpTextureManager = std::make_unique<TextureManager>(this, maxTextureCount, threadCount);

    mpRenderContext = std::make_unique<RenderContext>(this, mGfxCommandQueue);

    // TODO: Do we need to flush here or should RenderContext::create() bind the descriptor heaps automatically without flush? See #749.
    mpRenderContext->submit(); // This will bind the descriptor heaps.

    this->decRef(false);
}

std::vector<AdapterInfo> Device::getGPUs() {
    auto adapters = gfx::gfxGetAdapters(gfx::DeviceType::Vulkan);
    std::vector<AdapterInfo> result;

    for (gfx::GfxIndex i = 0; i < adapters.getCount(); ++i) {
        const gfx::AdapterInfo& gfxInfo = adapters.getAdapters()[i];
        AdapterInfo info;
        info.name = gfxInfo.name;
        info.vendorID = gfxInfo.vendorID;
        info.deviceID = gfxInfo.deviceID;
        info.luid = *reinterpret_cast<const AdapterLUID*>(&gfxInfo.luid);
        result.push_back(info);
    }
    
    // Move all NVIDIA adapters to the start of the list.
    std::stable_partition(
        result.begin(), result.end(), [](const AdapterInfo& info) { return toLowerCase(info.name).find("nvidia") != std::string::npos; }
    );
    return result;
}

std::string& Device::getPhysicalDeviceName() {
    return mPhysicalDeviceName;
}

ref<Sampler> Device::createSampler(const Sampler::Desc& desc)
{
    return make_ref<Sampler>(ref<Device>(this), desc);
}

ref<Fence> Device::createFence(const FenceDesc& desc)
{
    return make_ref<Fence>(ref<Device>(this), desc);
}

ref<Fence> Device::createFence(bool shared)
{
    FenceDesc desc;
    desc.shared = shared;
    return createFence(desc);
}

ref<ComputeStateObject> Device::createComputeStateObject(const ComputeStateObjectDesc& desc)
{
    return make_ref<ComputeStateObject>(ref<Device>(this), desc);
}

ref<GraphicsStateObject> Device::createGraphicsStateObject(const GraphicsStateObjectDesc& desc)
{
    return make_ref<GraphicsStateObject>(ref<Device>(this), desc);
}

ref<RtStateObject> Device::createRtStateObject(const RtStateObjectDesc& desc)
{
    return make_ref<RtStateObject>(ref<Device>(this), desc);
}

bool Device::isFeatureSupported(SupportedFeatures flags) const {
    return true;
    //return is_set(mSupportedFeatures, flags);
}

void Device::executeDeferredReleases()
{
    mpUploadHeap->executeDeferredReleases();
    mpReadBackHeap->executeDeferredReleases();
    uint64_t currentValue = mpFrameFence->getCurrentValue();
    while (mDeferredReleases.size() && mDeferredReleases.front().fenceValue < currentValue) {
        mDeferredReleases.pop();
    }
}

void Device::wait() {
    mpRenderContext->submit(true);
    mpRenderContext->signal(mpFrameFence.get());
    executeDeferredReleases();
}

size_t Device::getBufferDataAlignment(ResourceBindFlags bindFlags) {
    if (is_set(bindFlags, ResourceBindFlags::Constant)) return kConstantBufferDataPlacementAlignment;
    if (is_set(bindFlags, ResourceBindFlags::Index)) return kIndexBufferDataPlacementAlignment;
    return 1;
}

Device::~Device() {
    mpRenderContext->submit(true);

    mpProfiler.reset();

    disableRaytracingValidation();

    // Release all the bound resources. Need to do that before deleting the RenderContext
    mGfxCommandQueue.setNull();
    mDeferredReleases = decltype(mDeferredReleases)();
    mpTextureManager.reset();
    mpRenderContext.reset();
    mpUploadHeap.reset();
    mpReadBackHeap.reset();
    mpTimestampQueryHeap.reset();
    for (size_t i = 0; i < kInFlightFrameCount; ++i) {
        mpTransientResourceHeaps[i].setNull();
    }

    mpDefaultSampler.reset();
    mpFrameFence.reset();
    mpProgramManager.reset();

    mDeferredReleases = decltype(mDeferredReleases)();

    mGfxDevice.setNull();
}

bool Device::isShaderModelSupported(ShaderModel shaderModel) const {
    return ((uint32_t)shaderModel <= (uint32_t)mSupportedShaderModel);
}

ResourceBindFlags Device::getFormatBindFlags(ResourceFormat format) {
    gfx::ResourceStateSet stateSet;
    FALCOR_GFX_CALL(mGfxDevice->getFormatSupportedResourceStates(getGFXFormat(format), &stateSet));

    ResourceBindFlags flags = ResourceBindFlags::None;
    if (stateSet.contains(gfx::ResourceState::ConstantBuffer)) {
        flags |= ResourceBindFlags::Constant;
    }

    if (stateSet.contains(gfx::ResourceState::VertexBuffer)) {
        flags |= ResourceBindFlags::Vertex;
    }

    if (stateSet.contains(gfx::ResourceState::IndexBuffer)) {
        flags |= ResourceBindFlags::Index;
    }

    if (stateSet.contains(gfx::ResourceState::IndirectArgument)) {
        flags |= ResourceBindFlags::IndirectArg;
    }

    if (stateSet.contains(gfx::ResourceState::StreamOutput)) {
        flags |= ResourceBindFlags::StreamOutput;
    }

    if (stateSet.contains(gfx::ResourceState::ShaderResource)) {
        flags |= ResourceBindFlags::ShaderResource;
    }

    if (stateSet.contains(gfx::ResourceState::RenderTarget)) {
        flags |= ResourceBindFlags::RenderTarget;
    }

    if (stateSet.contains(gfx::ResourceState::DepthRead) || stateSet.contains(gfx::ResourceState::DepthWrite)) {
        flags |= ResourceBindFlags::DepthStencil;
    }

    if (stateSet.contains(gfx::ResourceState::UnorderedAccess)) {
        flags |= ResourceBindFlags::UnorderedAccess;
    }

    if (stateSet.contains(gfx::ResourceState::AccelerationStructure)) {
        flags |= ResourceBindFlags::AccelerationStructure;
    }

    flags |= ResourceBindFlags::Shared;
    return flags;
}

size_t Device::getTextureRowAlignment() const {
    size_t alignment = 1;
    mGfxDevice->getTextureRowAlignment(&alignment);
    return alignment;
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
