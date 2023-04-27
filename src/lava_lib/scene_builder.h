#ifndef SRC_LAVA_LIB_SCENEBUILDER_H_
#define SRC_LAVA_LIB_SCENEBUILDER_H_

#include "lava_dll.h"

#include <map>
#include <future>
#include <atomic>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Scene/SceneBuilder.h" 
#include "Falcor/Scene/Material/StandardMaterial.h" 
#include "Falcor/Utils/ThreadPool.h"

#include "Falcor/Core/API/Texture.h"

#include "reader_bgeo/bgeo/Bgeo.h"
#include "reader_lsd/scope.h"


using namespace Falcor;

namespace lava {

class LAVA_API SceneBuilder: public Falcor::SceneBuilder {
	public:
		using SharedPtr = std::shared_ptr<lava::SceneBuilder>;
		using Flags = Falcor::SceneBuilder::Flags;

		static SharedPtr create(Falcor::Device::SharedPtr pDevice, Flags buildFlags = Flags::Default);

		Falcor::Scene::SharedPtr getScene();


		uint32_t addGeometry(ika::bgeo::Bgeo::SharedConstPtr pBgeo, const std::string& name = "");
		std::shared_future<uint32_t> addGeometryAsync(lsd::scope::Geo::SharedConstPtr pGeo, const std::string& name = "");

		void finalize();

		~SceneBuilder();

	private:
		SceneBuilder(Falcor::Device::SharedPtr pDevice, Flags buildFlags = Flags::Default);

	private:
		StandardMaterial::SharedPtr mpDefaultMaterial = nullptr;

		std::atomic<uint32_t> mUniqueTrianglesCount = 0;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_SCENEBUILDER_H_
