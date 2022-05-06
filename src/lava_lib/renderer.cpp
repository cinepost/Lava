#include "renderer.h"

#include "Falcor/Core/API/ResourceManager.h"
#include "Falcor/Utils/Threading.h"
#include "Falcor/Utils/Scripting/Scripting.h"
#include "Falcor/Utils/Scripting/Dictionary.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"
#include "Falcor/Utils/ConfigStore.h"
#include "Falcor/Utils/Debug/debug.h"

#include "Falcor/Scene/Lights/EnvMap.h"
#include "Falcor/Scene/MaterialX/MaterialX.h"

#include "lava_utils_lib/logging.h"

namespace Falcor {  IFramework* gpFramework = nullptr; } // TODO: probably it's safe to remove now...

namespace lava {

static bool isInVector(const std::vector<std::string>& strVec, const std::string& str) {
    return std::find(strVec.begin(), strVec.end(), str) != strVec.end();
}


Renderer::SharedPtr Renderer::create(Device::SharedPtr pDevice) {
    assert(pDevice);
    return SharedPtr(new Renderer(pDevice));
}


Renderer::Renderer(Device::SharedPtr pDevice): mpDevice(pDevice), mIfaceAquired(false), mpClock(nullptr), mpFrameRate(nullptr), mActiveGraph(0), mInited(false), mGlobalDataInited(false) {
	LLOG_DBG << "Renderer::Renderer";
    mMainAOVPlaneExist = false;
}

bool Renderer::init(const Config& config) {
	if(mInited) return true;

	LLOG_DBG << "Renderer::init";

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

    mpDevice->resourceManager()->printStats();

    mpRenderGraph = nullptr;

    Falcor::Threading::shutdown();

    mpDevice->flushAndSync();

    mGraphs.clear();

    mpSceneBuilder = nullptr;
    
    mpSampler = nullptr;

#ifdef SCRIPTING
    Falcor::Scripting::shutdown();
#endif

    Falcor::RenderPassLibrary::instance(mpDevice).shutdown();

    mpTargetFBO.reset();

    //mpDevice->cleanup();

    //mpDevice.reset();

    Falcor::OSServices::stop();
}

AOVPlane::SharedPtr Renderer::addAOVPlane(const AOVPlaneInfo& info) {
    LLOG_WRN << "Adding aov " << info.name;
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
    LLOG_WRN << "Getting aov " << name;
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
    if(!vtoff) {
        auto vtexResolveChannelOutputFormat = ResourceFormat::RGBA8Unorm;
        mpTexturesResolvePassGraph = RenderGraph::create(mpDevice, imageSize, vtexResolveChannelOutputFormat, "VirtualTexturesGraph");

        // Depth pre-pass
        Falcor::Dictionary depthPrePassDictionary;
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
    Falcor::Dictionary depthPassDictionary;
    depthPassDictionary["disableAlphaTest"] = false; // take texture alpha into account
    depthPassDictionary["maxMipLevels"] = (uint8_t)5;


    mpDepthPass = DepthPass::create(pRenderContext, depthPassDictionary);
    mpDepthPass->setDepthBufferFormat(ResourceFormat::D32Float);
    mpDepthPass->setScene(pRenderContext, pScene);
    mpDepthPass->setCullMode(cullMode);
    mpRenderGraph->addPass(mpDepthPass, "DepthPass");

    // Test GBuffer pass
    mpGBufferRasterPass = GBufferRaster::create(pRenderContext);
    auto gbuffer_pass = mpRenderGraph->addPass(mpGBufferRasterPass, "GBufferRasterPass");

    // Test pathtracer pass
    Falcor::Dictionary minimalPathTracerPassDictionary;
    mpMinimalPathTracer = MinimalPathTracer::create(pRenderContext, minimalPathTracerPassDictionary);
    mpMinimalPathTracer->setScene(pRenderContext, pScene);

    auto ptracer_pass = mpRenderGraph->addPass(mpMinimalPathTracer, "MinimalPathTracerPass");


    // Forward lighting
    Falcor::Dictionary lightingPassDictionary;

    lightingPassDictionary["frameSampleCount"] =  frame_info.imageSamples;

    mpLightingPass = ForwardLightingPass::create(pRenderContext, lightingPassDictionary);
    mpLightingPass->setRasterizerState(Falcor::RasterizerState::create(rsDesc));
    mpLightingPass->setScene(pRenderContext, pScene);
    mpLightingPass->setColorFormat(ResourceFormat::RGBA32Float);

    auto pass2 = mpRenderGraph->addPass(mpLightingPass, "LightingPass");


    // SkyBox
    mpSkyBoxPass = SkyBox::create(pRenderContext);

    // TODO: handle transparency    
    mpSkyBoxPass->setTransparency(0.0f);

    mpSkyBoxPass->setScene(pRenderContext, pScene);
    auto pass3 = mpRenderGraph->addPass(mpSkyBoxPass, "SkyBoxPass");

    pMainAOV->createAccumulationPass(pRenderContext, mpRenderGraph);
    
    mpRenderGraph->addEdge("DepthPass.depth", "LightingPass.depth");
    mpRenderGraph->addEdge("DepthPass.depth", "SkyBoxPass.depth");
    
    mpRenderGraph->addEdge("SkyBoxPass.target", "LightingPass.color");
    
    mpRenderGraph->addEdge("LightingPass.color", pMainAOV->accumulationPassInputName());

    // Compile graph
    std::string log;
    bool result = mpRenderGraph->compile(pRenderContext, log);
    if(!result) {
        LLOG_ERR << "Error compiling rendering graph !!!";
        LLOG_ERR << log;
        mpRenderGraph = nullptr;
    }
    LLOG_DBG << "createRenderGraph done";
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
    mInvFrameDim = 1.f / float2({frame_info.imageWidth, frame_info.imageHeight});

    if(!mpSampleGenerator) {
        mpSampleGenerator = createSamplePattern(SamplePattern::Stratified, frame_info.imageSamples);
    }

    if (mpSampleGenerator) {
        mpCamera->setPatternGenerator(mpSampleGenerator, mInvFrameDim);
    }

    //mpCamera->setAspectRatio(static_cast<float>(frame_info.imageWidth) / static_cast<float>(frame_info.imageHeight));
    mpSceneBuilder->getScene()->update(mpDevice->getRenderContext(), 0);
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
    if (!mInited) {
        LLOG_ERR << "Renderer not initialized !!!";
        return false;
    }

    if (!mMainAOVPlaneExist) {
        LLOG_ERR << "No main output plane specified !!!";
        return false;
    }

    finalizeScene(frame_info);

    if (!mpRenderGraph) {
        createRenderGraph(frame_info);
        bindAOVPlanesToResources();
    } else if ((mCurrentFrameInfo.imageWidth != frame_info.imageWidth) || (mCurrentFrameInfo.imageHeight != frame_info.imageHeight) || (mCurrentFrameInfo.renderRegion != frame_info.renderRegion)) {
        // Change rendering graph frame dimensions
        auto graphRenderRegion = mCurrentFrameInfo.renderRegionDims();
        mpRenderGraph->resize(graphRenderRegion[0], graphRenderRegion[1], Falcor::ResourceFormat::RGBA32Float);

        std::string compilationLog;
        if(! mpRenderGraph->compile(mpDevice->getRenderContext(), compilationLog)) {
            LLOG_ERR << "Error render graph compilation ! " << compilationLog;
            return false;
        }
        bindAOVPlanesToResources();
    }

    mCurrentSampleNumber = 0;
    mCurrentFrameInfo = frame_info;
}

void Renderer::renderSample() {
    if (!mpRenderGraph) return;
    if ((mCurrentFrameInfo.imageSamples > 0) && mCurrentSampleNumber >= mCurrentFrameInfo.imageSamples) return;

    auto pScene = mpSceneBuilder->getScene();
    if (!pScene) {
        LLOG_ERR << "Unable to get scene from scene builder !!!";
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

    double currentTime = 0;
    pScene->update(pRenderContext, currentTime);

    mCurrentSampleNumber++;
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