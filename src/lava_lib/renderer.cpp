#include "renderer.h"

#include "Falcor/Utils/Threading.h"
#include "Falcor/Utils/Scripting/Scripting.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"
#include "Falcor/Utils/Debug/debug.h"

#include "lava_utils_lib/logging.h"

namespace Falcor { 

IFramework* gpFramework = nullptr;

}

namespace lava {

Renderer::UniquePtr Renderer::create() {
	return std::move(UniquePtr( new Renderer()));
}

Renderer::Renderer(): mIfaceAquired(false), mpClock(nullptr), mpFrameRate(nullptr), mActiveGraph(0), mInited(false) {
	LLOG_DBG << "Renderer::Renderer";
    LLOG_DBG << "Renderer::Renderer done!";
}

bool Renderer::init() {
	if(mInited) return true;

	LLOG_DBG << "Renderer::init";

	Falcor::OSServices::start();

	Falcor::Scripting::start();
    LLOG_DBG << "Register scripting";
    auto regBinding = [this](Falcor::ScriptBindings::Module& m) {this->registerScriptBindings(m); };
    Falcor::ScriptBindings::registerBinding(regBinding);

	Falcor::Threading::start();

	Falcor::Device::Desc device_desc;
    device_desc.width = 1280;
    device_desc.height = 720;

	mpDevice = Falcor::DeviceManager::instance().createRenderingDevice(0, device_desc);

	mpClock = new Falcor::Clock(mpDevice);
    //mpClock->setTimeScale(config.timeScale);

    mpFrameRate = new Falcor::FrameRate(mpDevice);

    Falcor::gpFramework = this;

    LLOG_DBG << "Getting offscreen FBO";
    auto pBackBufferFBO = mpDevice->getOffscreenFbo();
    LLOG_DBG << "Offscreen FBO get";
    if (!pBackBufferFBO) {
        LLOG_ERR << "Unable to get rendering device swapchain FBO!!!";
    }

    LLOG_DBG << "Created pBackBufferFBO size: " << pBackBufferFBO->getWidth() << " " << pBackBufferFBO->getHeight();
    mpTargetFBO = Falcor::Fbo::create2D(mpDevice, pBackBufferFBO->getWidth(), pBackBufferFBO->getHeight(), pBackBufferFBO->getDesc());
    LLOG_DBG << "Renderer::init done!";

    mInited = true;
    return true;
}

Renderer::~Renderer() {
	LLOG_DBG << "Renderer::~Renderer";

	if(!mInited)
		return;

	delete mpClock;
    delete mpFrameRate;
	
	Falcor::Threading::shutdown();
	Falcor::Scripting::shutdown();

	if(mpDisplay) mpDisplay->close();

	if(mpDevice) mpDevice->cleanup();
	mpDevice.reset();
    Falcor::OSServices::stop();

    Falcor::gpFramework = nullptr;
}

std::unique_ptr<RendererIfaceBase> Renderer::aquireInterface() {
	if (!mIfaceAquired) {
		return std::move(std::make_unique<RendererIfaceBase>(this));
	}
	LLOG_ERR << "cannot aquire renderer interface. relase old first!";
	return nullptr;
}

void Renderer::releaseInterface(std::unique_ptr<RendererIfaceBase> pInterface) {
	if(mIfaceAquired) {
		std::move(pInterface).reset();
		mIfaceAquired = false;
	}
}

bool Renderer::loadDisplayDriver(const std::string& display_name) {
	mpDisplay = Display::create(display_name);
	if(!mpDisplay)
		return false;

	return true;
}
/*
void Renderer::applyEditorChanges() {
    if (!mEditorProcess) return;
    // If the editor was closed, reset the handles
    if ((mEditorProcess != kInvalidProcessId) && isProcessRunning(mEditorProcess) == false) resetEditor();

    if (mEditorScript.empty()) return;

    // Unmark the current output if it wasn't originally marked
    auto pActiveGraph = mGraphs[mActiveGraph].pGraph;
    bool hasUnmarkedOut = (isInVector(mGraphs[mActiveGraph].originalOutputs, mGraphs[mActiveGraph].mainOutput) == false);
    if (hasUnmarkedOut) pActiveGraph->unmarkOutput(mGraphs[mActiveGraph].mainOutput);

    // Run the scripting
    Scripting::getGlobalContext().setObject("g", pActiveGraph);
    Scripting::runScript(mEditorScript);

    // Update the list of marked outputs
    mGraphs[mActiveGraph].originalOutputs = getGraphOutputs(pActiveGraph);

    // If the output before the update was not initially marked but still exists, re-mark it.
    // If it no longer exists, mark a new output from the list of currently marked outputs.
    if (hasUnmarkedOut && isInVector(pActiveGraph->getAvailableOutputs(), mGraphs[mActiveGraph].mainOutput)) {
        pActiveGraph->markOutput(mGraphs[mActiveGraph].mainOutput);
    } else if (isInVector(mGraphs[mActiveGraph].originalOutputs, mGraphs[mActiveGraph].mainOutput) == false) {
        mGraphs[mActiveGraph].mainOutput = mGraphs[mActiveGraph].originalOutputs[0];
    }

    mEditorScript.clear();
}
*/

bool isInVector(const std::vector<std::string>& strVec, const std::string& str) {
    return std::find(strVec.begin(), strVec.end(), str) != strVec.end();
}

bool Renderer::loadScript(const std::string& file_name) {
	//auto pGraph = Falcor::RenderGraph::create(mpDevice);

	try {
        LLOG_DBG << "Loading frame graph configuration: " << file_name;
        auto ctx = Falcor::Scripting::getGlobalContext();
        //ctx.setObject("g", pGraph);
        Falcor::Scripting::runScriptFromFile(file_name, ctx);
    } catch (const std::exception& e) {
        LLOG_ERR << "Error when loading configuration file: " << file_name << "\n" + std::string(e.what());
    	return false;
    }

    /*
    mGraphs.push_back({});
	GraphData* data = &mGraphs.back();

	data->pGraph = pGraph;
	data->pGraph->setScene(mpScene);
	if (data->pGraph->getOutputCount() != 0) data->mainOutput = data->pGraph->getOutputName(0);

	// Store the original outputs
    data->originalOutputs = getGraphOutputs(pGraph);

    //for (auto& e : mpExtensions) e->addGraph(pGraph.get());
	*/

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
    data.pGraph->setScene(mpScene);
    if (data.pGraph->getOutputCount() != 0) data.mainOutput = data.pGraph->getOutputName(0);

    // Store the original outputs
    data.originalOutputs = getGraphOutputs(pGraph);

    //for (auto& e : mpExtensions) e->addGraph(pGraph.get());
}

void Renderer::executeActiveGraph(Falcor::RenderContext* pRenderContext) {
    if (mGraphs.empty()) return;

    auto& pGraph = mGraphs[mActiveGraph].pGraph;
    LLOG_DBG << "Execute graph: " << pGraph->getName() << " output name: " << mGraphs[mActiveGraph].mainOutput;

    // Execute graph.
    (*pGraph->getPassesDictionary())[Falcor::kRenderPassRefreshFlags] = (uint32_t)Falcor::RenderPassRefreshFlags::None;
    pGraph->execute(pRenderContext);
}

void Renderer::renderFrame() {
	if (!mInited) {
		LLOG_ERR << "Renderer not initialized!";
		return;
	}

    auto pRenderContext = mpDevice->getRenderContext();

    LLOG_DBG << "Renderer::renderFrame";

    beginFrame(pRenderContext, mpTargetFBO);

    // Clear frame buffer.
    const Falcor::float4 clearColor(0.52f, 0.38f, 0.10f, 1);
    pRenderContext->clearFbo(mpTargetFBO.get(), clearColor, 1.0f, 0, Falcor::FboAttachmentType::All);

    if (mGraphs.size()) {
        LLOG_DBG << "process render graphs";
        auto& pGraph = mGraphs[mActiveGraph].pGraph;

        // Update scene and camera.
        if (mpScene) {
            mpScene->update(pRenderContext, Falcor::gpFramework->getClock().getTime());
        }

        executeActiveGraph(pRenderContext);

        
        // Blit main graph output to frame buffer.
        if (mGraphs[mActiveGraph].mainOutput.size()) {
            Falcor::Texture::SharedPtr pOutTex = std::dynamic_pointer_cast<Falcor::Texture>(pGraph->getOutput(mGraphs[mActiveGraph].mainOutput));
            assert(pOutTex);

            LLOG_DBG << "blit local";
            pRenderContext->blit(pOutTex->getSRV(), mpTargetFBO->getRenderTargetView(0));

            
            // image save test
            Falcor::Texture* pTex = pOutTex.get();//pGraph->getOutput(i)->asTexture().get();
            assert(pTex);
            std::string filename = "/home/max/test/lava_render_test.";
            auto ext = Falcor::Bitmap::getFileExtFromResourceFormat(pTex->getFormat());
            filename += ext;
            auto format = Falcor::Bitmap::getFormatFromFileExtension(ext);
            pTex->captureToFile(0, 0, filename, format);
        } else {
        	LLOG_WRN << "Invalid active graph output!";
        }

    } else {
    	LLOG_WRN << "No graphs to render!";
    }

    endFrame(pRenderContext, mpTargetFBO);
}

void Renderer::beginFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo) {
    //for (auto& pe : mpExtensions)  pe->beginFrame(pRenderContext, pTargetFbo);
}

void Renderer::endFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo) {
    //for (auto& pe : mpExtensions) pe->endFrame(pRenderContext, pTargetFbo);
}

// IFramework 
Falcor::RenderContext* Renderer::getRenderContext() {
	return mpDevice ? mpDevice->getRenderContext() : nullptr;
}

std::shared_ptr<Falcor::Fbo> Renderer::getTargetFbo() {
	return mpTargetFBO;
}

Falcor::Clock& Renderer::getClock() {
	return *mpClock;
}

Falcor::FrameRate& Renderer::getFrameRate() {
	return *mpFrameRate;
}

void Renderer::resizeSwapChain(uint32_t width, uint32_t height) {

}

Falcor::SampleConfig Renderer::getConfig() {
    Falcor::SampleConfig c;
    c.deviceDesc = mpDevice->getDesc();
    //c.windowDesc = mpWindow->getDesc();
    c.showMessageBoxOnError = false;//Logger::isBoxShownOnError();
    c.timeScale = (float)mpClock->getTimeScale();
    c.pauseTime = mpClock->isPaused();
    c.showUI = false;
    return c;
}

}  // namespace lava