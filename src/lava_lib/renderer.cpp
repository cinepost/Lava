#include "renderer.h"

#include "Falcor/Utils/Scripting/Scripting.h"
#include "Falcor/Utils/Debug/debug.h"

#include "lava_utils_lib/logging.h"

namespace lava {

Renderer::UniquePtr Renderer::create() {
	return std::move(UniquePtr( new Renderer));
}

Renderer::Renderer(): mIfaceAquired(false) {
	LLOG_DBG << "Renderer::Renderer";

	Falcor::Scripting::start();

	Falcor::Device::Desc device_desc;
    device_desc.width = 1280;
    device_desc.height = 720;

	mpDevice = Falcor::DeviceManager::instance().createRenderingDevice(0, device_desc);
}

Renderer::~Renderer() {
	LLOG_DBG << "Renderer::~Renderer";
	Falcor::Scripting::shutdown();
}

std::unique_ptr<RendererIfaceBase> Renderer::aquireInterface() {
	if (!mIfaceAquired) {
		return std::move(std::make_unique<RendererIfaceBase>(this));
	}
	LLOG_ERR << "cannot aquire renderer interface. relase old first!";
	return nullptr;
}

void Renderer::releaseInterface(std::unique_ptr<RendererIfaceBase> pInterface) {
	if(mIfaceAquired) {
		std::move(pInterface).reset();
		mIfaceAquired = false;
	}
}

}  // namespace lava