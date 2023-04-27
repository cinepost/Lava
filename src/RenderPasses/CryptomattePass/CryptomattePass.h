#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_CRYPTOMATTEPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_CRYPTOMATTEPASS_H_

#include "Falcor/Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Scene/Scene.h"

using namespace Falcor;


class PASS_API CryptomattePass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<CryptomattePass>;

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
		CryptomattePass& setColorFormat(ResourceFormat format);

	private:
		void calculateHashTables( const RenderData& renderData);
		CryptomattePass(Device::SharedPtr pDevice);
		
		Scene::SharedPtr                mpScene;
		ComputePass::SharedPtr          mpPass;

		Buffer::SharedPtr               mpMaterialHashBuffer;
		Buffer::SharedPtr               mpInstanceHashBuffer;
		Buffer::SharedPtr               mpCustattrHashBuffer;

		Buffer::SharedPtr               mpMaterialPreviewColorBuffer;
		Buffer::SharedPtr               mpInstancePreviewColorBuffer;
		Buffer::SharedPtr               mpCustattrPreviewColorBuffer;

		bool                            mOutputMaterialPreview = false;
		bool                            mOutputInstancePreview = false;
		bool                            mOutputCustattrPreview = false;

		uint2 mFrameDim = { 0, 0 };

		uint32_t 		mSampleNumber = 0;
		
		bool mDirty = true;
};

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_CRYPTOMATTEPASS_H_
