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
		using EdgeKernelType = EdgeDetectKernelType;

		static const Info kInfo;

		virtual ~EdgeDetectPass() = default;

		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

		virtual Dictionary getScriptingDictionary() override;
		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void compile(RenderContext* pContext, const CompileData& compileData) override;
		virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
		virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;

		void setOutputFormat(ResourceFormat format);
		void setDepthKernelSize(uint size);
		void setNormalKernelSize(uint size);
		void setMaterialKernelSize(uint size);
		void setInstanceKernelSize(uint size);

		void setDepthDistanceRange(float2 range);
		void setDepthDistanceRange(float distMin, float distMax) { setDepthDistanceRange(float2({distMin, distMax})); }

		void setNormalThresholdRange(float2 range);
		void setNormalThresholdRange(float distMin, float distMax) { setNormalThresholdRange(float2({distMin, distMax})); }

		void setEdgeDetectFlags(EdgeDetectFlags flags);

		void setLowPassFilterSize(uint size);

		void setTraceDepth(bool state);
		void setTraceNormal(bool state);
		void setTraceMaterialID(bool state);
		void setTraceInstanceID(bool state);
		void setTraceAlpha(bool state);

		inline Falcor::ResourceFormat format() const { return mOutputFormat; }

	protected:
		EdgeDetectPass(Device::SharedPtr pDevice, const Dictionary& dict);

		void prepareBuffers(RenderContext* pRenderContext, uint2 resolution);
		void prepareKernelTextures();

		Device::SharedPtr           mpDevice;

		// Internal state
		Scene::SharedPtr            mpScene;                        ///< The current scene (or nullptr if no scene).
		Camera::SharedPtr           mpCamera;                       ///< Current scene camera.
		uint                        mDepthKernelSize = 3;           ///< Edge detection kernel size [mKernelSize, mKernelSize]
		uint                        mNormalKernelSize = 3;          ///< Edge detection kernel size [mKernelSize, mKernelSize]
		uint                        mMaterialKernelSize = 3;        ///< Edge detection kernel size [mKernelSize, mKernelSize]
		uint                        mInstanceKernelSize = 3;        ///< Edge detection kernel size [mKernelSize, mKernelSize]
		uint                        mLowPassFilterSize = 0;

		ComputePass::SharedPtr      mpPassU;
		ComputePass::SharedPtr      mpPassV;
		ComputePass::SharedPtr      mpLowPass;

		Texture::SharedPtr          mpDepthKernelU;
		Texture::SharedPtr          mpDepthKernelV;
		Texture::SharedPtr          mpNormalKernelU;
		Texture::SharedPtr          mpNormalKernelV;
		Texture::SharedPtr          mpMaterialKernelU;
		Texture::SharedPtr          mpMaterialKernelV;
		Texture::SharedPtr          mpInstanceKernelU;
		Texture::SharedPtr          mpInstanceKernelV;

		EdgeKernelType							mDepthKernelType = EdgeKernelType::Sobel;
		EdgeKernelType              mNormalKernelType = EdgeKernelType::Sobel;
		EdgeKernelType              mMaterialKernelType = EdgeKernelType::Sobel;
		EdgeKernelType              mInstanceKernelType = EdgeKernelType::Sobel;

		ResourceFormat              mVBufferFormat = HitInfo::kDefaultFormat;
		ResourceFormat              mOutputFormat = ResourceFormat::RGBA16Float;

		// Temporary buffers        
		Texture::SharedPtr          mpTmpVBuffer;  // vbuffer after lowpass filter
		Texture::SharedPtr          mpTmpDepth;
		Texture::SharedPtr          mpTmpNormal;
		Texture::SharedPtr					mpTmpMaterialID;
		Texture::SharedPtr 					mpTmpInstanceID;

		bool                        mDirty = true;

		// Pass params
		EdgeDetectFlags             mEdgeDetectFlags = EdgeDetectFlags::TraceDepth;
		float2                      mDepthDistanceRange = {5.f, 10.f};
		float2                      mNormalThresholdRange = {2.f, 3.f};
};

#define ktype2str(a) case  EdgeDetectPass::EdgeKernelType::a: return #a
inline std::string to_string(EdgeDetectPass::EdgeKernelType kernelType) {
    switch (kernelType) {
        ktype2str(Sobel);
        ktype2str(Prewitt);
    default:
        should_not_get_here(); return "";
    }
}
#undef ktype2str

#endif  // SRC_FALCOR_RENDERPASSES_EDGEDETECTPASS_EDGEDETECTPASS_H_