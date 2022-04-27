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
#include "Falcor/Core/API/DeviceManager.h"
#include "Falcor/Core/API/Vulkan/FalcorVK.h"
#include "Falcor/Core/API/Vulkan/VKDevice.h"
#include "Falcor.h"


#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {

VkInstance gVulkanInstance = VK_NULL_HANDLE;


bool DeviceManager::init() {
    if (mInitialized) return true;

    bool enableDebugLayer = true;
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

VkInstance DeviceManager::vulkanInstance() const { return gVulkanInstance; }

}  // namespace Falcor
