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
#include <set>

#include "Falcor/stdafx.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DeviceManager.h"
#include "Falcor/Core/API/DescriptorPool.h"
#include "Falcor/Core/API/GpuFence.h"
#include "Falcor/Core/API/Vulkan/FalcorVK.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor.h"

#define VMA_IMPLEMENTATION
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include "VKDevice.h"

#define VK_REPORT_PERF_WARNINGS  // Uncomment this to see performance warnings


PFN_vkGetBufferDeviceAddressKHR Falcor::vkGetBufferDeviceAddressKHR = 0;
PFN_vkCmdTraceRaysKHR Falcor::vkCmdTraceRaysKHR = 0;

namespace Falcor {

#define RR_FAILED(res) (res != RR_SUCCESS)

#ifdef DEFAULT_ENABLE_DEBUG_LAYER
VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage,
    void*                       pUserData) {
    std::string type = "FalcorVK ";
    type += ((flags | VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "Error: " : "Warning: ");
    
    if(flags | VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        LOG_ERR("%s", pMessage);
        //throw std::runtime_error("VK my abort!");
    } else if ((flags | VK_DEBUG_REPORT_WARNING_BIT_EXT) || (flags | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)) {
        LOG_WARN("%s", pMessage);
    } else if (flags | VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
        LOG_INFO("%s", pMessage);
    } else if (flags | VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
        LOG_DBG("%s", pMessage);
    } else {
        return VK_FALSE;
    }
    
    return VK_SUCCESS;
}
#endif

uint32_t getMaxViewportCount(std::shared_ptr<Device> device) {
    assert(device);
    return device->getPhysicalDeviceLimits().maxViewports;
}


static uint32_t getMemoryBits(VkPhysicalDevice physicalDevice, VkMemoryPropertyFlagBits memFlagBits) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    uint32_t bits = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memProperties.memoryTypes[i].propertyFlags & memFlagBits) == memFlagBits) {
            bits |= (1 << i);
        }
    }
    return bits;
}

static uint32_t getCurrentBackBufferIndex(VkDevice device, uint32_t backBufferCount, DeviceApiData* pApiData) {
    VkFence fence = pApiData->presentFences.f[pApiData->presentFences.cur];
    vk_call(vkWaitForFences(device, 1, &fence, false, -1));

    pApiData->presentFences.cur = (pApiData->presentFences.cur + 1) % backBufferCount;
    fence = pApiData->presentFences.f[pApiData->presentFences.cur];
    vkResetFences(device, 1, &fence);
    uint32_t newIndex;
    vk_call(vkAcquireNextImageKHR(device, pApiData->swapchain, std::numeric_limits<uint64_t>::max(), nullptr, fence, &newIndex));
    return newIndex;
}

static bool initMemoryTypes(VkPhysicalDevice physicalDevice, DeviceApiData* pApiData) {
    VkMemoryPropertyFlagBits bits[(uint32_t)Device::MemoryType::Count];
    bits[(uint32_t)Device::MemoryType::Default] = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    bits[(uint32_t)Device::MemoryType::Upload] = VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    bits[(uint32_t)Device::MemoryType::Readback] = VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    for(uint32_t i = 0 ; i < arraysize(bits) ; i++) {
        pApiData->vkMemoryTypeBits[i] = getMemoryBits(physicalDevice, bits[i]);
        if (pApiData->vkMemoryTypeBits[i] == 0) {
            logError("Missing memory type " + std::to_string(i));
            return false;
        }
    }
    return true;
}

Device::~Device() {
    LOG_DBG("Device::~Device");
    vmaDestroyAllocator(mAllocator);
}

bool Device::getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle &apiHandle) {
    // https://github.com/SaschaWillems/Vulkan/blob/master/examples/offscreen/offscreen.cpp

    VkImage image;
    VmaAllocation allocation;
    VkImageCreateInfo imageInfo = {};

    imageInfo.arrayLayers = 1;
    imageInfo.extent.depth = 1;
    imageInfo.extent.height = height;
    imageInfo.extent.width = width;
    imageInfo.format = getVkFormat(colorFormat);
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.mipLevels = 1;
    imageInfo.pQueueFamilyIndices = nullptr;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    //auto result = vkCreateImage(mApiHandle, &imageInfo, nullptr, &image);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    
    auto result = vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &image, &allocation, nullptr );   
    if (VK_FAILED(result)) {
        throw std::runtime_error("Failed to create FBO texture.");
    }

    apiHandle = ResourceHandle::create(shared_from_this(), image, allocation);
    return true;
}

