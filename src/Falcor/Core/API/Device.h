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

#include "Falcor/Core/Framework.h"

#include "Falcor/Core/API/Common.h"
#include "Falcor/Core/API/NativeHandle.h"
#include "Falcor/Core/API/Formats.h"
#include "Falcor/Core/API/QueryHeap.h"
#include "Falcor/Core/API/LowLevelContextData.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/GpuMemoryHeap.h"

#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Sampler.h"

#include "Falcor/Core/Object.h"
#include "Falcor/Core/Window.h"

#include "GFXAPI.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include <list>
#include <string>
#include <memory>
#include <queue>
#include <vector>
#include <atomic>


namespace Falcor {

#ifdef _DEBUG
#define FALCOR_DEFAULT_ENABLE_DEBUG_LAYER true
#else
#define FALCOR_DEFAULT_ENABLE_DEBUG_LAYER false
#endif

struct DeviceApiData;

class Fbo;
class ShaderVar;
class TextureManager;
class ProgramManager;

class FALCOR_API Device: public Object {
    FALCOR_OBJECT(Device)
  public:
    using BreakableSharedPtr = Falcor::BreakableSharedPtr<Device>;
    using DeviceLocalUID = uint32_t;

    static constexpr uint32_t kInFlightFrameCount = 3;

    ~Device();

    using IDesc = gfx::IDevice::Desc;

    /** Device configuration
    */
    struct Desc {
        ResourceFormat colorFormat = ResourceFormat::BGRA8UnormSrgb;    ///< The color buffer format
        ResourceFormat depthFormat = ResourceFormat::D32Float;          ///< The depth buffer format
        uint32_t apiMajorVersion = 1;                                   ///< Requested API major version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        uint32_t apiMinorVersion = 2;                                   ///< Requested API minor version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        bool enableVsync = false;                                       ///< Controls vertical-sync
        bool enableDebugLayer = FALCOR_DEFAULT_ENABLE_DEBUG_LAYER;      ///< Enable the debug layer. The default for release build is false, for debug build it's true.
        std::string validationLayerOuputFilename;

        std::vector<std::string> requiredExtensions;

        uint32_t width = 1280;                                          ///< Headless FBO width
        uint32_t height = 720;                                          ///< Headless FBO height

        /// The maximum number of entries allowable in the shader cache. A value of 0 indicates no limit.
        int maxShaderCacheEntryCount = 0;

        /// The full path to the root directory for the shader cache. An empty string will disable the cache.
        std::string shaderCachePath;

#ifdef FALCOR_VK
        VkSurfaceKHR surface = VK_NULL_HANDLE;
#endif

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
    };

    /** Device unique id.
    */
    uint8_t uid() const { return _uid; }

