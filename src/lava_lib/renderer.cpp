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

// TODO: handle requred channels (RGB/RGBA)
static Falcor::ResourceFormat resolveShadingResourceFormat(Display::TypeFormat fmt, uint numchannels) {
    assert(numchannels <= 4);

    switch(fmt) {
        case Display::TypeFormat::SIGNED8:
            if( numchannels == 1) return Falcor::ResourceFormat::R8Snorm; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG8Snorm;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB8Snorm;
            return Falcor::ResourceFormat::RGBA8Snorm;

        case Display::TypeFormat::UNSIGNED8:
            if( numchannels == 1) return Falcor::ResourceFormat::R8Unorm; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG8Unorm;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB8Unorm;
            return Falcor::ResourceFormat::RGBA8Unorm;

        case Display::TypeFormat::SIGNED16:
            if( numchannels == 1) return Falcor::ResourceFormat::R16Int; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG16Int;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB16Int;
            return Falcor::ResourceFormat::RGBA16Int;  // TODO: add RGBA16Snorm to Falcor formats
        
        case Display::TypeFormat::UNSIGNED16:
            if( numchannels == 1) return Falcor::ResourceFormat::R16Unorm; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG16Unorm;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB16Unorm;
            return Falcor::ResourceFormat::RGBA16Unorm;
        
        case Display::TypeFormat::FLOAT16:
            if( numchannels == 1) return Falcor::ResourceFormat::R16Float; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG16Float;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB16Float;
            return Falcor::ResourceFormat::RGBA16Float;
        
        case Display::TypeFormat::FLOAT32:
        default:
            if( numchannels == 1) return Falcor::ResourceFormat::R32Float; 
            if( numchannels == 3) return Falcor::ResourceFormat::RG32Float;
            if( numchannels == 3) return Falcor::ResourceFormat::RGB32Float;
            return Falcor::ResourceFormat::RGBA32Float;
    }
}

Renderer::SharedPtr Renderer::create(Device::SharedPtr pDevice) {
    assert(pDevice);
    return SharedPtr(new Renderer(pDevice));
}


Renderer::Renderer(Device::SharedPtr pDevice): mpDevice(pDevice), mIfaceAquired(false), mpClock(nullptr), mpFrameRate(nullptr), mActiveGraph(0), mInited(false), mGlobalDataInited(false) {
	LLOG_DBG << "Renderer::Renderer";
    mpDisplay = nullptr;
}

bool Renderer::init() {
	if(mInited) return true;

	LLOG_DBG << "Renderer::init";

	Falcor::OSServices::start();

	//Falcor::Scripting::start();
    //Falcor::ScriptBindings::registerBinding(Renderer::registerBindings);

    Falcor::Threading::start();

    auto const& confgStore = Falcor::ConfigStore::instance();
    std::string tangentMode = confgStore.get<std::string>("geo_tangent_generation", "mikkt");

    auto sceneBuilderFlags = Falcor::SceneBuilder::Flags::DontMergeMeshes;
    if( tangentMode == "mikkt" ) {
        //sceneBuilderFlags |= SceneBuilder::Flags::MikkTSpaceTangets;
    }

    bool use_raytracing = confgStore.get<bool>("rton", true);;
    if (use_raytracing) {
        sceneBuilderFlags |= SceneBuilder::Flags::UseRaytracing;
    }


    mpSceneBuilder = lava::SceneBuilder::create(mpDevice, sceneBuilderFlags);
    mpCamera = Falcor::Camera::create();
    mpCamera->setName("main");
    mpSceneBuilder->addCamera(mpCamera);
    //mpSceneBuilder->setCamera("main");


    mInited = true;
    return true;
}

void Renderer::initGlobalData(const RendererIface::GlobalData& global_data) {
    if(mGlobalDataInited) {
        LLOG_WRN << "Renderer global data already initialized !!!";
        return;
    }

    mGlobalData = global_data;
    mGlobalDataInited = true;
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

    //Falcor::Scripting::shutdown();
    //Falcor::RenderPassLibrary::instance(mpDevice).shutdown();

    if(mpDisplay)
        mpDisplay=nullptr;

    mpTargetFBO.reset();

    //mpDevice->cleanup();

    //mpDevice.reset();

    Falcor::OSServices::stop();
}

