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
    VK_API_GLOBAL_PROCS(x) \

#define VK_API_ALL_INSTANCE_PROCS(x) \
    VK_API_INSTANCE_PROCS(x) \
    VK_API_INSTANCE_KHR_PROCS(x) \

#define VK_API_ALL_DEVICE_PROCS(x) \
    VK_API_DEVICE_PROCS(x) \
    VK_API_DEVICE_KHR_PROCS(x) \
    VK_API_DEVICE_OPT_PROCS(x) \
    VK_API_DEVICE_MESH_SHADERS_PROCS(x) \

#define VK_API_ALL_PROCS(x) \
    VK_API_ALL_GLOBAL_PROCS(x) \
    VK_API_ALL_INSTANCE_PROCS(x) \
    VK_API_ALL_DEVICE_PROCS(x) \
    VK_API_INSTANCE_PROCS_OPT(x) \

#define VK_API_DECLARE_PROC(NAME) PFN_##NAME NAME = nullptr;


struct VulkanExtendedFeatureProperties {
    // 16 bit storage features
    VkPhysicalDevice16BitStorageFeatures storage16BitFeatures;

    // Atomic Float features
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures;

    // Atomic Float2 features
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT atomicFloat2Features;

    // Image int64 atomic features
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT imageInt64AtomicFeatures;
    
    // Timeline Semaphore features
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures;
    
    // Extended dynamic state features
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures;
    
    // Subgroup extended type features
    VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures shaderSubgroupExtendedTypeFeatures;
    
    // Acceleration structure features
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures;
    
    // Ray tracing pipeline features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures;

    // Ray query (inline ray-tracing) features
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;

    // Buffer device address features
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures;
    
    // Inline uniform block features
    VkPhysicalDeviceInlineUniformBlockFeaturesEXT inlineUniformBlockFeatures;
    
    // Robustness2 features
    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features;

    // Fragment shader barycentrics features
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV fragmentShaderBarycentricFeaturesNV;

    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR fragmentShaderBarycentricFeaturesKHR;

    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures;

    //VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV rayTracingInvocationReorderFeatures;

    VkPhysicalDeviceRayTracingMotionBlurFeaturesNV rayTracingMotionBlurFeatures;

    VkPhysicalDeviceVariablePointerFeaturesKHR variablePointersFeatures;

    VkPhysicalDeviceComputeShaderDerivativesFeaturesNV computeShaderDerivativeFeatures;

    // Clock features
    VkPhysicalDeviceShaderClockFeaturesKHR clockFeatures;

#if VK_MESH_SHADERS_ENABLED
    // Mesh shader features
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures;
#endif // VK_MESH_SHADERS_ENABLED

    // Multiview features
    VkPhysicalDeviceMultiviewFeaturesKHR multiviewFeatures;

    // Fragment shading rate features
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures;

    // Vulkan1.2 features
    VkPhysicalDeviceVulkan12Features vulkan12Features;

    // Ray tracing validation features
    //VkPhysicalDeviceRayTracingValidationFeaturesNV rayTracingValidationFeatures;

    VulkanExtendedFeatureProperties() {
        storage16BitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
        storage16BitFeatures.pNext = NULL;

        atomicFloatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
        atomicFloatFeatures.pNext = NULL;
    
        atomicFloat2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT;
        atomicFloat2Features.pNext = NULL;
    
        imageInt64AtomicFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT;
        imageInt64AtomicFeatures.pNext = NULL;
        
        timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        timelineFeatures.pNext = NULL;
        
        extendedDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
        extendedDynamicStateFeatures.pNext = NULL;
        
        shaderSubgroupExtendedTypeFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES;
        shaderSubgroupExtendedTypeFeatures.pNext = NULL;
        
        accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelerationStructureFeatures.pNext = NULL;
        
        rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rayTracingPipelineFeatures.pNext = NULL;

        rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        rayQueryFeatures.pNext = NULL;

        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.pNext = NULL;
        
        inlineUniformBlockFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT;
        inlineUniformBlockFeatures.pNext = NULL;
        
        robustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
        robustness2Features.pNext = NULL;
    
        fragmentShaderBarycentricFeaturesNV.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV;
        fragmentShaderBarycentricFeaturesNV.pNext = NULL;
    
        fragmentShaderBarycentricFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR;
        fragmentShaderBarycentricFeaturesKHR.pNext = NULL;

        fragmentShaderInterlockFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
        fragmentShaderInterlockFeatures.pNext = NULL;

        //rayTracingInvocationReorderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV;
        //rayTracingInvocationReorderFeatures.pNext = NULL;

        rayTracingMotionBlurFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV;
        rayTracingMotionBlurFeatures.pNext = NULL;

        variablePointersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES_KHR;
        variablePointersFeatures.pNext = NULL;

        computeShaderDerivativeFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV;
        computeShaderDerivativeFeatures.pNext = NULL;

        clockFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
        clockFeatures.pNext = NULL;

#if VK_MESH_SHADERS_ENABLED
        meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        meshShaderFeatures.pNext = NULL;
#endif

        multiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR;
        multiviewFeatures.pNext = NULL;
        
        fragmentShadingRateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
        fragmentShadingRateFeatures.pNext = NULL;
        
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.pNext = NULL;

    }
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

    VmaAllocator& vmaAllocator() { return mVmaAllocator; }

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