#include <string>
#include <vector>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "scene_readers_registry.h"

#include "lava_utils_lib/logging.h"


namespace lava {

void SceneReadersRegistry::addReader(ReaderExtensions extensions, ReaderConstructor constructor) {
	std::string registered_extensions = "";
	for (auto ext : *extensions()) {
		readersByExtension.insert ( std::pair<std::string, ReaderConstructor>(ext, constructor) );
		registered_extensions += " " + ext;
	}
	LOG_DBG << "SceneReader registered for extensions: " << registered_extensions;
}

ReaderBase::SharedPtr SceneReadersRegistry::getReaderByExt(const std::string& ext) {
	if ( readersByExtension.find(ext) == readersByExtension.end() ) {
		// no reader registered for this extention
		LOG_ERR << "No reader registered for extention: " << ext;
		return nullptr;
	}
	return readersByExtension[ext]();
}

}