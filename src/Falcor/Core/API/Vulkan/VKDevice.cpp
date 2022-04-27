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
#include <chrono>
#include <thread>

#include "Falcor/stdafx.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DeviceManager.h"
#include "Falcor/Core/API/DescriptorPool.h"
#include "Falcor/Core/API/GpuFence.h"
#include "Falcor/Core/API/Vulkan/FalcorVK.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor.h"

#include "nvvk/extensions_vk.hpp"

#define VMA_IMPLEMENTATION
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include "VKDevice.h"

#define VK_REPORT_PERF_WARNINGS  // Uncomment this to see performance warnings

#include "lava_utils_lib/logging.h"

PFN_vkGetBufferDeviceAddressKHR                     Falcor::vkGetBufferDeviceAddressKHR = nullptr;
PFN_vkCmdTraceRaysKHR                               Falcor::vkCmdTraceRaysKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR                Falcor::vkCreateAccelerationStructureKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR               Falcor::vkDestroyAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR         Falcor::vkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR      Falcor::vkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR             Falcor::vkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkBuildAccelerationStructuresKHR                Falcor::vkBuildAccelerationStructuresKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR            Falcor::vkGetRayTracingShaderGroupHandlesKHR = nullptr;

PFN_vkWriteAccelerationStructuresPropertiesKHR      Falcor::vkWriteAccelerationStructuresPropertiesKHR = nullptr;
PFN_vkCmdWriteAccelerationStructuresPropertiesKHR   Falcor::vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
PFN_vkCmdCopyAccelerationStructureKHR               Falcor::vkCmdCopyAccelerationStructureKHR = nullptr;

PFN_vkDeferredOperationJoinKHR                      Falcor::vkDeferredOperationJoinKHR = nullptr;
PFN_vkGetDeferredOperationResultKHR                 Falcor::vkGetDeferredOperationResultKHR = nullptr;

PFN_vkCreateRayTracingPipelinesKHR                  Falcor::vkCreateRayTracingPipelinesKHR = nullptr;

PFN_vkGetSwapchainImagesKHR                         Falcor::vkGetSwapchainImagesKHR = nullptr;
PFN_vkDestroySwapchainKHR                           Falcor::vkDestroySwapchainKHR = nullptr;
PFN_vkCreateSwapchainKHR                            Falcor::vkCreateSwapchainKHR = nullptr;
PFN_vkAcquireNextImageKHR                           Falcor::vkAcquireNextImageKHR = nullptr;
PFN_vkQueuePresentKHR                               Falcor::vkQueuePresentKHR = nullptr;


