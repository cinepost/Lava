#include <algorithm>
#include <chrono>
#include <unordered_set>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Image/TextureManager.h"
#include "Falcor/Scene/Material/BasicMaterial.h"

#include "TexturesResolvePass.h"


const RenderPass::Info TexturesResolvePass::kInfo { "TexturesResolve", "Resolves sparse textures tiles to be loaded" };


// Don't remove this. it's required for hot-reload to function properly
extern "C" falcorexport const char* getProjDir() {
	return PROJECT_DIR;
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
	lib.registerPass(TexturesResolvePass::kInfo, TexturesResolvePass::create);
}

namespace {
	const std::string kProgramFile = "RenderPasses/TexturesResolvePass/TexturesResolvePass.ps.slang";

	const std::string kDepth = "depth";
	const std::string kOutput = "output";

	const std::string kTexResolveData = "gTexResolveData";
	const std::string kParameterBlockName = "gResolveData";

	const std::string kRayReflectLimit = "rayReflectLimit";
	const std::string kRayRefractLimit = "rayRefractLimit";
	const std::string kRayDiffuseLimit = "rayDiffuseLimit";

	const std::string kAsyncLtxLoading = "asyncLtxLoading";

}  // namespace

void TexturesResolvePass::parseDictionary(const Dictionary& dict) {
	for (const auto& [key, value] : dict) {
        if (key == kRayReflectLimit) setRayReflectLimit(value);
        else if (key == kRayRefractLimit) setRayRefractLimit(value);
        else if (key == kRayDiffuseLimit) setRayDiffuseLimit(value);
        else if (key == kAsyncLtxLoading) setAsyncLoading(static_cast<bool>(value));
    }
}

Dictionary TexturesResolvePass::getScriptingDictionary() {
	Dictionary d;
	return d;
}

TexturesResolvePass::SharedPtr TexturesResolvePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
	auto pTexturesResolvePass = new TexturesResolvePass(pRenderContext->device(), dict);

	pTexturesResolvePass->parseDictionary(dict);

	// Create calibration textures
	pTexturesResolvePass->createMipCalibrationTexture(pRenderContext);
	pTexturesResolvePass->createLtxCalibrationTexture(pRenderContext);

	return SharedPtr(pTexturesResolvePass);
}

TexturesResolvePass::TexturesResolvePass(Device::SharedPtr pDevice, const Dictionary& dict): RenderPass(pDevice, kInfo) {
	
	//Program::DefineList defines = { { "_MS_DISABLE_ALPHA_TEST", "" } };
	Program::Desc desc;
	desc.addShaderLibrary(kProgramFile).vsEntry("vsMain").psEntry("psMain");

	mpProgram = GraphicsProgram::create(pDevice, desc);

	mpFbo = Fbo::create(pDevice);

	mpState = GraphicsState::create(pDevice);

	DepthStencilState::Desc dsDesc;
	dsDesc.setDepthWriteMask(false).setDepthEnabled(true).setDepthFunc(DepthStencilState::Func::Equal);
	mpDsNoDepthWrite = DepthStencilState::create(dsDesc);
	mpState->setDepthStencilState(DepthStencilState::create(dsDesc));

	mpState->setProgram(mpProgram);

	parseDictionary(dict);

 	if (1 == 1) {
		mpState->getProgram()->addDefine("_OUTPUT_DEBUG_IMAGE");
	}
}

RenderPassReflection TexturesResolvePass::reflect(const CompileData& compileData) {
	RenderPassReflection reflector;

	reflector.addOutput(kOutput, "DebugOutput-buffer").format(mTileDataDebugFormat).texture2D(0, 0, 0);
	auto& depthField = reflector.addInputOutput(kDepth, "Depth-buffer. Should be pre-initialized or cleared before calling the pass")
		.bindFlags(Resource::BindFlags::DepthStencil).flags(RenderPassReflection::Field::Flags::Optional);
	return reflector;
}

void TexturesResolvePass::updateTexturesResolveData() {
 
}

void TexturesResolvePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) {
	mpScene = pScene;
	if (mpScene) {
		mpState->getProgram()->addDefines(mpScene->getSceneDefines());
		//updateTexturesResolveData();
	}
	mpVars = GraphicsVars::create(pRenderContext->device(), mpState->getProgram()->getReflector());
}

void TexturesResolvePass::initDepth(RenderContext* pContext, const RenderData& renderData) {
	const auto& pDepth = renderData[kDepth]->asTexture();

	if (pDepth) {
		mpState->setDepthStencilState(mpDsNoDepthWrite);
		mpFbo->attachDepthStencilTarget(pDepth);
	} else {
		LLOG_WRN << "No required depth channel provided !!!";
	}
}

