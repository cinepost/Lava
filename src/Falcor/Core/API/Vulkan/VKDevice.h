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
#ifndef SRC_FALCOR_CORE_API_VULKAN_VKDEVICE_H_
#define SRC_FALCOR_CORE_API_VULKAN_VKDEVICE_H_

#include "Falcor/Core/API/Vulkan/FalcorVK.h"
#include "Falcor/Core/API/Device.h"

namespace Falcor {

#ifdef DEFAULT_ENABLE_DEBUG_LAYER
VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage,
    void*                       pUserData);
#endif

struct DeviceApiData {
    VkSwapchainKHR swapchain;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    uint32_t falcorToVulkanQueueType[Device::kQueueTypeCount];
    uint32_t vkMemoryTypeBits[(uint32_t)Device::MemoryType::Count];
    VkPhysicalDeviceLimits deviceLimits;
    std::vector<VkExtensionProperties> deviceExtensions;

    struct {
        std::vector<VkFence> f;
        uint32_t cur = 0;
    } presentFences;

    #ifdef DEFAULT_ENABLE_DEBUG_LAYER
    VkDebugReportCallbackEXT debugReportCallbackHandle;
    #endif
};

VkInstance createInstance(DeviceApiData* pData, bool enableDebugLayer);

using OneTimeCommandFunc = std::function<void(VkCommandBuffer)>;
bool oneTimeCommandBuffer(Device::SharedPtr pDevice, VkCommandPool pool, const CommandQueueHandle& queue, OneTimeCommandFunc callback);

VkResult finishDeferredOperation(Device::SharedPtr pDevice, VkDeferredOperationKHR hOp);


}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_VULKAN_VKDEVICE_H_
