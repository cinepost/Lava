#ifndef LAVA_RENDERER_H_
#define LAVA_RENDERER_H_

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "types.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/DeviceManager.h"

namespace lava {

class Renderer {
 public:
 	~Renderer();
 	using UniquePtr = std::unique_ptr<Renderer>;

 public:
 	static UniquePtr create();

 private:
 	Renderer();
 	std::vector<std::string> 	mErrorMessages;
 	Falcor::Device::SharedPtr 	mpDevice;

};

}  // namespace Lava

#endif  // LAVA_RENDERER_H_
