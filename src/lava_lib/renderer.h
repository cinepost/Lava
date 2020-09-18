#ifndef SRC_LAVA_LIB_RENDERER_H_
#define SRC_LAVA_LIB_RENDERER_H_

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "types.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DeviceManager.h"

#include "renderer_iface_base.h"

namespace lava {

class Renderer {
 public:
 	~Renderer();
 	using UniquePtr = std::unique_ptr<Renderer>;

	std::unique_ptr<RendererIfaceBase> 	aquireInterface();
 	void						 		releaseInterface(std::unique_ptr<RendererIfaceBase> pInterface);

 public:
 	static UniquePtr create();

private:
	Renderer();
 	std::vector<std::string> 	mErrorMessages;
 	Falcor::Device::SharedPtr 	mpDevice;

 	//std::unique_ptr<RendererIfaceBase> mpInterface;
 	bool mIfaceAquired = false;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_H_
