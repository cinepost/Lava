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

#include "Falcor/Core/Window.h"
//#include "Falcor/Core/API/Texture.h"
//#include "Falcor/Core/API/FBO.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/LowLevelContextData.h"
#include "Falcor/Core/API/DescriptorPool.h"
#include "Falcor/Core/API/GpuMemoryHeap.h"
#include "Falcor/Core/API/QueryHeap.h"
#include "Falcor/Core/API/Sampler.h"

#include "VulkanMemoryAllocator/src/vk_mem_alloc.h"

namespace Falcor {

//#ifdef _DEBUG
    #define DEFAULT_ENABLE_DEBUG_LAYER true
//#else
//    #define DEFAULT_ENABLE_DEBUG_LAYER false
//#endif

struct DeviceApiData;

class DeviceManager;
class ResourceManager;

class dlldecl Device: public std::enable_shared_from_this<Device> {
 public:
    using SharedPtr = std::shared_ptr<Device>;
    using SharedConstPtr = std::shared_ptr<const Device>;
    using ApiHandle = DeviceHandle;
    using DeviceLocalUID = uint32_t;
    
    static const uint32_t kQueueTypeCount = (uint32_t)LowLevelContextData::CommandQueueType::Count;

    ~Device();

    /** Device configuration
    */
    struct Desc {
        ResourceFormat colorFormat = ResourceFormat::BGRA8UnormSrgb;    ///< The color buffer format
        ResourceFormat depthFormat = ResourceFormat::D32Float;          ///< The depth buffer format
        uint32_t apiMajorVersion = 0;                                   ///< Requested API major version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        uint32_t apiMinorVersion = 0;                                   ///< Requested API minor version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
        bool enableVsync = false;                                       ///< Controls vertical-sync
        bool enableDebugLayer = DEFAULT_ENABLE_DEBUG_LAYER;             ///< Enable the debug layer. The default for release build is false, for debug build it's true.

        static_assert((uint32_t)LowLevelContextData::CommandQueueType::Direct == 2, "Default initialization of cmdQueues assumes that Direct queue index is 2");
        std::array<uint32_t, kQueueTypeCount> cmdQueues = { 0, 0, 1 };  ///< Command queues to create. If no direct-queues are created, mpRenderContext will not be initialized

#ifdef FALCOR_D3D12
        // GUID list for experimental features
        std::vector<UUID> experimentalFeatures;
#endif

#ifdef FALCOR_VK
        std::vector<std::string> requiredExtensions;
#endif

        uint32_t width = 1280;                                          ///< Headless FBO width
        uint32_t height = 720;                                          ///< Headless FBO height
    };

    enum class SupportedFeatures {
        None = 0x0,
        ProgrammableSamplePositionsPartialOnly = 0x1,  // On D3D12, this means tier 1 support. Allows one sample position to be set.
        ProgrammableSamplePositionsFull = 0x2,         // On D3D12, this means tier 2 support. Allows up to 4 sample positions to be set.
        Raytracing = 0x4                               // On D3D12, DirectX Raytracing is supported. It is up to the user to not use raytracing functions when not supported.
    };

    using MemoryType = GpuMemoryHeap::Type;

    /** Device unique id.
    */
    uint8_t uid() { return _uid; }

    /** Acts as the destructor for Device. Some resources use gpDevice in their cleanup.
        Cleaning up the SharedPtr directly would clear gpDevice before calling destructors.
    */
    void cleanup();

    std::shared_ptr<ResourceManager> resourceManager() const { return mpResourceManager; }

    /** Enable/disable vertical sync
    */
    void toggleVSync(bool enable);

    bool isHeadless() { return headless; };

    /** Get physical device name
    */
    std::string& getPhysicalDeviceName();

    /** VMA allocator
    */
    const VmaAllocator& allocator() const { return mAllocator; }

    /** Check if the window is occluded
    */
    bool isWindowOccluded() const;

    /** Get the FBO object associated with the swap-chain.
        This can change each frame, depending on the API used
    */
    Fbo::SharedPtr getSwapChainFbo() const;

    /** Get the FBO object used for headless rendering.
        This can change each frame, depending on the API used
    */
    Fbo::SharedPtr getOffscreenFbo() const;

    /** Get the default render-context.
        The default render-context is managed completely by the device. The user should just queue commands into it, the device will take care of allocation, submission and synchronization
    */
    RenderContext* getRenderContext() const { return mpRenderContext.get(); }

    /** Get the command queue handle
    */
    CommandQueueHandle getCommandQueueHandle(LowLevelContextData::CommandQueueType type, uint32_t index) const;

    /** Get the API queue type.
        \return API queue type, or throws an exception if type is unknown.
    */
    ApiCommandQueueType getApiCommandQueueType(LowLevelContextData::CommandQueueType type) const;

    /** Get the native API handle
    */
    const DeviceHandle& getApiHandle() { return mApiHandle; }

    /** Present the back-buffer to the window
    */
    void present();

