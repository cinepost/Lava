#include <algorithm>
#include <chrono>

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Utils/Image/TextureManager.h"

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

}  // namespace

void TexturesResolvePass::parseDictionary(const Dictionary& dict) {
	float3 a;
}

Dictionary TexturesResolvePass::getScriptingDictionary() {
	Dictionary d;
	return d;
}

TexturesResolvePass::SharedPtr TexturesResolvePass::create(RenderContext* pRenderContext, const Dictionary& dict) {
	auto pTexturesResolvePass = new TexturesResolvePass(pRenderContext->device(), dict);

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
	dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
	//dsDesc.setDepthWriteMask(false).setDepthEnabled(true).setDepthFunc(DepthStencilState::Func::Never);
	mpDsNoDepthWrite = DepthStencilState::create(dsDesc);
	mpState->setDepthStencilState(DepthStencilState::create(dsDesc));

	mpState->setProgram(mpProgram);

	parseDictionary(dict);
}

RenderPassReflection TexturesResolvePass::reflect(const CompileData& compileData) {
	RenderPassReflection reflector;

	reflector.addOutput(kOutput, "DebugOutput-buffer").format(mTileDataDebugFormat).texture2D(0, 0, 0);
	auto& depthField = reflector.addInputOutput(kDepth, "Depth-buffer. Should be pre-initialized or cleared before calling the pass")
		.bindFlags(Resource::BindFlags::DepthStencil).flags(RenderPassReflection::Field::Flags::Optional);;
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
	initDepth(pContext, renderData);

	const auto& pDebugData = renderData[kOutput]->asTexture();
	mpFbo->attachColorTarget(pDebugData, 0);

	mpState->setFbo(mpFbo);
	pContext->clearRtv(pDebugData->getRTV().get(), {0, 0, 0, 0});

	if (!mpScene)
		return;

	auto exec_started = std::chrono::high_resolution_clock::now();

	uint32_t totalPagesToUpdateCount = 0;
	uint32_t currPagesStartOffset = 0;
	uint32_t currTextureResolveID = 0; // texture id used to identify texture inside pass. always starts from 0.

	std::vector<MaterialResolveData> materialsResolveBuffer;
	std::map<uint32_t, Texture::SharedPtr> texturesMap; // maps real texture ID to textures
	std::map<uint32_t, VirtualTextureData> virtualTexturesDataMap; //

	uint32_t materialsCount = mpScene->getMaterialCount();

	for( uint32_t m_i = 0; m_i < materialsCount; m_i++ ) {
		auto pMaterial = mpScene->getMaterial(m_i);
		if(!pMaterial) continue;

		std::vector<Texture::SharedPtr> materialSparseTextures;

		for( const auto& slot: std::vector<TextureSlot>({TextureSlot::BaseColor, TextureSlot::Metallic, TextureSlot::Roughness, TextureSlot::Normal})) {
			auto pTexture = pMaterial->getTexture(slot);
			if(pTexture && (pTexture->isSparse() || pTexture->isUDIMTexture())) {
				if(pTexture->isUDIMTexture()) {
					for(const auto& tileInfo: pTexture->getUDIMTileInfos()) {
						if(tileInfo.pTileTexture && tileInfo.pTileTexture->isSparse()) {
							materialSparseTextures.push_back(tileInfo.pTileTexture);
							LLOG_WRN << "!";
						}
					}
				} else {
					materialSparseTextures.push_back(pTexture);
				}
			}
		}

		MaterialResolveData materialResolveData = {};

		size_t virtualTexturesCount = std::min((size_t)MAX_VTEX_PER_MATERIAL_COUNT, materialSparseTextures.size());
		materialResolveData.virtualTexturesCount = virtualTexturesCount;
		
		// pre-fill some data
		for( size_t t_i = 0; t_i < static_cast<size_t>(MAX_VTEX_PER_MATERIAL_COUNT); t_i++)
			materialResolveData.virtualTextures[t_i].empty = true;

		// fill data for active(used) textures
		for( size_t t_i = 0; t_i < virtualTexturesCount; t_i++) {
			auto &pTexture = materialSparseTextures[t_i];
			uint32_t textureID = pTexture->id();
				
			auto &textureData = materialResolveData.virtualTextures[t_i];

			// Check if this sparse texture data not stored for resolving
			if (virtualTexturesDataMap.find(textureID) == virtualTexturesDataMap.end() ) {
				// Fill vitrual texture data
				textureData.empty = false;
				textureData.textureID = textureID;
				textureData.textureResolveID = currTextureResolveID;
				textureData.width = pTexture->getWidth();
				textureData.height = pTexture->getHeight();
				textureData.mipLevelsCount = pTexture->getMipCount();
				textureData.mipTailStart = pTexture->getMipTailStart();
				textureData.pagesStartOffset = currPagesStartOffset;

				auto const& pageRes = pTexture->sparseDataPageRes();
				textureData.pageSizeW = pageRes.x;
				textureData.pageSizeH = pageRes.y;
				textureData.pageSizeD = pageRes.z;
				
				auto const& mipBases = pTexture->getMipBases();

				memcpy(&textureData.mipBases, mipBases.data(), mipBases.size() * sizeof(uint32_t));

				currTextureResolveID++;
				currPagesStartOffset += pTexture->sparseDataPagesCount();
				virtualTexturesDataMap[textureID] = textureData; 
				texturesMap[textureID] = pTexture;
			
				// --- debug info 
#ifdef _DEBUG
				printf("Texture id: %u pages offset: %u width: %u height: %u\n", textureData.textureID, textureData.pagesStartOffset, textureData.width, textureData.height);
				printf("Texture id: %u mip levels: %u tail start: %u\n", textureData.textureID, textureData.mipLevelsCount, textureData.mipTailStart);

				std::cout << "Mip bases : \n";
				for( uint i = 0; i < 16; i++) std::cout << textureData.mipBases[i] << " ";
				std::cout << "\n";
#endif
			} else {
				// Virtual texture data cached in map, reuse it
				textureData = virtualTexturesDataMap[textureID];
			}
		}

		materialsResolveBuffer.push_back(materialResolveData);
	}

	totalPagesToUpdateCount = currPagesStartOffset;
	totalPagesToUpdateCount += 16;

	auto pDataToResolveBuffer = Buffer::createStructured(mpDevice, sizeof(MaterialResolveData), materialsCount, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialsResolveBuffer.data(), true);
	mpVars->setBuffer("materialsResolveData", pDataToResolveBuffer);

	uint32_t resolvedTexturesCount = currTextureResolveID;

#ifdef _DEBUG
	printf("Total pages to update for %u textures is %u\n", resolvedTexturesCount, totalPagesToUpdateCount);
#endif

	std::vector<int8_t> pagesInitDataBuffer(totalPagesToUpdateCount, 0);
	auto pPagesBuffer = Buffer::create(mpDevice, totalPagesToUpdateCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, pagesInitDataBuffer.data());
	mpVars->setBuffer("resolvedPagesBuff", pPagesBuffer);

	mpVars["PerFrameCB"]["gRenderTargetDim"] = float2(mpFbo->getWidth(), mpFbo->getHeight());
	mpVars["PerFrameCB"]["materialsToResolveCount"] = materialsResolveBuffer.size();
	mpVars["PerFrameCB"]["resolvedTexturesCount"] = resolvedTexturesCount;

	mpVars["mipCalibrationTexture"] = mpMipCalibrationTexture;
	mpVars["ltxCalibrationTexture"] = mpLtxCalibrationTexture;

	setDefaultSampler();

#ifdef _DEBUG
	printf("%u textures needs to be resolved\n", resolvedTexturesCount);
#endif

	mpScene->rasterize(pContext, mpState.get(), mpVars.get(), RasterizerState::CullMode::Back);
	pContext->flush(true);

	auto pTextureManager = mpScene->materialSystem()->textureManager();

	// Test resolved data
	const int8_t* pOutPagesData = reinterpret_cast<const int8_t*>(pPagesBuffer->map(Buffer::MapType::Read));

	// Load texture pages
	auto started = std::chrono::high_resolution_clock::now();

	uint32_t pagesStartOffset = 0;
	for (auto const& [textureID, pTexture] :texturesMap) {
		uint32_t texturePagesCount = pTexture->sparseDataPagesCount();
		LLOG_DBG << "Analyzing" << std::to_string(texturePagesCount) << " pages for texture " << std::to_string(textureID);

		std::vector<uint32_t> pageIDs;

		// index 'i' is a page index relative to the texture. starts with 0
		for(uint32_t i = 0; i < texturePagesCount; i++) {
			if (pOutPagesData[i + pagesStartOffset] != 0) {
				pageIDs.push_back(i + pagesStartOffset);
			}
		}

#ifdef _DEBUG
		for(uint32_t i = 0; i < texturePagesCount; i++) {
			std::cout << " " << std::to_string(i);
		}
		std::cout << std::endl;
#endif

		LLOG_DBG << std::to_string(pageIDs.size()) << " pages needs to be loaded for texture " << std::to_string(textureID);
		
		pTextureManager->loadPages(pTexture, pageIDs); 

		pagesStartOffset += texturePagesCount;
	}

	auto done = std::chrono::high_resolution_clock::now();
	LLOG_DBG << "Pages loading done in: " << std::chrono::duration_cast<std::chrono::milliseconds>(done-started).count() << " ms.";
	LLOG_DBG << "TexturesResolvePass::execute done in: " << std::chrono::duration_cast<std::chrono::milliseconds>(done-exec_started).count() << " ms.";

	//pContext->flush(true);

	LLOG_DBG << "TexturesResolvePass::execute done";
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

	mpMipCalibrationTexture = Texture::create2D(pRenderContext->device(), 128, 128, ResourceFormat::R8Unorm, 1, Texture::kMaxPossible, nullptr, Texture::BindFlags::ShaderResource);
	if (!mpMipCalibrationTexture) LLOG_ERR << "Error creating MIP calibration texture !!!";

	for(uint32_t mipLevel = 0; mipLevel < mpMipCalibrationTexture->getMipCount(); mipLevel++) {
		uint32_t width = mpMipCalibrationTexture->getWidth(mipLevel);
		uint32_t height = mpMipCalibrationTexture->getHeight(mipLevel); 
	
		// upload mip level data
		std::vector<unsigned char> initData(width*height, (unsigned char)mipLevel);
		uint32_t subresource = mpMipCalibrationTexture->getSubresourceIndex(0, mipLevel);
		pRenderContext->updateSubresourceData(mpMipCalibrationTexture.get(), subresource, initData.data());
	}
}

void TexturesResolvePass::createLtxCalibrationTexture(RenderContext* pRenderContext) {
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
  desc.setMaxAnisotropy(8);//(16);
  desc.setLodParams(0.0f, 1000.0f, -0.0f);
  desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
  desc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);

	mpSampler = Sampler::create(mpDevice, desc);
  mpVars["gSampler"] = mpSampler;
}