void TexturesResolvePass::execute(RenderContext* pContext, const RenderData& renderData) {
	if (!mpScene)
		return;

	if(mDirty) {
		uint maxRayLevel = std::max(std::max(mRayDiffuseLimit, mRayReflectLimit), mRayRefractLimit);
		mpState->getProgram()->addDefine("_MAX_RAY_LEVEL", std::to_string(maxRayLevel));
	}

	initDepth(pContext, renderData);

	const auto& pDebugData = renderData[kOutput]->asTexture();
	mpFbo->attachColorTarget(pDebugData, 0);

	mpState->setFbo(mpFbo);
	pContext->clearRtv(pDebugData->getRTV().get(), {0, 0, 0, 0});

	auto exec_started = std::chrono::high_resolution_clock::now();

	auto pTextureManager = mpScene->materialSystem()->textureManager();

	createMipCalibrationTexture(pContext);

	uint32_t totalPagesToUpdateCount = 0;
	uint32_t currTextureResolveID = 0; // texture id used to identify texture inside this pass. always starts from 0. nothing to do with real unique texture id or handle

	std::vector<MaterialResolveData> materialsResolveBuffer;
	std::vector<Texture::SharedPtr> materialTextures;

	std::vector<uint32_t> texturePagesStartMap;
	texturePagesStartMap.reserve(1024);

	uint32_t materialsCount = mpScene->getMaterialCount();

	for( uint32_t m_i = 0; m_i < materialsCount; ++m_i ) {
		const auto pMaterial = mpScene->getMaterial(m_i)->toBasicMaterial();
		if(!pMaterial) continue;

		std::vector<std::pair<TextureSlot, Texture::SharedPtr>> materialSparseTextures;

		for( const auto& slot: std::vector<TextureSlot>({TextureSlot::BaseColor, TextureSlot::Metallic, TextureSlot::Roughness, TextureSlot::Normal, TextureSlot::Emissive, TextureSlot::Opacity})) {
			auto const& pTexture = pMaterial->getTexture(slot);
			if(pTexture && (pTexture->isSparse() || pTexture->isUDIMTexture())) {
				materialSparseTextures.push_back({slot, pTexture});

				if (!std::count(materialTextures.begin(), materialTextures.end(), pTexture)) materialTextures.push_back(pTexture);
			}
		}

		materialsResolveBuffer.push_back({});
		auto& materialResolveData = materialsResolveBuffer.back();

		size_t virtualTexturesCount = std::min((size_t)MAX_VTEX_PER_MATERIAL_COUNT, materialSparseTextures.size());
		materialResolveData.virtualTexturesCount = virtualTexturesCount;

		// fill data for active(used) textures
		for( size_t i = 0; i < virtualTexturesCount; ++i) {
			const auto& pTexture = materialSparseTextures[i].second;
			const auto& textureHandle = pMaterial->getTextureHandle(materialSparseTextures[i].first);
			materialResolveData.virtualTextureHandles[i] = textureHandle;
		}
	}

	auto pDataToResolveBuffer = Buffer::createStructured(mpDevice, sizeof(MaterialResolveData), materialsCount, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialsResolveBuffer.data(), true);
	mpVars->setBuffer("materialsResolveDataBuffer", pDataToResolveBuffer);

	uint32_t resolvedTexturesCount = currTextureResolveID + 1;

	// ensure pages buffer is aligned to 4 bytes
	auto totalPagesToUpdateCountAligned = totalPagesToUpdateCount;
	auto dv = std::div(totalPagesToUpdateCount, 4);
	if(dv.rem != 0) {
		totalPagesToUpdateCountAligned = (dv.quot + 1) * 4;
	}

	mpVars["PerFrameCB"]["gRenderTargetDim"] = float2(mpFbo->getWidth(), mpFbo->getHeight());
	mpVars["PerFrameCB"]["materialsToResolveCount"] = materialsResolveBuffer.size();
	mpVars["PerFrameCB"]["resolvedTexturesCount"] = resolvedTexturesCount;
	mpVars["PerFrameCB"]["numberOfMipCalibrationTextures"] = (int32_t)mMipCalibrationTextures.size();

	mpVars["mipCalibrationTexture"] = mpMipCalibrationTexture;
	mpVars["ltxCalibrationTexture"] = mpLtxCalibrationTexture;

	for(uint32_t i = 0; i < mMipCalibrationTextures.size(); ++i)
		mpVars["mipCalibrationTextures"][i] = mMipCalibrationTextures[i];

	setDefaultSampler();
	mpVars["gCalibrationSampler"] = mpSampler;
	mpVars["gCalibrationMinSampler"] = mpMinSampler;
	mpVars["gCalibrationMaxSampler"] = mpMaxSampler;

	mpScene->rasterize(pContext, mpState.get(), mpVars.get(), RasterizerState::CullMode::None);
	pContext->flush(true);

	// Test resolved data
	auto pPagesBuffer = pTextureManager->getPagesResidencyBuffer();
	
	const int8_t* pOutPagesData = pPagesBuffer ? reinterpret_cast<const int8_t*>(pPagesBuffer->map(Buffer::MapType::Read)) : nullptr;

	// Load texture pages
	auto started = std::chrono::high_resolution_clock::now();

	std::unordered_set<Texture::SharedPtr> textures;
	
	for (auto const& pTexture : materialTextures) {
		if(pTexture->isUDIMTexture()) {
			for( const auto& tileInfo: pTexture->getUDIMTileInfos()) {
				if( tileInfo.pTileTexture && tileInfo.pTileTexture->isSparse()) textures.insert(tileInfo.pTileTexture);
			}
		} else if(pTexture->isSparse()) {
			textures.insert(pTexture);
		}

	}

	if(mLoadPagesAsync) mpDevice->flushAndSync();

	std::vector<std::pair<Texture::SharedPtr, std::vector<uint32_t>>> texturesToPageIDsList;
	texturesToPageIDsList.reserve(textures.size()); 

	for ( auto const& pTex: textures) {
		uint32_t pagesStartOffset = pTextureManager->getVirtualTexturePagesStartIndex(pTex.get());
		uint32_t texturePagesCount = pTex->sparseDataPagesCount();
		LLOG_DBG << "Analyzing " << std::to_string(texturePagesCount) << " pages for texture: " << pTex->getSourceFilename();
		LLOG_DBG << "Virtual texture " << pTex->getSourceFilename() << " pages start offset is " << std::to_string(pagesStartOffset);

		texturesToPageIDsList.emplace_back(std::make_pair(pTex,  std::vector<uint32_t>()));
		auto& texturePageIDs = texturesToPageIDsList.back().second;

		// index 'i' is a page index relative to the texture. starts with 0
		if(pOutPagesData) {
			for(uint32_t i = 0; i < texturePagesCount; ++i) {
				if (pOutPagesData[i + pagesStartOffset] != 0) {
					texturePageIDs.push_back(i);
				}
			}
			LLOG_DBG << std::to_string(texturePageIDs.size()) << " pages need to be loaded for texture " << pTex->getSourceFilename();
		}
	}

	if(mLoadPagesAsync) {
		pTextureManager->loadPagesAsync(texturesToPageIDsList); 
	} else {
		for(auto& textureToPagesPair: texturesToPageIDsList) {
			pTextureManager->loadPages(textureToPagesPair.first, textureToPagesPair.second); 
		} 
	}

	
	if(pPagesBuffer) pPagesBuffer->unmap();

	// In async mode we have to call updateSparseBindInfo on TextureManager as it triggers wait() function on pages loading multi-future
	if(mLoadPagesAsync) pTextureManager->updateSparseBindInfo();

	auto done = std::chrono::high_resolution_clock::now();
	LLOG_DBG << "Pages loading done in: " << std::chrono::duration_cast<std::chrono::milliseconds>(done-started).count() << " ms.";
	LLOG_INF << "TexturesResolvePass done in: " << std::setprecision(6) 
			 << (.001f * (float)std::chrono::duration_cast<std::chrono::milliseconds>(done-exec_started).count()) << " s";

	mDirty = false;
}