namespace Falcor {

#define NVVK_DEFAULT_STAGING_BLOCKSIZE (VkDeviceSize(64) * 1024 * 1024)

#define RR_FAILED(res) (res != RR_SUCCESS)

template<typename MainT, typename NewT>
inline void PnextChainPushFront(MainT* mainStruct, NewT* newStruct) {
    newStruct->pNext = mainStruct->pNext;
    mainStruct->pNext = newStruct;
}
template<typename MainT, typename NewT>
inline void PnextChainPushBack(MainT* mainStruct, NewT* newStruct) {
    struct VkAnyStruct {
        VkStructureType sType;
        void* pNext;
    };
    VkAnyStruct* lastStruct = (VkAnyStruct*)mainStruct;
    while(lastStruct->pNext != nullptr) {
        lastStruct = (VkAnyStruct*)lastStruct->pNext;
    }
    newStruct->pNext = nullptr;
    lastStruct->pNext = newStruct;
}

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

uint32_t getMaxViewportCount(std::shared_ptr<Device> pDevice) {
    assert(pDevice);
    return pDevice->getPhysicalDeviceLimits().maxViewports;
}

static constexpr uint32_t getVulkanApiVersion() {
#if VMA_VULKAN_VERSION == 1002000
    return VK_API_VERSION_1_2;
#elif VMA_VULKAN_VERSION == 1001000
    return VK_API_VERSION_1_1;
#elif VMA_VULKAN_VERSION == 1000000
    return VK_API_VERSION_1_0;
#else
#error Invalid VMA_VULKAN_VERSION.
    return UINT32_MAX;
#endif
}

static const uint32_t VENDOR_ID_AMD = 0x1002;
static const uint32_t VENDOR_ID_NVIDIA = 0x10DE;
static const uint32_t VENDOR_ID_INTEL = 0x8086;

const wchar_t* PhysicalDeviceTypeToStr(VkPhysicalDeviceType type) {
    // Skipping common prefix VK_PHYSICAL_DEVICE_TYPE_
    static const wchar_t* const VALUES[] = {
        L"OTHER",
        L"INTEGRATED_GPU",
        L"DISCRETE_GPU",
        L"VIRTUAL_GPU",
        L"CPU",
    };
    return (uint32_t)type < std::size(VALUES) ? VALUES[(uint32_t)type] : L"";
}

const wchar_t* VendorIDToStr(uint32_t vendorID) {
    switch(vendorID) {
        // Skipping common prefix VK_VENDOR_ID_ for these:
        case 0x10001: return L"VIV";
        case 0x10002: return L"VSI";
        case 0x10003: return L"KAZAN";
        case 0x10004: return L"CODEPLAY";
        case 0x10005: return L"MESA";
        case 0x10006: return L"POCL";
        // Others...
        case VENDOR_ID_AMD: return L"AMD";
        case VENDOR_ID_NVIDIA: return L"NVIDIA";
        case VENDOR_ID_INTEL: return L"Intel";
        case 0x1010: return L"ImgTec";
        case 0x13B5: return L"ARM";
        case 0x5143: return L"Qualcomm";
    }
    return L"";
}

static void PrintPhysicalDeviceProperties(const VkPhysicalDeviceProperties& properties) {
    wprintf(L"physicalDeviceProperties:\n");
    wprintf(L"    driverVersion: 0x%X\n", properties.driverVersion);
    wprintf(L"    vendorID: 0x%X (%s)\n", properties.vendorID, VendorIDToStr(properties.vendorID));
    wprintf(L"    deviceID: 0x%X\n", properties.deviceID);
    wprintf(L"    deviceType: %u (%s)\n", properties.deviceType, PhysicalDeviceTypeToStr(properties.deviceType));
    wprintf(L"    deviceName: %hs\n", properties.deviceName);
    wprintf(L"    limits:\n");
    wprintf(L"        maxMemoryAllocationCount: %u\n", properties.limits.maxMemoryAllocationCount);
    wprintf(L"        bufferImageGranularity: %llu B\n", properties.limits.bufferImageGranularity);
    wprintf(L"        nonCoherentAtomSize: %llu B\n", properties.limits.nonCoherentAtomSize);
}

#if VMA_VULKAN_VERSION >= 1002000
static void PrintPhysicalDeviceVulkan11Properties(const VkPhysicalDeviceVulkan11Properties& properties) {
    printf("physicalDeviceVulkan11Properties:\n");
    //std::wstring sizeStr = SizeToStr(properties.maxMemoryAllocationSize);
    //printf(L"    maxMemoryAllocationSize: %llu B (%s)\n", properties.maxMemoryAllocationSize, sizeStr.c_str());
}

static void PrintPhysicalDeviceVulkan12Properties(const VkPhysicalDeviceVulkan12Properties& properties) {
    printf("physicalDeviceVulkan12Properties:\n");
    //std::wstring str = DriverIDToStr(properties.driverID);
    //printf("    driverID: %u (%s)\n", properties.driverID, str.c_str());
    printf("    driverName: %hs\n", properties.driverName);
    printf("    driverInfo: %hs\n", properties.driverInfo);
}
#endif // #if VMA_VULKAN_VERSION > 1002000


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
    assert(imageCount == kSwapChainBuffersCount);

    std::vector<VkImage> swapchainImages(imageCount);
    vkGetSwapchainImagesKHR(mApiHandle, mpApiData->swapchain, &imageCount, swapchainImages.data());
    
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

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

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VmaAllocation allocation;

        auto result = vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &swapchainImages[i], &allocation, nullptr );   
        if (VK_FAILED(result)) {
            throw std::runtime_error("Failed to create swap chain image.");
        }   

        apiHandles[i] = ResourceHandle::create(shared_from_this(), swapchainImages[i], allocation);
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

    if(!mHeadless) 
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
    uint32_t availableInstanceExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableInstanceExtensions(availableInstanceExtensionCount);
    if (availableInstanceExtensionCount > 0) {
        vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, availableInstanceExtensions.data());
    }

    for (const VkExtensionProperties& extension : availableInstanceExtensions) {
        logInfo("Available Instance Extension: " + std::string(extension.extensionName) + " - VK Spec Version: " + std::to_string(extension.specVersion));
    }

    return availableInstanceExtensions;
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

VkInstance createInstance(bool enableDebugLayer) {
    // Initialize the layers
    const auto layerProperties = enumarateInstanceLayersProperties();
    std::vector<const char*> requiredLayers;

    if (enableDebugLayer) {
        enableLayerIfPresent("VK_LAYER_KHRONOS_validation", layerProperties, requiredLayers);
        //enableLayerIfPresent("VK_LAYER_KHRONOS_synchronization2", layerProperties, requiredLayers);
        //enableLayerIfPresent("VK_LAYER_LUNARG_monitor", layerProperties, requiredLayers);
        //enableLayerIfPresent("VK_LAYER_LUNARG_parameter_validation", layerProperties, requiredLayers);
        //enableLayerIfPresent("VK_LAYER_LUNARG_core_validation", layerProperties, requiredLayers);
        //enableLayerIfPresent("VK_LAYER_LUNARG_standard_validation", layerProperties, requiredLayers);
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
        return VK_NULL_HANDLE;
    }

    return instance;
}