std::unique_ptr<RendererIface> Renderer::aquireInterface() {
	if (!mIfaceAquired) {
		return std::move(std::make_unique<RendererIface>(shared_from_this()));
	}
	LLOG_ERR << "Сan't aquire renderer interface. Relase old first!";
	return nullptr;
}

void Renderer::releaseInterface(std::unique_ptr<RendererIface> pInterface) {
	if(mIfaceAquired) {
		std::move(pInterface).reset();
		mIfaceAquired = false;
	}
}

bool Renderer::addPlane(const RendererIface::PlaneData plane_data) {
    auto it = mPlanes.find(plane_data.channel);
    if( it != mPlanes.end()) {
        LLOG_ERR << "Output plane " << plane_data.name << " already exist !";
        return false;
    }
    mPlanes[plane_data.channel] = plane_data;
    LLOG_DBG << "Output plane " << plane_data.name << " added !";

    return true;
}

bool Renderer::loadDisplay(Display::DisplayType display_type) {
	mpDisplay = Display::create(display_type);
	if(!mpDisplay) {
        LLOG_ERR << "Unable to create display !!!";
		return false;
    }

	return true;
}

bool Renderer::closeDisplay() {
    if (!mpDisplay) return false;
    return mpDisplay->closeAll();
}


bool isInVector(const std::vector<std::string>& strVec, const std::string& str) {
    return std::find(strVec.begin(), strVec.end(), str) != strVec.end();
}