TexturesResolvePass& TexturesResolvePass::setDepthStencilState(const DepthStencilState::SharedPtr& pDsState) {
	mpState->setDepthStencilState(pDsState);
	return *this;
}

TexturesResolvePass& TexturesResolvePass::setRasterizerState(const RasterizerState::SharedPtr& pRsState) {
	mpRsState = pRsState;
	mpState->setRasterizerState(mpRsState);
	return *this;
}

void TexturesResolvePass::createMipCalibrationTexture(RenderContext* pRenderContext) {
	if (mpMipCalibrationTexture) return;

	// We use 8 mip levels calibration texture (128 x 128)

	mpMipCalibrationTexture = Texture::create2D(pRenderContext->device(), 128, 128, ResourceFormat::R32Float, 1, Texture::kMaxPossible, nullptr, Texture::BindFlags::ShaderResource);
	if (!mpMipCalibrationTexture) LLOG_ERR << "Error creating MIP calibration texture !!!";

	for(uint32_t mipLevel = 0; mipLevel < mpMipCalibrationTexture->getMipCount(); mipLevel++) {
		uint32_t width = mpMipCalibrationTexture->getWidth(mipLevel);
		uint32_t height = mpMipCalibrationTexture->getHeight(mipLevel); 
	
		// upload mip level data
		std::vector<float> initData(width*height, (float)mipLevel);
		uint32_t subresource = mpMipCalibrationTexture->getSubresourceIndex(0, mipLevel);
		pRenderContext->updateSubresourceData(mpMipCalibrationTexture.get(), subresource, initData.data());
	}

// For now we don't need them

	// Now. Let's make multiple calibration textures. This time only one particlar mip level contains non-zero data
	size_t buff_size = mpMipCalibrationTexture->getWidth() * mpMipCalibrationTexture->getHeight();
	static const std::vector<float> zero_buff(buff_size, 0.0f);
	static const std::vector<float> full_buff(buff_size, 1.0f);
	mMipCalibrationTextures.resize(mpMipCalibrationTexture->getMipCount());

	for(uint32_t i = 0; i < mpMipCalibrationTexture->getMipCount(); i++) {
		mMipCalibrationTextures[i] = nullptr;
		auto pMipCalibrationTexture = Texture::create2D(pRenderContext->device(), mpMipCalibrationTexture->getWidth(),  mpMipCalibrationTexture->getHeight(), ResourceFormat::R32Float, 1, Texture::kMaxPossible, nullptr, Texture::BindFlags::ShaderResource);
		if (!pMipCalibrationTexture) {
			LLOG_ERR << "Error creating calibration texture for mip level " << std::to_string(i) << " !!!";
			continue;
		}

		for(uint32_t mipLevel = 0; mipLevel < pMipCalibrationTexture->getMipCount(); mipLevel++) {
			// upload mip level data
			uint32_t subresource = pMipCalibrationTexture->getSubresourceIndex(0, mipLevel);
			if (mipLevel == i) {
				pRenderContext->updateSubresourceData(pMipCalibrationTexture.get(), subresource, full_buff.data());
			} else {
				pRenderContext->updateSubresourceData(pMipCalibrationTexture.get(), subresource, zero_buff.data());
			}
		}

		mMipCalibrationTextures[i] = pMipCalibrationTexture;
	}

}

