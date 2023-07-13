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

#include "RenderPasses/DebugShadingPass/DebugShadingPass.h"
#include "RenderPasses/DeferredLightingPass/DeferredLightingPass.h"
#include "RenderPasses/DeferredLightingCachedPass/DeferredLightingCachedPass.h"
#include "RenderPasses/CryptomattePass/CryptomattePass.h"
#include "RenderPasses/EdgeDetectPass/EdgeDetectPass.h"
#include "RenderPasses/AmbientOcclusionPass/AmbientOcclusionPass.h"
#include "RenderPasses/PathTracer/PathTracer.h"
#include "RenderPasses/GBuffer/VBuffer/VBufferRaster.h"
#include "RenderPasses/GBuffer/VBuffer/VBufferRT.h"
#include "RenderPasses/GBuffer/VBuffer/VBufferSW.h"

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

	if(info.name != AOVBuiltinName::MAIN && !mMainAOVPlaneExist) {
		LLOG_ERR << "Error creating AOV plane \"" << info.name << "\" without MAIN output plane!"; 
		return nullptr;
	}

	auto pAOVPlane = AOVPlane::create(info);
	if (!pAOVPlane) {
		LLOG_ERR << "Error creating AOV plane \"" << pAOVPlane->name() << "\" !!!";
		return nullptr;
	}

	mAOVPlanes[pAOVPlane->name()] = pAOVPlane;
	if (info.name == AOVBuiltinName::MAIN) mMainAOVPlaneExist = true; 

	// Check if this output plane is the same as main place source render pass. If so then just disable it...
	if (pAOVPlane->name() != AOVBuiltinName::MAIN) {
		auto pMainAOV = getAOVPlane(AOVBuiltinName::MAIN);
		if(pMainAOV->sourcePassName() == to_string(pAOVPlane->name())) pAOVPlane->setState(AOVPlane::State::Disabled);
	}

	mDirty = true;
	return pAOVPlane;
}

bool Renderer::deleteAOVPlane(const AOVName& name) {
	if (mAOVPlanes.find(name) == mAOVPlanes.end()) {
		LLOG_WRN << "No AOVPlane " << name << " to delete.";
		return false;
	}

	mAOVPlanes[name] = nullptr;
	mDirty = true;
	return true;
}

