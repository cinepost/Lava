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

#include <list>
#include <string>
#include <memory>
#include <queue>
#include <vector>
#include <atomic>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/Window.h"
#include "Falcor/Core/API/LowLevelContextData.h"
#include "Falcor/Core/API/GpuMemoryHeap.h"
#include "Falcor/Core/API/QueryHeap.h"
#include "Falcor/Core/API/ResourceViews.h"

namespace Falcor {

#ifdef _DEBUG
#define FALCOR_DEFAULT_ENABLE_DEBUG_LAYER true
#else
#define FALCOR_DEFAULT_ENABLE_DEBUG_LAYER false
#endif

struct DeviceApiData;

class Fbo;
class Sampler;
class ResourceManager;
class TextureManager;
class RenderContext;

class dlldecl Device: public std::enable_shared_from_this<Device> {
 public:
    using SharedPtr = std::shared_ptr<Device>;
    using SharedConstPtr = std::shared_ptr<const Device>;
    using ApiHandle = DeviceHandle;
    using DeviceLocalUID = uint32_t;
    
    static const uint32_t kQueueTypeCount = (uint32_t)LowLevelContextData::CommandQueueType::Count;
    static constexpr uint32_t kSwapChainBuffersCount = 3;

    /** Device configuration
    */
    struct Desc {
        ResourceFormat colorFormat = ResourceFormat::BGRA8UnormSrgb;    ///< The color buffer format
        ResourceFormat depthFormat = ResourceFormat::D32Float;          ///< The depth buffer format
        uint32_t apiMajorVersion = 1;                                   ///< Requested API major version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        uint32_t apiMinorVersion = 2;                                   ///< Requested API minor version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        bool enableVsync = false;                                       ///< Controls vertical-sync
        bool enableDebugLayer = FALCOR_DEFAULT_ENABLE_DEBUG_LAYER;      ///< Enable the debug layer. The default for release build is false, for debug build it's true.

        static_assert((uint32_t)LowLevelContextData::CommandQueueType::Direct == 2, "Default initialization of cmdQueues assumes that Direct queue index is 2");
        std::array<uint32_t, kQueueTypeCount> cmdQueues = { 0, 0, 2 };  ///< Command queues to create. If no direct-queues are created, mpRenderContext will not be initialized

        std::vector<std::string> requiredExtensions;

        uint32_t width = 1280;                                          ///< Headless FBO width
        uint32_t height = 720;                                          ///< Headless FBO height

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
    };

    enum class ShaderModel : uint32_t {
        Unknown,
        SM6_0,
        SM6_1,
        SM6_2,
        SM6_3,
        SM6_4,
        SM6_5,
        SM6_6,
        SM6_7,
    };

    using MemoryType = GpuMemoryHeap::Type;

    /** Device unique id.
    */
    inline uint8_t uid() const { return _uid; }

    /** Acts as the destructor for Device. Some resources use gpDevice in their cleanup.
        Cleaning up the SharedPtr directly would clear gpDevice before calling destructors.
    */
    void cleanup();

    inline std::shared_ptr<ResourceManager>& resourceManager() { return mpResourceManager; }
    inline std::shared_ptr<TextureManager>& textureManager() { return mpTextureManager; }

    /** Enable/disable vertical sync
    */
    void toggleVSync(bool enable);

    inline bool isHeadless() const { return mHeadless; };

    /** Get physical device name
    */
    std::string& getPhysicalDeviceName();

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
    inline RenderContext* getRenderContext() const { return mpRenderContext.get(); }

    /** Get the command queue handle
    */
    CommandQueueHandle getCommandQueueHandle(LowLevelContextData::CommandQueueType type, uint32_t index) const;

#if FALCOR_GFX_VK
    VkQueue            getCommandQueueNativeHandle(LowLevelContextData::CommandQueueType type, uint32_t index) const;
#endif  // FALCOR_GFX_VK