VkInstance createInstance(DeviceApiData* pData, bool enableDebugLayer) {
    VkInstance instance = createInstance(enableDebugLayer);

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
    uint32_t& transferQueueIndex = pData->falcorToVulkanQueueType[(uint32_t)LowLevelContextData::CommandQueueType::Copy];

    for (uint32_t i = 0; i < (uint32_t)queueFamilyProperties.size(); i++) {
        VkQueueFlags flags = queueFamilyProperties[i].queueFlags;

        if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0 && graphicsQueueIndex == (uint32_t)-1) {
            graphicsQueueIndex = i;
        } else if ((flags & VK_QUEUE_COMPUTE_BIT) != 0 && computeQueueIndex == (uint32_t)-1) {
            computeQueueIndex = i;
        } else if ((flags & VK_QUEUE_TRANSFER_BIT) != 0 && transferQueueIndex == (uint32_t)-1) {
            transferQueueIndex = i;
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

VkDevice createLogicalDevice(Device *pDevice, VkPhysicalDevice physicalDevice, DeviceApiData *pData, const Device::Desc& desc, std::vector<CommandQueueHandle> cmdQueues[Device::kQueueTypeCount], 
    VkPhysicalDeviceFeatures &deviceFeatures) {

    assert(pDevice);

    bool headless = false;
    if(desc.surface == VK_NULL_HANDLE) headless = true;

    bool sparseBindingEnabled = false;
    bool VK_KHR_raytracing_pipeline_enabled = false;
    bool VK_KHR_get_memory_requirements2_enabled = false;
    bool VK_KHR_dedicated_allocation_enabled = false;
    bool VK_AMD_device_coherent_memory_enabled = false;
    bool VK_KHR_buffer_device_address_enabled = false;
    bool VK_KHR_acceleration_structure_enabled = false;
    bool VK_KHR_acceleration_structure_host_commands_enabled = true;
    bool VK_KHR_bind_memory2_enabled = false;
    bool VK_EXT_memory_priority_enabled = false;
    bool VK_EXT_memory_budget_enabled = false;
    bool VK_KHR_ray_query_enabled = false;
    bool VK_KHR_synchronization2_enabled = false;
    bool VK_EXT_host_query_reset_enabled = true;
    bool VK_KHR_swapchain_enabled = false;
    // Query for device extensions

    uint32_t physicalDeviceExtensionPropertyCount = 0;
    if (VK_FAILED(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &physicalDeviceExtensionPropertyCount, nullptr))) {
        std::runtime_error("Error enumerating device extension properties count !!!");
    }

    std::vector<VkExtensionProperties> physicalDeviceExtensionProperties{physicalDeviceExtensionPropertyCount};
    if(physicalDeviceExtensionPropertyCount) {
        if (VK_FAILED(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &physicalDeviceExtensionPropertyCount, physicalDeviceExtensionProperties.data()))) {
            std::runtime_error("Error enumerating device extension properties !!!");
        }
    }

    for(uint32_t i = 0; i < physicalDeviceExtensionPropertyCount; ++i) {
        auto const& extensionName = physicalDeviceExtensionProperties[i].extensionName;

        if(strcmp(extensionName, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0) {
            if(getVulkanApiVersion() == VK_API_VERSION_1_0) {
                VK_KHR_get_memory_requirements2_enabled = true;
            }
        }
        else if(strcmp(extensionName, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0) {
            if(getVulkanApiVersion() == VK_API_VERSION_1_0) {
                VK_KHR_dedicated_allocation_enabled = true;
            }
        }
        else if(strcmp(extensionName, VK_KHR_BIND_MEMORY_2_EXTENSION_NAME) == 0) {
            if(getVulkanApiVersion() == VK_API_VERSION_1_0) {
                VK_KHR_bind_memory2_enabled = true;
            }
        }
        else if(strcmp(extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
            VK_KHR_acceleration_structure_enabled = true;
            VK_KHR_acceleration_structure_host_commands_enabled = true;
        }
        else if(strcmp(extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
            VK_KHR_raytracing_pipeline_enabled = true;
        }
        else if(strcmp(extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0) {
            VK_KHR_ray_query_enabled = true;
        }
        else if(strcmp(extensionName, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == 0) {
            VK_KHR_synchronization2_enabled = true;
        }
        else if(strcmp(extensionName, VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME) == 0) {
            VK_EXT_host_query_reset_enabled = true;
        }

        else if(strcmp(extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
            VK_EXT_memory_budget_enabled = true;
        }
        
        else if(strcmp(extensionName, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME) == 0) {
            VK_AMD_device_coherent_memory_enabled = true;
        }
        
        else if(strcmp(extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0) {
            if(getVulkanApiVersion() < VK_API_VERSION_1_2) {
                VK_KHR_buffer_device_address_enabled = true;
            }
        }
        else if(strcmp(extensionName, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME) == 0) {
            VK_EXT_memory_priority_enabled = true;
        }
        else if(strcmp(extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            VK_KHR_swapchain_enabled = true;
        }
    }

    if(getVulkanApiVersion() >= VK_API_VERSION_1_2)
        VK_KHR_buffer_device_address_enabled = true; // Promoted to core Vulkan 1.2.

    assert(VK_KHR_raytracing_pipeline_enabled == true);
    assert(VK_KHR_ray_query_enabled == true);

    // Features
    //vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    // Get ray tracing pipeline properties, which will be used later on in the sample
    //rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    //VkPhysicalDeviceProperties2 deviceProperties2{};
    //deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    //deviceProperties2.pNext = &rayTracingPipelineProperties;
    //vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    // Query for features

#if VMA_VULKAN_VERSION >= 1001000
    VkPhysicalDeviceProperties2 physicalDeviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    
#if VMA_VULKAN_VERSION >= 1002000
    // Vulkan spec says structure VkPhysicalDeviceVulkan11Properties is "Provided by VK_VERSION_1_2" - is this a mistake? Assuming not...
    VkPhysicalDeviceVulkan11Properties physicalDeviceVulkan11Properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES };
    VkPhysicalDeviceVulkan12Properties physicalDeviceVulkan12Properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
    PnextChainPushFront(&physicalDeviceProperties2, &physicalDeviceVulkan11Properties);
    PnextChainPushFront(&physicalDeviceProperties2, &physicalDeviceVulkan12Properties);
#endif

    vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);

    PrintPhysicalDeviceProperties(physicalDeviceProperties2.properties);
#if VMA_VULKAN_VERSION >= 1002000
    PrintPhysicalDeviceVulkan11Properties(physicalDeviceVulkan11Properties);
    PrintPhysicalDeviceVulkan12Properties(physicalDeviceVulkan12Properties);
#endif

#else // #if VMA_VULKAN_VERSION >= 1001000
    VkPhysicalDeviceProperties physicalDeviceProperties = {};
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    PrintPhysicalDeviceProperties(physicalDeviceProperties);

#endif // #if VMA_VULKAN_VERSION >= 1001000

   // VkPhysicalDeviceProperties2 physicalDeviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    //vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);

    //-----------------------
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

    if(VK_AMD_device_coherent_memory_enabled) {
        PnextChainPushFront(&physicalDeviceFeatures, &pDevice->mEnabledDeviceCoherentMemoryFeatures);
    }

    if(VK_KHR_buffer_device_address_enabled){
        PnextChainPushFront(&physicalDeviceFeatures, &pDevice->mEnabledBufferDeviceAddresFeatures);
    }

    if(VK_EXT_memory_priority_enabled) {
        PnextChainPushFront(&physicalDeviceFeatures, &pDevice->mEnabledMemoryPriorityFeatures);
    }

    if(VK_KHR_acceleration_structure_enabled) {
        PnextChainPushFront(&physicalDeviceFeatures, &pDevice->mEnabledAccelerationStructureFeatures);
    }

    if(VK_KHR_raytracing_pipeline_enabled) {
        PnextChainPushFront(&physicalDeviceFeatures, &pDevice->mEnabledRayTracingPipelineFeatures);
    }

    if(VK_EXT_host_query_reset_enabled) {
        PnextChainPushFront(&physicalDeviceFeatures, &pDevice->mEnabledHostQueryResetFeatures);
    }

    if(VK_KHR_ray_query_enabled) {
        PnextChainPushFront(&physicalDeviceFeatures, &pDevice->mEnabledRayQueryFeatures);
    }

    //VkPhysicalDeviceRayTracingPipelineFeaturesKHR       mEnabledRayTracingPipelineFeatures

    // Get acceleration structure properties, which will be used later on
    //accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    //VkPhysicalDeviceFeatures2 deviceFeatures2{};
    //deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    //deviceFeatures2.pNext = &accelerationStructureFeatures;
    //vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);
    
    vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures);

    sparseBindingEnabled = physicalDeviceFeatures.features.sparseBinding != 0;

    // The extension is supported as fake with no real support for this feature? Don't use it.
    if(VK_AMD_device_coherent_memory_enabled && !pDevice->mEnabledDeviceCoherentMemoryFeatures.deviceCoherentMemory)
        VK_AMD_device_coherent_memory_enabled = false;
    
    if(VK_KHR_buffer_device_address_enabled && !pDevice->mEnabledBufferDeviceAddresFeatures.bufferDeviceAddress)
        VK_KHR_buffer_device_address_enabled = false;
    
    if(VK_EXT_memory_priority_enabled && !pDevice->mEnabledMemoryPriorityFeatures.memoryPriority)
        VK_EXT_memory_priority_enabled = false;
    
    if(VK_KHR_acceleration_structure_enabled && !pDevice->mEnabledAccelerationStructureFeatures.accelerationStructure)
        VK_KHR_acceleration_structure_enabled = false;

    if(VK_KHR_acceleration_structure_enabled && !pDevice->mEnabledAccelerationStructureFeatures.accelerationStructureHostCommands)
        VK_KHR_acceleration_structure_host_commands_enabled = false;

    if(VK_KHR_raytracing_pipeline_enabled && !pDevice->mEnabledRayTracingPipelineFeatures.rayTracingPipeline)
        VK_KHR_raytracing_pipeline_enabled = false;

    if(VK_EXT_host_query_reset_enabled && !pDevice->mEnabledHostQueryResetFeatures.hostQueryReset)
        VK_EXT_host_query_reset_enabled = false;

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

    std::vector<const char*> extensionNames; extensionNames.empty();

    std::vector<std::string> defaultExtensionNames = { 
        //"VK_KHR_swapchain",
        "VK_KHR_spirv_1_4",
        "VK_KHR_ray_query",
        "VK_KHR_ray_tracing_pipeline",
        "VK_KHR_buffer_device_address",
        "VK_KHR_acceleration_structure",
        "VK_KHR_deferred_host_operations",
        "VK_KHR_synchronization2",
        //"VK_KHR_get_physical_device_properties2",
        "VK_EXT_host_query_reset"
    };

    if (desc.surface != VK_NULL_HANDLE) defaultExtensionNames.push_back("VK_KHR_swapchain");


    // check for default extensions availability
    for (const std::string& a : defaultExtensionNames) {
        if (isExtensionSupported(a, pData->deviceExtensions)) {
            extensionNames.push_back(a.c_str());
        } else {
            LLOG_ERR << "The device doesn't support the requested '" << a << "`default extension";
        }
    }

    // check for additional extensions availability
    for (const std::string& a : desc.requiredExtensions) {
        if (isExtensionSupported(a, pData->deviceExtensions)) {
            extensionNames.push_back(a.c_str());
        } else {
            LLOG_ERR << "The device doesn't support the requested '" << a << "`additional extension";
        }
    }


    // Logical Device
    pDevice->mPhysicalDeviceFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    pDevice->mPhysicalDeviceFeatures.features.multiViewport = VK_TRUE;
    pDevice->mPhysicalDeviceFeatures.features.multiDrawIndirect = VK_TRUE;
    pDevice->mPhysicalDeviceFeatures.features.samplerAnisotropy = VK_TRUE;
    pDevice->mPhysicalDeviceFeatures.features.sparseBinding = sparseBindingEnabled ? VK_TRUE : VK_FALSE;

    if(VK_AMD_device_coherent_memory_enabled) {
        pDevice->mEnabledDeviceCoherentMemoryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD };
        pDevice->mEnabledDeviceCoherentMemoryFeatures.deviceCoherentMemory = VK_TRUE;
        PnextChainPushBack(&pDevice->mPhysicalDeviceFeatures, &pDevice->mEnabledDeviceCoherentMemoryFeatures);
    }

    if(VK_KHR_buffer_device_address_enabled) {
        pDevice->mEnabledBufferDeviceAddresFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR };
        pDevice->mEnabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
        PnextChainPushBack(&pDevice->mPhysicalDeviceFeatures, &pDevice->mEnabledBufferDeviceAddresFeatures);
    }

    if(VK_KHR_acceleration_structure_enabled) {
        pDevice->mEnabledAccelerationStructureFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        pDevice->mEnabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;

        if(VK_KHR_acceleration_structure_host_commands_enabled) {
            pDevice->mEnabledAccelerationStructureFeatures.accelerationStructureHostCommands = VK_TRUE;
        }
        PnextChainPushBack(&pDevice->mPhysicalDeviceFeatures, &pDevice->mEnabledAccelerationStructureFeatures);
    }

    if(VK_KHR_raytracing_pipeline_enabled) {
        pDevice->mEnabledRayTracingPipelineFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        pDevice->mEnabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        PnextChainPushBack(&pDevice->mPhysicalDeviceFeatures, &pDevice->mEnabledRayTracingPipelineFeatures);
    }

    if(VK_KHR_ray_query_enabled) {
        pDevice->mEnabledRayQueryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
        pDevice->mEnabledRayQueryFeatures.rayQuery = VK_TRUE;
        PnextChainPushBack(&pDevice->mPhysicalDeviceFeatures, &pDevice->mEnabledRayQueryFeatures);
    }

    if(VK_KHR_synchronization2_enabled) {
        pDevice->mEnabledSynchronization2Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR };
        pDevice->mEnabledSynchronization2Features.synchronization2 = VK_TRUE;
        PnextChainPushBack(&pDevice->mPhysicalDeviceFeatures, &pDevice->mEnabledSynchronization2Features);
    }

    if(VK_EXT_host_query_reset_enabled) {
        pDevice->mEnabledHostQueryResetFeatures.hostQueryReset = VK_TRUE;
        PnextChainPushBack(&pDevice->mPhysicalDeviceFeatures, &pDevice->mEnabledHostQueryResetFeatures);
    }   

    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.pNext = &pDevice->mPhysicalDeviceFeatures;
    deviceInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = (uint32_t)extensionNames.size();
    deviceInfo.ppEnabledExtensionNames = extensionNames.data();

    VkDevice device;
    if (VK_FAILED(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device))) {
        LLOG_ERR << "Could not create Vulkan logical device !";
        return nullptr;
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR  rayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};

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

    // Get the ray tracing and accelertion structure related function pointers required
    if(VK_KHR_buffer_device_address_enabled) {
        if(getVulkanApiVersion() >= VK_API_VERSION_1_2) {
            vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress"));
        } else {
            vkGetBufferDeviceAddressKHR =  reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
        }
    
        assert(vkGetBufferDeviceAddressKHR != nullptr);
    }

    if(VK_KHR_raytracing_pipeline_enabled) {
        vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
        assert(vkCmdTraceRaysKHR != nullptr);
    
        vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
        assert(vkCreateRayTracingPipelinesKHR != nullptr);

        vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
        assert(vkGetRayTracingShaderGroupHandlesKHR != nullptr);
    }

    if(VK_KHR_acceleration_structure_enabled) {
        vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
        assert(vkGetAccelerationStructureBuildSizesKHR != nullptr);

        vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
        assert(vkCreateAccelerationStructureKHR != nullptr);

        vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
        assert(vkDestroyAccelerationStructureKHR != nullptr);

        vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
        assert(vkGetAccelerationStructureDeviceAddressKHR != nullptr);

        vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
        assert(vkCmdBuildAccelerationStructuresKHR != nullptr);

        vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
        assert(vkBuildAccelerationStructuresKHR != nullptr);

        vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
        assert(vkCmdWriteAccelerationStructuresPropertiesKHR != nullptr);

        vkWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddr(device, "vkWriteAccelerationStructuresPropertiesKHR"));
        assert(vkWriteAccelerationStructuresPropertiesKHR != nullptr);

        vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR"));
        assert(vkCmdCopyAccelerationStructureKHR != nullptr);
    }


    vkDeferredOperationJoinKHR = reinterpret_cast<PFN_vkDeferredOperationJoinKHR>(vkGetDeviceProcAddr(device, "vkDeferredOperationJoinKHR"));
    assert(vkDeferredOperationJoinKHR != nullptr);

    vkGetDeferredOperationResultKHR = reinterpret_cast<PFN_vkGetDeferredOperationResultKHR>(vkGetDeviceProcAddr(device, "vkGetDeferredOperationResultKHR"));
    assert(vkGetDeferredOperationResultKHR != nullptr);


    if (VK_KHR_swapchain_enabled && desc.surface != VK_NULL_HANDLE) {
        vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
        assert(vkGetSwapchainImagesKHR != nullptr);

        vkDestroySwapchainKHR =  reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
        assert(vkDestroySwapchainKHR != nullptr);

        vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
        assert(vkCreateSwapchainKHR != nullptr);

        vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));
        assert(vkAcquireNextImageKHR != nullptr);

        vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));
        assert(vkQueuePresentKHR != nullptr);
    }

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