bool Device::getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle apiHandles[kSwapChainBuffersCount], uint32_t& currentBackBufferIndex) {
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(mApiHandle, mpApiData->swapchain, &imageCount, nullptr);
    //assert(imageCount == apiHandles.size());
    assert(imageCount == kSwapChainBuffersCount);

    std::vector<VkImage> swapchainImages(imageCount);
    vkGetSwapchainImagesKHR(mApiHandle, mpApiData->swapchain, &imageCount, swapchainImages.data());
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VmaAllocationCreateInfo imageAllocCreateInfo = {};
        imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        //apiHandles[i] = ResourceHandle::create(shared_from_this(), swapchainImages[i], nullptr);
    }

    // Get the back-buffer
    mCurrentBackBufferIndex = getCurrentBackBufferIndex(mApiHandle, kSwapChainBuffersCount, mpApiData);
    return true;
}

void Device::destroyApiObjects() {
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = VK_NULL_HANDLE;
    DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(mApiHandle, "vkDestroyDebugReportCallbackEXT");
    if(DestroyDebugReportCallback) {
        DestroyDebugReportCallback(mApiHandle, mpApiData->debugReportCallbackHandle, nullptr);
    }

    if(!headless) 
        vkDestroySwapchainKHR(mApiHandle, mpApiData->swapchain, nullptr);

    for (auto& f : mpApiData->presentFences.f) {
        vkDestroyFence(mApiHandle, f, nullptr);
    }

    //vmaDestroyAllocator(mAllocator);

    safe_delete(mpApiData);
}

static std::vector<VkLayerProperties> enumarateInstanceLayersProperties() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

    for (const VkLayerProperties& layer : layerProperties) {
        logInfo("Available Vulkan Layer: " + std::string(layer.layerName) + " - VK Spec Version: " + std::to_string(layer.specVersion) + " - Implementation Version: " + std::to_string(layer.implementationVersion));
    }

    return layerProperties;
}

static bool isLayerSupported(const std::string& layer, const std::vector<VkLayerProperties>& supportedLayers) {
    for (const auto& l : supportedLayers) {
        if (l.layerName == layer) return true;
    }
    return false;
}

void enableLayerIfPresent(const char* layerName, const std::vector<VkLayerProperties>& supportedLayers, std::vector<const char*>& requiredLayers) {
    if (isLayerSupported(layerName, supportedLayers)) {
        requiredLayers.push_back(layerName);
    } else {
        LOG_WARN("Can't enable requested Vulkan layer %s", layerName);
        logWarning("Can't enable requested Vulkan layer " + std::string(layerName) + ". Something bad might happen. Or not, depends on the layer.");
    }
}

static std::vector<VkExtensionProperties> enumarateInstanceExtensions() {
    // Enumerate implicitly available extensions. The debug layers above just have VK_EXT_debug_report
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.data());

    for (const VkExtensionProperties& extension : supportedExtensions) {
        logInfo("Available Instance Extension: " + std::string(extension.extensionName) + " - VK Spec Version: " + std::to_string(extension.specVersion));
    }

    return supportedExtensions;
}

static void initDebugCallback(VkInstance instance, VkDebugReportCallbackEXT* pCallback) {
    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
    callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

#ifdef VK_REPORT_PERF_WARNINGS
    callbackCreateInfo.flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
#endif

#ifdef DEFAULT_ENABLE_DEBUG_LAYER
    callbackCreateInfo.pfnCallback = &debugReportCallback;
#endif

    // Function to create a debug callback has to be dynamically queried from the instance...
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
    CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

    if (VK_FAILED(CreateDebugReportCallback(instance, &callbackCreateInfo, nullptr, pCallback))) {
        logWarning("Could not initialize debug report callbacks.");
    }
}


static bool isExtensionSupported(const std::string& str, const std::vector<VkExtensionProperties>& vec) {
    for (const auto& s : vec) {
        if (str == std::string(s.extensionName)) return true;
    }
    return false;
}

