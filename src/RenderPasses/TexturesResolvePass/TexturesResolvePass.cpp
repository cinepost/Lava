#include <algorithm>
#include <chrono>

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

 	if (1 == 2) {
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

static void calculateVirtualTextureData(const Texture::SharedPtr& pTexture, uint textureResolveID, uint32_t pagesStartOffset, const TextureHandle& textureHandle, VirtualTextureData& vtexData) {
	assert(pTexture);

	// Fill vitrual texture data
	vtexData.empty = true;

	if(!pTexture) {
		LLOG_WRN << "Unable to calculate virtual texture data. pTexture is NULL.";
		return;
	}


	vtexData.textureID = pTexture->id();
	vtexData.textureHandle = textureHandle;
	vtexData.textureResolveID = textureResolveID;

	if(!pTexture->isUDIMTexture()) {
		vtexData.width = pTexture->getWidth(0);
		vtexData.height = pTexture->getHeight(0);
		vtexData.mipLevelsCount = pTexture->getMipCount();
		vtexData.mipTailStart = pTexture->getMipTailStart();
		vtexData.pagesStartOffset = pagesStartOffset;

		auto const& pageRes = pTexture->sparseDataPageRes();
		vtexData.pageSizeW = pageRes.x;
		vtexData.pageSizeH = pageRes.y;
		vtexData.pageSizeD = pageRes.z;
				
		auto const& mipBases = pTexture->getMipBases();

		memcpy(&vtexData.mipBases, mipBases.data(), mipBases.size() * sizeof(uint32_t));
	}
	vtexData.empty = false;
}

void TexturesResolvePass::execute(RenderContext* pContext, const RenderData& renderData) {
	if (!mpScene)
		return;

	initDepth(pContext, renderData);

	const auto& pDebugData = renderData[kOutput]->asTexture();
	mpFbo->attachColorTarget(pDebugData, 0);

	mpState->setFbo(mpFbo);
	pContext->clearRtv(pDebugData->getRTV().get(), {0, 0, 0, 0});

	auto exec_started = std::chrono::high_resolution_clock::now();

	auto pTextureManager = mpScene->materialSystem()->textureManager();

	createMipCalibrationTexture(pContext);

	uint32_t totalPagesToUpdateCount = 0;
	uint32_t currPagesStartOffset = 0;
	uint32_t currTextureResolveID = 0; // texture id used to identify texture inside pass. always starts from 0.

	std::vector<MaterialResolveData> materialsResolveBuffer;
	std::map<uint32_t, Texture::SharedPtr> texturesMap; // maps real texture ID to textures
	std::map<uint32_t, uint32_t> virtualTexturesDataIdMap; //
	std::map<uint32_t, Texture::SharedPtr> udimTextureTiles;

	std::vector<uint32_t> texturePagesStartMap;
	texturePagesStartMap.reserve(1024);

	uint32_t materialsCount = mpScene->getMaterialCount();

	std::vector<VirtualTextureData> vtexDataBuffer;
	vtexDataBuffer.reserve(1024);

	std::vector<uint32_t> vtexDataIdMapBuffer;
	vtexDataIdMapBuffer.reserve(1024);

	for( uint32_t m_i = 0; m_i < materialsCount; m_i++ ) {
		const auto pMaterial = mpScene->getMaterial(m_i)->toBasicMaterial();
		if(!pMaterial) continue;

		std::vector<std::pair<TextureSlot, Texture::SharedPtr>> materialSparseTextures;

		for( const auto& slot: std::vector<TextureSlot>({TextureSlot::BaseColor, TextureSlot::Metallic, TextureSlot::Roughness, TextureSlot::Normal, TextureSlot::Emissive})) {
			auto const& pTexture = pMaterial->getTexture(slot);
			if(pTexture && (pTexture->isSparse() || pTexture->isUDIMTexture())) {
				if(pTexture->isUDIMTexture()) {
					
					for(const auto& tileInfo: pTexture->getUDIMTileInfos()) {
						if(tileInfo.pTileTexture && tileInfo.pTileTexture->isSparse()) {
							auto textureID = tileInfo.pTileTexture->id();
							if (udimTextureTiles.find(textureID) == udimTextureTiles.end() ) {
								udimTextureTiles[textureID] = tileInfo.pTileTexture;
							}
						}
					}
				}
				materialSparseTextures.push_back({slot, pTexture});
			}
		}

		MaterialResolveData materialResolveData = {};

		size_t virtualTexturesCount = std::min((size_t)MAX_VTEX_PER_MATERIAL_COUNT, materialSparseTextures.size());
		materialResolveData.virtualTexturesCount = virtualTexturesCount;
		
		// pre-fill some data
		for( size_t t_i = 0; t_i < static_cast<size_t>(MAX_VTEX_PER_MATERIAL_COUNT); t_i++)
			materialResolveData.virtualTextureDataIDs[t_i] = 0; // It's safe as we should never have material texture with id 0!

		// fill data for active(used) textures
		for( size_t materialTextureID = 0; materialTextureID < virtualTexturesCount; materialTextureID++) {
			const auto &pTexture = materialSparseTextures[materialTextureID].second;
			assert(pTexture->id() != 0 && "There should be no material textures with id 0 !");

			auto slot = materialSparseTextures[materialTextureID].first;
			uint32_t textureID = pTexture->id(); // Core internal resource id. Has nothing to do with texture id inside MaterialSystem

			uint32_t vtexDataID;
			// Check if this sparse texture data not stored for resolving
			if (virtualTexturesDataIdMap.find(textureID) == virtualTexturesDataIdMap.end() ) {
				vtexDataID = vtexDataBuffer.size();
				materialResolveData.virtualTextureDataIDs[materialTextureID] = vtexDataID;
				vtexDataBuffer.push_back({});
				auto &vtexData = vtexDataBuffer.back();


				auto textureHandle = pMaterial->getTextureHandle(slot);
				calculateVirtualTextureData(pTexture, currTextureResolveID, currPagesStartOffset, textureHandle, vtexDataBuffer.back());

				if(!pTexture->isUDIMTexture()) {
					if(texturePagesStartMap.size() <= textureID) {
						texturePagesStartMap.resize(textureID + 1);
					}
					texturePagesStartMap[textureID] = currPagesStartOffset;

					currTextureResolveID++;
					currPagesStartOffset += pTexture->sparseDataPagesCount();
					texturesMap[textureID] = pTexture;
				}

				virtualTexturesDataIdMap[textureID] = vtexDataBuffer.size() - 1; 
			
			} else {
				// Virtual texture data cached in map, reuse it
				vtexDataID = virtualTexturesDataIdMap[textureID];
				materialResolveData.virtualTextureDataIDs[materialTextureID] = vtexDataID;
			}

			auto materialSystemTextureID = vtexDataBuffer[vtexDataID].textureHandle.getTextureID();
			if(vtexDataIdMapBuffer.size() <= materialSystemTextureID) {
				vtexDataIdMapBuffer.resize(materialSystemTextureID + 1);
			}
			vtexDataIdMapBuffer[materialSystemTextureID] = vtexDataID;

		}

		materialsResolveBuffer.push_back(materialResolveData);
	}

if (1 == 1) {
	// Now add texture tiles information
	for (const auto& pair: udimTextureTiles) {
		const auto &pTexture = pair.second;
		assert(pTexture);

		uint32_t textureID = pTexture->id();

		if(texturesMap.find(textureID) != texturesMap.end()) {
			continue;
		}

		TextureManager::TextureHandle textureManagerHandle = {};

		if(!pTextureManager->getTextureHandle(pTexture.get(), textureManagerHandle)) {
			LLOG_ERR << "Error getting texture handle from texture manager for texture " << pTexture->getSourceFilename();
			continue;
		}	

		uint32_t materialSystemTextureID = textureManagerHandle.getID();

////////////////////////////////////

    // Material texture handle.
	TextureHandle textureHandle;
    if(!pTexture->isSolid()) {
        textureHandle.setMode(TextureHandle::Mode::Texture);
    } else {
    	textureHandle.setMode(TextureHandle::Mode::Uniform);
    }
    textureHandle.setTextureID(materialSystemTextureID);
        
////////////////////////////////////

		uint32_t vtexDataID = vtexDataBuffer.size();
		vtexDataBuffer.push_back({});
		auto &vtexData = vtexDataBuffer.back();

		LLOG_DBG << "calculating virtual texture data for UDIM tile materialSystem textureHandleID " << std::to_string(materialSystemTextureID);
		calculateVirtualTextureData(pTexture, currTextureResolveID, currPagesStartOffset, textureHandle, vtexData);

		if(texturePagesStartMap.size() <= textureID) {
			texturePagesStartMap.resize(textureID + 1);
		}
		texturePagesStartMap[textureID] = currPagesStartOffset;

		currTextureResolveID++;
		currPagesStartOffset += pTexture->sparseDataPagesCount();

		if(vtexDataIdMapBuffer.size() <= materialSystemTextureID) {
			vtexDataIdMapBuffer.resize(materialSystemTextureID + 1);
		}
		vtexDataIdMapBuffer[materialSystemTextureID] = vtexDataID;

		texturesMap[textureID] = pTexture;
	}
}

	totalPagesToUpdateCount = currPagesStartOffset;
	totalPagesToUpdateCount += 16;

	auto pVtexDataBuffer = Buffer::createStructured(mpDevice, sizeof(VirtualTextureData), vtexDataBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, vtexDataBuffer.data());
	mpVars->setBuffer("virtualTextureDataBuffer", pVtexDataBuffer);

	auto pDataToResolveBuffer = Buffer::createStructured(mpDevice, sizeof(MaterialResolveData), materialsCount, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, materialsResolveBuffer.data(), true);
	mpVars->setBuffer("materialsResolveDataBuffer", pDataToResolveBuffer);

	auto pTextureIdToVtexDataIdBuffer = Buffer::createTyped<uint32_t>(mpDevice, vtexDataIdMapBuffer.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, vtexDataIdMapBuffer.data());
	mpVars->setBuffer("textureIdToVtexDataIdBuffer", pTextureIdToVtexDataIdBuffer);

	uint32_t resolvedTexturesCount = currTextureResolveID;

#ifdef _DEBUG
	printf("Total pages to update for %u textures is %u\n", resolvedTexturesCount, totalPagesToUpdateCount);
#endif

	std::vector<int8_t> pagesInitDataBuffer(totalPagesToUpdateCount, 0);
	auto pPagesBuffer = Buffer::create(mpDevice, totalPagesToUpdateCount, Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::Read, pagesInitDataBuffer.data());
	mpVars->setBuffer("resolvedPagesBuff", pPagesBuffer);

	mpVars["PerFrameCB"]["gRenderTargetDim"] = float2(mpFbo->getWidth(), mpFbo->getHeight());
	mpVars["PerFrameCB"]["materialsToResolveCount"] = materialsResolveBuffer.size();
	mpVars["PerFrameCB"]["resolvedTexturesCount"] = resolvedTexturesCount;
	mpVars["PerFrameCB"]["numberOfMipCalibrationTextures"] = (int32_t)mMipCalibrationTextures.size();

	mpVars["mipCalibrationTexture"] = mpMipCalibrationTexture;
	mpVars["ltxCalibrationTexture"] = mpLtxCalibrationTexture;

	for(uint32_t i = 0; i < mMipCalibrationTextures.size(); i++)
		mpVars["mipCalibrationTextures"][i] = mMipCalibrationTextures[i];

	setDefaultSampler();
	mpVars["gCalibrationSampler"] = mpSampler;
	mpVars["gCalibrationMinSampler"] = mpMinSampler;
	mpVars["gCalibrationMaxSampler"] = mpMaxSampler;

#ifdef _DEBUG
	printf("%u textures needs to be resolved\n", resolvedTexturesCount);
#endif

	mpScene->rasterize(pContext, mpState.get(), mpVars.get(), RasterizerState::CullMode::None);
	pContext->flush(true);

	// Test resolved data
	const int8_t* pOutPagesData = reinterpret_cast<const int8_t*>(pPagesBuffer->map(Buffer::MapType::Read));

	// Load texture pages
	auto started = std::chrono::high_resolution_clock::now();

	uint32_t pagesStartOffset = 0;
	for (auto const& [textureID, pTexture] :texturesMap) {
		pagesStartOffset = texturePagesStartMap[textureID];
		uint32_t texturePagesCount = pTexture->sparseDataPagesCount();
		LLOG_DBG << "Analyzing " << std::to_string(texturePagesCount) << " pages for texture: " << pTexture->getSourceFilename();

		std::vector<uint32_t> pageIDs;

		// index 'i' is a page index relative to the texture. starts with 0
		for(uint32_t i = 0; i < texturePagesCount; i++) {
			if (pOutPagesData[i + pagesStartOffset] != 0) {
				pageIDs.push_back(i + pagesStartOffset);
			}
		}

		LLOG_DBG << std::to_string(pageIDs.size()) << " pages need to be loaded for texture " << std::to_string(textureID);
		
		// It's important to sort page ids for later fseek() & fread() calls
		std::sort(pageIDs.begin(), pageIDs.end());
		
		if(mLoadPagesAsync) {
			// Critical!!! Call loadPagesAsync once per texture !!!
			pTextureManager->loadPagesAsync(pTexture, pageIDs); 
		} else {
			pTextureManager->loadPages(pTexture, pageIDs); 
		}
	}

	// In async mode we have to call updateSparseBindInfo on TextureManager as it triggers wait() function on pages loading multi-future
	if(mLoadPagesAsync) pTextureManager->updateSparseBindInfo();

	auto done = std::chrono::high_resolution_clock::now();
	LLOG_DBG << "Pages loading done in: " << std::chrono::duration_cast<std::chrono::milliseconds>(done-started).count() << " ms.";
	LLOG_INF << "TexturesResolvePass done in: " << std::setprecision(6) 
			 << (.001f * (float)std::chrono::duration_cast<std::chrono::milliseconds>(done-exec_started).count()) << " s";
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

	// Now. Let's make multiple calibration textures. This time only one particlar mip level contains non-zero data
	size_t buff_size = mpMipCalibrationTexture->getWidth() * mpMipCalibrationTexture->getHeight();
	std::vector<float> zero_buff(buff_size, 0.0f);
	std::vector<float> full_buff(buff_size, 1.0f);
	mMipCalibrationTextures.resize(mpMipCalibrationTexture->getMipCount());

	for(uint32_t i = 0; i < mpMipCalibrationTexture->getMipCount(); i++) {
		mMipCalibrationTextures[i] = nullptr;
		auto pMipCalibrationTexture = Texture::create2D(pRenderContext->device(), mpMipCalibrationTexture->getWidth(),  mpMipCalibrationTexture->getHeight(), ResourceFormat::R32Float, 1, Texture::kMaxPossible, nullptr, Texture::BindFlags::ShaderResource);
		if (!pMipCalibrationTexture) {
			LLOG_ERR << "Error creating calibraiotn texture for mip level " << std::to_string(i) << " !!!";
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