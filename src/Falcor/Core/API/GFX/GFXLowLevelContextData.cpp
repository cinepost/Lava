/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
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
#include "stdafx.h"

#include "GFXDeviceApiData.h"
#include "GFXLowLevelContextApiData.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/LowLevelContextData.h"

namespace Falcor {

LowLevelContextData::SharedPtr LowLevelContextData::create(Device::SharedPtr pDevice, CommandQueueType type, CommandQueueHandle queue) {
	return SharedPtr(new LowLevelContextData(pDevice, type, queue));
}

LowLevelContextData::LowLevelContextData(std::shared_ptr<Device> pDevice, CommandQueueType type, CommandQueueHandle queue)
	: mpDevice(pDevice)
	, mType(type)
	, mpQueue(queue)
{
	mpFence = GpuFence::create(mpDevice);
	mpApiData = new LowLevelContextApiData;
	FALCOR_ASSERT(mpFence && mpApiData);

	openCommandBuffer();
}

LowLevelContextData::~LowLevelContextData() {
	if (mpApiData->mIsCommandBufferOpen) {
		closeCommandBuffer();
	}
	safe_delete(mpApiData);
}

void LowLevelContextData::closeCommandBuffer() {
	mpApiData->mIsCommandBufferOpen = false;
	mpApiData->closeEncoders();
	mpApiData->pCommandBuffer->close();
}

void LowLevelContextData::openCommandBuffer() {
	mpApiData->mIsCommandBufferOpen = true;

	if(!mpDevice->isHeadless()) {
		auto transientHeap = mpDevice->getApiData()->pTransientResourceHeaps[mpDevice->getCurrentBackBufferIndex()].get();
		mpApiData->pCommandBuffer = transientHeap->createCommandBuffer();
	} else {
		auto transientHeap = mpDevice->getApiData()->pTransientResourceHeaps[0].get();
		mpApiData->pCommandBuffer = transientHeap->createCommandBuffer();
	}

}

void LowLevelContextData::beginDebugEvent(const char* name) {
	float blackColor[3] = { 0.0f, 0.0f, 0.0f };
	mpApiData->getResourceCommandEncoder()->beginDebugEvent(name, blackColor);
}

void LowLevelContextData::endDebugEvent() {
	mpApiData->getResourceCommandEncoder()->endDebugEvent();
}

void LowLevelContextData::flush() {
	closeCommandBuffer();
	mpQueue->executeCommandBuffers(1, mpApiData->pCommandBuffer.readRef(), mpFence->getApiHandle(), mpFence->externalSignal());
	openCommandBuffer();
}

void LowLevelContextApiData::closeEncoders() {
	if (mpResourceCommandEncoder) {
		mpResourceCommandEncoder->endEncoding();
		mpResourceCommandEncoder = nullptr;
	}
	if (mpRenderCommandEncoder) {
		mpRenderCommandEncoder->endEncoding();
		mpRenderCommandEncoder = nullptr;
	}
	if (mpComputeCommandEncoder) {
		mpComputeCommandEncoder->endEncoding();
		mpComputeCommandEncoder = nullptr;
	}
	if (mpRayTracingCommandEncoder) {
		mpRayTracingCommandEncoder->endEncoding();
		mpRayTracingCommandEncoder = nullptr;
	}
}

gfx::IResourceCommandEncoder* LowLevelContextApiData::getResourceCommandEncoder() {
	if (mpResourceCommandEncoder) {
		return mpResourceCommandEncoder;
	}
	if (mpComputeCommandEncoder) {
		return mpComputeCommandEncoder;
	}
	if (mpRayTracingCommandEncoder) {
		return mpRayTracingCommandEncoder;
	}
	closeEncoders();
	mpResourceCommandEncoder = pCommandBuffer->encodeResourceCommands();
	return mpResourceCommandEncoder;
}

gfx::IComputeCommandEncoder* LowLevelContextApiData::getComputeCommandEncoder() {
	if (mpComputeCommandEncoder) {
		return mpComputeCommandEncoder;
	}
	closeEncoders();
	mpComputeCommandEncoder = pCommandBuffer->encodeComputeCommands();
	return mpComputeCommandEncoder;
}

gfx::IRenderCommandEncoder* LowLevelContextApiData::getRenderCommandEncoder(gfx::IRenderPassLayout* renderPassLayout, gfx::IFramebuffer* framebuffer, bool& newEncoder) {
	if (mpRenderCommandEncoder && mpRenderPassLayout == renderPassLayout && mpFramebuffer == framebuffer) {
		newEncoder = false;
		return mpRenderCommandEncoder;
	}
	closeEncoders();
	mpRenderCommandEncoder = pCommandBuffer->encodeRenderCommands(renderPassLayout, framebuffer);
	mpRenderPassLayout = renderPassLayout;
	mpFramebuffer = framebuffer;
	newEncoder = true;
	return mpRenderCommandEncoder;
}

gfx::IRayTracingCommandEncoder* LowLevelContextApiData::getRayTracingCommandEncoder() {
	if (mpRayTracingCommandEncoder) {
		return mpRayTracingCommandEncoder;
	}
	closeEncoders();
	mpRayTracingCommandEncoder = pCommandBuffer->encodeRayTracingCommands();
	return mpRayTracingCommandEncoder;
}

}  // namespace Falcor