VkInstance createInstance(DeviceApiData* pData, bool enableDebugLayer) {
    // Initialize the layers
    const auto layerProperties = enumarateInstanceLayersProperties();
    std::vector<const char*> requiredLayers;

    if (enableDebugLayer) {
        enableLayerIfPresent("VK_LAYER_KHRONOS_validation", layerProperties, requiredLayers);
        enableLayerIfPresent("VK_LAYER_LUNARG_standard_validation", layerProperties, requiredLayers);
    }

    // Initialize the extensions
    std::vector<VkExtensionProperties> supportedExtensions = enumarateInstanceExtensions();

    // Extensions to use when creating instance
    std::vector<const char*> requiredExtensions = {
        "VK_KHR_surface",
#ifdef _WIN32
        "VK_KHR_win32_surface"
#else
        "VK_KHR_xlib_surface"
#endif
    };

    if (enableDebugLayer) { requiredExtensions.push_back("VK_EXT_debug_report"); }


    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Lava";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;


    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = (uint32_t)requiredLayers.size();
    createInfo.ppEnabledLayerNames = requiredLayers.data();
    createInfo.enabledExtensionCount = (uint32_t)requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    VkInstance instance;
    if (VK_FAILED(vkCreateInstance(&createInfo, nullptr, &instance))) {
        logError("Failed to create Vulkan instance");
        return nullptr;
    }

    // Hook up callbacks for VK_EXT_debug_report
    if (enableDebugLayer) {
        initDebugCallback(instance, &pData->debugReportCallbackHandle);
    }

    return instance;
}

void Device::toggleFullScreen(bool fullscreen){}

/** Select best physical device based on memory
*/
VkPhysicalDevice selectPhysicalDevice(const std::vector<VkPhysicalDevice>& devices) {
    LOG_DBG("Selecting physical Vulkan device...");
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    uint64_t bestMemory = 0;

    for (const VkPhysicalDevice& device : devices) {
        VkPhysicalDeviceMemoryProperties properties;
        vkGetPhysicalDeviceMemoryProperties(device, &properties);

        // Get local memory size from device
        uint64_t deviceMemory = 0;
        for (uint32_t i = 0; i < properties.memoryHeapCount; i++) {
            if ((properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) > 0) {
                deviceMemory = properties.memoryHeaps[i].size;
                break;
            }
        }

        // Save if best found so far
        if (bestDevice == VK_NULL_HANDLE || deviceMemory > bestMemory) {
            bestDevice = device;
            bestMemory = deviceMemory;
        }
    }

    VkPhysicalDeviceProperties pProperties;
    vkGetPhysicalDeviceProperties(bestDevice, &pProperties);

    LOG_DBG("Selected Vulkan physical device: %s", pProperties.deviceName);
    return bestDevice;
}

VkPhysicalDevice initPhysicalDevice(VkInstance instance, VkPhysicalDevice physicalDevice, DeviceApiData* pData, const Device::Desc& desc) {
    // Pick a device
    //VkPhysicalDevice physicalDevice = DeviceManager::instance().physicalDevices()[gpuId];

    // Get device physical properties
    vkGetPhysicalDeviceProperties(physicalDevice, &pData->properties);
    pData->deviceLimits = pData->properties.limits;

    // Get device memory properties
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &pData->memoryProperties);

    // Check that the device/driver supports the requested API version
    uint32_t vkApiVersion = VK_MAKE_VERSION(desc.apiMajorVersion, desc.apiMinorVersion, 0);
    if (vkApiVersion != 0 && pData->properties.apiVersion < vkApiVersion) {
        std::string reqVerStr = std::to_string(desc.apiMajorVersion) + "." + std::to_string(desc.apiMinorVersion);
        std::string supportedStr = std::to_string(VK_VERSION_MAJOR(pData->properties.apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(pData->properties.apiVersion));
        logError("Vulkan device does not support requested API version. Requested version: " + reqVerStr + ", Highest supported: " + supportedStr);
        return nullptr;
    }

    // Get queue families and match them to what type they are
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    LOG_WARN("Vulkan physical device queue family count is: %u", queueFamilyCount);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    // Init indices
    for (uint32_t i = 0 ; i < arraysize(pData->falcorToVulkanQueueType) ; i++) {
        pData->falcorToVulkanQueueType[i]= (uint32_t)-1;
    }

    // Determine which queue is what type
    uint32_t& graphicsQueueIndex = pData->falcorToVulkanQueueType[(uint32_t)LowLevelContextData::CommandQueueType::Direct];
    uint32_t& computeQueueIndex = pData->falcorToVulkanQueueType[(uint32_t)LowLevelContextData::CommandQueueType::Compute];
    uint32_t& transferQueue = pData->falcorToVulkanQueueType[(uint32_t)LowLevelContextData::CommandQueueType::Copy];

    for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++) {
        VkQueueFlags flags = queueFamilyProperties[i].queueFlags;

        if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0 && graphicsQueueIndex == (uint32_t)-1) {
            graphicsQueueIndex = i;
        } else if ((flags & VK_QUEUE_COMPUTE_BIT) != 0 && computeQueueIndex == (uint32_t)-1) {
            computeQueueIndex = i;
        } else if ((flags & VK_QUEUE_TRANSFER_BIT) != 0 && transferQueue == (uint32_t)-1) {
            transferQueue = i;
        }
    }

    return physicalDevice;
}

