#ifndef SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_CRYPTOMATTEPASS_H_
#define SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_CRYPTOMATTEPASS_H_

#include "Falcor/Falcor.h"
#include "FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/RenderGraph/RenderPass.h"
#include "Falcor/Scene/Scene.h"

#include "Falcor/Utils/Cryptomatte/Cryptomatte.h"
#include "CryptomattePass.slangh"


using namespace Falcor;

class CryptomattePass : public RenderPass {
	public:
		using SharedPtr = std::shared_ptr<CryptomattePass>;
		using CryptomatteMode = CryptomattePassMode;
		static const Info kInfo;

		/** Create a new object
		*/
		static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

		virtual RenderPassReflection reflect(const CompileData& compileData) override;
		virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
		virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override;
		virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
		virtual Dictionary getScriptingDictionary() override;

		/** Set the preview color target format. This is always enabled
		*/
		CryptomattePass& setColorFormat(ResourceFormat format);

		void setMode(CryptomatteMode mode);
		void setRank(uint32_t rank);
		
		inline uint32_t dataLayersCount() const { return (mRank >> 1) + (mRank - 2 * (mRank >> 1)); }

	private:
		struct HashResolveCounter {
			uint32_t hash;
			float    weight = 0.f;
		};

		using SortingPair = CryptomattePassSortingPair;

	private:
		void calculateHashTables(const RenderData& renderData);
		void createSortingBuffers();
		CryptomattePass(Device::SharedPtr pDevice);

		Scene::SharedPtr                mpScene;
		ComputePass::SharedPtr          mpPass;

		Buffer::SharedPtr               mpHashBuffer;
		Buffer::SharedPtr               mpFloatHashBuffer;
		std::vector<Buffer::SharedPtr>  mDataSortingBuffers;
		Buffer::SharedPtr               mpPreviewHashColorBuffer;

		CryptomatteMode                 mMode = CryptomatteMode::Material;
		uint32_t                        mRank = 0;

		bool                            mOutputPreview = true;

		Cryptomatte::CryptoNameFlags    mMaterialNameCleaningFlags = Cryptomatte::CryptoNameFlags::CRYPTO_NAME_NONE;
		Cryptomatte::CryptoNameFlags    mInstanceNameCleaningFlags = Cryptomatte::CryptoNameFlags::CRYPTO_NAME_NONE;

		uint2 mFrameDim = { 0, 0 };

		uint32_t 		mSampleNumber = 0;
		
		bool mDirty = true;
};

#define pftype2str(a) case CryptomattePass::CryptomatteMode::a: return #a
inline std::string to_string(CryptomattePass::CryptomatteMode a) {
    switch (a) {
        pftype2str(Material);
        pftype2str(Instance);
        pftype2str(Asset);
        default: should_not_get_here(); return "";
    }
}
#undef pftype2str

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_CRYPTOMATTEPASS_H_
