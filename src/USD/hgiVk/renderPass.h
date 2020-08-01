#ifndef PXR_IMAGING_HGIVK_RENDERPASS_H
#define PXR_IMAGING_HGIVK_RENDERPASS_H

#include <atomic>
#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;
class HgiVkCommandBuffer;

typedef std::vector<HgiAttachmentDesc const*> HgiAttachmentDescConstPtrVector;

// MoltenVk 1 does not support RenderPass2
// This means we cannot resolve depth attachments via render pass with MoltenVk.
#if defined(__APPLE__)
    #define VK_ATTACHMENT_DESC VkAttachmentDescription
    #define VK_ATTACHMENT_DESC_TYPE 0
    #define VK_ATTACHMENT_REF VkAttachmentReference
    #define VK_ATTACHMENT_REF_TYPE 0
    #define VK_SUBPASS_DESC VkSubpassDescription
    #define VK_SUBPASS_TYPE 0
    #define VK_RENDERPASS_CREATE_INFO VkRenderPassCreateInfo
    #define VK_RENDERPASS_CREATE_TYPE VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO
    #define VK_SUBPASS_DEPENDENCY VkSubpassDependency
    #define VK_SUBPASS_DEPENDENCY_TYPE 0
    #define VK_CREATE_RENDERPASS vkCreateRenderPass
#else
    #define VK_RENDERPASS_2
    #define VK_ATTACHMENT_DESC VkAttachmentDescription2KHR
    #define VK_ATTACHMENT_DESC_TYPE VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR
    #define VK_ATTACHMENT_REF VkAttachmentReference2KHR
    #define VK_ATTACHMENT_REF_TYPE VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR
    #define VK_SUBPASS_DESC VkSubpassDescription2KHR
    #define VK_SUBPASS_TYPE VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR
    #define VK_RENDERPASS_CREATE_INFO VkRenderPassCreateInfo2KHR
    #define VK_RENDERPASS_CREATE_TYPE VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR
    #define VK_SUBPASS_DEPENDENCY VkSubpassDependency2KHR
    #define VK_SUBPASS_DEPENDENCY_TYPE VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR
    #define VK_CREATE_RENDERPASS device->vkCreateRenderPass2KHR
#endif

/// \class HgiVkRenderPass
///
/// Vulkan render pass.
///
class HgiVkRenderPass final {
public:
    HGIVK_API
    HgiVkRenderPass(
        HgiVkDevice* device,
        HgiGraphicsEncoderDesc const& desc);

    HGIVK_API
    virtual ~HgiVkRenderPass();

    /// Returns true if this render pass was not currently in use by any
    /// thread / command buffer. It will atomically be flagged as 'in use' and
    /// will not be able to be used by any other thread until EndFrame.
    /// Returns false if the render pass was already in-use by another
    /// thread / command buffer. Vulkan render passes must begin and end in
    /// on command buffer and thus cannot span across multiple command buffers.
    /// ReleaseRenderPass must be called during EndFrame.
    HGIVK_API
    bool AcquireRenderPass();

    /// Releases the render pass so it can be used by a thread / command buffer.
    /// This should be called during EndFrame and not before, because we must
    /// ensure a render pass is not used across parallel command buffers.
    HGIVK_API
    void ReleaseRenderPass();

    /// Begin vulkan render pass so it is ready for graphics commands.
    /// If `usesSecondaryCommandBuffers` than the primary command buffer can
    /// contain no rendering commands until EndRenderPass is called.
    HGIVK_API
    void BeginRenderPass(
        HgiVkCommandBuffer* cb,
        bool usesSecondaryCommandBuffers);

    /// End vulkan render pass. No further graphics commands can be recorded.
    HGIVK_API
    void EndRenderPass(HgiVkCommandBuffer* cb);

    /// Get the vulkan render pass.
    HGIVK_API
    VkRenderPass GetVulkanRenderPass() const;

    /// Get the vulkan framebuffer.
    HGIVK_API
    VkFramebuffer GetVulkanFramebuffer() const;

    /// Get the graphics encoder descriptor used to make this render pass.
    HGIVK_API
    HgiGraphicsEncoderDesc const& GetDescriptor() const;

    /// Returns the vector of image views used to create render pass
    HGIVK_API
    std::vector<VkImageView> const& GetImageViews() const;

    /// Combines the color and depth attachments in one vector.
    /// If `getResolveAttachments` is true, the resolve attachments are returned.
    /// Otherwise the regular attachments are returned.
    HGIVK_API
    static HgiAttachmentDescConstPtrVector GetCombinedAttachments(
        HgiGraphicsEncoderDesc const& desc);

    /// Helper to transition images back to a known dest access mask.
    HGIVK_API
    static VkAccessFlags GetDefaultDstAccessMask();

    /// Returns the frame the render pass was last used
    HGIVK_API
    uint64_t GetLastUsedFrame() const;

private:
    HgiVkRenderPass() = delete;
    HgiVkRenderPass & operator=(const HgiVkRenderPass&) = delete;
    HgiVkRenderPass(const HgiVkRenderPass&) = delete;

    // Extracts the render pass information for one texture.
    // Returns that usage type of the texture (e.g. color target).
    // If `isResolveAttachment` is true the attachment will be processed as a
    // resolve attachment. Otherwise as a regular attachment.
    HgiTextureUsage _ProcessAttachment(HgiAttachmentDesc const& attachDesc);

private:
    HgiVkDevice* _device;
    HgiGraphicsEncoderDesc _descriptor;

    VkRenderPass _vkRenderPass;
    VkFramebuffer _vkFramebuffer;
    std::vector<VkClearValue> _vkClearValues;
    std::vector<VkImageView> _vkImageViews;
    std::vector<VK_ATTACHMENT_DESC> _vkDescriptions;
    std::vector<VK_ATTACHMENT_REF> _vkReferences;

    std::atomic_flag _acquired = ATOMIC_FLAG_INIT;
    uint64_t _lastUsedFrame;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