bool Device::createSwapChain(uint32_t width, uint32_t height, ResourceFormat colorFormat) {
    // Select/Validate SwapChain creation settings
    // Surface size
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mApiHandle, mApiHandle, &surfaceCapabilities);
    assert(surfaceCapabilities.supportedUsageFlags & (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));

    VkExtent2D swapchainExtent = {};
    if (surfaceCapabilities.currentExtent.width == (uint32_t)-1) {
        swapchainExtent.width = width;
        swapchainExtent.height = height;
    } else {
        swapchainExtent = surfaceCapabilities.currentExtent;
    }

    // Validate Surface format
    if (isSrgbFormat(colorFormat) == false) {
        LLOG_ERR << "Can't create a swap-chain with linear-space color format";
        return false;
    }

    const VkFormat requestedFormat = getVkFormat(colorFormat);
    const VkColorSpaceKHR requestedColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mApiHandle, mApiHandle, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mApiHandle, mApiHandle, &formatCount, surfaceFormats.data());

    bool formatValid = false;
    for (const VkSurfaceFormatKHR& format : surfaceFormats) {
        if (format.format == requestedFormat && format.colorSpace == requestedColorSpace) {
            formatValid = true;
            break;
        }
    }

    if (formatValid == false) {
        logError("Requested Swapchain format is not available");
        return false;
    }

    // Select present mode
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(mApiHandle, mApiHandle, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(mApiHandle, mApiHandle, &presentModeCount, presentModes.data());

    // Select present mode, FIFO for VSync, otherwise preferring IMMEDIATE -> MAILBOX -> FIFO
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    
    bool mVsyncOn = false; // TODO: make this configurable
    
    if (mVsyncOn == false) {
        for (size_t i = 0; i < presentModeCount; i++) {
            if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            } else if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    // Swapchain Creation
    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = mApiHandle;
    uint32_t maxImageCount = surfaceCapabilities.maxImageCount ? surfaceCapabilities.maxImageCount : UINT32_MAX; // 0 means no limit on the number of images
    info.minImageCount = clamp(kSwapChainBuffersCount, surfaceCapabilities.minImageCount, maxImageCount);
    info.imageFormat = requestedFormat;
    info.imageColorSpace = requestedColorSpace;
    info.imageExtent = { swapchainExtent.width, swapchainExtent.height };
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.preTransform = surfaceCapabilities.currentTransform;
    info.imageArrayLayers = 1;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;     // Only needed if VK_SHARING_MODE_CONCURRENT
    info.pQueueFamilyIndices = nullptr; // Only needed if VK_SHARING_MODE_CONCURRENT
    info.presentMode = presentMode;
    info.clipped = true;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.oldSwapchain = VK_NULL_HANDLE;

    if (VK_FAILED(vkCreateSwapchainKHR(mApiHandle, &info, nullptr, &mpApiData->swapchain))) {
        LLOG_ERR << "Could not create swapchain !!!";
        return false;
    }

    uint32_t swapChainCount = 0;
    vkGetSwapchainImagesKHR(mApiHandle, mpApiData->swapchain, &swapChainCount, nullptr);
    LLOG_DBG << "Swapchain image count is" << swapChainCount;
    assert(swapChainCount == kSwapChainBuffersCount);

    return true;
}

void Device::apiPresent() {
    assert(!mHeadless);  // presenting makes no sense in headless mode
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
    mVkInstance = pDeviceManager->vulkanInstance();
    if (!mVkInstance) return false;

    // Hook up callbacks for VK_EXT_debug_report
    if (mDesc.enableDebugLayer) {
        initDebugCallback(mVkInstance, &mpApiData->debugReportCallbackHandle);
    }

    mVkPhysicalDevice = initPhysicalDevice(mVkInstance, pDeviceManager->physicalDevices()[mGpuId], mpApiData, desc);
    if (!mVkPhysicalDevice) return false;

    mVkDevice = createLogicalDevice(this, mVkPhysicalDevice, mpApiData, desc, mCmdQueues, mDeviceFeatures);
    if (!mVkDevice) return false;

    assert(vkGetInstanceProcAddr);
    assert(vkGetDeviceProcAddr);
    load_VK_EXTENSIONS(mVkInstance, vkGetInstanceProcAddr, mVkDevice, vkGetDeviceProcAddr);
    //nvvk::load_VK_EXTENSIONS(VkInstance instance, PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkDevice device, PFN_vkGetDeviceProcAddr getDeviceProcAddr);


    if (initMemoryTypes(mVkPhysicalDevice, mpApiData) == false) return false;

    mApiHandle = DeviceHandle::create(shared_from_this(), mVkInstance, mVkPhysicalDevice, mVkDevice, mVkSurface);
    mGpuTimestampFrequency = getPhysicalDeviceLimits().timestampPeriod / (1000 * 1000);
    mPhysicalDeviceName = std::string(mpApiData->properties.deviceName);

    mSupportedFeatures = getSupportedFeatures(mVkPhysicalDevice);

    if(!mHeadless) {
        if (createSwapChain(desc.width, desc.height, desc.colorFormat) == false) return false;
    
        mpApiData->presentFences.f.resize(kSwapChainBuffersCount);
        for (auto& f : mpApiData->presentFences.f) {
            VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vk_call(vkCreateFence(mVkDevice, &info, nullptr, &f));
        }
    } else {
        if (createOffscreenFBO(desc.colorFormat) == false) {
            return false;
        }
    }

    VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {};
    vmaAllocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    vmaAllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaAllocatorCreateInfo.physicalDevice =mVkPhysicalDevice;
    vmaAllocatorCreateInfo.device = mVkDevice;
    vmaAllocatorCreateInfo.instance = mVkInstance;
    vmaAllocatorCreateInfo.preferredLargeHeapBlockSize = 0; // Set to 0 to use default, which is currently 256 MiB.
    vmaAllocatorCreateInfo.pRecordSettings = nullptr;
 
    vk_call(vmaCreateAllocator(&vmaAllocatorCreateInfo, &mAllocator));

    mNvvkResourceAllocator.init(mVkInstance, mApiHandle, mVkPhysicalDevice, NVVK_DEFAULT_STAGING_BLOCKSIZE, mAllocator);

    return true;
}

void Device::apiResizeSwapChain(uint32_t width, uint32_t height, ResourceFormat colorFormat) {
    assert(!mHeadless);  // swapchain resize makes no sense in headless mode
    vkDestroySwapchainKHR(mApiHandle, mpApiData->swapchain, nullptr);
    createSwapChain(width, height, colorFormat);
}

void Device::apiResizeOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat) {
    assert(mHeadless);
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

bool oneTimeCommandBuffer(Device::SharedPtr pDevice, VkCommandPool pool, const CommandQueueHandle& queue, OneTimeCommandFunc callback) {
    if (!callback)
        return false;

    const VkDevice& vkDevice = pDevice->getApiHandle();

    VkCommandBuffer cmdBuff = nullptr;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    if (VK_FAILED(vkAllocateCommandBuffers(vkDevice, &allocInfo, &cmdBuff))) {
        LOG_ERR("Error allocation command buffers");
        return false;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (VK_FAILED(vkBeginCommandBuffer(cmdBuff, &beginInfo))) {
        LOG_ERR("Error begin command buffer");
        return false;
    }

    callback(cmdBuff);

    if (VK_FAILED(vkEndCommandBuffer(cmdBuff))) {
        LOG_ERR("Error end command buffer");
        return false;
    }

    VkFence fence = VK_NULL_HANDLE;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    if (VK_FAILED(vkCreateFence(vkDevice, &fenceInfo, nullptr, &fence))) {
        LOG_ERR("Error creating fence");
        return false;
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuff;

    VkResult submit_result = vkQueueSubmit(queue, 1, &submitInfo, fence);

    if (submit_result != VK_SUCCESS) {
        LOG_ERR("Error submitting queue !!!");
        vkDestroyFence(vkDevice, fence, nullptr);
        return false;
    }

    vkDeviceWaitIdle(vkDevice);
    VkResult result = vkWaitForFences(vkDevice, 1, &fence, VK_TRUE, ~0ULL);

    if(result == VK_TIMEOUT) {
        LOG_ERR("Error waiting fot the fence. VK_TIMEOUT !!!");
        return false;
    } else if (result == VK_ERROR_DEVICE_LOST) {
        LOG_ERR("Error waiting fot the fence. VK_ERROR_DEVICE_LOST !!!");
        return false;
    }

    if(result == VK_SUCCESS) {
        vkDestroyFence(vkDevice, fence, nullptr);
        vkFreeCommandBuffers(vkDevice, pool, 1, &cmdBuff);
    }
    //VkResult result = vkQueueWaitIdle(queue);
    //if(result != VK_SUCCESS)
    //{
    //     LOG_ERR("Error vkQueueWaitIdle(queue) !!!");
    //}

    //vkFreeCommandBuffers(vkDevice, pool, 1, &cmdBuff);

    return true;
}

VkResult finishDeferredOperation(Device::SharedPtr pDevice, VkDeferredOperationKHR hOp) {
    // Attempt to join the operation until the implementation indicates that we should stop

    const VkDevice& vkDevice = pDevice->getApiHandle();

    VkResult result = vkDeferredOperationJoinKHR(vkDevice, hOp);
    while( result == VK_THREAD_IDLE_KHR ) {
        std::this_thread::yield();
        result = vkDeferredOperationJoinKHR(vkDevice, hOp);
    }

    switch( result ) {
        case VK_SUCCESS:
            {
                // deferred operation has finished.  Query its result
                result = vkGetDeferredOperationResultKHR(vkDevice, hOp);
            }
            break;

        case VK_THREAD_DONE_KHR:
            {
                // deferred operation is being wrapped up by another thread
                //  wait for that thread to finish
                do
                {
                    std::this_thread::yield();
                    result = vkGetDeferredOperationResultKHR(vkDevice, hOp);
                } while( result == VK_NOT_READY );
            }
            break;

        default:
            assert(false); // other conditions are illegal.
            break;
    }

    return result;
}


}  // namespace Falcor