void Renderer::createRenderGraph() {
    if (mpRenderGraph) 
        return; 

    assert(mpDevice);

    auto pRenderContext = mpDevice->getRenderContext();
    auto pScene = mpSceneBuilder->getScene();

    assert(pScene);
    
    auto const& confgStore = Falcor::ConfigStore::instance();
    bool vtoff = confgStore.get<bool>("vtoff", true);

    Falcor::uint2 imageSize = {mGlobalData.imageWidth, mGlobalData.imageHeight};

    LOG_ERR("createRenderGraph frame dim %u %u", mGlobalData.imageWidth, mGlobalData.imageHeight);

    // Pick rendering resource format's according to required Display::TypeFormat
    Falcor::ResourceFormat shadingResourceFormat;
    Falcor::ResourceFormat auxAlbedoResourceFormat;

    for(const auto& [channel, plane]: mPlanes) {
        switch(channel) {
            case RendererIface::PlaneData::Channel::COLOR:
                shadingResourceFormat = resolveShadingResourceFormat(plane.format, 3);
                break;
            case RendererIface::PlaneData::Channel::COLOR_ALPHA:
                shadingResourceFormat = resolveShadingResourceFormat(plane.format, 4);
                break;
            case RendererIface::PlaneData::Channel::ALBEDO:
                auxAlbedoResourceFormat = resolveShadingResourceFormat(plane.format, 3);
                break;
            default:
                break;
        }
    }

    //// EnvMapSampler stuff
    Texture::SharedPtr pEnvTexture = nullptr;
    if (pEnvTexture) {
        auto pEnvMap = Falcor::EnvMap::create(mpDevice, pEnvTexture);
        //pEnvMap->setTint(...);
        pScene->setEnvMap(pEnvMap);
    }

    //EnvMapSampler::SharedPtr pEnvMapSampler = nullptr;
    auto pEnvMap = pScene->getEnvMap();

    //if (pEnvMap) {
    //    pEnvMapSampler = EnvMapSampler::create(pRenderContext, pEnvMap);
    //}

    ////

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
    auto mainChannelOutputFormat = ResourceFormat::RGBA16Float;
    mpRenderGraph = RenderGraph::create(mpDevice, imageSize, mainChannelOutputFormat, "MainImageRenderGraph");
    
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

    lightingPassDictionary["frameSampleCount"] =  mGlobalData.imageSamples;

    mpLightingPass = ForwardLightingPass::create(pRenderContext, lightingPassDictionary);
    mpLightingPass->setRasterizerState(Falcor::RasterizerState::create(rsDesc));
    mpLightingPass->setScene(pRenderContext, pScene);
    mpLightingPass->setColorFormat(mainChannelOutputFormat);

    //if (pEnvMapSampler) {
    //    mpLightingPass->setEnvMapSampler(pEnvMapSampler);
    //}

    auto pass2 = mpRenderGraph->addPass(mpLightingPass, "LightingPass");


    // SkyBox
    mpSkyBoxPass = SkyBox::create(pRenderContext);

    // TODO: handle transparency    
    mpSkyBoxPass->setTransparency(0.0f);

    mpSkyBoxPass->setScene(pRenderContext, pScene);
    auto pass3 = mpRenderGraph->addPass(mpSkyBoxPass, "SkyBoxPass");

    // Accumulaion
    mpAccumulatePass = AccumulatePass::create(pRenderContext);
    mpAccumulatePass->enableAccumulation(true);
    mpRenderGraph->addPass(mpAccumulatePass, "AccumulatePass");

    mpRenderGraph->addEdge("DepthPass.depth", "LightingPass.depth");
    mpRenderGraph->addEdge("DepthPass.depth", "SkyBoxPass.depth");
    
    mpRenderGraph->addEdge("SkyBoxPass.target", "LightingPass.color");
    mpRenderGraph->addEdge("LightingPass.color", "AccumulatePass.input");

/*
    mpRenderGraph->addEdge("GBufferRasterPass.posW", "MinimalPathTracerPass.posW");
    mpRenderGraph->addEdge("GBufferRasterPass.normW", "MinimalPathTracerPass.normalW");
    mpRenderGraph->addEdge("GBufferRasterPass.faceNormalW", "MinimalPathTracerPass.faceNormalW");

    mpRenderGraph->addEdge("GBufferRasterPass.diffuseOpacity", "MinimalPathTracerPass.mtlDiffOpacity");
    mpRenderGraph->addEdge("GBufferRasterPass.specRough", "MinimalPathTracerPass.mtlSpecRough");
    mpRenderGraph->addEdge("GBufferRasterPass.emissive", "MinimalPathTracerPass.mtlEmissive");
    mpRenderGraph->addEdge("GBufferRasterPass.matlExtra", "MinimalPathTracerPass.mtlParams");

    mpRenderGraph->addEdge("MinimalPathTracerPass.color", "AccumulatePass.input");
*/
    //mpRenderGraph->addEdge("SkyBoxPass.target", "AccumulatePass.input");

    mpRenderGraph->markOutput("AccumulatePass.output");
    
    LOG_ERR("createRenderGraph done");

}

