// vk-api.h
#ifndef SRC_GFX_VULKAN_VK_API_H_
#define SRC_GFX_VULKAN_VK_API_H_

#include "vk-module.h"
#include "vk-device-props.h"
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

namespace gfx {

#define VK_API_GLOBAL_PROCS(x) \
    x(vkGetInstanceProcAddr) \
    x(vkCreateInstance) \
    x(vkEnumerateInstanceLayerProperties) \
    x(vkEnumerateDeviceExtensionProperties) \
    x(vkDestroyInstance) \
    /* */

#define VK_API_INSTANCE_PROCS_OPT(x) \
    x(vkGetPhysicalDeviceFeatures2) \
    x(vkGetPhysicalDeviceProperties2) \
    x(vkCreateDebugReportCallbackEXT) \
    x(vkDestroyDebugReportCallbackEXT) \
    x(vkDebugReportMessageEXT) \
    x(vkGetPhysicalDeviceMultisamplePropertiesEXT) \
    /* */

#define VK_API_INSTANCE_PROCS(x) \
    x(vkCreateDevice) \
    x(vkDestroyDevice) \
    x(vkEnumeratePhysicalDevices) \
    x(vkGetPhysicalDeviceProperties) \
    x(vkGetPhysicalDeviceFeatures) \
    x(vkGetPhysicalDeviceMemoryProperties) \
    x(vkGetPhysicalDeviceQueueFamilyProperties) \
    x(vkGetPhysicalDeviceFormatProperties) \
    x(vkGetDeviceProcAddr) \
    /* */

#define VK_API_DEVICE_PROCS(x) \
    x(vkCreateDescriptorPool) \
    x(vkDestroyDescriptorPool) \
    x(vkResetDescriptorPool) \
    x(vkGetDeviceQueue) \
    x(vkDeviceWaitIdle) \
    x(vkQueueSubmit) \
    x(vkQueueWaitIdle) \
    x(vkQueueBindSparse) \
    x(vkCreateBuffer) \
    x(vkAllocateMemory) \
    x(vkMapMemory) \
    x(vkUnmapMemory) \
    x(vkCmdCopyBuffer) \
    x(vkDestroyBuffer) \
    x(vkFreeMemory) \
    x(vkCreateDescriptorSetLayout) \
    x(vkDestroyDescriptorSetLayout) \
    x(vkAllocateDescriptorSets) \
    x(vkFreeDescriptorSets) \
    x(vkUpdateDescriptorSets) \
    x(vkCreatePipelineLayout) \
    x(vkDestroyPipelineLayout) \
    x(vkCreateComputePipelines) \
    x(vkCreateGraphicsPipelines) \
    x(vkDestroyPipeline) \
    x(vkCreateShaderModule) \
    x(vkDestroyShaderModule) \
    x(vkCreateFramebuffer) \
    x(vkDestroyFramebuffer) \
    x(vkCreateImage) \
    x(vkDestroyImage) \
    x(vkCreateImageView) \
    x(vkDestroyImageView) \
    x(vkCreateRenderPass) \
    x(vkDestroyRenderPass) \
    x(vkCreateCommandPool) \
    x(vkDestroyCommandPool) \
    x(vkCreateSampler) \
    x(vkDestroySampler) \
    x(vkCreateBufferView) \
    x(vkDestroyBufferView) \
    \
    x(vkGetBufferMemoryRequirements) \
    x(vkGetImageMemoryRequirements) \
    x(vkGetImageSparseMemoryRequirements) \
    \
    x(vkCmdBindPipeline) \
    x(vkCmdClearAttachments) \
    x(vkCmdClearColorImage) \
    x(vkCmdClearDepthStencilImage) \
    x(vkCmdFillBuffer) \
    x(vkCmdBindDescriptorSets) \
    x(vkCmdDispatch) \
    x(vkCmdDispatchIndirect) \
    x(vkCmdDraw) \
    x(vkCmdDrawIndexed) \
    x(vkCmdDrawIndirect) \
    x(vkCmdDrawIndexedIndirect) \
    x(vkCmdSetScissor) \
    x(vkCmdSetViewport) \
    x(vkCmdBindVertexBuffers) \
    x(vkCmdBindIndexBuffer) \
    x(vkCmdBeginRenderPass) \
    x(vkCmdEndRenderPass) \
    x(vkCmdPipelineBarrier) \
    x(vkCmdCopyBufferToImage)\
    x(vkCmdCopyImage) \
    x(vkCmdCopyImageToBuffer) \
    x(vkCmdResolveImage) \
    x(vkCmdPushConstants) \
    x(vkCmdSetStencilReference) \
    x(vkCmdWriteTimestamp) \
    x(vkCmdBeginQuery) \
    x(vkCmdEndQuery) \
    x(vkCmdResetQueryPool) \
    x(vkCmdCopyQueryPoolResults) \
    \
    x(vkCreateFence) \
    x(vkDestroyFence) \
    x(vkResetFences) \
    x(vkGetFenceStatus) \
    x(vkWaitForFences) \
    \
    x(vkCreateSemaphore) \
    x(vkDestroySemaphore) \
    \
    x(vkCreateEvent) \
    x(vkDestroyEvent) \
    x(vkGetEventStatus) \
    x(vkSetEvent) \
    x(vkResetEvent) \
    \
    x(vkFreeCommandBuffers) \
    x(vkAllocateCommandBuffers) \
    x(vkBeginCommandBuffer) \
    x(vkEndCommandBuffer) \
    x(vkResetCommandBuffer) \
    x(vkResetCommandPool) \
    \
    x(vkBindImageMemory) \
    x(vkBindBufferMemory) \
    \
    x(vkCreateQueryPool) \
    x(vkGetQueryPoolResults) \
    x(vkDestroyQueryPool) \
    \
    x(vkCreatePipelineCache) \
    x(vkGetPipelineCacheData) \
    x(vkMergePipelineCaches) \
    x(vkDestroyPipelineCache) \
    /* */

#if SLANG_WINDOWS_FAMILY
#   define VK_API_INSTANCE_PLATFORM_KHR_PROCS(x)          \
    x(vkCreateWin32SurfaceKHR) \
    /* */
#else
#   define VK_API_INSTANCE_PLATFORM_KHR_PROCS(x)          \
    x(vkCreateXlibSurfaceKHR) \
    /* */
#endif

#define VK_API_INSTANCE_KHR_PROCS(x)          \
    VK_API_INSTANCE_PLATFORM_KHR_PROCS(x) \
    x(vkGetPhysicalDeviceSurfaceSupportKHR) \
    x(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    x(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    x(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    x(vkCmdDrawIndexedIndirectCountKHR) \
    x(vkCmdDrawIndirectCountKHR) \
    x(vkDestroySurfaceKHR) \
    x(vkGetPhysicalDeviceFeatures2KHR) \
    x(vkGetPhysicalDeviceFormatProperties2KHR) \
    x(vkGetPhysicalDeviceImageFormatProperties2KHR) \
    x(vkGetPhysicalDeviceMemoryProperties2KHR) \
    x(vkGetPhysicalDeviceProperties2KHR) \
    x(vkGetPhysicalDeviceQueueFamilyProperties2KHR) \
    x(vkGetPhysicalDeviceSparseImageFormatProperties2KHR) \

    /* */

#define VK_API_DEVICE_KHR_PROCS(x) \
    x(vkQueuePresentKHR) \
    x(vkCreateSwapchainKHR) \
    x(vkGetSwapchainImagesKHR) \
    x(vkDestroySwapchainKHR) \
    x(vkAcquireNextImageKHR) \
    x(vkCreateRayTracingPipelinesKHR) \
    x(vkCmdTraceRaysKHR) \
    x(vkGetRayTracingShaderGroupHandlesKHR) \
    /* */

#if SLANG_WINDOWS_FAMILY
#   define VK_API_DEVICE_PLATFORM_OPT_PROCS(x) \
    x(vkGetMemoryWin32HandleKHR) \
    x(vkGetSemaphoreWin32HandleKHR) \
    /* */
#else
#   define VK_API_DEVICE_PLATFORM_OPT_PROCS(x) \
    x(vkGetMemoryFdKHR) \
    /* */
#endif

#define VK_API_DEVICE_OPT_PROCS(x) \
    VK_API_DEVICE_PLATFORM_OPT_PROCS(x) \
    x(vkCmdSetPrimitiveTopologyEXT) \
    x(vkGetBufferDeviceAddress) \
    x(vkGetBufferDeviceAddressKHR) \
    x(vkGetBufferDeviceAddressEXT) \
    x(vkCmdBuildAccelerationStructuresKHR) \
    x(vkCmdCopyAccelerationStructureKHR) \
    x(vkCmdCopyAccelerationStructureToMemoryKHR) \
    x(vkCmdCopyMemoryToAccelerationStructureKHR) \
    x(vkCmdWriteAccelerationStructuresPropertiesKHR) \
    x(vkCreateAccelerationStructureKHR) \
    x(vkDestroyAccelerationStructureKHR) \
    x(vkGetAccelerationStructureBuildSizesKHR) \
    x(vkGetSemaphoreCounterValue) \
    x(vkGetSemaphoreCounterValueKHR) \
    x(vkSignalSemaphore) \
    x(vkSignalSemaphoreKHR) \
    x(vkWaitSemaphores) \
    x(vkWaitSemaphoresKHR) \
    x(vkCmdSetSampleLocationsEXT) \
    x(vkCmdDebugMarkerBeginEXT) \
    x(vkCmdDebugMarkerEndEXT) \
    x(vkDebugMarkerSetObjectNameEXT) \
    /* */

#if VK_MESH_SHADERS_ENABLED
    #define VK_API_DEVICE_MESH_SHADERS_PROCS(x) \
    VK_API_DEVICE_PLATFORM_OPT_PROCS(x) \
    x(vkCmdDrawMeshTasksEXT) \
    /* */
#else
    #define VK_API_DEVICE_MESH_SHADERS_PROCS(x)
#endif // VK_MESH_SHADERS_ENABLED

#define VK_API_ALL_GLOBAL_PROCS(x) \
    VK_API_GLOBAL_PROCS(x)

#define VK_API_ALL_INSTANCE_PROCS(x) \
    VK_API_INSTANCE_PROCS(x) \
    VK_API_INSTANCE_KHR_PROCS(x)

#define VK_API_ALL_DEVICE_PROCS(x) \
    VK_API_DEVICE_PROCS(x) \
    VK_API_DEVICE_KHR_PROCS(x) \
    VK_API_DEVICE_OPT_PROCS(x) \
    VK_API_DEVICE_MESH_SHADERS_PROCS(x)

#define VK_API_ALL_PROCS(x) \
    VK_API_ALL_GLOBAL_PROCS(x) \
    VK_API_ALL_INSTANCE_PROCS(x) \
    VK_API_ALL_DEVICE_PROCS(x) \
    \
    VK_API_INSTANCE_PROCS_OPT(x) \
    /* */

#define VK_API_DECLARE_PROC(NAME) PFN_##NAME NAME = nullptr;


struct VulkanExtendedFeatureProperties {
    // 16 bit storage features
    VkPhysicalDevice16BitStorageFeatures storage16BitFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,
        .pNext = NULL
    };

    // Atomic Float features
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        .pNext = NULL
    };

    // Atomic Float2 features
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT atomicFloat2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT,
        .pNext = NULL
    };

    // Image int64 atomic features
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT imageInt64AtomicFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT,
        .pNext = NULL
    };
    
