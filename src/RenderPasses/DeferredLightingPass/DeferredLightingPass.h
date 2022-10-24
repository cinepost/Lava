#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINGPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINGPASS_H_

#include "Falcor/Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Utils/Sampling/SampleGenerator.h"
#include "Falcor/Scene/Scene.h"
#include "Experimental/Scene/Lights/EnvMapLighting.h"
#include "Experimental/Scene/Lights/EnvMapSampler.h"

using namespace Falcor;

class DeferredLightingPass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<DeferredLightingPass>;

		static const Info kInfo;

		/** Create a new object
		*/
		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
		virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
		virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
		virtual Dictionary getScriptingDictionary() override;

		/** Set samples per frame count
		*/
		DeferredLightingPass& setFrameSampleCount(uint32_t samples);

		/** Set the color target format. This is always enabled
		*/
		DeferredLightingPass& setColorFormat(ResourceFormat format);

		/** Set the required output supersample-count. 0 will use the swapchain sample count
		*/
		DeferredLightingPass& setSuperSampleCount(uint32_t samples);

		/** Enable super-sampling in the pixel-shader
		*/
		DeferredLightingPass& setSuperSampling(bool enable);

	private:
		DeferredLightingPass(Device::SharedPtr pDevice);
		
		Scene::SharedPtr                mpScene;
		ComputePass::SharedPtr          mpLightingPass;
		
		uint2 mFrameDim = { 0, 0 };
		uint32_t mFrameSampleCount = 16;
		uint32_t mSuperSampleCount = 1;

		uint32_t mSampleNumber = 0;

		Sampler::SharedPtr                  mpNoiseSampler;
		Texture::SharedPtr                  mpBlueNoiseTexture;
		CPUSampleGenerator::SharedPtr       mpNoiseOffsetGenerator;      ///< Blue noise texture offsets generator. Sample in the range [-0.5, 0.5) in each dimension.
		SampleGenerator::SharedPtr          mpSampleGenerator;           ///< GPU sample generator.
		
		EnvMapLighting::SharedPtr           mpEnvMapLighting = nullptr;
		EnvMapSampler::SharedPtr            mpEnvMapSampler = nullptr;

		bool mEnableSuperSampling = false;
		bool mUseSimplifiedEnvLighting = false;
		
		bool mDirty = true;
		bool mEnvMapDirty = true;
};

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINGPASS_H_
