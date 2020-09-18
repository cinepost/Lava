#include <regex>

#include "renderer_iface_base.h"
#include "lava_utils_lib/ut_string.h"
#include "lava_utils_lib/logging.h"

namespace lava {

RendererIfaceBase::RendererIfaceBase(Renderer *renderer): mpRenderer(renderer) {
	// TODO: fill envmap with system wide variables
}

RendererIfaceBase::~RendererIfaceBase() { }

void RendererIfaceBase::setEnvVariable(const std::string& key, const std::string& value){
	LLOG_DBG << "setEnvVariable: " << key << " : " << value;
	mEnvmap[key] = value;
	LLOG_DBG << "set!\n";
}

std::string RendererIfaceBase::getExpandedString(const std::string& s) {
	std::string result = s;

	for( auto const& [key, val] : mEnvmap )
		result = ut::string::replace(result, key, val);

	return result;
}

}  // namespace lava