    // Timeline Semaphore features
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
        .pNext = NULL
    };
    
    // Extended dynamic state features
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = NULL
    };
    
    // Subgroup extended type features
    VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures shaderSubgroupExtendedTypeFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES,
        .pNext = NULL
    };
    
    // Acceleration structure features
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = NULL
    };
    
    // Ray tracing pipeline features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = NULL
    };

    // Ray query (inline ray-tracing) features
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
        .pNext = NULL
    };

    // Buffer device address features
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext = NULL
    };
    
    // Inline uniform block features
    VkPhysicalDeviceInlineUniformBlockFeaturesEXT inlineUniformBlockFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT,
        .pNext = NULL
    };
    
    // Robustness2 features
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = NULL
    };

    // Fragment shader barycentrics features
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV fragmentShaderBarycentricFeaturesNV = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV,
        .pNext = NULL
    };

    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR fragmentShaderBarycentricFeaturesKHR = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,
        .pNext = NULL
    };

    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,
        .pNext = NULL
    };

    //VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV rayTracingInvocationReorderFeatures = {
    //    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV,
    //    .pNext = NULL
    //};

    VkPhysicalDeviceRayTracingMotionBlurFeaturesNV rayTracingMotionBlurFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV,
        .pNext = NULL
    };

    VkPhysicalDeviceVariablePointerFeaturesKHR variablePointersFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES_KHR,
        .pNext = NULL
    };

    VkPhysicalDeviceComputeShaderDerivativesFeaturesNV computeShaderDerivativeFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV,
        .pNext = NULL
    };

    // Clock features
    VkPhysicalDeviceShaderClockFeaturesKHR clockFeatures = { 
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR,
        .pNext = NULL
    };

