#ifndef SRC_FALCOR_RENDERPASSES_EDGEDETECTPASS_EDGEDETECTPASS_H_
#define SRC_FALCOR_RENDERPASSES_EDGEDETECTPASS_EDGEDETECTPASS_H_

#include "Falcor/Falcor.h"
#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Scene/Scene.h"
#include "Falcor/Utils/Sampling/SampleGenerator.h"
#include "Falcor/Utils/Math/MathConstants.slangh"
#include "Falcor/RenderGraph/RenderPass.h"

#include "EdgeDetectPass.slangh"


using namespace Falcor;


/** Edge detection render pass.

*/
class EdgeDetectPass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<EdgeDetectPass>;
		using SharedConstPtr = std::shared_ptr<const EdgeDetectPass>;

		using EdgeDetectFlags = EdgeDetectTraceFlags;

		static const Info kInfo;

		virtual ~EdgeDetectPass() = default;

		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

		virtual Dictionary getScriptingDictionary() override;
		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void compile(RenderContext* pContext, const CompileData& compileData) override;
		virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
		virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;

		void setOutputFormat(ResourceFormat format);
		void setKernelSize(uint2 size);

		void setDepthDistanceRange(float2 range);
		void setDepthDistanceRange(float distMin, float distMax) { setDepthDistanceRange(float2({distMin, distMax})); }

		void setEdgeDetectFlags(EdgeDetectFlags flags);

		inline Falcor::ResourceFormat format() const { return mOutputFormat; }

	protected:
		EdgeDetectPass(Device::SharedPtr pDevice, const Dictionary& dict);

		void prepareBuffers(RenderContext* pRenderContext, uint2 resolution);
		void prepareKernelTextures();

		Device::SharedPtr           mpDevice;

		// Internal state
		Scene::SharedPtr            mpScene;                        ///< The current scene (or nullptr if no scene).
		Camera::SharedPtr           mpCamera;                       ///< Current scene camera.
		uint2                       mKernelSize = {3, 3};           ///< Edge detection kernel size

		ComputePass::SharedPtr      mpPassU;
		ComputePass::SharedPtr      mpPassV;

		Texture::SharedPtr          mpDepthKernelU;
		Texture::SharedPtr          mpDepthKernelV;
		Texture::SharedPtr          mpNormalKernelU;
		Texture::SharedPtr          mpNormalKernelV;

		ResourceFormat              mVBufferFormat = HitInfo::kDefaultFormat;
		ResourceFormat              mOutputFormat = ResourceFormat::RGBA16Float;

		// Temporary buffers        
		Texture::SharedPtr          mpTmpDepth;
		Texture::SharedPtr          mpTmpNormal;
		Texture::SharedPtr					mpTmpMaterial;
		Texture::SharedPtr 					mpTmpInstance;

		bool                        mDirty = true;

		// Pass params
		EdgeDetectFlags             mEdgeDetectFlags = EdgeDetectFlags::TraceDepth;
		float2                      mDepthDistanceRange = {5.f, 10.f};
		float2                      mNormalThresholdRange = {2.f, 3.f};
};

#endif  // SRC_FALCOR_RENDERPASSES_EDGEDETECTPASS_EDGEDETECTPASS_H_