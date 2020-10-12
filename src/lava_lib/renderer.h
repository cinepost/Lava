#ifndef SRC_LAVA_LIB_RENDERER_H_
#define SRC_LAVA_LIB_RENDERER_H_

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "types.h"

#include "Falcor/Falcor.h"
#include "Falcor/FalcorExperimental.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DeviceManager.h"
#include "Falcor/Utils/Timing/FrameRate.h"
#include "Falcor/Core/Renderer.h"

#include "display.h"
#include "renderer_iface.h"
#include "scene.h"

namespace lava {

class Renderer: public Falcor::IFramework {
	struct GraphData {
        Falcor::RenderGraph::SharedPtr pGraph;
        std::string mainOutput;
        bool showAllOutputs = false;
        std::vector<std::string> originalOutputs;
        std::unordered_map<std::string, uint32_t> graphOutputRefs;
	};

 public:
 	virtual ~Renderer();
 	using UniquePtr = std::unique_ptr<Renderer>;

	std::unique_ptr<RendererIface> 	aquireInterface();
 	void						 	releaseInterface(std::unique_ptr<RendererIface> pInterface);

 public:
 	static UniquePtr create();

 public:
 	bool init();
 	bool isInited() const { return mInited; }
 	bool loadDisplayDriver(const std::string& display_name);
    bool openDisplay(const std::string& image_name, uint width, uint height);
    bool closeDisplay();
 	bool loadScript(const std::string& file_name);

    bool pushDisplayStringParameter(const std::string& name, const std::vector<std::string>& strings);
    bool pushDisplayIntParameter(const std::string& name, const std::vector<int>& ints);
    bool pushDisplayFloatParameter(const std::string& name, const std::vector<float>& floats);

 	void renderFrame(uint samples = 16);

 	void registerScriptBindings(Falcor::ScriptBindings::Module& m);

 private:
 	void addGraph(const Falcor::RenderGraph::SharedPtr& pGraph);
 	void initGraph(const Falcor::RenderGraph::SharedPtr& pGraph, GraphData* pData);
 	std::vector<std::string> getGraphOutputs(const Falcor::RenderGraph::SharedPtr& pGraph);
 	void executeActiveGraph(Falcor::RenderContext* pRenderContext);
 	void beginFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo);
 	void endFrame(Falcor::RenderContext* pRenderContext, const Falcor::Fbo::SharedPtr& pTargetFbo);

 // IFramework
 public:
 	/** Get the render-context for the current frame. This might change each frame*/
    virtual Falcor::RenderContext* getRenderContext() override;

    /** Get the current FBO*/
    virtual std::shared_ptr<Falcor::Fbo> getTargetFbo() override;

    /** Get the window*/
    virtual Falcor::Window* getWindow() override { return nullptr; };

    /** Get the global Clock object
    */
    virtual Falcor::Clock& getClock() override;

    /** Get the global FrameRate object
    */
    virtual Falcor::FrameRate& getFrameRate() override;

    /** Resize the swap-chain buffers*/
    virtual void resizeSwapChain(uint32_t width, uint32_t height) override;

    /** Check if a key is pressed*/
    virtual bool isKeyPressed(const Falcor::KeyboardEvent::Key& key) override { return false; };

    /** Show/hide the UI */
    virtual void toggleUI(bool showUI) override {};

    /** Show/hide the UI */
    virtual bool isUiEnabled() override { return false; };

    /** Get the object storing command line arguments */
    virtual Falcor::ArgList getArgList() override { return mArgList; };

    /** Takes and outputs a screenshot.
    */
    virtual std::string captureScreen(const std::string explicitFilename = "", const std::string explicitOutputDirectory = "") override { return ""; };

    /* Shutdown the app
    */
    virtual void shutdown() override {};

    /** Pause/resume the renderer. The GUI will still be rendered
    */
    virtual void pauseRenderer(bool pause) override {};

    /** Check if the renderer running
    */
    virtual bool isRendererPaused() override {return false; };

    /** Get the current configuration
    */
    virtual Falcor::SampleConfig getConfig() override;

    /** Render the global UI. You'll can open a GUI window yourself before calling it
    */
    virtual void renderGlobalUI(Falcor::Gui* pGui) override {};

    /** Get the global shortcuts message
    */
    virtual std::string getKeyboardShortcutsStr() override { return ""; };

    /** Set VSYNC
    */
    virtual void toggleVsync(bool on) override {};

    /** Get the VSYNC state
    */
    virtual bool isVsyncEnabled() override{ return false; };

 private:
	Renderer();
 	std::vector<std::string> 	mErrorMessages;
 	Falcor::Device::SharedPtr 	mpDevice;

 	//std::unique_ptr<RendererIfaceBase> mpInterface;
 	bool mIfaceAquired = false;

 	Display::UniquePtr 			mpDisplay;

 	Falcor::Fbo::SharedPtr 		mpTargetFBO;		///< The FBO available to renderers
 	Falcor::FrameRate*			mpFrameRate;
    Falcor::Clock* 	   			mpClock;
    Falcor::ArgList 			mArgList;

    lava::Scene::SharedPtr      mpScene;
    std::vector<GraphData>      mGraphs;
    uint32_t mActiveGraph = 0;

    bool mInited = false;

    friend class RendererIface;
}; 

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_H_