        /**
     * Create a new buffer.
     * @param[in] size Size of the buffer in bytes.
     * @param[in] bindFlags Buffer bind flags.
     * @param[in] memoryType Type of memory to use for the buffer.
     * @param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer size should be at least 'size' bytes.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    Falcor::SharedPtr<Buffer> createBuffer(
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
    Falcor::SharedPtr<Buffer> createTypedBuffer(
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
    Falcor::SharedPtr<Buffer> createTypedBuffer(
        uint32_t elementCount,
        ResourceBindFlags bindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType memoryType = MemoryType::DeviceLocal,
        const T* pInitData = nullptr
    )
    {
        return createTypedBuffer(FormatForElementType<T>::kFormat, elementCount, bindFlags, memoryType, pInitData);
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
    Falcor::SharedPtr<Buffer> createStructuredBuffer(
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
    Falcor::SharedPtr<Buffer> createStructuredBuffer(
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
    Falcor::SharedPtr<Buffer> createStructuredBuffer(
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
    Falcor::SharedPtr<Buffer> createBufferFromResource(gfx::IBufferResource* pResource, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType);

    /**
     * Create a new buffer from an existing native handle.
     * @param[in] handle Handle of already allocated resource.
     * @param[in] size The size of the buffer in bytes.
     * @param[in] bindFlags Buffer bind flags. Flags must match the bind flags of the original resource.
     * @param[in] memoryType Type of memory to use for the buffer. Flags must match those of the heap the original resource is
     * allocated on.
     * @return A pointer to a new buffer object, or throws an exception if creation failed.
     */
    Falcor::SharedPtr<Buffer> createBufferFromNativeHandle(VkBuffer handle, size_t size, ResourceBindFlags bindFlags, MemoryType memoryType);

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
    Falcor::SharedPtr<Texture> createTextureFromResource(
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

    Falcor::SharedPtr<Sampler> createSampler(const Sampler::Desc& desc);

    /**
     * Create a new fence object.
     * @return A new object, or throws an exception if creation failed.
     */
    Falcor::SharedPtr<Fence> createFence(const FenceDesc& desc);

    /**
     * Create a new fence object.
     * @return A new object, or throws an exception if creation failed.
     */
    Falcor::SharedPtr<Fence> createFence(bool shared = false);

    TextureManager* getTextureManager() { return mpTextureManager.get(); }

    ProgramManager* getProgramManager() const { return mpProgramManager.get(); }

    bool setShaderCache(const std::string& shaderCachePath, int maxShaderCacheEntryCount = 0);

    gfx::ShaderCacheStats getShaderCacheStats() const;

    bool resetShaderCacheStats();


    /** Enable/disable vertical sync
    */
    void toggleVSync(bool enable);

    bool isHeadless() const { return mHeadless; };

    /** Get physical device name
    */
    const std::string& getPhysicalDeviceName() const { return mPhysicalDeviceName; } 

    const VmaAllocator& allocator() const { return mGfxDevice->getVmaAllocator(); }

    /** Check if the window is occluded
    */
    bool isWindowOccluded() const;

    /** Get the default render-context.
        The default render-context is managed completely by the device. The user should just queue commands into it, the device will take care of allocation, submission and synchronization
    */
    RenderContext* getRenderContext() const { return mpRenderContext.get(); }

    VkPhysicalDevice getApiNativeHandle() const;

    /** Present the back-buffer to the window
    */
    void present();

    /** Flushes pipeline, releases resources, and blocks until completion
    */
    void wait();

    /** Check if vertical sync is enabled
    */
    bool isVsyncEnabled() const { return mDesc.enableVsync; }

    /** Get the desc
    */
    const Desc& getDesc() const { return mDesc; }

    /** Get default sampler object
    */
    const Falcor::SharedPtr<Sampler>& getDefaultSampler() const;

    DeviceApiData* getApiData() const { return mpApiData; }

    size_t getBufferDataAlignment(ResourceBindFlags bindFlags);

    const Falcor::SharedPtr<GpuMemoryHeap>& getReadBackHeap() const { return mpReadBackHeap; }
    const Falcor::SharedPtr<GpuMemoryHeap>& getUploadHeap() const { return mpUploadHeap; }
    const Falcor::SharedPtr<QueryHeap>& getTimestampQueryHeap() const { return mpTimestampQueryHeap; }

    double getGpuTimestampFrequency() const { return mGpuTimestampFrequency; }  // ms/tick

    /** Check if features are supported by the device
    */
    bool isFeatureSupported(SupportedFeatures flags) const;

    uint32_t subgroupSize() const;

    /**
     * Return the default shader model to use
     */
    ShaderModel getDefaultShaderModel() const { return mDefaultShaderModel; }

    gfx::ITransientResourceHeap* getCurrentTransientResourceHeap();

    /// Returns the global slang session.
    slang::IGlobalSession* getSlangGlobalSession() const { return mSlangGlobalSession; }

    std::vector<std::string> getFeatures() const { return mGfxDevice->getFeatures(); }

    void releaseResource(ISlangUnknown* pResource);

    uint64_t getMinAccelerationStructureScratchOffsetAlignment() const;

    const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const;

    DeviceApiData* apiData() const { return mpApiData; };

    /** Check if a shader model is supported by the device
    */
    bool isShaderModelSupported(ShaderModel shaderModel) const;

    /** Return the highest supported shader model by the device
    */
    ShaderModel getSupportedShaderModel() const { return mSupportedShaderModel; }

    /** Return the current index of the back buffer being rendered to.
    */
    uint32_t getCurrentBackBufferIndex() const { return mCurrentBackBufferIndex; }

    /* Return the GFX command queue.
    */
    gfx::IDevice* getGfxDevice() const { return mGfxDevice; }
    
    /* Return the GFX command queue.
    */
    gfx::ICommandQueue* getGfxCommandQueue() const { return mGfxCommandQueue; }

    Window::SharedPtr getWindow() { return mpWindow; }

 private:
    Device(Window::SharedPtr pWindow, const Desc& desc);

    struct ResourceRelease {
        uint64_t fenceValue;
        Slang::ComPtr<ISlangUnknown> mObject;
    };

    uint32_t mCurrentTransientResourceHeapIndex = 0;
    Falcor::SharedPtr<Sampler> mpDefaultSampler;
    std::queue<ResourceRelease> mDeferredReleases;

    uint32_t mCurrentBackBufferIndex;
    Falcor::SharedPtr<Fbo> mpSwapChainFbos[kInFlightFrameCount];
    Falcor::SharedPtr<Fbo> mpOffscreenFbo;

    void executeDeferredReleases();
    void release();

    Desc mDesc;
    Slang::ComPtr<gfx::IDevice>         mGfxDevice;
    Falcor::SharedPtr<GpuMemoryHeap>    mpReadBackHeap;
    Falcor::SharedPtr<GpuMemoryHeap>    mpUploadHeap;
    Falcor::SharedPtr<QueryHeap>        mpTimestampQueryHeap;
    Slang::ComPtr<slang::IGlobalSession> mSlangGlobalSession;

    bool mIsWindowOccluded = false;
    Fence::SharedPtr mpFrameFence;

    Slang::ComPtr<gfx::ICommandQueue> mGfxCommandQueue;
    Slang::ComPtr<gfx::ITransientResourceHeap> mpTransientResourceHeaps[kInFlightFrameCount];

    Window::SharedPtr mpWindow = nullptr;
    DeviceApiData* mpApiData;
    std::unique_ptr<RenderContext> mpRenderContext = nullptr;
    size_t mFrameID = 0;
    std::list<QueryHeap::SharedPtr> mTimestampQueryHeaps;
    double mGpuTimestampFrequency;

    bool mHeadless = false;

    SupportedFeatures mSupportedFeatures = SupportedFeatures::None;
    ShaderModel mSupportedShaderModel = ShaderModel::Unknown;
    ShaderModel mDefaultShaderModel = ShaderModel::Unknown;

    // API specific functions
    bool getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle &apiHandle);
    bool getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle apiHandles[kInFlightFrameCount], uint32_t& currentBackBufferIndex);
    void apiPresent();

