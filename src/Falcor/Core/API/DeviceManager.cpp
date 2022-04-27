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
#include <pybind11/pybind11.h>

#include "Falcor/stdafx.h"
#include "DeviceManager.h"
#include "Falcor/Core/API/Vulkan/FalcorVK.h"


#include "Falcor/Utils/Debug/debug.h"
#include "lava_utils_lib/logging.h"

namespace py = pybind11;

namespace Falcor {

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


DeviceManager::DeviceManager() {

}

DeviceManager::~DeviceManager() {
    for( auto& [id, pDevice]: mRenderingDevices ) {
        if (pDevice) {
            pDevice->cleanup();
            pDevice.reset();
        }
    }
}

//static
DeviceManager::SharedPtr DeviceManager::create() {
    auto pDeviceManager = new DeviceManager();

    if (!pDeviceManager->init()) {
        delete pDeviceManager;
        return nullptr;
    }

    pDeviceManager->printEnumeratedDevices();

    return SharedPtr(pDeviceManager);
}

std::vector<Device::SharedPtr> DeviceManager::renderingDevices() const {
    std::vector<Device::SharedPtr> devices;
    for( auto& device: mRenderingDevices ) {
        devices.push_back(device.second);
    }
    return devices;
}

bool DeviceManager::deviceEnumerated(uint8_t gpuId) const {
    if(mDeviceNames.find(gpuId) != mDeviceNames.end()) {
        return true;
    }
    LLOG_ERR << "Device " << gpuId << "not enumerated !";
    return false;
}

Device::SharedPtr DeviceManager::createRenderingDevice(uint8_t gpuId, const Device::Desc &desc) {
    if (!deviceEnumerated(gpuId)) {
        LLOG_ERR << "Rendering device " << to_string(gpuId) << " not enumerated !!!";
        return nullptr;
    }

    Device::SharedPtr pDevice = renderingDevice(gpuId);
    if(pDevice) return pDevice;

    pDevice = Device::create(shared_from_this(), gpuId, desc);
    if (!pDevice) {
        LLOG_ERR << "Unable to create rendering device on gpu " << gpuId << " !";
        return nullptr;
    }

    mRenderingDevices[gpuId] = pDevice;
    return pDevice;
}

Device::SharedPtr DeviceManager::renderingDevice(uint8_t gpuId) const {
    if (!deviceEnumerated(gpuId)) return nullptr;

    auto it = mRenderingDevices.find(gpuId);

    if (it == mRenderingDevices.end()) {
        // device not created yet ...
        return nullptr;
    }
    return it->second;
}


void DeviceManager::printEnumeratedDevices() const {
    LLOG_INF << "Enumerated Vulkan physical devices...";
    for( auto &it: mDeviceNames) {
        LLOG_INF << "Physical device id: " << it.first << ", name: " << it.second;
    }
}

Device::SharedPtr DeviceManager::defaultRenderingDevice() const {
    return renderingDevice(mDefaultRenderingDeviceID);
}

Device::SharedPtr DeviceManager::defaultDisplayDevice() const {
    return renderingDevice(mDefaultDisplayDeviceID);
}

void DeviceManager::setDefaultRenderingDevice(uint8_t gpuId) {
    if(gpuId < mRenderingDevices.size())
        mDefaultRenderingDeviceID = gpuId;
}

VkInstance DeviceManager::createVulkanInstance(bool enableDebugLayer) {
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
        LLOG_FTL << "Failed to create Vulkan instance !!!";
        return VK_NULL_HANDLE;
    }

    return instance;
}

#ifdef SCRIPTING
SCRIPT_BINDING(DeviceManager) {
    pybind11::class_<DeviceManager, Device::SharedPtr> deviceManagerClass(m, "DeviceManager");
    deviceManagerClass.def_static("create", [](){ return Device::SharedPtr<DeviceManager, py::nodelete>(&DeviceManager::create()); });
    deviceManagerClass.def("listDevices", &DeviceManager::listDevices);
    deviceManagerClass.def("defaultRenderingDevice", &DeviceManager::defaultRenderingDevice);
}
#endif

}  // namespace Falcor
