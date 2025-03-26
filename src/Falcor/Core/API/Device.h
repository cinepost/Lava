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
#ifndef SRC_FALCOR_CORE_API_DEVICE_H_
#define SRC_FALCOR_CORE_API_DEVICE_H_

#include "Types.h"
#include "Handles.h"
#include "NativeHandle.h"
#include "Formats.h"
#include "QueryHeap.h"
#include "LowLevelContextData.h"
#include "RenderContext.h"
#include "GpuMemoryHeap.h"
#include "Falcor/Core/Macros.h"
#include "Falcor/Core/Object.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/ComputeStateObject.h"
#include "Falcor/Core/API/GraphicsStateObject.h"
#include "Falcor/Core/API/RtStateObject.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include <array>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace Falcor {

#ifdef _DEBUG
#define FALCOR_DEFAULT_ENABLE_DEBUG_LAYER true
#else
#define FALCOR_DEFAULT_ENABLE_DEBUG_LAYER false
#endif

struct DeviceApiData;

class Fbo;
class Sampler;
class CopyContext;
class RenderContext;
class TextureManager;
class ProgramManager;
class Profiler;
class PipelineCreationAPIDispatcher;
class GraphicsStateObjectDesc;

/// Holds the adapter LUID (or UUID).
/// Note: The adapter LUID is actually just 8 bytes, but on Linux the LUID is
/// not supported, so we use this to store the 16-byte UUID instead.
struct AdapterLUID {
    std::array<uint8_t, 16> luid;

    AdapterLUID() { luid.fill(0); }
    bool isValid() const { return *this != AdapterLUID(); }
    bool operator==(const AdapterLUID& other) const { return luid == other.luid; }
    bool operator!=(const AdapterLUID& other) const { return luid != other.luid; }
    bool operator<(const AdapterLUID& other) const { return luid < other.luid; }
};

struct AdapterInfo {
    /// Descriptive name of the adapter.
    std::string name;

    /// Unique identifier for the vendor.
    uint32_t vendorID;

    // Unique identifier for the physical device among devices from the vendor.
    uint32_t deviceID;

    // Logically unique identifier of the adapter.
    AdapterLUID luid;
};


class FALCOR_API Device: public Object {
    FALCOR_OBJECT(Device)
 public:
    /**
     * Maximum number of in-flight frames.
     * Typically there are at least two frames, one being rendered to, the other being presented.
     * We add one more to be on the save side.
     */
    static constexpr uint32_t kInFlightFrameCount = 3;

    /** Device configuration
    */
    struct Desc {
        uint32_t gpu = 0; ///< GPU index (indexing into GPU list returned by getGPUList()).

        ResourceFormat colorFormat = ResourceFormat::BGRA8UnormSrgb;    ///< The color buffer format
        ResourceFormat depthFormat = ResourceFormat::D32Float;          ///< The depth buffer format
        uint32_t apiMajorVersion = 1;                                   ///< Requested API major version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        uint32_t apiMinorVersion = 2;                                   ///< Requested API minor version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        bool enableVsync = false;                                       ///< Controls vertical-sync
        bool enableDebugLayer = FALCOR_DEFAULT_ENABLE_DEBUG_LAYER;      ///< Enable the debug layer. The default for release build is false, for debug build it's true.
        std::string validationLayerOuputFilename;

        std::vector<std::string> requiredExtensions;

        /// The maximum number of entries allowable in the shader cache. A value of 0 indicates no limit.
        uint32_t maxShaderCacheEntryCount = 1000;

        /// The full path to the root directory for the shader cache. An empty string will disable the cache.
        std::string shaderCachePath = (getRuntimeDirectory() / ".shadercache").string();

        /// Whether to enable ray tracing validation (requires NVAPI)
        bool enableRaytracingValidation = false;
    };

    struct Info {
        std::string adapterName;
        AdapterLUID adapterLUID;
        std::string apiName;
    };

    struct Limits {
        uint3 maxComputeDispatchThreadGroups;
        uint32_t maxShaderVisibleSamplers;
    };

