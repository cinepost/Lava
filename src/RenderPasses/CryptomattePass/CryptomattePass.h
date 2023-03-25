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

	private:
		struct HashResolveCounter {
			uint32_t hash;
			float    weight = 0.f;
		};

	private:
		void calculateHashTables( const RenderData& renderData);
		CryptomattePass(Device::SharedPtr pDevice);

		void setLayerNames();

		Scene::SharedPtr                mpScene;
		ComputePass::SharedPtr          mpPass;

		Buffer::SharedPtr               mpMaterialHashBuffer;
		Buffer::SharedPtr               mpInstanceHashBuffer;
		Buffer::SharedPtr               mpCustattrHashBuffer;
		Buffer::SharedPtr               mpPreviewHashColorBuffer;

		CryptomatteMode                 mMode = CryptomatteMode::Material;
		uint32_t                        mRank = 0;

		bool                            mOutputPreview = true;

		std::string                     mMasterLayerName = "unknown"; ///< Preview layer name and data layers name prefix
		std::vector<std::string>				mDataLayerNames;							///< Cryptomatte layer names (excluding preview layer)
		uint32_t                        mDataLayersCount = 0;					///< Cryptomatte data layers

		Cryptomatte::CryptoNameFlags    mMaterialNameCleaningFlags = Cryptomatte::CryptoNameFlags::CRYPTO_NAME_NONE;
		Cryptomatte::CryptoNameFlags    mInstanceNameCleaningFlags = Cryptomatte::CryptoNameFlags::CRYPTO_NAME_NONE;

		uint2 mFrameDim = { 0, 0 };

		uint32_t 		mSampleNumber = 0;
		
		bool mDirty = true;
};

#endif  // SRC_FALCOR_RENDERPASSES_DEFERREDLIGHTINGPASS_CRYPTOMATTEPASS_H_