    bool apiInit(const std::string& validationLayerOuputFilename);

    bool createSwapChain(ResourceFormat colorFormat);

#if defined(FALCOR_VK)
    bool createSwapChain(uint32_t width, uint32_t height, ResourceFormat colorFormat);
#endif

    bool createOffscreenFBO(ResourceFormat colorFormat);

    void apiResizeSwapChain(uint32_t width, uint32_t height, ResourceFormat colorFormat);
    void apiResizeOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat);

    void toggleFullScreen(bool fullscreen);

 public:
    /** Create a new rendering(headless) device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(const Desc& desc);

    /** Create a new display device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(Falcor::SharedPtr<Window>pWindow, const Desc& desc);

    /** Create a new rendering(headless) device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(const Device::IDesc& idesc, const Desc& desc);

    /** Create a new display device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(Falcor::SharedPtr<Window> pWindow, const Device::IDesc& idesc, const Desc& desc);

  protected:
    bool init();

    std::string mPhysicalDeviceName;

    uint8_t _uid;
    static std::atomic<std::uint8_t> UID;

    IDesc mIDesc; // device creation using gfx::IDevice::Desc

    bool mUseIDesc = false; // create device using gfx::IDevice::Desc
    bool mInitialized;

    std::unique_ptr<TextureManager>  mpTextureManager;
    std::unique_ptr<ProgramManager>  mpProgramManager;

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
};

inline constexpr uint32_t getMaxViewportCount() {
    return 8;
}


enum_class_operators(Device::SupportedFeatures);

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_DEVICE_H_
