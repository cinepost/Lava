#ifndef SRC_LAVA_LIB_SCENE_H_
#define SRC_LAVA_LIB_SCENE_H_

#include <map>
#include <future>
#include <atomic>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Scene/SceneBuilder.h" 

#include "Falcor/Core/API/Texture.h"

#include "reader_bgeo/bgeo/Bgeo.h"

using namespace Falcor;

namespace lava {

class SceneBuilder: public Falcor::SceneBuilder {
 public:
    using SharedPtr = std::shared_ptr<lava::SceneBuilder>;
    using Flags = Falcor::SceneBuilder::Flags;

    static SharedPtr create(Falcor::Device::SharedPtr pDevice, Flags buildFlags = Flags::Default);

    Falcor::Scene::SharedPtr getScene();


    uint32_t addGeometry(ika::bgeo::Bgeo::SharedConstPtr pBgeo, const std::string& name = "");
    std::shared_future<uint32_t> addGeometryAsync(ika::bgeo::Bgeo::SharedConstPtr pBgeo, const std::string& name = "");

    void finalize();

    ~SceneBuilder();

 private:
    SceneBuilder(Falcor::Device::SharedPtr pDevice, Flags buildFlags = Flags::Default);

 private:
    Material::SharedPtr mpDefaultMaterial = nullptr;

    std::atomic<uint32_t> mUniqueTrianglesCount = 0;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENE_H_