void TexturesResolvePass::createLtxCalibrationTexture(RenderContext* pRenderContext) {
//	if(!mpLtxCalibrationTexture) { mpLtxCalibrationTexture = nullptr; } return; // We don't need it right now

	if (mpLtxCalibrationTexture) return;

	// Worst case scenario is 1024 pages per dimension

	mpLtxCalibrationTexture = Texture::create2D(pRenderContext->device(), 1024, 1024, ResourceFormat::R32Float, 1, Texture::kMaxPossible, nullptr, Texture::BindFlags::ShaderResource);
	if (!mpLtxCalibrationTexture) LLOG_ERR << "Error creating LTX calibration texture !!!";

	for(uint32_t mipLevel = 0; mipLevel < mpLtxCalibrationTexture->getMipCount(); mipLevel++) {
		uint32_t width = mpLtxCalibrationTexture->getWidth(mipLevel);
		uint32_t height = mpLtxCalibrationTexture->getHeight(mipLevel); 
	}
}


void TexturesResolvePass::setDefaultSampler() {
	if (mpSampler) return;

	Sampler::Desc desc;
	desc.setMaxAnisotropy(16); // Set 16x anisotropic filtering for improved min/max precision
	desc.setLodParams(-1000.0f, 1000.0f, -0.0f);
	desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
	desc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);

	mpSampler = Sampler::create(mpDevice, desc);

	desc.setReductionMode(Sampler::ReductionMode::Min);
	mpMinSampler = Sampler::create(mpDevice, desc);
	desc.setReductionMode(Sampler::ReductionMode::Max);
	mpMaxSampler = Sampler::create(mpDevice, desc);
}

TexturesResolvePass& TexturesResolvePass::setAsyncLoading(bool mode) {
	if(mLoadPagesAsync == mode) return *this;
	mLoadPagesAsync = mode;
	mDirty = true;
	return *this;
}

TexturesResolvePass& TexturesResolvePass::setRayReflectLimit(int limit) {
    uint32_t _limit = std::max(0u, std::min(10u, static_cast<uint32_t>(limit)));
    if(mRayReflectLimit == _limit) return *this;
    mRayReflectLimit = _limit;
    mDirty = true;
    return *this;
}

TexturesResolvePass& TexturesResolvePass::setRayRefractLimit(int limit) {
    uint32_t _limit = std::max(0u, std::min(10u, static_cast<uint32_t>(limit)));
    if(mRayRefractLimit == _limit) return *this;
    mRayRefractLimit = _limit;
    mDirty = true;
    return *this;
}

TexturesResolvePass& TexturesResolvePass::setRayDiffuseLimit(int limit) {
    uint32_t _limit = std::max(0u, std::min(10u, static_cast<uint32_t>(limit)));
    if(mRayDiffuseLimit == _limit) return *this;
    mRayDiffuseLimit = _limit;
    mDirty = true;
    return *this;
}