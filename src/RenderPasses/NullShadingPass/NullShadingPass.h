#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_NULLSHADINGPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_NULLSHADINGPASS_H_

#include "Falcor/Falcor.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/RenderGraph/BasePasses/ComputePass.h"

using namespace Falcor;

class PASS_API NullShadingPass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<NullShadingPass>;

		static const Info kInfo;

		/** Create a new object
		*/
		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
		virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;

	private:
		NullShadingPass(Device::SharedPtr pDevice);

		ComputePass::SharedPtr          	mpShadingPass;

		bool mDirty = true;
};

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_NULLSHADINGPASS_H_
