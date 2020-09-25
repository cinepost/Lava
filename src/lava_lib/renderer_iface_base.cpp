#include <regex>

#include "renderer.h"
#include "renderer_iface_base.h"

#include "lava_utils_lib/ut_string.h"
#include "lava_utils_lib/logging.h"

namespace lava {

RendererIfaceBase::RendererIfaceBase(Renderer *renderer): mpRenderer(renderer) {
	// TODO: fill envmap with system wide variables
	mEnvmap["$LAVA_HOME"] = getenv("LAVA_HOME");
}

RendererIfaceBase::~RendererIfaceBase() { }

void RendererIfaceBase::setEnvVariable(const std::string& key, const std::string& value){
	LLOG_DBG << "setEnvVariable: " << key << " : " << value;
	mEnvmap[key] = "$" + value; // prepend $ so it's easier to replace later
}

std::string RendererIfaceBase::getExpandedString(const std::string& s) {
	std::string result = s;

	for( auto const& [key, val] : mEnvmap )
		result = ut::string::replace(result, key, val);

	return result;
}

bool RendererIfaceBase::loadDisplay(const std::string& display_name) {
	return mpRenderer->loadDisplayDriver(display_name);
}

bool RendererIfaceBase::loadScript(const std::string& file_name) {
	if(!mpRenderer->isInited()) return false;
	return mpRenderer->loadScript(getExpandedString(file_name));
}

bool RendererIfaceBase::initRenderer() {
	LLOG_DBG << "initRenderer";
	return mpRenderer->init();
}

void RendererIfaceBase::renderFrame() {
	LLOG_DBG << "RendererIfaceBase::renderFrame";
	mpRenderer->init();
	mpRenderer->renderFrame();
}

}  // namespace lava
