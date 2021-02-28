#ifndef SRC_FALCOR_CORE_API_EMBREE_EMBREEDEVICE_H_
#define SRC_FALCOR_CORE_API_EMBREE_EMBREEDEVICE_H_

#include <memory>
#include "embree/include/embree3/rtcore.h"

namespace Falcor {

class EmbreeScene;

class EmbreeDevice: public std::enable_shared_from_this<EmbreeDevice>  {

public:
	using SharedPtr = std::shared_ptr<EmbreeDevice>;
	static SharedPtr create();

	~EmbreeDevice();

	const RTCDevice embreeDevice() const { return mEmbDevice; }

private:
	EmbreeDevice();

	RTCDevice mEmbDevice;
};

}  // namspace Falcor

#endif  // SRC_FALCOR_CORE_API_EMBREE_EMBREEDEVICE_H_