static void initDeviceQueuesInfo(const Device::Desc& desc, const DeviceApiData *pData, std::vector<VkDeviceQueueCreateInfo>& queueInfos, std::vector<CommandQueueHandle> cmdQueues[Device::kQueueTypeCount], std::vector<std::vector<float>>& queuePriorities) {
    queuePriorities.resize(arraysize(pData->falcorToVulkanQueueType));

    // Set up info to create queues for each type
    for (uint32_t type = 0; type < arraysize(pData->falcorToVulkanQueueType); type++) {
        const uint32_t queueCount = desc.cmdQueues[type];
        queuePriorities[type].resize(queueCount, 1.0f); // Setting all priority at max for now
        cmdQueues[type].resize(queueCount); // Save how many queues of each type there will be so we can retrieve them easier after device creation

        VkDeviceQueueCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueCount = queueCount;
        info.queueFamilyIndex = pData->falcorToVulkanQueueType[type];
        info.pQueuePriorities = queuePriorities[type].data();

        if (info.queueCount > 0) {
            queueInfos.push_back(info);
        }
    }
}

VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, DeviceApiData *pData, const Device::Desc& desc, std::vector<CommandQueueHandle> cmdQueues[Device::kQueueTypeCount], 
    VkPhysicalDeviceFeatures &deviceFeatures, VkPhysicalDeviceRayTracingPipelinePropertiesKHR &rayTracingPipelineProperties, VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeatures) {
    // Features
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    // Get ray tracing pipeline properties, which will be used later on in the sample
    rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProperties2{};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &rayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    // Get acceleration structure properties, which will be used later on in the sample
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &accelerationStructureFeatures;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

    // Queues
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::vector<std::vector<float>> queuePriorities;
    initDeviceQueuesInfo(desc, pData, queueInfos, cmdQueues, queuePriorities);

    // Extensions
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    pData->deviceExtensions.resize(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, pData->deviceExtensions.data());

    for (const VkExtensionProperties& extension : pData->deviceExtensions) {
        logInfo("Available Device Extension: " + std::string(extension.extensionName) + " - VK Spec Version: " + std::to_string(extension.specVersion));
    }

    std::vector<const char*> extensionNames = { "VK_KHR_swapchain" };
    assert(isExtensionSupported(extensionNames[0], pData->deviceExtensions));

    for (const auto& a : desc.requiredExtensions) {
        if (isExtensionSupported(a, pData->deviceExtensions)) {
            extensionNames.push_back(a.c_str());
        } else {
            logWarning("The device doesn't support the requested '" + a + "` extension");
        }
    }

    // Logical Device
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = (uint32_t)extensionNames.size();
    deviceInfo.ppEnabledExtensionNames = extensionNames.data();
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    VkDevice device;
    if (VK_FAILED(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device))) {
        logError("Could not create Vulkan logical device.");
        return nullptr;
    }

    // Get the ray tracing and accelertion structure related function pointers required
    
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));

    // Get the queues we created
    for (uint32_t type = 0; type < arraysize(pData->falcorToVulkanQueueType); type++) {
        for (uint32_t i = 0; i < (uint32_t)cmdQueues[type].size(); i++) {
            vkGetDeviceQueue(device, pData->falcorToVulkanQueueType[type], i, &cmdQueues[type][i]);
        }
    }

    return device;
}

bool Device::createOffscreenFBO(ResourceFormat colorFormat) {
    return true;
}

void Device::apiPresent() {
    assert(!headless);  // presenting makes no sense in headless mode
    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.swapchainCount = 1;
    info.pSwapchains = &mpApiData->swapchain;
    info.pImageIndices = &mCurrentBackBufferIndex;
    auto pQueue = mpRenderContext->getLowLevelData()->getCommandQueue();
    assert(pQueue);
    vk_call(vkQueuePresentKHR(pQueue, &info));
    mCurrentBackBufferIndex = getCurrentBackBufferIndex(mApiHandle, kSwapChainBuffersCount, mpApiData);
}

