#include <memory>
#include <array>

#include <dlfcn.h>
#include <stdlib.h>
#include <boost/format.hpp>

#include "display.h"
#include "lava_utils_lib/logging.h"

namespace lava {

const Display::ChannelNamingDesc kChannelNameDesc[] = {
	// NamingScheme                             ChannelNames,
	{ Display::NamingScheme::RGBA,                        {"r", "g", "b", "a"}},
	{ Display::NamingScheme::XYZW,                        {"x", "y", "z", "w"}},
};

const std::string& Display::makeChannelName(Display::NamingScheme namingScheme, uint32_t channelIndex) {
	assert(kChannelNameDesc[(uint32_t)namingScheme].namingScheme == namingScheme);
	return kChannelNameDesc[(uint32_t)namingScheme].channelNames[channelIndex];
}


bool Display::setStringParameter(const std::string& name, const std::vector<std::string>& strings) {
	return false;
}

bool Display::setIntParameter(const std::string& name, const std::vector<int>& ints) {
  return false;
}

bool Display::setFloatParameter(const std::string& name, const std::vector<float>& floats) {
	return false;
}

}  // namespace lava