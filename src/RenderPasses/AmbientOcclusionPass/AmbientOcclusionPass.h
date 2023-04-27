#ifndef SRC_FALCOR_RENDERPASSES_AMBIENTOCCLUSIONPASS_AMBIENTOCCLUSIONPASS_H_
#define SRC_FALCOR_RENDERPASSES_AMBIENTOCCLUSIONPASS_AMBIENTOCCLUSIONPASS_H_

#include "Falcor/Falcor.h"
#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/Utils/Sampling/SampleGenerator.h"
#include "Falcor/Utils/Math/MathConstants.slangh"
#include "Falcor/RenderGraph/RenderPass.h"


using namespace Falcor;


/** Ambient occlusion render pass.

*/
class AmbientOcclusionPass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<AmbientOcclusionPass>;
		using SharedConstPtr = std::shared_ptr<const AmbientOcclusionPass>;

		static const Info kInfo;

		virtual ~AmbientOcclusionPass() = default;

		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

		virtual Dictionary getScriptingDictionary() override;
		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void compile(RenderContext* pContext, const CompileData& compileData) override;
		virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
		virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;

		/** Set shading rate (supersampling)
		*/
		AmbientOcclusionPass& setShadingRate(int rate);

		void setOutputFormat(ResourceFormat format);

		void setDistanceRange(float2 range);
		inline Falcor::ResourceFormat format() const { return mOutputFormat; }

	protected:
		AmbientOcclusionPass(Device::SharedPtr pDevice, const Dictionary& dict);

		Device::SharedPtr           mpDevice;

		// Internal state
		Scene::SharedPtr            mpScene;                        ///< The current scene (or nullptr if no scene).
		
		ComputePass::SharedPtr      mpPassRayTrace;
		
		ResourceFormat              mOutputFormat = ResourceFormat::R16Float;
		SampleGenerator::SharedPtr  mpSampleGenerator;           		///< GPU sample generator.

		bool                        mDirty = true;

		float2                      mDistanceRange = {1.f, 2.f};
		uint                        mShadingRate = 1;
		uint                        mSampleNumber = 0;
};

#endif  // SRC_FALCOR_RENDERPASSES_AMBIENTOCCLUSIONPASS_AMBIENTOCCLUSIONPASS_H_