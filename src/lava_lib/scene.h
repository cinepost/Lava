#ifndef SRC_LAVA_LIB_SCENE_H_
#define SRC_LAVA_LIB_SCENE_H_

#include <map>

#include "Falcor/Scene/SceneBuilder.h"
#include "Falcor/Core/API/Device.h"

#include "reader_bgeo/bgeo/Bgeo.h"

namespace lava {


class Scene {
 public:
    using SharedPtr = std::shared_ptr<Scene>;

    SharedPtr create(Falcor::Device::SharedPtr pDevice, Falcor::SceneBuilder::Flags buildFlags = Falcor::SceneBuilder::Flags::Default);

    Falcor::Scene::SharedPtr getScene() { return mpSceneBuilder->getScene(); };

    uint32_t addMesh(const ika::bgeo::Bgeo& bgeo);

 private:
    Scene();

 
 private:
    Falcor::SceneBuilder::SharedPtr mpSceneBuilder;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENE_H_