    enum class SupportedFeatures {
        None = 0x0,
        ProgrammableSamplePositionsPartialOnly = 0x1, // On D3D12, this means tier 1 support. Allows one sample position to be set.
        ProgrammableSamplePositionsFull = 0x2,        // On D3D12, this means tier 2 support. Allows up to 4 sample positions to be set.
        Barycentrics = 0x4,                           // On D3D12, pixel shader barycentrics are supported.
        Raytracing = 0x8,                             // On D3D12, DirectX Raytracing is supported. It is up to the user to not use raytracing functions when not supported.
        RaytracingTier1_1 = 0x10,                     // On D3D12, DirectX Raytracing Tier 1.1 is supported.
        ConservativeRasterizationTier1 = 0x20,        // On D3D12, conservative rasterization tier 1 is supported.
        ConservativeRasterizationTier2 = 0x40,        // On D3D12, conservative rasterization tier 2 is supported.
        ConservativeRasterizationTier3 = 0x80,        // On D3D12, conservative rasterization tier 3 is supported.
        RasterizerOrderedViews = 0x100,               // On D3D12, rasterizer ordered views (ROVs) are supported.
        WaveOperations = 0x200,
        AtomicInt64 = 0x400,
        AtomicFloat = 0x800,
        ShaderExecutionReorderingAPI = 0x1000,         ///< On D3D12 and Vulkan, this means SER API is available (in the future this will be part of the shader model).
        RaytracingReordering = 0x2000,                 ///< On D3D12, this means SER is supported on the hardware.

    };

    /**
     * Constructor. Throws an exception if creation failed.
     * @param[in] desc Device configuration descriptor.
     */
    Device(const Desc& desc);
    ~Device();

    /**
     * Create a new buffer.
     * @param[in] size Size of the buffer in bytes.
     * @param[in] bindFlags Buffer bind flags.
     * @param[in] memoryType Type of memory to use for the buffer.
     * @param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer size should be at least 'size' bytes.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    ref<Buffer> createBuffer(
        size_t size,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType memoryType = MemoryType::DeviceLocal,
        const void* pInitData = nullptr
    );

    /**
     * Create a new typed buffer.
     * @param[in] format Typed buffer format.
     * @param[in] elementCount Number of elements.
     * @param[in] bindFlags Buffer bind flags.
     * @param[in] memoryType Type of memory to use for the buffer.
     * @param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    ref<Buffer> createTypedBuffer(
        ResourceFormat format,
        uint32_t elementCount,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType memoryType = MemoryType::DeviceLocal,
        const void* pInitData = nullptr
    );

    /**
     * Create a new typed buffer. The format is deduced from the template parameter.
     * @param[in] elementCount Number of elements.
     * @param[in] bindFlags Buffer bind flags.
     * @param[in] memoryType Type of memory to use for the buffer.
     * @param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    template<typename T>
    ref<Buffer> createTypedBuffer(
        uint32_t elementCount,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType memoryType = MemoryType::DeviceLocal,
        const T* pInitData = nullptr
    )
    {
        return createTypedBuffer(detail::FormatForElementType<T>::kFormat, elementCount, bindFlags, memoryType, pInitData);
    }

    /**
     * Create a new structured buffer.
     * @param[in] structSize Size of the struct in bytes.
     * @param[in] elementCount Number of elements.
     * @param[in] bindFlags Buffer bind flags.
     * @param[in] memoryType Type of memory to use for the buffer.
     * @param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
     * @param[in] createCounter True if the associated UAV counter should be created.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    ref<Buffer> createStructuredBuffer(
        uint32_t structSize,
        uint32_t elementCount,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType memoryType = MemoryType::DeviceLocal,
        const void* pInitData = nullptr,
        bool createCounter = false
    );

    /**
     * Create a new structured buffer.
     * @param[in] pType Type of the structured buffer.
     * @param[in] elementCount Number of elements.
     * @param[in] bindFlags Buffer bind flags.
     * @param[in] memoryType Type of memory to use for the buffer.
     * @param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
     * @param[in] createCounter True if the associated UAV counter should be created.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    ref<Buffer> createStructuredBuffer(
        const ReflectionType* pType,
        uint32_t elementCount,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType memoryType = MemoryType::DeviceLocal,
        const void* pInitData = nullptr,
        bool createCounter = false
    );

    /**
     * Create a new structured buffer.
     * @param[in] shaderVar ShaderVar pointing to the buffer variable.
     * @param[in] elementCount Number of elements.
     * @param[in] bindFlags Buffer bind flags.
     * @param[in] memoryType Type of memory to use for the buffer.
     * @param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
     * @param[in] createCounter True if the associated UAV counter should be created.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    ref<Buffer> createStructuredBuffer(
        const ShaderVar& shaderVar,
        uint32_t elementCount,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType memoryType = MemoryType::DeviceLocal,
        const void* pInitData = nullptr,
        bool createCounter = false
    );

    /**
     * Create a new buffer from an existing resource.
     * @param[in] pResource Already allocated resource.
     * @param[in] size The size of the buffer in bytes.
     * @param[in] bindFlags Buffer bind flags. Flags must match the bind flags of the original resource.
     * @param[in] memoryType Type of memory to use for the buffer. Flags must match those of the heap the original resource is
     * allocated on.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    ref<Buffer> createBufferFromResource(gfx::IBufferResource* pResource, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType);

    /**
     * Create a new buffer from an existing native handle.
     * @param[in] handle Handle of already allocated resource.
     * @param[in] size The size of the buffer in bytes.
     * @param[in] bindFlags Buffer bind flags. Flags must match the bind flags of the original resource.
     * @param[in] memoryType Type of memory to use for the buffer. Flags must match those of the heap the original resource is
     * allocated on.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    ref<Buffer> createBufferFromNativeHandle(NativeHandle handle, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType);

    /**
     * Create a 1D texture.
     * @param[in] width The width of the texture.
     * @param[in] format The format of the texture.
     * @param[in] arraySize The array size of the texture.
     * @param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is
     * given then the data for at least that number of miplevels must be provided.
     * @param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
     * @param[in] bindFlags The requested bind flags for the resource.
     * @return A pointer to a new texture, or throws an exception if creation failed.
     */
    ref<Texture> createTexture1D(
        uint32_t width,
        ResourceFormat format,
        uint32_t arraySize = 1,
        uint32_t mipLevels = Resource::kMaxPossible,
        const void* pInitData = nullptr,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource
    );

