#include <regex>

#include "renderer_iface_base.h"
#include "lava_utils_lib/string.h"

namespace lava {

RendererIfaceBase::RendererIfaceBase() {
	// TODO: fill envmap with system wide variables
}

void RendererIfaceBase::setEnvVariable(const std::string& key, const std::string& value){
	envmap[key] = value;
}

std::string RendererIfaceBase::getExpandedString(const std::string& s) {
	std::string result = s;

	for( auto const& [key, val] : envmap )
		result = ut::string::replace(result, key, val);

	return result;
}

}  // namespace lava