bool Renderer::loadScript(const std::string& file_name) {
    return true;

	try {
        LLOG_DBG << "Loading frame graph configuration: " << file_name;
        auto ctx = Falcor::Scripting::getGlobalContext();
        ctx.setObject("renderer", this);
        Falcor::Scripting::runScriptFromFile(file_name, ctx);
    } catch (const std::exception& e) {
        LLOG_ERR << "Error when loading configuration file: " << file_name << "\n" + std::string(e.what());
    	return false;
    }

    LLOG_DBG << "Frame graph configuration loaded!";
    return true;
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

static CPUSampleGenerator::SharedPtr createSamplePattern(RendererIface::SamplePattern type, uint32_t sampleCount) {
    switch (type) {
        case RendererIface::SamplePattern::Center:
            return nullptr;
        case RendererIface::SamplePattern::DirectX:
            return DxSamplePattern::create(sampleCount);
        case RendererIface::SamplePattern::Halton:
            return HaltonSamplePattern::create(sampleCount);
        case RendererIface::SamplePattern::Stratified:
            return StratifiedSamplePattern::create(sampleCount);
        default:
            should_not_get_here();
            return nullptr;
    }
}

void Renderer::finalizeScene(const RendererIface::FrameData& frame_data) {
    // finalize camera
    mInvFrameDim = 1.f / float2({mGlobalData.imageWidth, mGlobalData.imageHeight});

    mpSampleGenerator = createSamplePattern(mGlobalData.samplePattern, mGlobalData.imageSamples);
    if (mpSampleGenerator) {
        mpCamera->setPatternGenerator(mpSampleGenerator, mInvFrameDim);
    }

    mpCamera->setAspectRatio(static_cast<float>(mGlobalData.imageWidth) / static_cast<float>(mGlobalData.imageHeight));
    mpCamera->setNearPlane(frame_data.cameraNearPlane);
    mpCamera->setFarPlane(frame_data.cameraFarPlane);
    mpCamera->setViewMatrix(frame_data.cameraTransform);
    mpCamera->setFocalLength(frame_data.cameraFocalLength);
    mpCamera->setFrameHeight(frame_data.cameraFrameHeight);
    //mpCamera->beginFrame(true); // Not sure we need it

    // finalize scene
    auto pScene = mpSceneBuilder->getScene();

    if (pScene) {
        pScene->setCameraAspectRatio(static_cast<float>(mGlobalData.imageWidth) / static_cast<float>(mGlobalData.imageHeight));

        if (mpSampler == nullptr) {
            // create common texture sampler
            Sampler::Desc desc;
            desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Linear, Sampler::Filter::Linear);
            desc.setLodParams(0,16,0);
            desc.setMaxAnisotropy(8);
            mpSampler = Falcor::Sampler::create(mpDevice, desc);
        }
        //pScene->bindSamplerToMaterials(mpSampler);
    }
}

