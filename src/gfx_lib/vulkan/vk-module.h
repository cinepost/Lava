// vk-module.h
#ifndef SRC_GFX_VULKAN_VK_MODULE_H_
#define SRC_GFX_VULKAN_VK_MODULE_H_

#include <assert.h>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
#include <slang/slang.h>
#include <slang/slang-com-helper.h>
#pragma GCC diagnostic pop

#if SLANG_WINDOWS_FAMILY
#   define VK_USE_PLATFORM_WIN32_KHR 1
#else
#   define VK_USE_PLATFORM_XLIB_KHR 1
#endif

#define VK_NO_PROTOTYPES

#include <vulkan/vulkan.h>

#define VK_MESH_SHADERS_ENABLED 0

#if VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

// Undef xlib macros (included by vulkan.h) that cause conflicts
#ifndef _WIN32
#ifdef Bool
#   undef Bool
#endif
#ifdef Always
#    undef Always
#endif
#ifdef None
#    undef None
#endif
#ifdef Status
#   undef Status
#endif
#endif // _WIN32

namespace gfx {

namespace vk {

#if !VK_USE_PLATFORM_XLIB_KHR
typedef void Display;
typedef int Window;
#endif

// string conversions

inline std::string to_string(VkDescriptorType dt) {
#define dt2s(t_) case t_: return #t_;
    switch (dt) {
        dt2s(VK_DESCRIPTOR_TYPE_SAMPLER);
        dt2s(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        dt2s(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        dt2s(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        dt2s(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
        dt2s(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
        dt2s(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        dt2s(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        dt2s(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        dt2s(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
        dt2s(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);


        dt2s(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK);
        dt2s(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
        dt2s(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV);
        dt2s(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE);
        dt2s(VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM);
        dt2s(VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM);
        dt2s(VK_DESCRIPTOR_TYPE_MAX_ENUM);
        default:
            assert(false);
            return "";
        //    should_not_get_here();
        //    return "";
    }
#undef dt2s
}

inline std::string to_string(VkResult result) {
#define vkresult_2_string(a) case a: return #a;
    switch (result) {
        vkresult_2_string(VK_SUCCESS);
        vkresult_2_string(VK_NOT_READY);
        vkresult_2_string(VK_TIMEOUT);
        vkresult_2_string(VK_EVENT_SET);
        vkresult_2_string(VK_EVENT_RESET);
        vkresult_2_string(VK_INCOMPLETE);
        vkresult_2_string(VK_ERROR_OUT_OF_HOST_MEMORY);
        vkresult_2_string(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        vkresult_2_string(VK_ERROR_INITIALIZATION_FAILED);
        vkresult_2_string(VK_ERROR_DEVICE_LOST);
        vkresult_2_string(VK_ERROR_MEMORY_MAP_FAILED);
        vkresult_2_string(VK_ERROR_LAYER_NOT_PRESENT);
        vkresult_2_string(VK_ERROR_EXTENSION_NOT_PRESENT);
        vkresult_2_string(VK_ERROR_FEATURE_NOT_PRESENT);
        vkresult_2_string(VK_ERROR_INCOMPATIBLE_DRIVER);
        vkresult_2_string(VK_ERROR_TOO_MANY_OBJECTS);
        vkresult_2_string(VK_ERROR_FORMAT_NOT_SUPPORTED);
        vkresult_2_string(VK_ERROR_FRAGMENTED_POOL);
        vkresult_2_string(VK_ERROR_UNKNOWN);
        vkresult_2_string(VK_ERROR_OUT_OF_POOL_MEMORY);
        vkresult_2_string(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        vkresult_2_string(VK_ERROR_FRAGMENTATION);
        vkresult_2_string(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    #ifdef VK_PIPELINE_COMPILE_REQUIRED
        vkresult_2_string(VK_PIPELINE_COMPILE_REQUIRED);
    #endif 
        vkresult_2_string(VK_ERROR_SURFACE_LOST_KHR);
        vkresult_2_string(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        vkresult_2_string(VK_SUBOPTIMAL_KHR);
        vkresult_2_string(VK_ERROR_OUT_OF_DATE_KHR);
        vkresult_2_string(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        vkresult_2_string(VK_ERROR_VALIDATION_FAILED_EXT);
        vkresult_2_string(VK_ERROR_INVALID_SHADER_NV);
    #ifdef VK_ENABLE_BETA_EXTENSIONS
        vkresult_2_string(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
    #endif
    #ifdef VK_ENABLE_BETA_EXTENSIONS
        vkresult_2_string(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
    #endif
    #ifdef VK_ENABLE_BETA_EXTENSIONS
        vkresult_2_string(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
    #endif
    #ifdef VK_ENABLE_BETA_EXTENSIONS
        vkresult_2_string(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
    #endif
    #ifdef VK_ENABLE_BETA_EXTENSIONS
        vkresult_2_string(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
    #endif
    #ifdef VK_ENABLE_BETA_EXTENSIONS
        vkresult_2_string(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
    #endif
        vkresult_2_string(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    #ifdef VK_ERROR_NOT_PERMITTED_KHR    
        vkresult_2_string(VK_ERROR_NOT_PERMITTED_KHR);
    #endif    
        vkresult_2_string(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
        vkresult_2_string(VK_THREAD_IDLE_KHR);
        vkresult_2_string(VK_THREAD_DONE_KHR);
        vkresult_2_string(VK_OPERATION_DEFERRED_KHR);
        vkresult_2_string(VK_OPERATION_NOT_DEFERRED_KHR);
    #ifdef VK_ERROR_COMPRESSION_EXHAUSTED_EXT
        vkresult_2_string(VK_ERROR_COMPRESSION_EXHAUSTED_EXT);
    #endif

        default:
            assert(false);
            return "Unknown VkResult";
    }
#undef vkresult_2_string
}

} // namespace vk

struct VulkanModule {
        /// true if has been initialized
    SLANG_FORCE_INLINE bool isInitialized() const { return m_module != nullptr; }

        /// Get a function by name
    PFN_vkVoidFunction getFunction(const char* name) const;

        /// true if using a software Vulkan implementation.
    bool isSoftware() const { return m_isSoftware; }

        /// Initialize
    Slang::Result init(bool useSoftwareImpl);
        /// Destroy
    void destroy();

        /// Dtor
    ~VulkanModule() { destroy(); }

 protected:
     void* m_module = nullptr;
     bool m_isSoftware = false;
};

} // renderer_test

#endif // SRC_GFX_VULKAN_VK_MODULE_H_