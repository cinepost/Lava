#ifndef SRC_LAVA_LIB_SCENE_H_
#define SRC_LAVA_LIB_SCENE_H_

#include <map>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Scene/SceneBuilder.h" 

#include "reader_bgeo/bgeo/Bgeo.h"

using namespace Falcor;

namespace lava {

class SceneBuilder: public Falcor::SceneBuilder {
 public:
    using SharedPtr = std::shared_ptr<lava::SceneBuilder>;
    using Flags = Falcor::SceneBuilder::Flags;

    static SharedPtr create(Falcor::Device::SharedPtr pDevice, Flags buildFlags = Flags::Default);

    Falcor::Scene::SharedPtr getScene();

    uint32_t addMesh(const ika::bgeo::Bgeo& bgeo, const std::string& name = "");

    void finalize();

 private:
    SceneBuilder(Falcor::Device::SharedPtr pDevice, Flags buildFlags = Flags::Default);

 private:
    Material::SharedPtr mpDefaultMatreial = nullptr;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENE_H_
