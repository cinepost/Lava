#ifndef SRC_LAVA_LIB_SCENE_READERS_REGISTRY_H_
#define SRC_LAVA_LIB_SCENE_READERS_REGISTRY_H_

#include <string>
#include <vector>
#include <map>

#include "scene_reader_base.h"

namespace lava {

typedef std::vector<std::string> *(*ReaderExtensions)();
typedef ReaderBase::SharedPtr (*ReaderConstructor)();

class LAVA_API SceneReadersRegistry {
 public:
	static SceneReadersRegistry& getInstance() {
		static SceneReadersRegistry instance;
		return instance;
	}

    SceneReadersRegistry(SceneReadersRegistry const&) = delete;
    void operator=(SceneReadersRegistry const&) = delete;

	void	          		addReader(ReaderExtensions extensions, ReaderConstructor constructor);
	ReaderBase::SharedPtr	getReaderByExt(const std::string& ext);

 private:
	SceneReadersRegistry() {};
	std::map<std::string, ReaderConstructor> readersByExtension;
};


}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENE_READERS_REGISTRY_H_
