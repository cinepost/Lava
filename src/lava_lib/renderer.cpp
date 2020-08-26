#include "renderer.h"

#include "Falcor/Utils/Scripting/Scripting.h"
#include "Falcor/Utils/Debug/debug.h"

namespace lava {

Renderer::UniquePtr Renderer::create() {
	return UniquePtr(new Renderer());
}

Renderer::Renderer() {
	LOG_DBG("Renderer::Renderer");
	Falcor::Scripting::start();

	Falcor::Device::Desc device_desc;
    device_desc.width = 1280;
    device_desc.height = 720;

	mpDevice = Falcor::DeviceManager::instance().createRenderingDevice(0, device_desc);
}

Renderer::~Renderer() {
	LOG_DBG("Renderer::~Renderer");
	Falcor::Scripting::shutdown();
}

}  // namespace lava