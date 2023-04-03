#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINGPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINGPASS_H_

#include "Falcor/Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Utils/Sampling/SampleGenerator.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/Scene/Lights/Light.h"
#include "Experimental/Scene/Lights/EnvMapLighting.h"
#include "Experimental/Scene/Lights/EnvMapSampler.h"

using namespace Falcor;

//static constexpr float16_t MAX_COLOR_COMPONENT_VALUE = std::numeric_limits<float16_t>::max();

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

		/** Set the color limit
		*/
		DeferredLightingPass& setColorLimit(const float3& limit);

		/** Set the indirect color limit
		*/
		DeferredLightingPass& setIndirectColorLimit(const float3& limit);

		/** Set the STBN sampling as preferred samping method
		*/
		DeferredLightingPass& setSTBNSampling(bool enable);

		/** Set ray tracing bias
		*/
		DeferredLightingPass& setRayBias(float bias);

		/** Set shading rate (supersampling)
		*/
		DeferredLightingPass& setShadingRate(int rate);

		/** Set area lights sampling mode
		*/
		DeferredLightingPass& setAreaLightsSamplingMode(AnalyticAreaLight::LightSamplingMode areaLightsSamplingMode);
		DeferredLightingPass& setAreaLightsSamplingMode(const std::string& areaLightsSamplingModeName);

		DeferredLightingPass& setRayReflectLimit(int limit);
		DeferredLightingPass& setRayRefractLimit(int limit);
		DeferredLightingPass& setRayDiffuseLimit(int limit);

		void setRayContribThreshold(float value = 0.1f);
    void setRussRoulleteLevel(uint value = 2u);

	private:
		DeferredLightingPass(Device::SharedPtr pDevice);
		
		void createBuffers(RenderContext* pContext, const RenderData& renderData);

		Scene::SharedPtr                mpScene;
		ComputePass::SharedPtr          mpLightingPass;

		uint2 mFrameDim = { 0, 0 };
		uint32_t mFrameSampleCount = 16;
		uint32_t mSuperSampleCount = 1;

		uint32_t 		mSampleNumber = 0;
		float16_t3  mColorLimit = float16_t3(HLF_MAX, HLF_MAX, HLF_MAX);
		float16_t3  mIndirectColorLimit = float16_t3(HLF_MAX, HLF_MAX, HLF_MAX);
		bool     		mUseSTBN = false;
		float    		mRayBias = 0.001f;
		uint      	mShadingRate = 1;
		uint 				mRayReflectLimit = 0;
		uint 				mRayRefractLimit = 0;
		uint      	mRayDiffuseLimit = 0;

		bool        mUseVariance = true;

		float 			mRayContribThreshold = 0.1f;
		uint        mRussRouletteLevel = 2u;

		Sampler::SharedPtr                  mpNoiseSampler;
		Texture::SharedPtr                  mpBlueNoiseTexture;
		CPUSampleGenerator::SharedPtr       mpNoiseOffsetGenerator;      ///< Blue noise texture offsets generator. Sample in the range [-0.5, 0.5) in each dimension.
		SampleGenerator::SharedPtr          mpSampleGenerator;           ///< GPU sample generator.
		
		Texture::SharedPtr                  mpLastFrameSum;              ///< RGB - Last fram sum, A - variance

		EnvMapLighting::SharedPtr           mpEnvMapLighting = nullptr;
		EnvMapSampler::SharedPtr            mpEnvMapSampler = nullptr;

		bool mEnableSuperSampling = false;
		bool mUseSimplifiedEnvLighting = false;
		AnalyticAreaLight::LightSamplingMode mAreaLightsSamplingMode = AnalyticAreaLight::LightSamplingMode::SOLID_ANGLE;
		
		bool mDirty = true;
		bool mEnvMapDirty = true;
};

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_DEFERREDLIGHTINGPASS_H_
