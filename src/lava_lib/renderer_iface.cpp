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

RendererIface::~RendererIface() { }

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

bool RendererIface::loadDisplay(const std::string& display_name) {
	return mpRenderer->loadDisplayDriver(getExpandedString(display_name));
}

bool RendererIface::openDisplay(const std::string& image_name, uint width, uint height) {
	return mpRenderer->openDisplay(getExpandedString(image_name), width, height);
}

bool RendererIface::closeDisplay() {
	return mpRenderer->closeDisplay();
}

bool RendererIface::loadScriptFile(const std::string& file_name) {
	if(!mpRenderer->isInited()) {
		LLOG_ERR << "Unable lo load script! Renderer is NOT initialized !!!";
		return false;
	}
	return mpRenderer->loadScript(getExpandedString(file_name));
}

bool RendererIface::initRenderer() {
	LLOG_DBG << "initRenderer";
	if(mpRenderer->isInited())
		return true;

	return mpRenderer->init();
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

	for(auto const& parm: frame_data.displayStringParameters)
		mpRenderer->pushDisplayStringParameter(parm.first, parm.second);

	for(auto const& parm: frame_data.displayIntParameters)
		mpRenderer->pushDisplayIntParameter(parm.first, parm.second);

	for(auto const& parm: frame_data.displayFloatParameters)
		mpRenderer->pushDisplayFloatParameter(parm.first, parm.second);

	mpRenderer->openDisplay(frame_data.imageFileName, frame_data.imageWidth, frame_data.imageHeight);
	mpRenderer->renderFrame();
	mpRenderer->closeDisplay();
}

}  // namespace lava
