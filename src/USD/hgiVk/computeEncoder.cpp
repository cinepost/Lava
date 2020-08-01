#include "USD/hgiVk/computeEncoder.h"

#include "USD/hgiVk/device.h"
#include "USD/hgiVk/diagnostic.h"
#include "USD/hgiVk/pipeline.h"
#include "USD/hgiVk/renderPass.h"
#include "USD/hgiVk/resourceBindings.h"
#include "USD/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiVkComputeEncoder::HgiVkComputeEncoder(
    HgiVkDevice* device,
    HgiVkCommandBuffer* cb)
    : HgiComputeEncoder()
    , _device(device)
    , _commandBuffer(_commandBuffer)
    , _isRecording(true)
{
}

HgiVkComputeEncoder::~HgiVkComputeEncoder()
{
    if (_isRecording) {
        TF_WARN("ComputeEncoder: [%x] is missing an EndEncodig() call.", this);
        EndEncoding();
    }
}

void
HgiVkComputeEncoder::EndEncoding()
{
    _commandBuffer = nullptr;
    _isRecording = false;
}

void
HgiVkComputeEncoder::BindPipeline(HgiPipelineHandle pipeline)
{
    if (HgiVkPipeline* p = static_cast<HgiVkPipeline*>(pipeline)) {
        p->BindPipeline(_commandBuffer, /*renderpass*/ nullptr);
    }
}

void
HgiVkComputeEncoder::BindResources(HgiResourceBindingsHandle res)
{
    if (HgiVkResourceBindings* r = static_cast<HgiVkResourceBindings*>(res)) {
        r->BindResources(_commandBuffer);
    }
}

void
HgiVkComputeEncoder::Dispatch(
    uint32_t threadGrpCntX,
    uint32_t threadGrpCntY,
    uint32_t threadGrpCntZ)
{
    vkCmdDispatch(
        _commandBuffer->GetCommandBufferForRecoding(),
        threadGrpCntX,
        threadGrpCntY,
        threadGrpCntZ);
}

void
HgiVkComputeEncoder::PushDebugGroup(const char* label)
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkBeginDebugMarker(_commandBuffer, label);
}

void
HgiVkComputeEncoder::PopDebugGroup()
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkEndDebugMarker(_commandBuffer);
}

PXR_NAMESPACE_CLOSE_SCOPE
