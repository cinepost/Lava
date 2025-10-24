#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEBUGSHADINGPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEBUGSHADINGPASS_H_

#include "Falcor/Falcor.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/Scene/Lights/Light.h"
#include "Falcor/Utils/Sampling/VisibilitySamplesContainer.h"
#include "Falcor/Utils/Color/FalseColorGenerator.h"
#include "Falcor/Utils/Color/HeatMapColorGenerator.h"

using namespace Falcor;

class PASS_API DebugShadingPass : public RenderPass {
		FALCOR_OBJECT(DebugShadingPass)
		FALCOR_PLUGIN_CLASS(DebugShadingPass, "DebugShadingPass", "Debug shading pass.");
	public:
		/** Create a new object
		*/
		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Properties& props = {});

		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
		virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
		virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
		virtual Properties getProperties() const override;


		/** Set the color target format. This is always enabled
		*/
		DebugShadingPass& setColorFormat(ResourceFormat format);

		void setVisibilitySamplesContainer(VisibilitySamplesContainer::SharedConstPtr pVisibilitySamplesContainer);

	private:
		DebugShadingPass(Device::SharedPtr pDevice, const Properties& props = {});

		void generateMeshletColorBuffer(const RenderData& renderData);
		
		Buffer::SharedPtr 					mpOpaquePassIndirectionArgsBuffer;
		
		Scene::SharedPtr                	mpScene;
		ComputePass::SharedPtr          	mpShadingPass;
		ComputePass::SharedPtr          	mpTransparentShadingPass;

		// Sampling buffer (optional)
		VisibilitySamplesContainer::SharedConstPtr 	mpVisibilitySamplesContainer;

		ResourceFormat                  	mFalseColorFormat = ResourceFormat::RGBA16Float;

		FalseColorGenerator::SharedPtr  	mpFalseColorGenerator;
		HeatMapColorGenerator::SharedPtr  	mpHeatMapColorGenerator;
		Buffer::SharedPtr               	mpMeshletColorBuffer;

		uint2 mFrameDim = { 0, 0 };
		
		bool mDirty = true;
};

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEBUGSHADINGPASS_H_