    /** Flushes pipeline, releases resources, and blocks until completion
    */
    void flushAndSync();

    /** Check if vertical sync is enabled
    */
    bool isVsyncEnabled() const { return mDesc.enableVsync; }

    /** Resize the swap-chain
        \return A new FBO object
    */
    Fbo::SharedPtr resizeSwapChain(uint32_t width, uint32_t height);

    /** Get the desc
    */
    const Desc& getDesc() const { return mDesc; }

    /** Get window
    */
    const Window::SharedPtr& getWindow() const { return mpWindow; }

    /** Get default sampler object
    */
    const Sampler::SharedPtr& getDefaultSampler() const { return mpDefaultSampler; }

    /** Create a new query heap.
        \param[in] type Type of queries.
        \param[in] count Number of queries.
        \return New query heap.
    */
    std::weak_ptr<QueryHeap> createQueryHeap(QueryHeap::Type type, uint32_t count);

    const DescriptorPool::SharedPtr& getCpuDescriptorPool() const { return mpCpuDescPool; }
    const DescriptorPool::SharedPtr& getGpuDescriptorPool() const { return mpGpuDescPool; }
    const GpuMemoryHeap::SharedPtr& getUploadHeap() const { return mpUploadHeap; }
    void releaseResource(ApiObjectHandle pResource);
    double getGpuTimestampFrequency() const { return mGpuTimestampFrequency; }  // ms/tick

    /** Check if features are supported by the device
    */
    bool isFeatureSupported(SupportedFeatures flags) const;
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
#endif

    DeviceApiData* apiData() const { return mpApiData; };

 private:
    static constexpr uint32_t kSwapChainBuffersCount = 5;

    struct ResourceRelease {
        size_t frameID;
        ApiObjectHandle pApiObject;
    };

    std::queue<ResourceRelease> mDeferredReleases;

    uint32_t mCurrentBackBufferIndex;
    Fbo::SharedPtr mpSwapChainFbos[kSwapChainBuffersCount];
    Fbo::SharedPtr mpOffscreenFbo;

    Device(const Desc& desc);
    Device(uint32_t gpuId, const Desc& desc);
    Device(Window::SharedPtr pWindow, const Desc& desc);

    void executeDeferredReleases();
    void releaseFboData();
    void release();

    bool updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);
    bool updateOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);

    Desc mDesc;
    ApiHandle mApiHandle;
    GpuMemoryHeap::SharedPtr mpUploadHeap;
    DescriptorPool::SharedPtr mpCpuDescPool;
    DescriptorPool::SharedPtr mpGpuDescPool;
    bool mIsWindowOccluded = false;
    GpuFence::SharedPtr mpFrameFence;

    Sampler::SharedPtr  mpDefaultSampler = nullptr;

    Window::SharedPtr mpWindow;
    DeviceApiData* mpApiData;
    RenderContext::SharedPtr mpRenderContext;
    size_t mFrameID = 0;
    std::list<QueryHeap::SharedPtr> mTimestampQueryHeaps;
    double mGpuTimestampFrequency;
    std::vector<CommandQueueHandle> mCmdQueues[kQueueTypeCount];

    bool headless = false;

    SupportedFeatures mSupportedFeatures = SupportedFeatures::None;

    // API specific functions
    bool getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle &apiHandle);
    bool getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle apiHandles[kSwapChainBuffersCount], uint32_t& currentBackBufferIndex);
    void destroyApiObjects();
    void apiPresent();
    bool apiInit(std::shared_ptr<const DeviceManager> pDeviceManager);

    bool createSwapChain(ResourceFormat colorFormat);
    bool createOffscreenFBO(ResourceFormat colorFormat);

    void apiResizeSwapChain(uint32_t width, uint32_t height, ResourceFormat colorFormat);
    void apiResizeOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat);

    void toggleFullScreen(bool fullscreen);

 protected:
    /** Create a new rendering(headless) device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(std::shared_ptr<const DeviceManager> pDeviceManager, const Desc& desc);

    /** Create a new rendering(headless) device.
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(std::shared_ptr<const DeviceManager> pDeviceManager, uint32_t deviceId, const Desc& desc);

    /** Create a new device.
        \param[in] pWindow a previously-created window object
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(std::shared_ptr<const DeviceManager> pDeviceManager, Window::SharedPtr& pWindow, const Desc& desc);

    friend class DeviceManager;
    friend class ResourceManager;
    
    bool init(std::shared_ptr<const DeviceManager> pDeviceManager);

    std::string mPhysicalDeviceName;
    uint8_t mGpuId = 0;

    uint8_t _uid;
    static std::atomic<std::uint8_t> UID;

    VkPhysicalDeviceFeatures deviceFeatures;
    VmaAllocator  mAllocator; 

    std::shared_ptr<ResourceManager> mpResourceManager = nullptr;
};

enum_class_operators(Device::SupportedFeatures);

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_DEVICE_H_
