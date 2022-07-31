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

#include <vulkan/vulkan.h>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/DeviceManager.h"

namespace Falcor {

VkInstance gVulkanInstance = VK_NULL_HANDLE;

static bool isLayerSupported(const std::string& layer, const std::vector<VkLayerProperties>& supportedLayers) {
    for (const auto& l : supportedLayers) {
        if (l.layerName == layer) return true;
    }
    return false;
}


static void enableLayerIfPresent(const char* layerName, const std::vector<VkLayerProperties>& supportedLayers, std::vector<const char*>& requiredLayers) {
    if (isLayerSupported(layerName, supportedLayers)) {
        requiredLayers.push_back(layerName);
    } else {
        LLOG_ERR << "Can't enable requested Vulkan layer " << std::string(layerName) << ". Something bad might happen. Or not, depends on the layer.";
    }
}

static std::vector<VkLayerProperties> enumarateInstanceLayersProperties() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

    for (const VkLayerProperties& layer : layerProperties) {
        LLOG_DBG << "Available Vulkan Layer: " << std::string(layer.layerName) << 
                    " - VK Spec Version: " << std::to_string(layer.specVersion) <<
                    " - Implementation Version: " << std::to_string(layer.implementationVersion);
    }

    return layerProperties;
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
        LLOG_DBG << "Available Instance Extension: " << std::string(extension.extensionName) << " - VK Spec Version: " << std::to_string(extension.specVersion);
    }

    return availableInstanceExtensions;
}

static VkInstance createVulkanInstance(bool enableDebugLayer) {
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
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        LLOG_FTL << "Failed to create Vulkan instance !!!";
        return VK_NULL_HANDLE;
    }

    return instance;
}

bool DeviceManager::init() {
    if (mInitialized) return true;

#ifdef _DEBUG
    bool enableDebugLayer = true;
#else
    bool enableDebugLayer = false;
#endif
    
    gVulkanInstance = createVulkanInstance(enableDebugLayer);
    
    if (gVulkanInstance == VK_NULL_HANDLE) return false;

    enumerateDevices();

    mInitialized = true;
    return mInitialized;
}

void DeviceManager::enumerateDevices() {
    // Enumerate devices
    vkEnumeratePhysicalDevices(gVulkanInstance, &mPhysicalDevicesCount, nullptr);
    assert(mPhysicalDevicesCount > 0);

    mPhysicalDevices.clear();
    mPhysicalDevices.resize(mPhysicalDevicesCount);
    vkEnumeratePhysicalDevices(gVulkanInstance, &mPhysicalDevicesCount, mPhysicalDevices.data());

    uint8_t id = 0;
    VkPhysicalDeviceProperties properties;
    for( auto const &physicalDevice: mPhysicalDevices) {
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        mDeviceNames[id] = std::string(properties.deviceName);
        id++;
    }
}

/*
enum class InteropHandleAPI
{
    Unknown,
    D3D12, // A D3D12 object pointer.
    Vulkan, // A general Vulkan object handle.
    CUDA, // A general CUDA object handle.
    Win32, // A general Win32 HANDLE.
    FileDescriptor, // A file descriptor.
    DeviceAddress, // A device address.
    D3D12CpuDescriptorHandle, // A D3D12_CPU_DESCRIPTOR_HANDLE value.
};

struct InteropHandle
{
    InteropHandleAPI api = InteropHandleAPI::Unknown;
    uint64_t handleValue = 0;
};

or Vulkan, the first InteropHandle is the VkInstance, VkPhysicalDevice, VkDevice.
        InteropHandles existingDeviceHandles;
*/


Device::SharedPtr DeviceManager::createRenderingDevice(uint8_t gpuId, const Device::Desc &desc) {
    if (!deviceEnumerated(gpuId)) {
        LLOG_ERR << "Rendering device " << to_string(gpuId) << " not enumerated !!!";
        return nullptr;
    }

    Device::SharedPtr pDevice = renderingDevice(gpuId);
    if(pDevice) return pDevice;

    gfx::IDevice::Desc iDesc;


#ifdef FALCOR_GFX_VK

    // VkInstance
    iDesc.existingDeviceHandles.handles[0].api = gfx::InteropHandleAPI::Vulkan;
    VkInstance instance = vulkanInstance();
    iDesc.existingDeviceHandles.handles[0].handleValue = reinterpret_cast<uint64_t>(instance);

    // VkPhysicalDevice
    iDesc.existingDeviceHandles.handles[1].api = gfx::InteropHandleAPI::Vulkan;
    iDesc.existingDeviceHandles.handles[1].handleValue = reinterpret_cast<uint64_t>(mPhysicalDevices[gpuId]);

    // VkDevice here is 0, to be created by GFX
    iDesc.existingDeviceHandles.handles[2].api = gfx::InteropHandleAPI::Vulkan;
    iDesc.existingDeviceHandles.handles[2].handleValue = 0;

#endif  // FALCOR_GFX_VK

    Window::SharedPtr pWindow = nullptr;
    pDevice = Device::create(pWindow, iDesc, desc);
    if (!pDevice) {
        LLOG_ERR << "Unable to create rendering device on gpu " << std::to_string(gpuId) << " !";
        return nullptr;
    } else {
        LLOG_INF << "Rendering device created on gpu id " << std::to_string(gpuId) << "!";
    }

    mRenderingDevices[gpuId] = pDevice;
    return pDevice;
}

VkInstance DeviceManager::vulkanInstance() const { return gVulkanInstance; }

}  // namespace Falcor
