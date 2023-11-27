// vk-command-queue.cpp
#include "vk-command-queue.h"

#include "vk-command-buffer.h"
#include "vk-fence.h"
#include "vk-transient-heap.h"

#include "lava_utils_lib/logging.h"

namespace gfx {

using namespace Slang;

namespace vk {

ICommandQueue* CommandQueueImpl::getInterface(const Guid& guid) {
    if (guid == GfxGUID::IID_ISlangUnknown || guid == GfxGUID::IID_ICommandQueue)
        return static_cast<ICommandQueue*>(this);
    return nullptr;
}

CommandQueueImpl::~CommandQueueImpl() {
    m_renderer->m_api.vkQueueWaitIdle(m_queue);

    m_renderer->m_queueAllocCount--;
    m_renderer->m_api.vkDestroySemaphore(m_renderer->m_api.m_device, m_semaphore, nullptr);
}

void CommandQueueImpl::init(DeviceImpl* renderer, VkQueue queue, uint32_t queueFamilyIndex) {
    m_renderer = renderer;
    m_queue = queue;
    m_queueFamilyIndex = queueFamilyIndex;
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.flags = 0;
    m_renderer->m_api.vkCreateSemaphore(m_renderer->m_api.m_device, &semaphoreCreateInfo, nullptr, &m_semaphore);
}

void CommandQueueImpl::waitOnHost() {
    auto& vkAPI = m_renderer->m_api;
    vkAPI.vkQueueWaitIdle(m_queue);
}

Result CommandQueueImpl::getNativeHandle(InteropHandle* outHandle) {
    outHandle->api = InteropHandleAPI::D3D12;
    outHandle->handleValue = (uint64_t)m_queue;
    return SLANG_OK;
}

const CommandQueueImpl::Desc& CommandQueueImpl::getDesc() { return m_desc; }

Result CommandQueueImpl::waitForFenceValuesOnDevice(
    GfxCount fenceCount, IFence** fences, uint64_t* waitValues)
{
    for (GfxIndex i = 0; i < fenceCount; ++i)
    {
        FenceWaitInfo waitInfo;
        waitInfo.fence = static_cast<FenceImpl*>(fences[i]);
        waitInfo.waitValue = waitValues[i];
        m_pendingWaitFences.add(waitInfo);
    }
    return SLANG_OK;
}

void CommandQueueImpl::queueSubmitImpl(uint32_t count, ICommandBuffer* const* commandBuffers, IFence* fence, uint64_t valueToSignal) {
    auto& vkAPI = m_renderer->m_api;
    m_submitCommandBuffers.clear();

    //printf("queueSubmitImpl()\n");

    for (uint32_t i = 0; i < count; i++) {
        auto cmdBufImpl = static_cast<CommandBufferImpl*>(commandBuffers[i]);
        if (!cmdBufImpl->m_isPreCommandBufferEmpty) {
            m_submitCommandBuffers.add(cmdBufImpl->m_preCommandBuffer);
        }
        auto vkCmdBuf = cmdBufImpl->m_commandBuffer;
        m_submitCommandBuffers.add(vkCmdBuf);
    }
    
    //printf("queueSubmitImpl() 1\n");

    Array<VkSemaphore, 2> signalSemaphores;
    Array<uint64_t, 2> signalValues;
    signalSemaphores.add(m_semaphore);
    signalValues.add(0);

    //printf("queueSubmitImpl() 2\n");

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags stageFlag[] = { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
    submitInfo.pWaitDstStageMask = stageFlag;
    submitInfo.commandBufferCount = (uint32_t)m_submitCommandBuffers.getCount();
    submitInfo.pCommandBuffers = m_submitCommandBuffers.getBuffer();
    Array<VkSemaphore, 3> waitSemaphores;
    Array<uint64_t, 3> waitValues;
    
    //printf("queueSubmitImpl() 3\n");

    for (auto s : m_pendingWaitSemaphores) {
        if (s != VK_NULL_HANDLE) {
            waitSemaphores.add(s);
            waitValues.add(0);
        }
    }

    //printf("queueSubmitImpl() 4\n");
    
    for (auto& fenceWait : m_pendingWaitFences) {
        waitSemaphores.add(fenceWait.fence->m_semaphore);
        waitValues.add(fenceWait.waitValue);
    }

    //printf("queueSubmitImpl() 5\n");

    m_pendingWaitFences.clear();
    VkTimelineSemaphoreSubmitInfo timelineSubmitInfo = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    
    if (fence) {
        //printf("queueSubmitImpl() 5.1\n");
        auto fenceImpl = static_cast<FenceImpl*>(fence);
        signalSemaphores.add(fenceImpl->m_semaphore);
        signalValues.add(valueToSignal);
        submitInfo.pNext = &timelineSubmitInfo;
        timelineSubmitInfo.signalSemaphoreValueCount = (uint32_t)signalValues.getCount();
        timelineSubmitInfo.pSignalSemaphoreValues = signalValues.getBuffer();
        timelineSubmitInfo.waitSemaphoreValueCount = (uint32_t)waitValues.getCount();
        timelineSubmitInfo.pWaitSemaphoreValues = waitValues.getBuffer();
    }

    //printf("queueSubmitImpl() 6\n");
    
    submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.getCount();
    
    if (submitInfo.waitSemaphoreCount) {
        submitInfo.pWaitSemaphores = waitSemaphores.getBuffer();
    }
    
    //printf("queueSubmitImpl() 7\n");

    submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.getCount();
    submitInfo.pSignalSemaphores = signalSemaphores.getBuffer();

    VkFence vkFence = VK_NULL_HANDLE;
    
    //printf("queueSubmitImpl() 8\n");
    //printf("queueSubmitImpl() 8 count %d\n", count);

    if (count) {
        //printf("queueSubmitImpl() 8.1\n");
        auto commandBufferImpl = static_cast<CommandBufferImpl*>(commandBuffers[0]);
        //printf("queueSubmitImpl() 8.2\n");
        vkFence = commandBufferImpl->m_transientHeap->getCurrentFence();
        //printf("queueSubmitImpl() 8.3\n");

        if(vkFence == VK_NULL_HANDLE) {
            LLOG_ERR << "VK_NULL_HANDLE !!!";
        }

        if(vkAPI.vkResetFences(vkAPI.m_device, 1, &vkFence) == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            LLOG_FTL << "vkResetFences() VK_ERROR_OUT_OF_DEVICE_MEMORY !!!";
        }
        //printf("queueSubmitImpl() 8.4\n");
        commandBufferImpl->m_transientHeap->advanceFence();
        //printf("queueSubmitImpl() 8.5\n");
    }

    //printf("queueSubmitImpl() vkAPI.vkQueueSubmit\n");

    vkAPI.vkQueueSubmit(m_queue, 1, &submitInfo, vkFence);

    //printf("queueSubmitImpl() done\n");

    m_pendingWaitSemaphores[0] = m_semaphore;
    m_pendingWaitSemaphores[1] = VK_NULL_HANDLE;
}

void CommandQueueImpl::executeCommandBuffers(GfxCount count, ICommandBuffer* const* commandBuffers, IFence* fence, uint64_t valueToSignal) {
    if (count == 0 && fence == nullptr) {
        return;
    }
    queueSubmitImpl(count, commandBuffers, fence, valueToSignal);
}

} // namespace vk
} // namespace gfx
