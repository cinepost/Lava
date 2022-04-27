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
#ifndef SRC_FALCOR_CORE_API_VULKAN_FALCORVK_H_
#define SRC_FALCOR_CORE_API_VULKAN_FALCORVK_H_

#define NOMINMAX
#include "Falcor/Core/API/Formats.h"

#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#else
    #define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <vulkan/vulkan.h>

// Remove defines from XLib.h (included by vulkan.h) that cause conflicts
#ifndef _WIN32
#undef None
#undef Status
#undef Bool
#undef Always
#endif

#ifdef _WIN32
    #pragma comment(lib, "vulkan-1.lib")
#endif

#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Core/API/Vulkan/VKSmartHandle.h"


namespace Falcor {

extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;

extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
extern PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;

extern PFN_vkWriteAccelerationStructuresPropertiesKHR vkWriteAccelerationStructuresPropertiesKHR;
extern PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
extern PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;

extern PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
extern PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;

extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

extern PFN_vkGetSwapchainImagesKHR  vkGetSwapchainImagesKHR;
extern PFN_vkDestroySwapchainKHR    vkDestroySwapchainKHR;
extern PFN_vkCreateSwapchainKHR     vkCreateSwapchainKHR;
extern PFN_vkAcquireNextImageKHR    vkAcquireNextImageKHR;
extern PFN_vkQueuePresentKHR        vkQueuePresentKHR;

struct VkFormatDesc {
    ResourceFormat falcorFormat;
    VkFormat vkFormat;
};

extern const VkFormatDesc kVkFormatDesc[];

inline VkFormat getVkFormat(ResourceFormat format) {
    assert(kVkFormatDesc[(uint32_t)format].falcorFormat == format);
    assert(kVkFormatDesc[(uint32_t)format].vkFormat != VK_FORMAT_UNDEFINED);
    return kVkFormatDesc[(uint32_t)format].vkFormat;
}

inline VkIndexType getVkIndexType(ResourceFormat format) {
    switch (format) {
        case ResourceFormat::R16Uint:
            return VK_INDEX_TYPE_UINT16;
        case ResourceFormat::R32Uint:
            return VK_INDEX_TYPE_UINT32;
        default:
            should_not_get_here();
            return VK_INDEX_TYPE_MAX_ENUM;
    }
}

using HeapCpuHandle = void*;
using HeapGpuHandle = void*;

class DescriptorHeapEntry;

using DeviceHandle = VkDeviceData::SharedPtr;
using CommandListHandle = VkCommandBuffer;
using CommandQueueHandle = VkQueue;
using ApiCommandQueueType = uint32_t;
using CommandAllocatorHandle = VkHandle<VkCommandPool>::SharedPtr;
using CommandSignatureHandle = void*;
using FenceHandle = VkSemaphore;
using ResourceHandle = VkResource<VkImage, VkBuffer>::SharedPtr;
using RtvHandle = VkResource<VkImageView, VkBufferView>::SharedPtr;
using DsvHandle = VkResource<VkImageView, VkBufferView>::SharedPtr;
using SrvHandle = VkResource<VkImageView, VkBufferView>::SharedPtr;
using UavHandle = VkResource<VkImageView, VkBufferView>::SharedPtr;
using CbvHandle = VkResource<VkImageView, VkBufferView>::SharedPtr;
using FboHandle = VkFbo::SharedPtr;
using SamplerHandle = VkHandle<VkSampler>::SharedPtr;
using GpuAddress = size_t;
using DescriptorSetApiHandle = VkDescriptorSet;
using QueryHeapHandle = VkHandle<VkQueryPool>::SharedPtr;

using GraphicsStateHandle = VkHandle<VkPipeline>::SharedPtr;
using ComputeStateHandle = VkHandle<VkPipeline>::SharedPtr;
using ShaderHandle = VkHandle<VkShaderModule>::SharedPtr;
using ShaderReflectionHandle = void*;
using RootSignatureHandle = VkRootSignature::SharedPtr;
using DescriptorHeapHandle = VkHandle<VkDescriptorPool>::SharedPtr;

using VaoHandle = void*;
using VertexShaderHandle = void*;
using FragmentShaderHandle = void*;
using DomainShaderHandle = void*;
using HullShaderHandle = void*;
using GeometryShaderHandle = void*;
using ComputeShaderHandle = void*;
using ProgramHandle = void*;
using DepthStencilStateHandle = void*;
using RasterizerStateHandle = void*;
using BlendStateHandle = void*;

static const uint32_t kDefaultSwapChainBuffers = 5;

using ApiObjectHandle = VkBaseApiHandle::SharedPtr;
class Device;

uint32_t getMaxViewportCount(std::shared_ptr<Device> device);

// The max scalars supported by our driver
#define FALCOR_RT_MAX_PAYLOAD_SIZE_IN_BYTES (14 * sizeof(float))
#define FALCOR_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES 32

}  // namespace Falcor

#define VK_FAILED(res) (res != VK_SUCCESS)

#if _LOG_ENABLED
#define vk_call(a) \
{ \
    auto r = a; \
    if (VK_FAILED(r)) { \
        LOG_ERR("Vulkan call %s failed!", #a); \
        /* logError("Vulkan call failed.\n"#a); */ \
    } \
}
#else
#define vk_call(a) a
#endif

#define UNSUPPORTED_IN_VULKAN(msg_) {logWarning(msg_ + std::string(" is not supported in Vulkan. Ignoring call."));}

#endif  // SRC_FALCOR_CORE_API_VULKAN_FALCORVK_H_
