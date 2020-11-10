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
#include <cstring>

#include "stdafx.h"
#include "Falcor/Core/API/ComputeContext.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DescriptorSet.h"
#include "Falcor/Utils/Debug/debug.h"

#define VULKAN_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION 65536 //1024 
//VkPhysicalDeviceLimits::maxComputeWorkGroupSize[3]

namespace Falcor {

    ComputeContext::ComputeContext(std::shared_ptr<Device> device, LowLevelContextData::CommandQueueType type, CommandQueueHandle queue) : CopyContext(device, type, queue) {
        assert(queue);
    }

    ComputeContext::~ComputeContext() = default;

    /*
    bool ComputeContext::prepareForDispatch(ComputeState* pState, ComputeVars* pVars)
    {
        assert(pState);

        auto pCSO = pState->getCSO(pVars);

        // Apply the vars. Must be first because applyComputeVars() might cause a flush
        if (pVars)
        {
            if (applyComputeVars(pVars, pCSO->getDesc().getProgramKernels()->getRootSignature().get()) == false) return false;
        }
        else mpLowLevelData->getCommandList()->SetComputeRootSignature(RootSignature::getEmpty()->getApiHandle());

        mpLastBoundComputeVars = pVars;
        mpLowLevelData->getCommandList()->SetPipelineState(pCSO->getApiHandle());
        mCommandsPending = true;
        return true;
    }
    */

    bool ComputeContext::prepareForDispatch(ComputeState* pState, ComputeVars* pVars) {
        assert(pState);
        assert(pVars);

        ComputeStateObject::SharedPtr pCSO = pState->getCSO(pVars);

        // Apply the vars. Must be first because applyComputeVars() might cause a flush
        if(pVars) {
            if (applyComputeVars(pVars, pCSO->getDesc().getProgramKernels()->getRootSignature().get()) == false) {
                return false;
            }
        } 

        LOG_DBG("get command list");
        auto cmd_list = mpLowLevelData->getCommandList();

        LOG_DBG("get pCSO api handle");
        auto cso_api_handle = pCSO->getApiHandle();
        
        LOG_DBG("vkCmdBindPipeline");
        //vkCmdBindPipeline(mpLowLevelData->getCommandList(), VK_PIPELINE_BIND_POINT_COMPUTE, pCSO->getApiHandle());
        vkCmdBindPipeline(cmd_list, VK_PIPELINE_BIND_POINT_COMPUTE, cso_api_handle);

        mpLastBoundComputeVars = pVars;
        mCommandsPending = true;
        return true;
    }

    template<typename ViewType, typename ClearType>
    void clearColorImageCommon(CopyContext* pCtx, const ViewType* pView, const ClearType& clearVal) {
        if(pView->getApiHandle().getType() != VkResourceType::Image) {
            logWarning("Looks like you are trying to clear a buffer. Vulkan only supports clearing Buffers with a single uint value. Please use the uint version of clearUav(). Call is ignored");
            should_not_get_here();
            return;
        }
        pCtx->resourceBarrier(pView->getResource(), Resource::State::CopyDest);
        VkClearColorValue colVal;
        assert(sizeof(ClearType) <= sizeof(colVal.float32));
        std::memcpy(colVal.float32, &clearVal, sizeof(clearVal)); // VkClearColorValue is a union, so should work regardless of the ClearType
        VkImageSubresourceRange range;
        const auto& viewInfo = pView->getViewInfo();
        range.baseArrayLayer = viewInfo.firstArraySlice;
        range.baseMipLevel = viewInfo.mostDetailedMip;
        range.layerCount = viewInfo.arraySize;
        range.levelCount = viewInfo.mipCount;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        vkCmdClearColorImage(pCtx->getLowLevelData()->getCommandList(), pView->getResource()->getApiHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colVal, 1, &range);
    }

    template void clearColorImageCommon(CopyContext* pCtx, const RenderTargetView* pView, const float4& clearVal);

    void ComputeContext::clearUAV(const UnorderedAccessView* pUav, const float4& value) {
        clearColorImageCommon(this, pUav, value);
        mCommandsPending = true;
    }

    void ComputeContext::clearUAV(const UnorderedAccessView* pUav, const uint4& value) {
        if(pUav->getApiHandle().getType() == VkResourceType::Buffer) {
            if ((value.x != value.y) || ((value.x != value.z) && (value.x != value.w))) {
                logWarning("Vulkan buffer clears only support a single element. A vector was supplied which has different elements per channel. only `x` will be used'");
            }
            const Buffer* pBuffer = dynamic_cast<const Buffer*>(pUav->getResource());
            vkCmdFillBuffer(getLowLevelData()->getCommandList(), pBuffer->getApiHandle(), pBuffer->getGpuAddressOffset(), pBuffer->getSize(), value.x);
        } else {
            clearColorImageCommon(this, pUav, value);
        }
        mCommandsPending = true;
    }

    void ComputeContext::clearUAVCounter(Buffer::ConstSharedPtrRef pBuffer, uint32_t value) {
        if (pBuffer->getUAVCounter()) {
            clearUAV(pBuffer->getUAVCounter()->getUAV().get(), uint4(value));
        }
    }

    /*
    void ComputeContext::initDispatchCommandSignature()
    {

    }
    */

    void ComputeContext::dispatch(ComputeState* pState, ComputeVars* pVars, const uint3& dispatchSize) {
        assert(pState);
        assert(pVars);
        // Check dispatch dimensions. TODO: Should be moved into Falcor.
        if (dispatchSize.x > VULKAN_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
            dispatchSize.y > VULKAN_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
            dispatchSize.z > VULKAN_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION) {
            logError("ComputeContext::dispatch(...) - Dispatch dimension exceeds maximum. Skipping.");
            return;
        }

        if (prepareForDispatch(pState, pVars) == false) {
            logError("ComputeContext::dispatch(...) - prepareForDispatch(...) call failed !!! Skipping.");
            return;
        }
        vkCmdDispatch(mpLowLevelData->getCommandList(), dispatchSize.x, dispatchSize.y, dispatchSize.z);
    }

    void ComputeContext::dispatchIndirect(ComputeState* pState, ComputeVars* pVars, const Buffer* pArgBuffer, uint64_t argBufferOffset) {
        assert(pState);
        assert(pVars);
        assert(pArgBuffer);

        if (prepareForDispatch(pState, pVars) == false) {
            logError("ComputeContext::dispatch(...) - prepareForDispatch(...) call failed !!! Skipping.");
            return;
        }
        resourceBarrier(pArgBuffer, Resource::State::IndirectArg);
        vkCmdDispatchIndirect(mpLowLevelData->getCommandList(), pArgBuffer->getApiHandle(), pArgBuffer->getGpuAddressOffset() + argBufferOffset);
    }
}