void Renderer::setAOVPlaneState(const AOVName& name, AOVPlane::State state) {
	if (mAOVPlanes.find(name) == mAOVPlanes.end()) return;

	auto& pPlane = mAOVPlanes[name];
	AOVPlane::State prevAOVState = pPlane->getState();
	pPlane->setState(state);
	if (prevAOVState != pPlane->getState()) {
		mDirty = true;
	}
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

	// Get one of possible main output channels
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

	// Main render graph
	mpRenderGraph = RenderGraph::create(mpDevice, imageSize, ResourceFormat::RGBA32Float, "MainImageRenderGraph");
	
	// Depth pass
	Falcor::Dictionary depthPassDictionary(mRenderPassesDict);
	depthPassDictionary["disableAlphaTest"] = false; // take texture alpha into account

	mpDepthPass = DepthPass::create(pRenderContext, depthPassDictionary);
	//mpDepthPass->setDepthBufferFormat(ResourceFormat::D32Float);
	mpDepthPass->setScene(pRenderContext, pScene);
	mpDepthPass->setCullMode(cullMode);
	mpRenderGraph->addPass(mpDepthPass, "DepthPass");

	// Lighting (shading) pass
	Falcor::Dictionary lightingPassDictionary(mRenderPassesDict);

	lightingPassDictionary["frameSampleCount"] = frame_info.imageSamples;


	const std::string shadingPassType = mRendererConfDict.getValue("shadingpasstype", std::string("deferred"));

	if (shadingPassType == std::string("pathtracer")) {

		auto pPathTracerPass = PathTracer::create(pRenderContext, {});
		pPathTracerPass->setScene(pRenderContext, pScene);
		//pPathTracerPass->setColorFormat(ResourceFormat::RGBA16Float);
		mpRenderGraph->addPass(pPathTracerPass, "ShadingPass");

  } else if (shadingPassType == std::string("deferred")) {

		auto pDeferredLightingPass = DeferredLightingPass::create(pRenderContext, lightingPassDictionary);
		pDeferredLightingPass->setScene(pRenderContext, pScene);
		mpRenderGraph->addPass(pDeferredLightingPass, "ShadingPass");

	} else if (shadingPassType == std::string("debug")) {

		auto pDebugShadingPass = DebugShadingPass::create(pRenderContext, {});
		pDebugShadingPass->setScene(pRenderContext, pScene);
		mpRenderGraph->addPass(pDebugShadingPass, "ShadingPass");

	} else {
		LLOG_FTL << "Unsupported shading pass type \"" << shadingPassType << "\" requested!!!";
		mpRenderGraph = nullptr;
		return;
	}


	// VBuffer
	Falcor::Dictionary vbufferPassDictionary(mRenderPassesDict);
	
	static const std::string kPrimaryRayGenTypeKey = "primaryraygentype";
	const std::string primaryRaygenType = mRendererConfDict.getValue<std::string>(kPrimaryRayGenTypeKey);
	LLOG_INF << "Primary ray generation type set to \"" << primaryRaygenType << "\"";

	if( primaryRaygenType == std::string("compute")) {

		// Compute raytraced (rayquery) vbuffer generator
		auto pVBufferPass = VBufferRT::create(pRenderContext, vbufferPassDictionary);
		pVBufferPass->setScene(pRenderContext, pScene);
		pVBufferPass->setCullMode(cullMode);
		mpRenderGraph->addPass(pVBufferPass, "VBufferPass");

	} else if ( primaryRaygenType == std::string("hwraster")) {

		// Hardware rasterizer vbuffer generator
		auto pVBufferPass = VBufferRaster::create(pRenderContext, vbufferPassDictionary);
		pVBufferPass->setScene(pRenderContext, pScene);
		pVBufferPass->setCullMode(cullMode);
		mpRenderGraph->addPass(pVBufferPass, "VBufferPass");

	} else if ( primaryRaygenType == std::string("swraster")) {

		// Compute shader rasterizer vbuffer generator
		auto pVBufferPass = VBufferSW::create(pRenderContext, vbufferPassDictionary);
		pVBufferPass->setScene(pRenderContext, pScene);
		pVBufferPass->setCullMode(cullMode);
		mpRenderGraph->addPass(pVBufferPass, "VBufferPass");

	} else {

		LLOG_FTL << "Unsupported primary ray (vbuffer) generator type \"" << primaryRaygenType << "\" requested!!!";
		mpRenderGraph = nullptr;
		return;

	}

		// Virtual textures resolve render graph
	if(pScene->materialSystem()->hasSparseTextures()) {
		auto vtexResolveChannelOutputFormat = ResourceFormat::RGBA8Unorm;
		mpTexturesResolvePassGraph = RenderGraph::create(mpDevice, imageSize, vtexResolveChannelOutputFormat, "VirtualTexturesGraph");

		// Depth pre-pass
		Falcor::Dictionary depthPrePassDictionary(mRenderPassesDict);
		depthPrePassDictionary["disableAlphaTest"] = true; // no virtual textures loaded at this point

		auto pDepthPrePass = DepthPass::create(pRenderContext, depthPrePassDictionary);
		pDepthPrePass->setDepthBufferFormat(ResourceFormat::D32Float);
		pDepthPrePass->setScene(pRenderContext, pScene);
		pDepthPrePass->setCullMode(cullMode);
		mpTexturesResolvePassGraph->addPass(pDepthPrePass, "DepthPrePass");

		// Vitrual textures resolve pass
		Falcor::Dictionary texturesResolvePassDictionary(mRenderPassesDict);
		mpTexturesResolvePass = TexturesResolvePass::create(pRenderContext, texturesResolvePassDictionary);
		mpTexturesResolvePass->setRasterizerState(Falcor::RasterizerState::create(rsDesc));
		mpTexturesResolvePass->setScene(pRenderContext, pScene);

		mpTexturesResolvePassGraph->addPass(mpTexturesResolvePass, "SparseTexturesResolvePrePass");
		mpTexturesResolvePassGraph->markOutput("SparseTexturesResolvePrePass.output");

		mpTexturesResolvePassGraph->addEdge("DepthPrePass.depth", "SparseTexturesResolvePrePass.depth");
	} else {
		mpTexturesResolvePassGraph = nullptr;
		mpTexturesResolvePass = nullptr;
	}

	// RTXDIPass
	//Falcor::Dictionary rtxdiPassDictionary(mRenderPassesDict);
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
	mpRenderGraph->addEdge("VBufferPass.depth", "SkyBoxPass.depth");

#ifdef USE_FORWARD_LIGHTING_PASS
	// Forward lighting pass
	mpRenderGraph->addEdge("VBufferPass.depth", "ShadingPass.depth");
	mpRenderGraph->addEdge("SkyBoxPass.target", "ShadingPass.color");
	
#else
	// Deferred lighting pass
	mpRenderGraph->addEdge("VBufferPass.depth",    "ShadingPass.depth");
	mpRenderGraph->addEdge("VBufferPass.vbuffer",  "ShadingPass.vbuffer");
	mpRenderGraph->addEdge("VBufferPass.texGrads", "ShadingPass.texGrads");
	mpRenderGraph->addEdge("SkyBoxPass.target",    "ShadingPass.color");

#endif

	// Create optional render passes
	for(const auto &entry: mAOVPlanes) {
		const auto& pPlane = entry.second;
		const std::string renderPassName = (pPlane->sourcePassName() == "") ? entry.first : pPlane->sourcePassName();
		const std::string planeName = pPlane->name();
		const std::string planeOutputName = pPlane->outputName();
		const std::string planeOutputVariable = pPlane->outputVariableName();
		const std::string renderPassOutputName = planeName + "." + planeOutputVariable;

		auto pPlaneAccumulationPass = pPlane->accumulationPass() ? pPlane->accumulationPass() : pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);
		if(!pPlaneAccumulationPass) {
			LLOG_WRN << "Plane " << planeName << " has no accumulation pass. This might lead to an error to wrong plane rendering!";
			continue;
		}

		// Main pass
		if (renderPassName == to_string(AOVBuiltinName::MAIN)) {
			continue;
		}

		// Optional edgedetect pass
		if (renderPassName == "EdgeDetectPass") {
			auto pEdgeDetectPass = EdgeDetectPass::create(pRenderContext, pPlane->getRenderPassesDict());
			pEdgeDetectPass->setScene(pRenderContext, pScene);
			mpRenderGraph->addPass(pEdgeDetectPass, planeName);
			mpRenderGraph->addEdge("VBufferPass.vbuffer", planeName + ".vbuffer");

			if(pPlaneAccumulationPass) {
				pPlaneAccumulationPass->setScene(pScene);
				mpRenderGraph->addEdge(renderPassOutputName, pPlane->accumulationPassColorInputName());
			}
			continue;
		}

		// Optional ambient occlusion pass
		if (renderPassName == "AmbientOcclusionPass") {
			auto pAmbientOcclusionPass = AmbientOcclusionPass::create(pRenderContext, pPlane->getRenderPassesDict());
			pAmbientOcclusionPass->setScene(pRenderContext, pScene);
			mpRenderGraph->addPass(pAmbientOcclusionPass, planeName);
			mpRenderGraph->addEdge("VBufferPass.vbuffer", planeName + ".vbuffer");

			if(pPlaneAccumulationPass) {
				pPlaneAccumulationPass->setScene(pScene);
				mpRenderGraph->addEdge(renderPassOutputName, pPlane->accumulationPassColorInputName());
			}
			continue;
		}

		// Optional cryptomatte pass
		if (renderPassName == "CryptomattePass") {
			auto pCryptomattePass = CryptomattePass::create(pRenderContext, pPlane->getRenderPassesDict());
			pCryptomattePass->setScene(pRenderContext, pScene);
			mpRenderGraph->addPass(pCryptomattePass, planeName);
			mpRenderGraph->addEdge("VBufferPass.vbuffer", planeName + ".vbuffer");

			auto pMetaDataTargetPlane = (pPlane->filename() != "") ? pPlane : pMainAOV;

			pMetaDataTargetPlane->addMetaData("Leonid", std::string("vonyuchka ;)"));
			pMetaDataTargetPlane->addMetaDataProvider(pCryptomattePass);

			if(pPlaneAccumulationPass) {
				pPlaneAccumulationPass->setScene(pScene);
				mpRenderGraph->addEdge(renderPassOutputName, pPlane->accumulationPassColorInputName());
			}
			continue;
		}

		// Optional existing pass edditional output
		if (mpRenderGraph->doesPassExist(renderPassName)) {
			if(pPlaneAccumulationPass) {
				pPlaneAccumulationPass->setScene(pScene);
				const std::string renderPassOutputName = renderPassName + "." + planeOutputVariable;
				mpRenderGraph->addEdge(renderPassOutputName, pPlane->accumulationPassColorInputName());
			}
			continue;
		}
	}

	// MAIN (beauty) plane accumulation pass and bind with render graph output
	auto pMainAOVAccumulationPass = pMainAOV->accumulationPass() ? pMainAOV->accumulationPass() : pMainAOV->createAccumulationPass(pRenderContext, mpRenderGraph);
	if(pMainAOVAccumulationPass) {
		pMainAOVAccumulationPass->setScene(pScene);
		if(pMainAOV->sourcePassName() == "EdgeDetectPass") {
			mpRenderGraph->addEdge("EdgeDetectPass.output", pMainAOV->accumulationPassColorInputName());	
		} else if (pMainAOV->sourcePassName() == "AmbientOcclusionPass"){
			mpRenderGraph->addEdge("AmbientOcclusionPass.output", pMainAOV->accumulationPassColorInputName());
		} else {
			mpRenderGraph->addEdge("ShadingPass.color", pMainAOV->accumulationPassColorInputName());
		}

		mpRenderGraph->addEdge("VBufferPass.depth", pMainAOV->accumulationPassDepthInputName());
	}

	// Create and bind additional AOV planes
	for (const auto &entry: mAOVPlanes) {
		auto &pPlane = entry.second;
		if(!pPlane || !pPlane->isEnabled()) continue;

		auto pAccPass = pPlane->accumulationPass() ? pPlane->accumulationPass() : pPlane->createAccumulationPass(pRenderContext, mpRenderGraph);

		switch(pPlane->name()) {
			case AOVBuiltinName::DEPTH:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						pPlane->setOutputFormat(ResourceFormat::R32Float);
						mpRenderGraph->addEdge("ShadingPass.Pz", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::POSITION:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.posW", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::NORMAL:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.normals", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::SHADOW:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.shadows", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::ALBEDO:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.albedo", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::EMISSION:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.emission", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::FRESNEL:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.fresnel", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::Prim_Id:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.prim_id", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::Op_Id:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						mpRenderGraph->addEdge("ShadingPass.op_id", pPlane->accumulationPassColorInputName());
					}
				}
				break;
			case AOVBuiltinName::VARIANCE:
				{
					if(pAccPass) {
						pAccPass->setScene(pScene);
						pAccPass->enableAccumulation(false);
						mpRenderGraph->addEdge("ShadingPass.variance", pPlane->accumulationPassColorInputName());
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
	if(mRenderPassesDict.getValue<bool>("MAIN.ToneMappingPass.enable", false) == true) {
		Falcor::Dictionary lightingPassDictionary({});

		if(mRenderPassesDict.keyExists("MAIN.ToneMappingPass.operator"))
			lightingPassDictionary["operator"] = static_cast<ToneMapperPass::Operator>(uint32_t(mRenderPassesDict["MAIN.ToneMappingPass.operator"]));

		if(mRenderPassesDict.keyExists("MAIN.ToneMappingPass.filmSpeed"))
			lightingPassDictionary["filmSpeed"] = mRenderPassesDict["MAIN.ToneMappingPass.filmSpeed"];

		if(mRenderPassesDict.keyExists("MAIN.ToneMappingPass.exposureValue"))
			lightingPassDictionary["exposureValue"] = mRenderPassesDict["MAIN.ToneMappingPass.exposureValue"];

		if(mRenderPassesDict.keyExists("MAIN.ToneMappingPass.autoExposure"))
			lightingPassDictionary["autoExposure"] = mRenderPassesDict["MAIN.ToneMappingPass.autoExposure"];
	
		auto pToneMapperPass = pMainAOV->createTonemappingPass(pRenderContext, lightingPassDictionary);
	}

	if(mRenderPassesDict.getValue<bool>("MAIN.OpenDenoisePass.enable", false) == true) {
		auto pDenoisingPass = pMainAOV->createOpenDenoisePass(pRenderContext, {});
		if (pDenoisingPass) {
			//Set denoiser parameters here
			const auto pToneMapperPass = pMainAOV->tonemappingPass();
			if(pToneMapperPass) pDenoisingPass->disableHDRInput(pToneMapperPass->getClamp());
		}
	}

	LLOG_DBG << "createRenderGraph done";
	mDirty = false;
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

	//mpCamera->setJitter({0.0f, 0.0f}); // TODO: remove!!!

	mpSceneBuilder->getScene()->update(mpDevice->getRenderContext(), frame_info.frameNumber);
}

void Renderer::bindAOVPlanesToResources() {
	for (auto const& [name, pAOVPlane] : mAOVPlanes) {
		std::string passOutputName = pAOVPlane->accumulationPassColorOutputName();
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
	}
	bindAOVPlanesToResources();

	for(auto &pair: mAOVPlanes) {
		pair.second->reset();
	}

	mCurrentSampleNumber = 0;
	mCurrentFrameInfo = frame_info;

	auto pScene = mpSceneBuilder->getScene();
	if (pScene) {
		_mpScene = pScene.get();
	}

	mpDevice->getRenderContext()->flush(true);
	mDirty = false;
}

void Renderer::renderSample() {
	if (mDirty) {
		prepareFrame(mCurrentFrameInfo);
	}

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
			if( 1 == 2) {
				// internal texture debugging 
				Falcor::Texture::SharedPtr pOutTex = std::dynamic_pointer_cast<Falcor::Texture>(mpTexturesResolvePassGraph->getOutput("SparseTexturesResolvePrePass.output"));
				pOutTex->captureToFile(0, 0, "/home/max/vtex_dbg.png");
			}
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

}

const uint8_t* Renderer::getAOVPlaneImageData(const AOVName& name) {
	auto pAOVPlane = getAOVPlane(name);
	if (!pAOVPlane) return nullptr;

	return pAOVPlane->getImageData();
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