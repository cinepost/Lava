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
#ifndef SRC_FALCOR_CORE_API_DEVICEMANAGER_H_
#define SRC_FALCOR_CORE_API_DEVICEMANAGER_H_

#include <list>
#include <string>
#include <memory>
#include <queue>
#include <vector>
#include <atomic>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Falcor/Core/API/Device.h"

namespace Falcor {

#ifdef _DEBUG
    #define DEFAULT_ENABLE_DEBUG_LAYER true
#else
    #define DEFAULT_ENABLE_DEBUG_LAYER false
#endif

class FALCOR_API DeviceManager {
  public:
    struct DeviceInfo {
        std::string deviceName;
        std::array<uint8_t, VK_UUID_SIZE> pipelineCacheUUID;
        VkPhysicalDeviceLimits limits;
    };

  public:
     DeviceManager();
    ~DeviceManager();

    const std::unordered_map<uint8_t, DeviceInfo>& deviceInfos() { return mDeviceInfos; }
    std::vector<ref<Device>> renderingDevices() const;

    ref<Device> renderingDevice(uint8_t gpuId) const;

    ref<Device> createRenderingDevice(uint8_t gpuId, const Device::Desc &desc);

    ref<Device> defaultDisplayDevice() const;
    ref<Device> defaultRenderingDevice() const;

    void setDefaultRenderingDevice(uint8_t gpuId);

    void printEnumeratedDevices() const;

    uint32_t physicalDevicesCount() const { return mPhysicalDevicesCount; }

    const std::vector<VkPhysicalDevice>& physicalDevices() const { return mPhysicalDevices; }
    VkInstance vulkanInstance() const;

    static std::unique_ptr<DeviceManager> create(bool enableValidationLayer = false);

 private:

    static VkInstance createVulkanInstance(bool enableDebugLayer);

    bool init();
    bool deviceEnumerated(uint8_t gpuId) const;
    void enumerateDevices();

    bool mInitialized = false;
    bool mEnableValidationLayer = false;

    std::unordered_map<uint8_t, DeviceInfo> mDeviceInfos;
    std::unordered_map<uint8_t, ref<Device>> mRenderingDevices; 

    uint8_t mDefaultDisplayDeviceID = 0;
    uint8_t mDefaultRenderingDeviceID = 0;   

    uint32_t                        mPhysicalDevicesCount = 0;

    std::vector<VkPhysicalDevice>   mPhysicalDevices;
};


}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_DEVICEMANAGER_H_