    /**
     * Create a 2D texture.
     * @param[in] width The width of the texture.
     * @param[in] height The height of the texture.
     * @param[in] format The format of the texture.
     * @param[in] arraySize The array size of the texture.
     * @param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is
     * given then the data for at least that number of miplevels must be provided.
     * @param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
     * @param[in] bindFlags The requested bind flags for the resource.
     * @return A pointer to a new texture, or throws an exception if creation failed.
     */
    ref<Texture> createTexture2D(
        uint32_t width,
        uint32_t height,
        ResourceFormat format,
        uint32_t arraySize = 1,
        uint32_t mipLevels = Resource::kMaxPossible,
        const void* pInitData = nullptr,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource
    );

    /**
     * Create a 3D texture.
     * @param[in] width The width of the texture.
     * @param[in] height The height of the texture.
     * @param[in] depth The depth of the texture.
     * @param[in] format The format of the texture.
     * @param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is
     * given then the data for at least that number of miplevels must be provided.
     * @param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
     * @param[in] bindFlags The requested bind flags for the resource.
     * @return A pointer to a new texture, or throws an exception if creation failed.
     */
    ref<Texture> createTexture3D(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        ResourceFormat format,
        uint32_t mipLevels = Resource::kMaxPossible,
        const void* pInitData = nullptr,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource
    );

    /**
     * Create a cube texture.
     * @param[in] width The width of the texture.
     * @param[in] height The height of the texture.
     * @param[in] format The format of the texture.
     * @param[in] arraySize The array size of the texture.
     * @param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is
     * given then the data for at least that number of miplevels must be provided.
     * @param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
     * @param[in] bindFlags The requested bind flags for the resource.
     * @return A pointer to a new texture, or throws an exception if creation failed.
     */
    ref<Texture> createTextureCube(
        uint32_t width,
        uint32_t height,
        ResourceFormat format,
        uint32_t arraySize = 1,
        uint32_t mipLevels = Resource::kMaxPossible,
        const void* pInitData = nullptr,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource
    );

    /**
     * Create a multi-sampled 2D texture.
     * @param[in] width The width of the texture.
     * @param[in] height The height of the texture.
     * @param[in] format The format of the texture.
     * @param[in] sampleCount The sample count of the texture.
     * @param[in] arraySize The array size of the texture.
     * @param[in] bindFlags The requested bind flags for the resource.
     * @return A pointer to a new texture, or throws an exception if creation failed.
     */
    ref<Texture> createTexture2DMS(
        uint32_t width,
        uint32_t height,
        ResourceFormat format,
        uint32_t sampleCount,
        uint32_t arraySize = 1,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource
    );

