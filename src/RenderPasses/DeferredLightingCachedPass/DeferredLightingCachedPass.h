#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINCACHEDGPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINCACHEDGPASS_H_

#include "Falcor/Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Utils/Sampling/SampleGenerator.h"
#include "Falcor/Scene/Scene.h"
#include "Experimental/Scene/Lights/EnvMapLighting.h"
#include "Experimental/Scene/Lights/EnvMapSampler.h"

using namespace Falcor;

//static constexpr float16_t MAX_COLOR_COMPONENT_VALUE = std::numeric_limits<float16_t>::max();

class DeferredLightingCachedPass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<DeferredLightingCachedPass>;

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
		DeferredLightingCachedPass& setFrameSampleCount(uint32_t samples);

		/** Set the color target format. This is always enabled
		*/
		DeferredLightingCachedPass& setColorFormat(ResourceFormat format);

		/** Set the required output supersample-count. 0 will use the swapchain sample count
		*/
		DeferredLightingCachedPass& setSuperSampleCount(uint32_t samples);

		/** Set the color limit
		*/
		DeferredLightingCachedPass& setColorLimit(const float3& limit);

		/** Set the indirect color limit
		*/
		DeferredLightingCachedPass& setIndirectColorLimit(const float3& limit);

		/** Set the STBN sampling as preferred samping method
		*/
		DeferredLightingCachedPass& setSTBNSampling(bool enable);

		/** Set ray tracing bias
		*/
		DeferredLightingCachedPass& setRayBias(float bias);

		/** Set shading rate (supersampling)
		*/
		DeferredLightingCachedPass& setShadingRate(int rate);

		DeferredLightingCachedPass& setRayReflectLimit(int limit);
		DeferredLightingCachedPass& setRayDiffuseLimit(int limit);

	private:
		DeferredLightingCachedPass(Device::SharedPtr pDevice);
		
		Scene::SharedPtr                mpScene;
		Camera::SharedPtr               mpCamera;
		ComputePass::SharedPtr          mpDirectLightingPass;
		ComputePass::SharedPtr          mpIndirectLightingPass;

		uint2 mFrameDim = { 0, 0 };
		uint32_t mFrameSampleCount = 16;
		uint32_t mSuperSampleCount = 1;

		uint32_t 		mSampleNumber = 0;
		float16_t3  mColorLimit = float16_t3(HLF_MAX, HLF_MAX, HLF_MAX);
		float16_t3  mIndirectColorLimit = float16_t3(HLF_MAX, HLF_MAX, HLF_MAX);
		bool     		mUseSTBN = false;
		float    		mRayBias = 0.001f;
		int      		mShadingRate = 1;
		uint 				mRayReflectLimit = 0;
		uint      	mRayDiffuseLimit = 0;

		Sampler::SharedPtr                  mpDepthSampler;
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

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINCACHEDGPASS_H_
