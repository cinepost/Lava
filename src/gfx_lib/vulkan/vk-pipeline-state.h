// vk-pipeline-state.h
#pragma once

#include <future>

#include "vk-base.h"
#include "vk-pipeline-cache.h"
#include "Falcor/Utils/ThreadPool.h"


namespace gfx {

using namespace Slang;

namespace vk {

class PipelineStateImpl : public PipelineStateBase {
	public:
		PipelineStateImpl(DeviceImpl* pDevice);
		virtual ~PipelineStateImpl() override;

		// Turns `m_device` into a strong reference.
		// This method should be called before returning the pipeline state object to
		// external users (i.e. via an `IPipelineState` pointer).
		void establishStrongDeviceReference();

		virtual void comFree() override;

		void init(const GraphicsPipelineStateDesc& inDesc);
		void init(const ComputePipelineStateDesc& inDesc);
		void init(const RayTracingPipelineStateDesc& inDesc);

		Result createVKGraphicsPipelineState();

		Result createVKComputePipelineState();

		virtual Result ensureAPIPipelineStateCreated() override;

		virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(InteropHandle* outHandle) override;

		BreakableReference<DeviceImpl> m_device;

		VkPipeline getPipeline();

		void destroy();

		virtual bool hasCacheBlob() override;

	protected:
		VkPipeline m_pipeline = VK_NULL_HANDLE;
		std::future<VkResult> mResult;

		std::unique_ptr<PipelineCache> mpPipelineCache;
};

class RayTracingPipelineStateImpl : public PipelineStateImpl {
	public:
		Dictionary<String, Index> shaderGroupNameToIndex;
		Int shaderGroupCount;

		RayTracingPipelineStateImpl(DeviceImpl* device);

		uint32_t findEntryPointIndexByName(
			const Dictionary<String, Index>& entryPointNameToIndex, const char* name);

		Result createVKRayTracingPipelineState();

		virtual Result ensureAPIPipelineStateCreated() override;

		virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(InteropHandle* outHandle) override;
};

} // namespace vk
} // namespace gfx