    /**
     * Create a new texture from an resource.
     * @param[in] pResource Already allocated resource.
     * @param[in] type The type of texture.
     * @param[in] format The format of the texture.
     * @param[in] width The width of the texture.
     * @param[in] height The height of the texture.
     * @param[in] depth The depth of the texture.
     * @param[in] arraySize The array size of the texture.
     * @param[in] mipLevels The number of mip levels.
     * @param[in] sampleCount The sample count of the texture.
     * @param[in] bindFlags Texture bind flags. Flags must match the bind flags of the original resource.
     * @param[in] initState The initial resource state.
     * @return A pointer to a new texture, or throws an exception if creation failed.
     */
    ref<Texture> createTextureFromResource(
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
        Resource::State initState
    );

    /** Device unique id.
    */
    uint8_t uid() const { return _uid; }

    /** Acts as the destructor for Device. Some resources use gpDevice in their cleanup.
        Cleaning up the SharedPtr directly would clear gpDevice before calling destructors.
    */
    void cleanup();

    TextureManager* textureManager() { return mpTextureManager.get(); }

    /** Get physical device name
    */
    std::string& getPhysicalDeviceName();

    const VmaAllocator& allocator() const { return mGfxDevice->getVmaAllocator(); }

    /** Check if the window is occluded
    */
    bool isWindowOccluded() const;

    /** Get the FBO object associated with the swap-chain.
        This can change each frame, depending on the API used
    */
    std::shared_ptr<Fbo> getSwapChainFbo() const;

    /** Get the FBO object used for headless rendering.
        This can change each frame, depending on the API used
    */
    std::shared_ptr<Fbo> getOffscreenFbo() const;

    /** Get the default render-context.
        The default render-context is managed completely by the device. The user should just queue commands into it, the device will take care of allocation, submission and synchronization
    */
    RenderContext* getRenderContext() const { return mpRenderContext.get(); }

    /** Flushes pipeline, releases resources, and blocks until completion
    */
    void flushAndSync();

    /** Check if vertical sync is enabled
    */
    bool isVsyncEnabled() const { return mDesc.enableVsync; }

    /** Get the desc
    */
    const Desc& getDesc() const { return mDesc; }

    /**
     * Flushes pipeline, releases resources, and blocks until completion
     */
    void wait();

    /**
     * Create a new sampler object.
     * @param[in] desc Describes sampler settings.
     * @return A new object, or throws an exception if creation failed.
     */
    ref<Sampler> createSampler(const Sampler::Desc& desc);

    /** Get default sampler object
    */
    const ref<Sampler>& getDefaultSampler() const { return mpDefaultSampler; }

    /**
     * Create a new fence object.
     * @return A new object, or throws an exception if creation failed.
     */
    ref<Fence> createFence(const FenceDesc& desc);

    /**
     * Create a new fence object.
     * @return A new object, or throws an exception if creation failed.
     */
    ref<Fence> createFence(bool shared = false);

    /// Create a compute state object.
    ref<ComputeStateObject> createComputeStateObject(const ComputeStateObjectDesc& desc);
    /// Create a graphics state object.
    ref<GraphicsStateObject> createGraphicsStateObject(const GraphicsStateObjectDesc& desc);
    /// Create a raytracing state object.
    ref<RtStateObject> createRtStateObject(const RtStateObjectDesc& desc);

    ProgramManager* getProgramManager() const { return mpProgramManager.get(); }

    Profiler* getProfiler() const { return mpProfiler.get(); }

    size_t getBufferDataAlignment(ResourceBindFlags bindFlags);

    const ref<GpuMemoryHeap>& getUploadHeap() const { return mpUploadHeap; }
    const ref<GpuMemoryHeap>& getReadBackHeap() const { return mpReadBackHeap; }
    const ref<QueryHeap>& getTimestampQueryHeap() const { return mpTimestampQueryHeap; }
    double getGpuTimestampFrequency() const { return mGpuTimestampFrequency; }  // ms/tick

    /** Check if features are supported by the device
    */
    bool isFeatureSupported(SupportedFeatures flags) const;

    /**
     * Get a list of all available GPUs.
     */
    static std::vector<AdapterInfo> getGPUs();

    /**
     * Get the global device mutex.
     * WARNING: Falcor is generally not thread-safe. This mutex is used in very specific
     * places only, currently only for doing parallel texture loading.
     */
    std::mutex& getGlobalGfxMutex() { return mGlobalGfxMutex; }

    uint32_t subgroupSize() const;

