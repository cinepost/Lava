#include <string>
#include <vector>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "renderer_input_registry.h"

#include "lava_utils_lib/logging.h"


namespace lava {

void SceneReadersRegistry::addReader(ReaderExtensions extensions, ReaderConstructor constructor) {
	std::string registered_extensions = "";
	for (auto ext : *extensions()) {
		translatorsByExtension.insert ( std::pair<std::string, ReaderConstructor>(ext, constructor) );
		registered_extensions += " " + ext;
	}
	LOG_DBG("SceneReader registered for extensions: %s", registered_extensions);
}

ReaderBase *SceneReadersRegistry::getReaderByExt(const std::string& ext) {
	return translatorsByExtension[ext]();
}

}