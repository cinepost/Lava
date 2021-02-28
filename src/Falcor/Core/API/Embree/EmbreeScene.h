#ifndef SRC_FALCOR_CORE_API_EMBREE_EMBREESCENE_H_
#define SRC_FALCOR_CORE_API_EMBREE_EMBREESCENE_H_

#include <memory>
#include "embree/include/embree3/rtcore.h"


namespace Falcor {

class EmbreeDevice;

class EmbreeScene: public std::enable_shared_from_this<EmbreeScene> {

public:
	using SharedPtr = std::shared_ptr<EmbreeScene>;
	static SharedPtr create(std::shared_ptr<EmbreeDevice> pDevice);

	~EmbreeScene();

	const RTCScene embreeScene() const { return mEmbScene; }

protected:
	EmbreeScene(std::shared_ptr<EmbreeDevice> pDevice);

	std::shared_ptr<EmbreeDevice> mpDevice;
	RTCScene mEmbScene;

};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_EMBREE_EMBREESCENE_H_