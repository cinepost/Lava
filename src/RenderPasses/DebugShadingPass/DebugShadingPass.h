#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEBUGSHADINGPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEBUGSHADINGPASS_H_

#include "Falcor/Falcor.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/Scene/Lights/Light.h"

using namespace Falcor;

class DebugShadingPass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<DebugShadingPass>;

		static const Info kInfo;

		/** Create a new object
		*/
		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
		virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
		virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
		virtual Dictionary getScriptingDictionary() override;


		/** Set the color target format. This is always enabled
		*/
		DebugShadingPass& setColorFormat(ResourceFormat format);

	private:
		DebugShadingPass(Device::SharedPtr pDevice);
		
		Scene::SharedPtr                mpScene;
		ComputePass::SharedPtr          mpShadingPass;

		uint2 mFrameDim = { 0, 0 };
		
		bool mDirty = true;
};

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEBUGSHADINGPASS_H_
