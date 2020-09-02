#ifndef SRC_LAVA_LIB_SCENE_READERS_REGISTRY_H_
#define SRC_LAVA_LIB_SCENE_READERS_REGISTRY_H_

#include <string>
#include <vector>
#include <map>

#include "scene_reader_base.h"

namespace lava {

typedef std::vector<std::string> *(*ReaderExtensions)();
typedef SceneReaderBase::SharedPtr *(*ReaderConstructor)();

class SceneReadersRegistry {
 public:
	static SceneReadersRegistry& getInstance() {
		static SceneReadersRegistry instance;
		return instance;
	}

    SceneReadersRegistry(SceneReadersRegistry const&) = delete;
    void operator=(SceneReadersRegistry const&) = delete;

	void	          	addReader(ReaderExtensions extensions, ReaderConstructor constructor);
	SCN_IOTranslator*	getReaderByExt(const std::string& ext);

 private:
	SceneReadersRegistry() {};
	std::map<std::string, ReaderConstructor> translatorsByExtension;
};


}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENE_READERS_REGISTRY_H_