    void releaseResource(ISlangUnknown* pResource) { releaseResource(ApiObjectHandle(pResource)); }

    gfx::ITransientResourceHeap* getCurrentTransientResourceHeap();

    uint32_t getMaxComputeWorkgroupSubgroups() const;

    const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const;

    /** Check if a shader model is supported by the device
    */
    bool isShaderModelSupported(ShaderModel shaderModel) const;

    /** Return the highest supported shader model by the device
    */
    ShaderModel getSupportedShaderModel() const { return mSupportedShaderModel; }

    /**
     * Return the default shader model to use
     */
    ShaderModel getDefaultShaderModel() const { return mDefaultShaderModel; }

    /// Returns the global slang session.
    slang::IGlobalSession* getSlangGlobalSession() const { return mSlangGlobalSession; }

    /* Return the GFX command queue.
    */
    gfx::IDevice* getGfxDevice() const { return mGfxDevice; }
    
    /* Return the GFX command queue.
    */
    gfx::ICommandQueue* getGfxCommandQueue() const { return mGfxCommandQueue; }

    /**
     * Get the supported bind-flags for a specific format.
     */
    ResourceBindFlags getFormatBindFlags(ResourceFormat format);

    /// Get the texture row memory alignment in bytes.
    size_t getTextureRowAlignment() const;

 private:
    struct ResourceRelease {
        uint64_t fenceValue;
        Slang::ComPtr<ISlangUnknown> mObject;
    };

    std::queue<ResourceRelease> mDeferredReleases;

    void executeDeferredReleases();

    void enableRaytracingValidation();
    void disableRaytracingValidation();

    Desc mDesc;
    Slang::ComPtr<slang::IGlobalSession> mSlangGlobalSession;
    Slang::ComPtr<gfx::IDevice> mGfxDevice;
    Slang::ComPtr<gfx::ICommandQueue> mGfxCommandQueue;
    Slang::ComPtr<gfx::ITransientResourceHeap> mpTransientResourceHeaps[kInFlightFrameCount];
    uint32_t mCurrentTransientResourceHeapIndex = 0;

    gfx::IDevice::InteropHandles mInteropHandles;

    ref<Sampler> mpDefaultSampler;
    ref<GpuMemoryHeap> mpUploadHeap;
    ref<GpuMemoryHeap> mpReadBackHeap;
    ref<QueryHeap> mpTimestampQueryHeap;

    ref<Fence> mpFrameFence;

    std::unique_ptr<RenderContext> mpRenderContext;
    double mGpuTimestampFrequency;

    Info mInfo;
    Limits mLimits;

    SupportedFeatures mSupportedFeatures = SupportedFeatures::None;
    ShaderModel mSupportedShaderModel = ShaderModel::Unknown;
    ShaderModel mDefaultShaderModel = ShaderModel::Unknown;

  protected:
    std::string mPhysicalDeviceName;

    uint8_t _uid;
    static std::atomic<std::uint8_t> UID;

    std::unique_ptr<TextureManager>     mpTextureManager;
    std::unique_ptr<ProgramManager>     mpProgramManager;
    std::unique_ptr<Profiler>           mpProfiler;

    friend class DeviceManager;
    friend class ResourceManager;

#ifdef FALCOR_VK
 public:
    VkPhysicalDeviceFeatures                            mDeviceFeatures;
    VkPhysicalDeviceFeatures2                           mPhysicalDeviceFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR     mRayTracingPipelineProperties{};

    VkPhysicalDeviceCoherentMemoryFeaturesAMD           mEnabledDeviceCoherentMemoryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD };
    VkPhysicalDeviceBufferDeviceAddressFeatures         mEnabledBufferDeviceAddresFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR       mEnabledRayTracingPipelineFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    VkPhysicalDeviceRayQueryFeaturesKHR                 mEnabledRayQueryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    VkPhysicalDeviceMemoryPriorityFeaturesEXT           mEnabledMemoryPriorityFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR    mEnabledAccelerationStructureFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceSynchronization2FeaturesKHR         mEnabledSynchronization2Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR };
    VkPhysicalDeviceHostQueryResetFeatures              mEnabledHostQueryResetFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES };
#endif

    std::mutex mGlobalGfxMutex;
};

inline constexpr uint32_t getMaxViewportCount() {
    return 8;
}

ENUM_CLASS_OPERATORS(Device::SupportedFeatures);

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_DEVICE_H_
