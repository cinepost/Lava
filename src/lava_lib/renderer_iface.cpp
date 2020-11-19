#include <regex>

#include "renderer.h"
#include "renderer_iface.h"

#include "lava_utils_lib/ut_string.h"
#include "lava_utils_lib/logging.h"

namespace lava {

RendererIface::RendererIface(Renderer *renderer): mpRenderer(renderer) {
	// TODO: fill envmap with system wide variables
	mEnvmap["LAVA_HOME"] = getenv("LAVA_HOME");
}

RendererIface::~RendererIface() {
	LLOG_DBG << "RendererIface::~RendererIface";
	if (mpRenderer) delete mpRenderer;
	LLOG_DBG << "RendererIface::~RendererIface done";
}

void RendererIface::setEnvVariable(const std::string& key, const std::string& value){
	LLOG_DBG << "setEnvVariable: " << key << " : " << value;
	mEnvmap[key] = value;
}

std::string RendererIface::getExpandedString(const std::string& s) {
	std::string result = s;

	for( auto const& [key, val] : mEnvmap )
		result = ut::string::replace(result, '$' + key, val);

	return result;
}

std::shared_ptr<SceneBuilder> RendererIface::getSceneBuilder(){ return mpRenderer->mpSceneBuilder; }

bool RendererIface::openDisplay(const std::string& image_name, uint width, uint height) {
	return mpRenderer->openDisplay(getExpandedString(image_name), width, height);
}

bool RendererIface::loadScriptFile(const std::string& file_name) {
	if(!mpRenderer->isInited()) {
		LLOG_ERR << "Unable lo load script! Renderer is NOT initialized !!!";
		return false;
	}
	return mpRenderer->loadScript(getExpandedString(file_name));
}

void RendererIface::loadDeferredScriptFile(const std::string& file_name) {
	mDeferredScriptFileNames.push_back(file_name);
}

bool RendererIface::initRenderer() {
	LLOG_DBG << "initRenderer";
	if(mpRenderer->isInited())
		return true;

	return mpRenderer->init();
}

bool RendererIface::setDisplay(const DisplayData& display_data) {
	if(!mpRenderer->loadDisplay(display_data.displayType)) {
		return false;
	}

	auto pDisplay = mpRenderer->display();

	// push display driver parameters
	for(auto const& parm: display_data.displayStringParameters)
		pDisplay->setStringParameter(parm.first, parm.second);

	for(auto const& parm: display_data.displayIntParameters)
		pDisplay->setIntParameter(parm.first, parm.second);

	for(auto const& parm: display_data.displayFloatParameters)
		pDisplay->setFloatParameter(parm.first, parm.second);

	return true;
}

bool RendererIface::isRendererInitialized() const {
	return mpRenderer->isInited();
}

void RendererIface::renderFrame(const FrameData& frame_data) {
	LLOG_DBG << "RendererIfaceBase::renderFrame";
	if(!mpRenderer->isInited()) {
		LLOG_ERR << "Unable lo render frame! Renderer is NOT initialized !!!";
		return;
	}

	// load deferred scripts
	for( auto const& file_name: mDeferredScriptFileNames ) {
		if(!loadScriptFile(file_name)) {
			LLOG_ERR << "Error loading deferred script file " << file_name;
		}
	}

	if(!mpRenderer->display()) {
		LLOG_ERR << "Renderer display is not ready !!!";
		return;
	}

	mpRenderer->renderFrame(frame_data);
	mpRenderer->closeDisplay();
}

}  // namespace lava