    /** Get the API queue type.
        \return API queue type, or throws an exception if type is unknown.
    */
    ApiCommandQueueType getApiCommandQueueType(LowLevelContextData::CommandQueueType type) const;

    /** Get the native API handle
    */
    inline const DeviceHandle& getApiHandle() { return mApiHandle; }

#if FALCOR_GFX_VK
    inline VkPhysicalDevice getApiNativeHandle() const { return mVkPhysicalDevice; }
#endif

    /** Get a D3D12 handle for user code that wants to call D3D12 directly.
        \return A valid ID3D12Device* value for all backend that are using D3D12, otherwise nullptr.
    */
    const D3D12DeviceHandle getD3D12Handle();

    /** Present the back-buffer to the window
    */
    void present();

    /** Flushes pipeline, releases resources, and blocks until completion
    */
    void flushAndSync();

    /** Check if vertical sync is enabled
    */
    inline bool isVsyncEnabled() const { return mDesc.enableVsync; }

    /** Resize the swap-chain
        \return A new FBO object
    */
    std::shared_ptr<Fbo> resizeSwapChain(uint32_t width, uint32_t height);

    /** Get the desc
    */
    inline const Desc& getDesc() const { return mDesc; }

    /** Get default sampler object
    */
    inline std::shared_ptr<Sampler> getDefaultSampler() const { return mpDefaultSampler; }

    /** Create a new query heap.
        \param[in] type Type of queries.
        \param[in] count Number of queries.
        \return New query heap.
    */
    std::weak_ptr<QueryHeap> createQueryHeap(QueryHeap::Type type, uint32_t count);

#if FALCOR_D3D12_AVAILABLE
        const D3D12DescriptorPool::SharedPtr& getD3D12CpuDescriptorPool() const { return mpD3D12CpuDescPool; }
        const D3D12DescriptorPool::SharedPtr& getD3D12GpuDescriptorPool() const { return mpD3D12GpuDescPool; }
#endif // FALCOR_D3D12_AVAILABLE

    DeviceApiData* getApiData() const { return mpApiData; }

    const GpuMemoryHeap::SharedPtr& getUploadHeap() const { return mpUploadHeap; }
    double getGpuTimestampFrequency() const { return mGpuTimestampFrequency; }  // ms/tick

    /** Check if features are supported by the device
    */
    bool isFeatureSupported(SupportedFeatures flags) const;

    void releaseResource(ApiObjectHandle pResource);

#ifdef FALCOR_GFX
    void releaseResource(ISlangUnknown* pResource) { releaseResource(ApiObjectHandle(pResource)); }

    gfx::ITransientResourceHeap* getCurrentTransientResourceHeap();


#if FALCOR_GFX_VK
    inline VkInstance       getVkInstance() const { return mVkInstance; };
    inline VkPhysicalDevice getVkPhysicalDevice() const { return mVkPhysicalDevice; }
    inline VkDevice         getVkDevice() const { return mVkDevice; };
    inline VkSurfaceKHR     getVkSurface() const { return mVkSurface; };    
#endif  // FALCOR_GFX_VK
#endif  // FALCOR_GFX

#ifdef FALCOR_VK
    uint32_t getVkMemoryType(GpuMemoryHeap::Type falcorType, uint32_t memoryTypeBits) const;

    /** Get the index of a memory type that has all the requested property bits set
        *
        * @param typeBits Bitmask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
        * @param properties Bitmask of properties for the memory type to request
        * @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
        * 
        * @return Index of the requested memory type
        *
        * @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
        */
    uint32_t getVkMemoryTypeNative(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr) const;
    
    const VkPhysicalDeviceLimits& getPhysicalDeviceLimits() const;
    uint32_t  getDeviceVendorID() const;
#endif  // FALCOR_VK

    inline DeviceApiData* apiData() const { return mpApiData; };

    /** Check if a shader model is supported by the device
    */
    bool isShaderModelSupported(ShaderModel shaderModel) const;

