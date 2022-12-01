#include <chrono>

#include "renderer.h"

#include "Falcor/Utils/Threading.h"
#include "Falcor/RenderGraph/RenderPassLibrary.h"

#include "Falcor/Utils/SampleGenerators/StratifiedSamplePattern.h"
#include "Falcor/Utils/SampleGenerators/DxSamplePattern.h"
#include "Falcor/Utils/SampleGenerators/HaltonSamplePattern.h"

#include "Falcor/Utils/Timing/Profiler.h"
#include "Falcor/Utils/Scripting/Scripting.h"
#include "Falcor/Utils/Scripting/Dictionary.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"
#include "Falcor/Utils/ConfigStore.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/RenderGraph/RenderPassStandardFlags.h"
#include "Falcor/Scene/Lights/EnvMap.h"
#include "Falcor/Scene/MaterialX/MaterialX.h"

#include "RenderPasses/ForwardLightingPass/ForwardLightingPass.h"
#include "RenderPasses/DeferredLightingPass/DeferredLightingPass.h"
#include "RenderPasses/DeferredLightingCachedPass/DeferredLightingCachedPass.h"
#include "RenderPasses/CryptomattePass/CryptomattePass.h"

#include "lava_utils_lib/logging.h"

//#define USE_FORWARD_LIGHTING_PASS

namespace Falcor {  
	IFramework* gpFramework = nullptr;  // TODO: probably it's safe to remove now...
}

