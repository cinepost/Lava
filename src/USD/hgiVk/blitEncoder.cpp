#include "USD/hgiVk/blitEncoder.h"
#include "USD/hgiVk/commandBuffer.h"
#include "USD/hgiVk/conversions.h"
#include "USD/hgiVk/device.h"
#include "USD/hgiVk/diagnostic.h"
#include "USD/hgiVk/renderPass.h"
#include "USD/hgiVk/texture.h"
#include "USD/hgiVk/vulkan.h"

#include "pxr/imaging/hgi/blitEncoderOps.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiVkBlitEncoder::HgiVkBlitEncoder(
    HgiVkDevice* device,
    HgiVkCommandBuffer* cmdBuf)
    : HgiBlitEncoder()
    , _device(device)
    , _commandBuffer(cmdBuf)
    , _isRecording(true)
{

}

HgiVkBlitEncoder::~HgiVkBlitEncoder()
{
    if (_isRecording) {
        EndEncoding();
    }
}

void
HgiVkBlitEncoder::EndEncoding()
{
    _commandBuffer = nullptr;
    _isRecording = false;
}

void
HgiVkBlitEncoder::CopyTextureGpuToCpu(
    HgiTextureGpuToCpuOp const& copyOp)
{
    HgiVkTexture* srcTexture =
        static_cast<HgiVkTexture*>(copyOp.gpuSourceTexture);

    if (!TF_VERIFY(srcTexture && srcTexture->GetImage(),
        "Invalid texture handle")) {
        return;
    }

    if (copyOp.destinationBufferByteSize == 0) {
        TF_WARN("The size of the data to copy was zero (aborted)");
        return;
    }

    HgiTextureDesc const& desc = srcTexture->GetDescriptor();

    uint32_t layerCnt = copyOp.startLayer + copyOp.numLayers;
    if (!TF_VERIFY(desc.layerCount >= layerCnt,
        "Texture has less layers than attempted to be copied")) {
        return;
    }

    // Create a new command pool and command buffer for this command since we
    // need to submit it immediately and wait for it to complete so that the
    // CPU can read the pixels data.
    HgiVkCommandPool cp(_device);
    HgiVkCommandBuffer cb(_device, &cp, HgiVkCommandBufferUsagePrimary);
    VkCommandBuffer vkCmdBuf = cb.GetCommandBufferForRecoding();

    // Create the GPU buffer that will receive a copy of the GPU texels that
    // we can then memcpy to CPU buffer.
    HgiBufferDesc dstDesc;
    dstDesc.usage = HgiBufferUsageTransferDst | HgiBufferUsageGpuToCpu;
    dstDesc.byteSize = copyOp.destinationBufferByteSize;
    dstDesc.data = nullptr;

    HgiVkBuffer dstBuffer(_device, dstDesc);

    // Setup info to copy data form gpu texture to gpu buffer
    HgiTextureDesc const& texDesc = srcTexture->GetDescriptor();

    VkOffset3D imageOffset;
    imageOffset.x = copyOp.sourceTexelOffset[0];
    imageOffset.y = copyOp.sourceTexelOffset[1];
    imageOffset.z = copyOp.sourceTexelOffset[2];

    VkExtent3D imageExtent;
    imageExtent.width = texDesc.dimensions[0];
    imageExtent.height = texDesc.dimensions[1];
    imageExtent.depth = texDesc.dimensions[2];

    VkImageSubresourceLayers imageSub;
    imageSub.aspectMask = HgiVkConversions::GetImageAspectFlag(texDesc.usage);
    imageSub.baseArrayLayer = copyOp.startLayer;
    imageSub.layerCount = copyOp.numLayers;
    imageSub.mipLevel = copyOp.mipLevel;

    // See vulkan docs: Copying Data Between Buffers and Images
    VkBufferImageCopy region;
    region.bufferImageHeight = 0; // Buffer is tightly packed, like image
    region.bufferRowLength = 0;   // Buffer is tightly packed, like image
    region.bufferOffset = (VkDeviceSize) copyOp.destinationByteOffset;
    region.imageExtent = imageExtent;
    region.imageOffset = imageOffset;
    region.imageSubresource = imageSub;

    // Transition image to TRANSFER_READ
    VkImageLayout oldLayout = srcTexture->GetImageLayout();
    srcTexture->TransitionImageBarrier(
        &cb,
        srcTexture,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // transition tex to this layout
        VK_ACCESS_TRANSFER_READ_BIT,          // type of access
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    // producer stage
        VK_PIPELINE_STAGE_TRANSFER_BIT);      // consumer stage

    // Copy gpu texture to gpu buffer
    vkCmdCopyImageToBuffer(
        vkCmdBuf,
        srcTexture->GetImage(),
        srcTexture->GetImageLayout(),
        dstBuffer.GetBuffer(),
        1,
        &region);

    // Transition image back to what it was.
    srcTexture->TransitionImageBarrier(
        &cb,
        srcTexture,
        oldLayout, // transition tex to this layout
        HgiVkRenderPass::GetDefaultDstAccessMask(), // type of access
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // producer stage
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);       // consumer stage

    cb.EndRecording();

    // Create a fence we can block the CPU on until copy is completed
    VkFence vkFence;
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

    TF_VERIFY(
        vkCreateFence(
            _device->GetVulkanDevice(),
            &fenceInfo,
            HgiVkAllocator(),
            &vkFence) == VK_SUCCESS
    );

    // Submit the command buffer
    std::vector<VkSubmitInfo> submitInfos;
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCmdBuf;
    submitInfos.emplace_back(std::move(submitInfo));
    _device->SubmitToQueue(submitInfos, vkFence);

    // Wait for the copy from GPU to CPU to complete.
    // XXX Performance warning: This call is going to stall CPU.
    TF_VERIFY(
        vkWaitForFences(
            _device->GetVulkanDevice(),
            1,
            &vkFence,
            VK_TRUE,
            100000000000) == VK_SUCCESS
    );

    vkDestroyFence(_device->GetVulkanDevice(), vkFence, HgiVkAllocator());

    // Copy the data from gpu buffer to cpu destination buffer
    dstBuffer.CopyBufferTo(copyOp.cpuDestinationBuffer);
}

void
HgiVkBlitEncoder::PushDebugGroup(const char* label)
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkBeginDebugMarker(_commandBuffer, label);
}

void
HgiVkBlitEncoder::PopDebugGroup()
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkEndDebugMarker(_commandBuffer);
}

void
HgiVkBlitEncoder::PushTimeQuery(const char* name)
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    _commandBuffer->PushTimeQuery(name);
}

void
HgiVkBlitEncoder::PopTimeQuery()
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    _commandBuffer->PopTimeQuery();
}

PXR_NAMESPACE_CLOSE_SCOPE