#if VK_MESH_SHADERS_ENABLED
    // Mesh shader features
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext = NULL
    };
#endif // VK_MESH_SHADERS_ENABLED

    // Multiview features
    VkPhysicalDeviceMultiviewFeaturesKHR multiviewFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR, 
        .pNext = NULL
    };

    // Fragment shading rate features
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR, 
        .pNext = NULL
    };

    // Vulkan1.2 features
    VkPhysicalDeviceVulkan12Features vulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = NULL
    };

    // Ray tracing validation features
    //VkPhysicalDeviceRayTracingValidationFeaturesNV rayTracingValidationFeatures = {
    //    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV
    //};
};

struct VulkanApi
{
    VK_API_ALL_PROCS(VK_API_DECLARE_PROC)

    enum class ProcType
    {
        Global,
        Instance,
        Device,
    };

        /// Returns true if all the functions in the class are defined
    bool areDefined(ProcType type) const;

        /// Sets up global parameters
    Slang::Result initGlobalProcs(const VulkanModule& module);
        /// Initialize the instance functions
    Slang::Result initInstanceProcs(VkInstance instance);

        /// Called before initDevice
    Slang::Result initPhysicalDevice(VkPhysicalDevice physicalDevice);

        /// Initialize the device functions
    Slang::Result initDeviceProcs(VkDevice device);

        /// Type bits control which indices are tested against bit 0 for testing at index 0
        /// properties - a memory type must have all the bits set as passed in
        /// Returns -1 if couldn't find an appropriate memory type index
    int findMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

        /// Given queue required flags, finds a queue
    int findQueue(VkQueueFlags reqFlags) const;

    const VulkanModule* m_module = nullptr;               ///< Module this was all loaded from
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties                              m_deviceProperties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR         m_rtProperties;
    VkPhysicalDeviceSubgroupProperties                      m_deviceSubgroupProperties;
    VkPhysicalDeviceFeatures                                m_deviceFeatures;
    VkPhysicalDeviceMemoryProperties                        m_deviceMemoryProperties;
    VulkanExtendedFeatureProperties                         m_extendedFeatures;

    vk::SubgroupSizeControlProperties                       mSubgroupSizeControlProperties;
    vk::AccelerationStructureProperties                     mAccelerationStructureProperties;

    VmaAllocator                                            mVmaAllocator;
};

} // namespace gfx

#endif // SRC_GFX_VULKAN_VK_API_H_