namespace lava {

static bool isInVector(const std::vector<std::string>& strVec, const std::string& str) {
	return std::find(strVec.begin(), strVec.end(), str) != strVec.end();
}


Renderer::SharedPtr Renderer::create(Device::SharedPtr pDevice) {
	assert(pDevice);
	return SharedPtr(new Renderer(pDevice));
}


Renderer::Renderer(Device::SharedPtr pDevice): mpDevice(pDevice), mIfaceAquired(false), mpClock(nullptr), mpFrameRate(nullptr), mActiveGraph(0), mInited(false), mGlobalDataInited(false) {
	mMainAOVPlaneExist = false;
}

bool Renderer::init(const Config& config) {
	if(mInited) return true;

	mCurrentConfig = config;

	Falcor::OSServices::start();

#ifdef SCRIPTING
	Falcor::Scripting::start();
	Falcor::ScriptBindings::registerBinding(Renderer::registerBindings);
#endif

	Falcor::Threading::start();

	auto sceneBuilderFlags = Falcor::SceneBuilder::Flags::DontMergeMeshes;
	if( mCurrentConfig.tangentGenerationMode != "mikkt" ) {
		sceneBuilderFlags |= SceneBuilder::Flags::UseOriginalTangentSpace;
	}

	if (mCurrentConfig.useRaytracing) {
		sceneBuilderFlags |= SceneBuilder::Flags::UseRaytracing;
	}

	//sceneBuilderFlags |= SceneBuilder::Flags::Force32BitIndices;
	sceneBuilderFlags |= SceneBuilder::Flags::DontOptimizeMaterials;
	//sceneBuilderFlags |= SceneBuilder::Flags::DontMergeMaterials;

	sceneBuilderFlags != SceneBuilder::Flags::AssumeLinearSpaceTextures;

	mpSceneBuilder = lava::SceneBuilder::create(mpDevice, sceneBuilderFlags);
	mpCamera = Falcor::Camera::create();
	mpCamera->setName("main");
	mpSceneBuilder->addCamera(mpCamera);

	mInited = true;
	return true;
}

Renderer::~Renderer() {
	if(!mInited)
		return;

	mpRenderGraph = nullptr;

	Falcor::Threading::shutdown();

	mpDevice->flushAndSync();

	mGraphs.clear();

	mpSceneBuilder = nullptr;
	
	mpSampler = nullptr;

	Falcor::Scripting::shutdown();
	Falcor::RenderPassLibrary::instance().shutdown();

	mpTargetFBO.reset();

	//mpDevice->cleanup();

	//mpDevice.reset();

	Falcor::OSServices::stop();
}

AOVPlane::SharedPtr Renderer::addAOVPlane(const AOVPlaneInfo& info) {
	LLOG_DBG << "Adding aov " << info.name;
	if (mAOVPlanes.find(info.name) != mAOVPlanes.end()) {
		LLOG_ERR << "AOV plane named \"" << info.name << "\" already exist !";
		return nullptr;
	}

	auto pAOVPlane = AOVPlane::create(info);
	if (!pAOVPlane) {
		LLOG_ERR << "Error creating AOV plane \"" << pAOVPlane->name() << "\" !!!";
		return nullptr;
	}

	mAOVPlanes[pAOVPlane->name()] = pAOVPlane;
	if (info.name == AOVBuiltinName::MAIN) mMainAOVPlaneExist = true; 

	return pAOVPlane;
}

AOVPlane::SharedPtr Renderer::getAOVPlane(const AOVName& name) {
	LLOG_DBG << "Getting aov " << name;
	if (mAOVPlanes.find(name) == mAOVPlanes.end()) {
		LLOG_ERR << "No AOV plane named \"" << name << "\" exist !";
		return nullptr;
	}

	return mAOVPlanes[name];
}

void Renderer::createRenderGraph(const FrameInfo& frame_info) {
	if (mpRenderGraph) 
		return; 

	assert(mpDevice);

	auto renderRegionDims = frame_info.renderRegionDims();
	auto pRenderContext = mpDevice->getRenderContext();
	auto pScene = mpSceneBuilder->getScene();

	assert(pScene);

	auto pMainAOV = getAOVPlane(AOVBuiltinName::MAIN);
	assert(pMainAOV);
	
	auto const& confgStore = Falcor::ConfigStore::instance();
	bool vtoff = confgStore.get<bool>("vtoff", true);

	Falcor::uint2 imageSize = {renderRegionDims[0], renderRegionDims[1]};

	LLOG_DBG << "createRenderGraph frame dimensions: " << imageSize[0] << " " << imageSize[1];

	//// EnvMapSampler stuff
	Texture::SharedPtr pEnvTexture = nullptr;
	if (pEnvTexture) {
		auto pEnvMap = Falcor::EnvMap::create(mpDevice, pEnvTexture);
		pScene->setEnvMap(pEnvMap);
	}

	auto pEnvMap = pScene->getEnvMap();

	
	// Rasterizer state
	RasterizerState::CullMode cullMode;
	Falcor::RasterizerState::Desc rsDesc;
	const std::string& cull_mode = confgStore.get<std::string>("cull_mode", "none");
	if (cull_mode == "back") {
		cullMode = RasterizerState::CullMode::Back;
		rsDesc.setCullMode(RasterizerState::CullMode::Back);
	} else if (cull_mode == "front") {
		cullMode = RasterizerState::CullMode::Front;
		rsDesc.setCullMode(RasterizerState::CullMode::Front);
	} else {
		cullMode = RasterizerState::CullMode::None;
		rsDesc.setCullMode(RasterizerState::CullMode::None);
	}

	rsDesc.setFillMode(RasterizerState::FillMode::Solid);

	// Virtual textures resolve render graph
	if(pScene->materialSystem()->hasSparseTextures()) {
		auto vtexResolveChannelOutputFormat = ResourceFormat::RGBA8Unorm;
		mpTexturesResolvePassGraph = RenderGraph::create(mpDevice, imageSize, vtexResolveChannelOutputFormat, "VirtualTexturesGraph");

		// Depth pre-pass
		Falcor::Dictionary depthPrePassDictionary(mRenderPassesDictionary);
		depthPrePassDictionary["disableAlphaTest"] = true; // no virtual textures loaded at this point

		auto pDepthPrePass = DepthPass::create(pRenderContext, depthPrePassDictionary);
		pDepthPrePass->setDepthBufferFormat(ResourceFormat::D32Float);
		pDepthPrePass->setScene(pRenderContext, pScene);
		pDepthPrePass->setCullMode(cullMode);
		mpTexturesResolvePassGraph->addPass(pDepthPrePass, "DepthPrePass");

		// Vitrual textures resolve pass
		mpTexturesResolvePass = TexturesResolvePass::create(pRenderContext);
		mpTexturesResolvePass->setRasterizerState(Falcor::RasterizerState::create(rsDesc));
		mpTexturesResolvePass->setScene(pRenderContext, pScene);

		mpTexturesResolvePassGraph->addPass(mpTexturesResolvePass, "SparseTexturesResolvePrePass");
		mpTexturesResolvePassGraph->markOutput("SparseTexturesResolvePrePass.output");

		mpTexturesResolvePassGraph->addEdge("DepthPrePass.depth", "SparseTexturesResolvePrePass.depth");
	} else {
		mpTexturesResolvePassGraph = nullptr;
		mpTexturesResolvePass = nullptr;
	}

	// Main render graph
	mpRenderGraph = RenderGraph::create(mpDevice, imageSize, ResourceFormat::RGBA32Float, "MainImageRenderGraph");
	
	// Depth pass
	Falcor::Dictionary depthPassDictionary(mRenderPassesDictionary);
	depthPassDictionary["disableAlphaTest"] = false; // take texture alpha into account

	mpDepthPass = DepthPass::create(pRenderContext, depthPassDictionary);
	mpDepthPass->setDepthBufferFormat(ResourceFormat::D32Float);
	mpDepthPass->setScene(pRenderContext, pScene);
	mpDepthPass->setCullMode(cullMode);
	mpRenderGraph->addPass(mpDepthPass, "DepthPass");

	// Forward lighting
	Falcor::Dictionary lightingPassDictionary(mRenderPassesDictionary);

	lightingPassDictionary["frameSampleCount"] = frame_info.imageSamples;

#ifdef USE_FORWARD_LIGHTING_PASS

	auto pForwardLightingPass = ForwardLightingPass::create(pRenderContext, lightingPassDictionary);
	pForwardLightingPass->setRasterizerState(Falcor::RasterizerState::create(rsDesc));
	pForwardLightingPass->setScene(pRenderContext, pScene);
	pForwardLightingPass->setColorFormat(ResourceFormat::RGBA16Float);

	mpRenderGraph->addPass(pForwardLightingPass, "ShadingPass");

#else

	auto pDeferredLightingPass = DeferredLightingPass::create(pRenderContext, lightingPassDictionary);
	//auto pDeferredLightingPass = DeferredLightingCachedPass::create(pRenderContext, lightingPassDictionary);
	pDeferredLightingPass->setScene(pRenderContext, pScene);
	
	mpRenderGraph->addPass(pDeferredLightingPass, "ShadingPass");

#endif


	// VBuffer
	Falcor::Dictionary vbufferPassDictionary(mRenderPassesDictionary);
	auto pVBufferPass = VBufferRaster::create(pRenderContext, vbufferPassDictionary);
	pVBufferPass->setScene(pRenderContext, pScene);
	pVBufferPass->setCullMode(cullMode);

	mpRenderGraph->addPass(pVBufferPass, "VBufferPass");
	mpRenderGraph->addEdge("DepthPass.depth", "VBufferPass.depth");

	// RTXDIPass
	//Falcor::Dictionary rtxdiPassDictionary(mRenderPassesDictionary);
	//auto pRTXDIPass = RTXDIPass::create(pRenderContext, rtxdiPassDictionary);
	//pRTXDIPass->setScene(pRenderContext, pScene);
	//mpRenderGraph->addPass(pRTXDIPass, "RTXDIPass");

	// SkyBox
	mpSkyBoxPass = SkyBox::create(pRenderContext);

	// TODO: handle transparency    
	mpSkyBoxPass->setOpacity(1.0f);

	mpSkyBoxPass->setScene(pRenderContext, pScene);
	mpRenderGraph->addPass(mpSkyBoxPass, "SkyBoxPass");
	
	//mpRenderGraph->addEdge("VBufferPass.vbuffer", "RTXDIPass.vbuffer");
	mpRenderGraph->addEdge("DepthPass.depth", "SkyBoxPass.depth");

#ifdef USE_FORWARD_LIGHTING_PASS
	// Forward lighting pass
	mpRenderGraph->addEdge("DepthPass.depth", "ShadingPass.depth");
	mpRenderGraph->addEdge("SkyBoxPass.target", "ShadingPass.color");
	
#else
	// Deferred lighting pass
	mpRenderGraph->addEdge("DepthPass.depth", "ShadingPass.depth");
	mpRenderGraph->addEdge("VBufferPass.vbuffer", "ShadingPass.vbuffer");
	mpRenderGraph->addEdge("VBufferPass.texGrads", "ShadingPass.texGrads");
	mpRenderGraph->addEdge("SkyBoxPass.target", "ShadingPass.color");

#endif

	// Optional cryptomatte pass
	bool createCryptomattePass = false;
	
	for (const auto &entry: mAOVPlanes) {
		auto &pPlane = entry.second;
		switch(pPlane->name()) {
			case AOVBuiltinName::CRYPTOMATTE_MAT:
			case AOVBuiltinName::CRYPTOMATTE_OBJ:
				createCryptomattePass = true;
				break;
			default:
				break;
		}
	}

	if (createCryptomattePass) {
		auto pCryptomattePass = CryptomattePass::create(pRenderContext, {});
		pCryptomattePass->setScene(pRenderContext, pScene);

		mpRenderGraph->addPass(pCryptomattePass, "CryptomattePass");
		mpRenderGraph->addEdge("VBufferPass.vbuffer", "CryptomattePass.vbuffer");
	}


	// Create MAIN (beauty) plane accumulation pass and bind with render graph output
	pMainAOV->createAccumulationPass(pRenderContext, mpRenderGraph);
	mpRenderGraph->addEdge("ShadingPass.color", pMainAOV->accumulationPassInputName());
	//mpRenderGraph->addEdge("RTXDIPass.color", pMainAOV->accumulationPassInputName());

	// Create and bind additional AOV planes
	for (const auto &entry: mAOVPlanes) {
		auto &pPlane = entry.second;
		switch(pPlane->name()) {
			case AOVBuiltinName::DEPTH:
				{
					if(pPlane->createAccumulationPass(pRenderContext, mpRenderGraph)) {
						pPlane->setOutputFormat(ResourceFormat::R32Float);
						mpRenderGraph->addEdge("DepthPass.depth", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::POSITION:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						mpRenderGraph->addEdge("ShadingPass.posW", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::NORMAL:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						//pAccPass->setOutputFormat(ResourceFormat::RGBA16Float);
						mpRenderGraph->addEdge("ShadingPass.normals", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::SHADOW:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						//pAccPass->setOutputFormat(ResourceFormat::RGBA16Float);
						mpRenderGraph->addEdge("ShadingPass.shadows", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::ALBEDO:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						//pAccPass->setOutputFormat(ResourceFormat::RGBA16Float);
						mpRenderGraph->addEdge("ShadingPass.albedo", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::OCCLUSION:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						mpRenderGraph->addEdge("ShadingPass.occlusion", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::Prim_Id:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						//pAccPass->setOutputFormat(ResourceFormat::RGBA16Float);
						mpRenderGraph->addEdge("ShadingPass.prim_id", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::Op_Id:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						//pAccPass->setOutputFormat(ResourceFormat::RGBA16Float);
						mpRenderGraph->addEdge("ShadingPass.op_id", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::CRYPTOMATTE_MAT:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						//pAccPass->setOutputFormat(ResourceFormat::RGBA16Float);
						mpRenderGraph->addEdge("CryptomattePass.material_color", pPlane->accumulationPassInputName());
					}
				}
				break;
			case AOVBuiltinName::CRYPTOMATTE_OBJ:
				{
					auto pAccPass = pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
					if(pAccPass) {
						//pAccPass->setOutputFormat(ResourceFormat::RGBA16Float);
						mpRenderGraph->addEdge("CryptomattePass.object_color", pPlane->accumulationPassInputName());
					}
				}
				break;
			default:
				break;
		}
	}
	
	// Compile graph
	std::string log;
	bool result = mpRenderGraph->compile(pRenderContext, log);
	if(!result) {
		LLOG_ERR << "Error compiling rendering graph !!!\n" << log;
		mpRenderGraph = nullptr;
		return;
	}

	// Once rendering graph compiled we can create additional AOV processing (tonemapping, denoising, etc.) if required

	// MAIN (Beauty) pass image processing
	if(mRenderPassesDictionary.getValue<bool>("MAIN.ToneMappingPass.enable", false) == true) {
		Falcor::Dictionary lightingPassDictionary({});

		if(mRenderPassesDictionary.keyExists("MAIN.ToneMappingPass.operator"))
			lightingPassDictionary["operator"] = static_cast<ToneMapperPass::Operator>(uint32_t(mRenderPassesDictionary["MAIN.ToneMappingPass.operator"]));

		if(mRenderPassesDictionary.keyExists("MAIN.ToneMappingPass.filmSpeed"))
			lightingPassDictionary["filmSpeed"] = mRenderPassesDictionary["MAIN.ToneMappingPass.filmSpeed"];

		if(mRenderPassesDictionary.keyExists("MAIN.ToneMappingPass.exposureValue"))
			lightingPassDictionary["exposureValue"] = mRenderPassesDictionary["MAIN.ToneMappingPass.exposureValue"];

		if(mRenderPassesDictionary.keyExists("MAIN.ToneMappingPass.autoExposure"))
			lightingPassDictionary["autoExposure"] = mRenderPassesDictionary["MAIN.ToneMappingPass.autoExposure"];
	
		auto pToneMapperPass = pMainAOV->createTonemappingPass(pRenderContext, lightingPassDictionary);
	}

	if(mRenderPassesDictionary.getValue<bool>("MAIN.OpenDenoisePass.enable", false) == true) {
		auto pDenoisingPass = pMainAOV->createOpenDenoisePass(pRenderContext, {});
		if (pDenoisingPass) {
			//Set denoiser parameters here
			const auto pToneMapperPass = pMainAOV->tonemappingPass();
			if(pToneMapperPass) pDenoisingPass->disableHDRInput(pToneMapperPass->getClamp());
		}
	}

	LLOG_DBG << "createRenderGraph done";
}

bool Renderer::hasAOVPlane(const AOVName& name) const {
	if(mAOVPlanes.find(name) != mAOVPlanes.end()) return true;
	return false;
}

std::vector<std::string> Renderer::getGraphOutputs(const Falcor::RenderGraph::SharedPtr& pGraph) {
	std::vector<std::string> outputs;
	for (size_t i = 0; i < pGraph->getOutputCount(); i++) outputs.push_back(pGraph->getOutputName(i));
	return outputs;
}

void Renderer::addGraph(const Falcor::RenderGraph::SharedPtr& pGraph) {
	LLOG_DBG << "Renderer::addGraph";

	if (pGraph == nullptr) {
		LLOG_ERR << "Can't add an empty graph";
		return;
	}

	// If a graph with the same name already exists, remove it
	GraphData* pGraphData = nullptr;
	for (size_t i = 0; i < mGraphs.size(); i++) {
		if (mGraphs[i].pGraph->getName() == pGraph->getName()) {
			LLOG_WRN << "Replacing existing graph \"" << pGraph->getName() << "\" with new graph.";
			pGraphData = &mGraphs[i];
			break;
		}
	}

	// FIXME: put individual graphs initalization down the pipeline. Also cache inited graph until scene changed
	initGraph(pGraph, pGraphData);
}

void Renderer::initGraph(const Falcor::RenderGraph::SharedPtr& pGraph, GraphData* pData) {
	if (!pData) {
		mGraphs.push_back({});
		pData = &mGraphs.back();
	}

	GraphData& data = *pData;
	// Set input image if it exists
	data.pGraph = pGraph;
	//data.pGraph->setScene(mpSceneBuilder->getScene());
	if (data.pGraph->getOutputCount() != 0) data.mainOutput = data.pGraph->getOutputName(0);

	// Store the original outputs
	data.originalOutputs = getGraphOutputs(pGraph);
}

void Renderer::resolvePerFrameSparseResourcesForActiveGraph(Falcor::RenderContext* pRenderContext) {
	if (mGraphs.empty()) return;

	auto& pGraph = mGraphs[mActiveGraph].pGraph;
	LLOG_DBG << "Resolve per frame sparse resources for graph: " << pGraph->getName() << " output name: " << mGraphs[mActiveGraph].mainOutput;

	// Execute graph.
	(*pGraph->getPassesDictionary())[Falcor::kRenderPassRefreshFlags] = Falcor::RenderPassRefreshFlags::None;
	pGraph->resolvePerFrameSparseResources(pRenderContext);

	//mpSceneBuilder->finalize();
}

void Renderer::executeActiveGraph(Falcor::RenderContext* pRenderContext) {
	if (mpRenderGraph)
		mpRenderGraph->execute(pRenderContext);

	return;

	if (mGraphs.empty()) return;

	auto& pGraph = mGraphs[mActiveGraph].pGraph;
	LLOG_DBG << "Execute graph: " << pGraph->getName() << " output name: " << mGraphs[mActiveGraph].mainOutput;

	// Execute graph.
	(*pGraph->getPassesDictionary())[Falcor::kRenderPassRefreshFlags] = Falcor::RenderPassRefreshFlags::None;
	//pGraph->resolvePerSampleSparseResources(pRenderContext);
	pGraph->execute(pRenderContext);
}

static CPUSampleGenerator::SharedPtr createSamplePattern(Renderer::SamplePattern type, uint32_t sampleCount) {
	if (sampleCount == 0) sampleCount = 1024 * 4;

	switch (type) {
		case Renderer::SamplePattern::Center:
			return nullptr;
		case Renderer::SamplePattern::DirectX:
			return DxSamplePattern::create(sampleCount);
		case Renderer::SamplePattern::Halton:
			return HaltonSamplePattern::create(sampleCount);
		case Renderer::SamplePattern::Stratified:
			return StratifiedSamplePattern::create(sampleCount);
		default:
			should_not_get_here();
			return nullptr;
	}
}

void Renderer::finalizeScene(const FrameInfo& frame_info) {
	// finalize camera
	auto renderRegionDims = frame_info.renderRegionDims();

	if(!mpSampleGenerator || (mCurrentFrameInfo.renderRegionDims() != renderRegionDims)) {
		auto mInvRegionDim = 1.f / float2({renderRegionDims[0], renderRegionDims[1]});
		mpSampleGenerator = createSamplePattern(SamplePattern::Stratified, frame_info.imageSamples);
		mpCamera->setPatternGenerator(mpSampleGenerator, mInvRegionDim);
	}

	mpSceneBuilder->getScene()->update(mpDevice->getRenderContext(), frame_info.frameNumber);
}

void Renderer::bindAOVPlanesToResources() {
	for (auto const& [name, pAOVPlane] : mAOVPlanes) {
		std::string passOutputName = pAOVPlane->accumulationPassOutputName();
		if (passOutputName.empty()) {
			LLOG_ERR << "AOV plane " << pAOVPlane->name() << " has no render pass output name !!! Resource binding skipped ...";
		} else {
			Falcor::Resource::SharedPtr pResource = mpRenderGraph->getOutput(passOutputName);
			if(!pResource) {
				LLOG_ERR << "Unable to find render graph output " << passOutputName << " !!! Resource binding skipped ...";
			} else {
				pAOVPlane->bindToTexture(pResource->asTexture());
			}
		}
	}
}

bool Renderer::prepareFrame(const FrameInfo& frame_info) {
	_mpScene = nullptr;

	if (!mInited) {
		LLOG_ERR << "Renderer not initialized !!!";
		return false;
	}

	if (!mMainAOVPlaneExist) {
		LLOG_ERR << "No main output plane specified !!!";
		return false;
	}

	auto renderRegionDims = frame_info.renderRegionDims();
	finalizeScene(frame_info);

	if (!mpRenderGraph) {
		createRenderGraph(frame_info);
		bindAOVPlanesToResources();
	} else if (
		(mCurrentFrameInfo.imageWidth != frame_info.imageWidth) || 
		(mCurrentFrameInfo.imageHeight != frame_info.imageHeight) || 
		(mCurrentFrameInfo.renderRegionDims() != renderRegionDims)) {
		
		// Change rendering graph frame dimensions
		mpRenderGraph->resize(renderRegionDims[0], renderRegionDims[1], Falcor::ResourceFormat::RGBA32Float);
		if(mpTexturesResolvePassGraph) {
			mpTexturesResolvePassGraph->resize(renderRegionDims[0], renderRegionDims[1], Falcor::ResourceFormat::R8Unorm);
		}

		std::string compilationLog;
		if(! mpRenderGraph->compile(mpDevice->getRenderContext(), compilationLog)) {
			LLOG_ERR << "Error render graph compilation ! " << compilationLog;
			return false;
		}
		bindAOVPlanesToResources();
	}

	for(auto &pair: mAOVPlanes) {
		pair.second->reset();
	}

	mCurrentSampleNumber = 0;
	mCurrentFrameInfo = frame_info;

	auto pScene = mpSceneBuilder->getScene();
	if (pScene) {
		_mpScene = pScene.get();
	}
}

void Renderer::renderSample() {
	auto start = std::chrono::high_resolution_clock::now();

	if ((mCurrentFrameInfo.imageSamples > 0) && mCurrentSampleNumber >= mCurrentFrameInfo.imageSamples) return;

	if (!mpRenderGraph) {
		LLOG_ERR << "RenderGraph not ready for rendering !!!";
		return;
	}

	if (!_mpScene) {
		LLOG_ERR << "Scene not ready for rendering !!!";
		return;
	}

	auto pRenderContext = mpDevice->getRenderContext();

	if (mCurrentSampleNumber == 0) {
		// First frame sample
		if(mpTexturesResolvePassGraph) {
			mpTexturesResolvePassGraph->execute(pRenderContext);
		}
	}

	mpRenderGraph->execute(pRenderContext, mCurrentFrameInfo.frameNumber, mCurrentSampleNumber);
	
	// Hard sync every 16 samples. TODO: this is UGLY !
	if (mCurrentSampleNumber % 16 == 0) {
		//pRenderContext->flush(true);
	}

	double currentTime = 0;
	_mpScene->update(pRenderContext, currentTime);

	mCurrentSampleNumber++;


	auto stop = std::chrono::high_resolution_clock::now();
	LLOG_TRC << "Sample " << mCurrentSampleNumber << " time: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms.";
}

bool Renderer::getAOVPlaneImageData(const AOVName& name, uint8_t* pData) {
	assert(pData);

	auto pAOVPlane = getAOVPlane(name);
	if (!pAOVPlane) return false;

	return pAOVPlane->getImageData(pData);
}

void Renderer::beginFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo) {
	//for (auto& pe : mpExtensions)  pe->beginFrame(pRenderContext, pTargetFbo);
}

void Renderer::endFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo) {
	//for (auto& pe : mpExtensions) pe->endFrame(pRenderContext, pTargetFbo);
}

bool Renderer::addMaterialX(Falcor::MaterialX::UniquePtr pMaterialX) {
	std::string materialName = pMaterialX->name();
	if (mMaterialXs.find(materialName) == mMaterialXs.end() ) {
		mMaterialXs.insert(make_pair(materialName, std::move(pMaterialX)));
	} else {
		// MaterialX with this name already exist !
		LLOG_ERR << "MaterialX with name " << materialName << " already exist !!!";
		return false;
	}
	//mpSceneBuilder->addMaterialX(std::move(pMaterial));
}

// HYDRA section begin

bool  Renderer::queryAOVPlaneGeometry(const AOVName& name, AOVPlaneGeometry& aov_plane_geometry) const {
	auto pAOVPlane = getAOVPlane(name);
	if(!pAOVPlane) {
		return false;
	}   

	return pAOVPlane->getAOVPlaneGeometry(aov_plane_geometry);
}

// HYDRA section end

}  // namespace lava