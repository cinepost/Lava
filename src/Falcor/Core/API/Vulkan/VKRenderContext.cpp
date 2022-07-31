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
#include "stdafx.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/DescriptorPool.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Debug/debug.h"
#include "glm/gtc/type_ptr.hpp"
#include "VKState.h"

#include "Falcor/Core/Program/RtProgram.h"
#include "Falcor/Core/Program/ProgramVars.h"

#include "VKRtAccelerationStructure.h"

namespace Falcor {
	
VkImageAspectFlags getAspectFlagsFromFormat(ResourceFormat format);
VkImageLayout getImageLayout(Resource::State state);

RenderContext::RenderContext(std::shared_ptr<Device> pDevice, CommandQueueHandle queue) : ComputeContext(pDevice, LowLevelContextData::CommandQueueType::Direct, queue) {}

RenderContext::~RenderContext() = default;

template<typename ViewType, typename ClearType>
void clearColorImageCommon(CopyContext* pCtx, const ViewType* pView, const ClearType& clearVal);

void RenderContext::clearRtv(const RenderTargetView* pRtv, const float4& color) {
	// LOG_DBG("clear rtv");
	clearColorImageCommon(this, pRtv, color);
	mCommandsPending = true;
}

void RenderContext::clearDsv(const DepthStencilView* pDsv, float depth, uint8_t stencil, bool clearDepth, bool clearStencil) {
	resourceBarrier(pDsv->getResource().get(), Resource::State::CopyDest);

	VkClearDepthStencilValue val;
	val.depth = depth;
	val.stencil = stencil;

	VkImageSubresourceRange range;
	const auto& viewInfo = pDsv->getViewInfo();
	range.baseArrayLayer = viewInfo.firstArraySlice;
	range.baseMipLevel = viewInfo.mostDetailedMip;
	range.layerCount = viewInfo.arraySize;
	range.levelCount = viewInfo.mipCount;
	range.aspectMask = clearDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	range.aspectMask |= clearStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

	vkCmdClearDepthStencilImage(mpLowLevelData->getCommandList(), pDsv->getResource()->getApiHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &val, 1, &range);
	mCommandsPending = true;
}

void setViewports(CommandListHandle cmdList, const std::vector<GraphicsState::Viewport>& viewports) {
	static_assert(offsetof(GraphicsState::Viewport, originX) == offsetof(VkViewport, x), "VP originX offset");
	static_assert(offsetof(GraphicsState::Viewport, originY) == offsetof(VkViewport, y), "VP originY offset");
	static_assert(offsetof(GraphicsState::Viewport, width) == offsetof(VkViewport, width), "VP width offset");
	static_assert(offsetof(GraphicsState::Viewport, height) == offsetof(VkViewport, height), "VP height offset");
	static_assert(offsetof(GraphicsState::Viewport, minDepth) == offsetof(VkViewport, minDepth), "VP minDepth offset");
	static_assert(offsetof(GraphicsState::Viewport, maxDepth) == offsetof(VkViewport, maxDepth), "VP maxDepth offset");

	vkCmdSetViewport(cmdList, 0, (uint32_t)viewports.size(), (VkViewport*)viewports.data());
}

void setScissors(CommandListHandle cmdList, const std::vector<GraphicsState::Scissor>& scissors) {
	std::vector<VkRect2D> vkScissors(scissors.size());
	for (size_t i = 0; i < scissors.size(); i++) {
		vkScissors[i].offset.x = scissors[i].left;
		vkScissors[i].offset.y = scissors[i].top;
		vkScissors[i].extent.width = scissors[i].right - scissors[i].left;
		vkScissors[i].extent.height = scissors[i].bottom - scissors[i].top;
	}
	vkCmdSetScissor(cmdList, 0, (uint32_t)scissors.size(), vkScissors.data());
}

void setVao(CopyContext* pCtx, const Vao* pVao) {
	CommandListHandle cmdList = pCtx->getLowLevelData()->getCommandList();
	for (uint32_t i = 0; i < pVao->getVertexBuffersCount(); i++) {
		const Buffer* pVB = pVao->getVertexBuffer(i).get();
		VkDeviceSize offset = pVB->getGpuAddressOffset();
		VkBuffer handle = pVB->getApiHandle();
		vkCmdBindVertexBuffers(cmdList, i, 1, &handle, &offset);
		pCtx->resourceBarrier(pVB, Resource::State::VertexBuffer);
	}

	const Buffer* pIB = pVao->getIndexBuffer().get();
	if (pIB) {
		VkDeviceSize offset = pIB->getGpuAddressOffset();
		VkBuffer handle = pIB->getApiHandle();
		vkCmdBindIndexBuffer(cmdList, handle, offset, getVkIndexType(pVao->getIndexBufferFormat()));
		pCtx->resourceBarrier(pIB, Resource::State::IndexBuffer);
	}
}

void beginRenderPass(CommandListHandle cmdList, const Fbo* pFbo) {
	const auto& fboHandle = pFbo->getApiHandle();
	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = *fboHandle;
	beginInfo.framebuffer = *fboHandle;
	beginInfo.renderArea.offset = { 0, 0 };
	beginInfo.renderArea.extent = { pFbo->getWidth(), pFbo->getHeight() };

	// Only needed if attachments use VK_ATTACHMENT_LOAD_OP_CLEAR
	beginInfo.clearValueCount = 0;
	beginInfo.pClearValues = nullptr;

	vkCmdBeginRenderPass(cmdList, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void transitionFboResources(RenderContext* pCtx, const Fbo* pFbo) {
	// We are setting the entire RTV array to make sure everything that was previously bound is detached
	uint32_t colorTargets = Fbo::getMaxColorTargetCount(pCtx->device());

	if (pFbo) {
		for (uint32_t i = 0; i < colorTargets; i++) {
			auto pTexture = pFbo->getColorTexture(i);
			if (pTexture) pCtx->resourceBarrier(pTexture.get(), Resource::State::RenderTarget);
		}

		auto pTexture = pFbo->getDepthStencilTexture();
		if (pTexture) pCtx->resourceBarrier(pTexture.get(), Resource::State::DepthStencil);
	}
}

static void endVkDraw(CommandListHandle cmdList) {
	vkCmdEndRenderPass(cmdList);
}

bool RenderContext::prepareForDraw(GraphicsState* pState, GraphicsVars* pVars) {
	assert(pState);
	// Vao must be valid so at least primitive topology is known
	assert(pState->getVao().get());

	auto pGSO = pState->getGSO(pVars);
	// Apply the vars. Must be first because applyGraphicsVars() might cause a flush
	if (is_set(RenderContext::StateBindFlags::Vars, mBindFlags)) {
		if (pVars) {
			if (applyGraphicsVars(pVars, pGSO->getDesc().getRootSignature().get()) == false) {
				return false; // Skip the call
			}
		}
	}
	if (is_set(RenderContext::StateBindFlags::PipelineState, mBindFlags)) {
		vkCmdBindPipeline(mpLowLevelData->getCommandList(), VK_PIPELINE_BIND_POINT_GRAPHICS, pGSO->getApiHandle());
	}
	if (is_set(RenderContext::StateBindFlags::Fbo, mBindFlags)) {
		transitionFboResources(this, pState->getFbo().get());
	}
	if (is_set(StateBindFlags::SamplePositions, mBindFlags)) {
		if (pState->getFbo() && pState->getFbo()->getSamplePositions().size()) {
			LLOG_WRN << "The Vulkan backend doesn't support programmable sample positions";
		}
	}
	if (is_set(RenderContext::StateBindFlags::Viewports, mBindFlags)) {
		setViewports(mpLowLevelData->getCommandList(), pState->getViewports());
	}
	if (is_set(RenderContext::StateBindFlags::Scissors, mBindFlags)) {
		setScissors(mpLowLevelData->getCommandList(), pState->getScissors());
	}
	if (is_set(RenderContext::StateBindFlags::Vao, mBindFlags)) {
		setVao(this, pState->getVao().get());
	}
	beginRenderPass(mpLowLevelData->getCommandList(), pState->getFbo().get());
	return true;
}

void RenderContext::drawInstanced(GraphicsState* pState, GraphicsVars* pVars, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
	if (vertexCount == 0) return;  // early termination

	if (prepareForDraw(pState, pVars) == false) return;
	vkCmdDraw(mpLowLevelData->getCommandList(), vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
	endVkDraw(mpLowLevelData->getCommandList());
}

void RenderContext::draw(GraphicsState* pState, GraphicsVars* pVars, uint32_t vertexCount, uint32_t startVertexLocation) {
	drawInstanced(pState,pVars, vertexCount, 1, startVertexLocation, 0);
}

void RenderContext::drawIndexedInstanced(GraphicsState* pState, GraphicsVars* pVars, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
	if (prepareForDraw(pState, pVars) == false) return;
	
	vkCmdDrawIndexed(mpLowLevelData->getCommandList(), indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
	endVkDraw(mpLowLevelData->getCommandList());
}

void RenderContext::drawIndexed(GraphicsState* pState, GraphicsVars* pVars, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation){
	drawIndexedInstanced(pState, pVars, indexCount, 1, startIndexLocation, baseVertexLocation, 0);
}

void RenderContext::drawIndirect(GraphicsState* pState, GraphicsVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset) {
	resourceBarrier(pArgBuffer, Resource::State::IndirectArg);
	if (prepareForDraw(pState, pVars) == false) return;
	vkCmdDrawIndirect(mpLowLevelData->getCommandList(), pArgBuffer->getApiHandle(), argBufferOffset + pArgBuffer->getGpuAddressOffset(), 1, 0);
	endVkDraw(mpLowLevelData->getCommandList());
}

void RenderContext::drawIndexedIndirect(GraphicsState* pState, GraphicsVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset) {
	resourceBarrier(pArgBuffer, Resource::State::IndirectArg);
	if (prepareForDraw(pState, pVars) == false) return;
	vkCmdDrawIndexedIndirect(mpLowLevelData->getCommandList(), pArgBuffer->getApiHandle(), argBufferOffset + pArgBuffer->getGpuAddressOffset(), maxCommandCount, sizeof(VkDrawIndexedIndirectCommand));
	endVkDraw(mpLowLevelData->getCommandList());
}

template<uint32_t offsetCount, typename ViewType>
void initBlitData(const ViewType* pView, const uint4& rect, VkImageSubresourceLayers& layer, VkOffset3D offset[offsetCount]) {
	const Texture* pTex = dynamic_cast<const Texture*>(pView->getResource().get());

	layer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Can't blit depth texture
	const auto& viewInfo = pView->getViewInfo();
	layer.baseArrayLayer = viewInfo.firstArraySlice;
	layer.layerCount = viewInfo.arraySize;
	layer.mipLevel = viewInfo.mostDetailedMip;
	assert(pTex->getDepth(viewInfo.mostDetailedMip) == 1);

	offset[0].x =  (rect.x == -1) ? 0 : rect.x;
	offset[0].y = (rect.y == -1) ? 0 : rect.y;
	offset[0].z = 0;

	if(offsetCount > 1) {
		offset[1].x = (rect.z == -1) ? pTex->getWidth(viewInfo.mostDetailedMip) : rect.z;
		offset[1].y = (rect.w == -1) ? pTex->getHeight(viewInfo.mostDetailedMip) : rect.w;
		offset[1].z = 1;
	}
}
void RenderContext::blit(const ShaderResourceView::SharedPtr& pSrc, const RenderTargetView::SharedPtr& pDst, uint4 srcRect, uint4 dstRect, Sampler::Filter filter) {
	const Texture* pTexture = dynamic_cast<const Texture*>(pSrc->getResource().get());
	resourceBarrier(pSrc->getResource().get(), Resource::State::CopySource, &pSrc->getViewInfo());
	resourceBarrier(pDst->getResource().get(), Resource::State::CopyDest, &pDst->getViewInfo());

	if (pTexture && pTexture->getSampleCount() > 1) {
		// Resolve
		VkImageResolve resolve;
		initBlitData<1>(pSrc.get(), srcRect, resolve.srcSubresource, &resolve.srcOffset);
		initBlitData<1>(pDst.get(), dstRect, resolve.dstSubresource, &resolve.dstOffset);
		const auto& viewInfo = pSrc->getViewInfo();
		resolve.extent.width = pTexture->getWidth(viewInfo.mostDetailedMip);
		resolve.extent.height = pTexture->getHeight(viewInfo.mostDetailedMip);
		resolve.extent.depth = 1;

		vkCmdResolveImage(mpLowLevelData->getCommandList(), pSrc->getResource()->getApiHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDst->getResource()->getApiHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolve);
	} else {
		VkImageBlit blt;
		initBlitData<2>(pSrc.get(), srcRect, blt.srcSubresource, blt.srcOffsets);
		initBlitData<2>(pDst.get(), dstRect, blt.dstSubresource, blt.dstOffsets);

		// Vulkan spec requires VK_FILTER_NEAREST if blit source is a depth and/or stencil format
		VkFilter vkFilter = isDepthStencilFormat(pTexture->getFormat()) ? VK_FILTER_NEAREST : getVkFilter(filter);
		vkCmdBlitImage(
			mpLowLevelData->getCommandList(), 
			pSrc->getResource()->getApiHandle(), 
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
			pDst->getResource()->getApiHandle(), 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			1, 
			&blt, 
			vkFilter
		);
	}
	mCommandsPending = true;
}

void RenderContext::blit(const ShaderResourceView::SharedPtr& pSrc, const RenderTargetView::SharedPtr& pDst, uint4 srcRect, uint4 dstRect, Sampler::Filter filter, const Sampler::ReductionMode componentsReduction[4], const float4 componentsTransform[4])
{ 
	// TODO: Implement complex blit here...
}

void RenderContext::resolveResource(const Texture::SharedPtr& pSrc, const Texture::SharedPtr& pDst) {
	// Just blit. It will work
	blit(pSrc->getSRV(), pDst->getRTV());
}

void RenderContext::resolveSubresource(const Texture::SharedPtr& pSrc, uint32_t srcSubresource, const Texture::SharedPtr& pDst, uint32_t dstSubresource) {
	uint32_t srcArray = pSrc->getSubresourceArraySlice(srcSubresource);
	uint32_t srcMip = pSrc->getSubresourceMipLevel(srcSubresource);
	const auto& pSrcSrv = pSrc->getSRV(srcMip, 1, srcArray, 1);

	uint32_t dstArray = pDst->getSubresourceArraySlice(dstSubresource);
	uint32_t dstMip = pDst->getSubresourceMipLevel(dstSubresource);
	const auto& pDstRtv = pDst->getRTV(dstMip, dstArray, 1);

	blit(pSrcSrv, pDstRtv);
}

void RenderContext::raytrace(RtProgram* pProgram, RtProgramVars* pVars, uint32_t width, uint32_t height, uint32_t depth) {
	auto pRtso = pProgram->getRtso(pVars);
	pVars->prepareShaderTable(this, pRtso.get());
	pVars->prepareDescriptorSets(this);

	//auto rtEncoder = mpLowLevelData->getApiData()->getRayTracingCommandEncoder();
	//FALCOR_GFX_CALL(rtEncoder->bindPipelineWithRootObject(pRtso->getApiHandle(), pVars->getShaderObject()));

//	dispatchRays(0, pVars->getShaderTable(), width, height, depth);
	mCommandsPending = true;
}

void RenderContext::buildAccelerationStructure(const RtAccelerationStructure::BuildDesc& desc, uint32_t postBuildInfoCount, RtAccelerationStructurePostBuildInfoDesc* pPostBuildInfoDescs) {
    RtAccelerationStructure::BuildDesc buildDesc = {};
    buildDesc.dest = desc.dest;
    buildDesc.scratchData = desc.scratchData;
    buildDesc.source = desc.source ? desc.source : nullptr;
    buildDesc.inputs = desc.inputs; //translator.translate(desc.inputs);

    std::vector<AccelerationStructureQueryDesc> queryDescs(postBuildInfoCount);
    for (uint32_t i = 0; i < postBuildInfoCount; i++) {
        queryDescs[i].firstQueryIndex = pPostBuildInfoDescs[i].index;
        queryDescs[i].queryPool = pPostBuildInfoDescs[i].pool->getRtQueryPool();
        queryDescs[i].queryType = getVKAccelerationStructurePostBuildQueryType(pPostBuildInfoDescs[i].type);
    }
    //auto rtEncoder = getLowLevelData()->getApiData()->getRayTracingCommandEncoder();
    //rtEncoder->buildAccelerationStructure(buildDesc, (int)postBuildInfoCount, queryDescs.data());
    mCommandsPending = true;
}

void RenderContext::copyAccelerationStructure(RtAccelerationStructure* dest, RtAccelerationStructure* source, RenderContext::RtAccelerationStructureCopyMode mode) {
    //auto rtEncoder = getLowLevelData()->getApiData()->getRayTracingCommandEncoder();
    //rtEncoder->copyAccelerationStructure(dest->getApiHandle(), source->getApiHandle(), getGFXAcclerationStructureCopyMode(mode));
    mCommandsPending = true;
}

}