void Renderer::renderFrame(const RendererIface::FrameData frame_data) {

	if (!mInited) {
		LLOG_ERR << "Renderer not initialized !!!";
		return;
	}

    if (!mGlobalDataInited) {
        LLOG_ERR << "Renderer global data not initialized !!!";
        return;
    }

    if(!mpDisplay && (mDisplayData.displayType != Display::DisplayType::__HYDRA__)) {
        LLOG_ERR << "Renderer display not initialized !!!";
        return;
    }

    if( mGlobalData.imageSamples == 0) {
        LLOG_WRN << "Not enough image samples specified !!!";
    }

    uint hImage;

    if(mpDisplay) {
        mpDisplay->closeAll(); // close previous frame display images (if still opened)

        std::vector<Display::Channel> channels;
        channels.push_back({"r", Display::TypeFormat::FLOAT16});
        channels.push_back({"g", Display::TypeFormat::FLOAT16});
        channels.push_back({"b", Display::TypeFormat::FLOAT16});
        channels.push_back({"a", Display::TypeFormat::FLOAT16});
        

        if(!mpDisplay->openImage(frame_data.imageFileName, mGlobalData.imageWidth, mGlobalData.imageHeight, channels, hImage)) {
            LLOG_FTL << "Unable to open image " << frame_data.imageFileName << " !!!";
            return;
        }
    }


    finalizeScene(frame_data);

    createRenderGraph();

    if (!mpRenderGraph) {
        LLOG_ERR << "Renderer global data not initialized !!!";
        return; 
    }

    LLOG_DBG << "Renderer::renderFrame";

    LLOG_DBG << "process render graph(s)";
    
    auto pScene = mpSceneBuilder->getScene();
    if (!pScene) {
        LLOG_ERR << "Unable to get scene from scene builder !!!";
        return;
    }

    auto pRenderContext = mpDevice->getRenderContext();

    // TODO: set passes parameters in a more unified way

    uint32_t frameNumber = 0; // for now

    // render image samples
    double shutter_length = 0.5;
    double fps = 25.0;
    double time = frame_data.time;
    double sample_time_duration = (1.0 * shutter_length) / mGlobalData.imageSamples;
    
    //resolvePerFrameSparseResourcesForActiveGraph(pRenderContext);
    pScene->update(pRenderContext, time);

    if(mpTexturesResolvePassGraph) {
        mpTexturesResolvePassGraph->execute(pRenderContext);
    }

    mpRenderGraph->execute(pRenderContext, frameNumber, 0);

    if ( mGlobalData.imageSamples > 1 ) {
        for (uint sampleNumber = 1; sampleNumber < mGlobalData.imageSamples; sampleNumber++) {
            LLOG_DBG << "Rendering sample no " << sampleNumber << " of " << mGlobalData.imageSamples;
            
            // Update scene and camera.
            time += sample_time_duration;
            pScene->update(pRenderContext, time);
            
            mpRenderGraph->execute(pRenderContext, frameNumber, sampleNumber);
        }
    }

    LLOG_DBG << "Rendering done.";

    // capture graph(s) ouput(s).
    LLOG_DBG << "Reading rendered image data...";
    auto& pGraph = mGraphs[mActiveGraph].pGraph;

    const auto pResource = mpRenderGraph->getOutput("AccumulatePass.output");

    if(!pResource) {
        LLOG_FTL << "No output resource found !";
        return;
    }

    auto pOutputTexture = pResource->asTexture();
    if(!pOutputTexture) {
        LLOG_FTL << "Error getting output resource texture !";
        return;
    }

    {
        Falcor::ResourceFormat outputResourceFormat;
        uint32_t outputChannelsCount = 0;
        
        LLOG_DBG << "readTextureData";
        if( mpDisplay ) {
            // PRman display
            std::vector<uint8_t> textureData;
            
            assert(mGlobalData.imageWidth == pOutputTexture->getWidth(0));
            assert(mGlobalData.imageHeight == pOutputTexture->getHeight(0));

            Falcor::ResourceFormat outputTextureFormat = pOutputTexture->getFormat();

            textureData.resize( mGlobalData.imageWidth * mGlobalData.imageHeight * Falcor::getFormatBytesPerBlock(outputTextureFormat));
            
            pOutputTexture->readTextureData(0, 0, textureData, outputResourceFormat, outputChannelsCount);
            LLOG_DBG << "Texture read data size is: " << textureData.size() << " bytes";

            try {
                if (!mpDisplay->sendImage(hImage, mGlobalData.imageWidth, mGlobalData.imageHeight, textureData.data())) {
                    LLOG_ERR << "Error sending image to display !";
                } else {
                    LLOG_DBG << "Image sent to display succcessfuly!";
                }
            } catch (std::exception& e) {
                LLOG_ERR << "Error: " << e.what();
            }

            mpDisplay->closeImage(hImage);

        } else {
            // __HYDRA__ direct data copy
            if(mDisplayData.pDstData) {
                pOutputTexture->readTextureData(0, 0, mDisplayData.pDstData, outputResourceFormat, outputChannelsCount);
            }
        }

    }

    return;
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

bool  Renderer::queryAOVGeometry(const std::string& aov_name, AOVGeometry& aovGeometry) {
    auto const pResource = mpRenderGraph->getOutput("AccumulatePass.output");
    if (!pResource) {
        LLOG_ERR << "No AOV named \"" << aov_name << "\" exist in rendering graph !";
        return false;
    }

    auto const pTexture = pResource->asTexture();
    if (!pTexture) {
        LLOG_ERR << "Buffer AOV outputs not supported (yet) !";
        return false;
    }

    auto resourceFormat = pTexture->getFormat();

    aovGeometry.width = pTexture->getWidth(0);
    aovGeometry.height = pTexture->getHeight(0);
    aovGeometry.resourceFormat = resourceFormat;
    aovGeometry.bytesPerPixel = Falcor::getFormatBytesPerBlock(resourceFormat);
    aovGeometry.channelsCount = Falcor::getFormatChannelCount(resourceFormat);
    aovGeometry.bitsPerComponent[0] = Falcor::getNumChannelBits(resourceFormat, 0);
    aovGeometry.bitsPerComponent[1] = Falcor::getNumChannelBits(resourceFormat, 1);
    aovGeometry.bitsPerComponent[2] = Falcor::getNumChannelBits(resourceFormat, 2);
    aovGeometry.bitsPerComponent[3] = Falcor::getNumChannelBits(resourceFormat, 3);
}

// HYDRA section end

}  // namespace lava