Device::SupportedFeatures getSupportedFeatures(VkPhysicalDevice device) {

    Device::SupportedFeatures supported = Device::SupportedFeatures::None;

    // TODO: check conservative rasterization
    supported |= Device::SupportedFeatures::ConservativeRasterizationTier1 | Device::SupportedFeatures::ConservativeRasterizationTier2 | Device::SupportedFeatures::ConservativeRasterizationTier3;
    
    // TODO: check rasterizer ordered views (ROVs)
    supported |= Device::SupportedFeatures::RasterizerOrderedViews;
    
    // TODO: check programmable sample positions supoprt
    supported |= Device::SupportedFeatures::ProgrammableSamplePositionsPartialOnly;
    supported |= Device::SupportedFeatures::ProgrammableSamplePositionsFull;

    // TODO: check barycentrics support
    supported |= Device::SupportedFeatures::Barycentrics;

    // TODO: check raytracing support
    supported |= Device::SupportedFeatures::Raytracing;

    return supported;
}

/**
 * Initialize vulkan device
 */
bool Device::apiInit(std::shared_ptr<const DeviceManager> pDeviceManager) {
    const Desc desc;

    mpApiData = new DeviceApiData;
    VkInstance instance = createInstance(mpApiData, desc.enableDebugLayer);
    if (!instance) return false;

    VkPhysicalDevice physicalDevice = initPhysicalDevice(instance, pDeviceManager->physicalDevices()[mGpuId], mpApiData, desc);
    if (!physicalDevice) return false;

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkDevice device = createLogicalDevice(physicalDevice, mpApiData, desc, mCmdQueues, mDeviceFeatures, mRayTracingPipelineProperties, mAaccelerationStructureFeatures);
    if (!device) return false;

    if (initMemoryTypes(physicalDevice, mpApiData) == false) return false;

    mApiHandle = DeviceHandle::create(shared_from_this(), instance, physicalDevice, device, surface);
    mGpuTimestampFrequency = getPhysicalDeviceLimits().timestampPeriod / (1000 * 1000);
    mPhysicalDeviceName = std::string(mpApiData->properties.deviceName);

    mSupportedFeatures = getSupportedFeatures(physicalDevice);

    if (createOffscreenFBO(desc.colorFormat) == false) {
        return false;
    }

    VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {};
    vmaAllocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    vmaAllocatorCreateInfo.physicalDevice =physicalDevice;
    vmaAllocatorCreateInfo.device = device;
    vmaAllocatorCreateInfo.instance = instance;
    vmaAllocatorCreateInfo.preferredLargeHeapBlockSize = 0; // Set to 0 to use default, which is currently 256 MiB.
    vmaAllocatorCreateInfo.pRecordSettings = nullptr;
 
    vk_call(vmaCreateAllocator(&vmaAllocatorCreateInfo, &mAllocator));

    return true;
}

void Device::apiResizeOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat) {
    assert(headless);
    mpOffscreenFbo = nullptr;
    createOffscreenFBO(colorFormat);
}

bool Device::isWindowOccluded() const {
    // #VKTODO Is there a test for it?
    return false;
}

/*
bool Device::isExtensionSupported(const std::string& name) const
{
    //return Falcor::isExtensionSupported(name, mpApiData->deviceExtensions);
    return true;
}
*/

CommandQueueHandle Device::getCommandQueueHandle(LowLevelContextData::CommandQueueType type, uint32_t index) const {
    auto& queue = mCmdQueues[(uint32_t)type];

    if(index >= queue.size()) {
        throw std::runtime_error("No queue index " + to_string(index) + " for queue type " + to_string(type));
    }

    return queue[index];
}

ApiCommandQueueType Device::getApiCommandQueueType(LowLevelContextData::CommandQueueType type) const {
    return mpApiData->falcorToVulkanQueueType[(uint32_t)type];
}

uint32_t Device::getVkMemoryType(GpuMemoryHeap::Type falcorType, uint32_t memoryTypeBits) const {
    uint32_t mask = mpApiData->vkMemoryTypeBits[(uint32_t)falcorType] & memoryTypeBits;
    assert(mask != 0);
    return bitScanForward(mask);
}


uint32_t Device::getVkMemoryTypeNative(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound) const {
    auto& deviceMemoryProperties = apiData()->memoryProperties;

    for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeBits & 1) == 1) {
            if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                if (memTypeFound) {
                    *memTypeFound = true;
                }
                return i;
            }
        }
        typeBits >>= 1;
    }

    if (memTypeFound) {
        *memTypeFound = false;
        return 0;
    } else {
        throw std::runtime_error("Could not find a matching Vulkan memory type !!!");
    }
}

const VkPhysicalDeviceLimits& Device::getPhysicalDeviceLimits() const {
    return mpApiData->deviceLimits;
}

uint32_t Device::getDeviceVendorID() const {
    return mpApiData->properties.vendorID;
}

}  // namespace Falcor