    /** Return the highest supported shader model by the device
    */
    ShaderModel getSupportedShaderModel() const { return mSupportedShaderModel; }

    /** Return the current index of the back buffer being rendered to.
    */
    uint32_t getCurrentBackBufferIndex() const { return mCurrentBackBufferIndex; }

 private:
    Device(Window::SharedPtr pWindow, const Desc& desc);

    struct ResourceRelease {
        size_t frameID;
        ApiObjectHandle pApiObject;
    };

    std::shared_ptr<Sampler> mpDefaultSampler = nullptr;
    std::queue<ResourceRelease> mDeferredReleases;

    uint32_t mCurrentBackBufferIndex;
    std::shared_ptr<Fbo> mpSwapChainFbos[kSwapChainBuffersCount];
    std::shared_ptr<Fbo> mpOffscreenFbo;

    void executeDeferredReleases();
    void releaseFboData();
    void release();

    bool updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);
    bool updateOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);

    Desc mDesc;
    ApiHandle mApiHandle;
    GpuMemoryHeap::SharedPtr mpUploadHeap;

#if FALCOR_D3D12_AVAILABLE
    D3D12DescriptorPool::SharedPtr mpD3D12CpuDescPool;
    D3D12DescriptorPool::SharedPtr mpD3D12GpuDescPool;
#endif
    bool mIsWindowOccluded = false;
    GpuFence::SharedPtr mpFrameFence;

#if FALCOR_GFX_VK
    VkPhysicalDevice    mVkPhysicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR        mVkSurface        = VK_NULL_HANDLE;    
    VkDevice            mVkDevice         = VK_NULL_HANDLE;
    VkInstance          mVkInstance       = VK_NULL_HANDLE;

    std::vector<VkQueue>            mCmdNativeQueues[kQueueTypeCount];
#endif

    Window::SharedPtr mpWindow = nullptr;
    DeviceApiData* mpApiData;
    std::shared_ptr<RenderContext> mpRenderContext = nullptr;
    size_t mFrameID = 0;
    std::list<QueryHeap::SharedPtr> mTimestampQueryHeaps;
    double mGpuTimestampFrequency;

    std::vector<CommandQueueHandle> mCmdQueues[kQueueTypeCount];

    bool mHeadless = false;

    SupportedFeatures mSupportedFeatures = SupportedFeatures::None;
    ShaderModel mSupportedShaderModel = ShaderModel::Unknown;

    // API specific functions
    bool getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle &apiHandle);
    bool getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle apiHandles[kSwapChainBuffersCount], uint32_t& currentBackBufferIndex);
    void destroyApiObjects();
    void apiPresent();
    bool apiInit();

    bool createSwapChain(ResourceFormat colorFormat);
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
    static SharedPtr create(Window::SharedPtr pWindow, const Desc& desc);

#ifdef FALCOR_GFX
    /** Create a new rendering(headless) device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(const gfx::IDevice::Desc& idesc, const Desc& desc);

    /** Create a new display device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(Window::SharedPtr pWindow, const gfx::IDevice::Desc& idesc, const Desc& desc);
#endif

    inline const NullResourceViews& nullResourceViews() const { return mNullViews; };

  protected:
    bool init();

    void createNullViews();
    void releaseNullViews();

    std::string mPhysicalDeviceName;

    uint8_t _uid;
    static std::atomic<std::uint8_t> UID;

#ifdef FALCOR_GFX
    gfx::IDevice::Desc mIDesc; // device creation gfx::IDevice::Desc
#endif

    bool mUseIDesc = false; // create device using gfx::IDevice::Desc

    NullResourceViews mNullViews;

    std::shared_ptr<ResourceManager> mpResourceManager = nullptr;
    std::shared_ptr<TextureManager>  mpTextureManager = nullptr;

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

enum_class_operators(Device::SupportedFeatures);

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_DEVICE_H_
