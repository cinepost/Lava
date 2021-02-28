#include "EmbreeScene.h"
#include "EmbreeDevice.h"

namespace Falcor {

EmbreeScene::SharedPtr EmbreeScene::create(std::shared_ptr<EmbreeDevice> pDevice) {
	auto pScene = new EmbreeScene(pDevice);

	pScene->mEmbScene = rtcNewScene(pDevice->embreeDevice());
	if (!pScene->mEmbScene) {
    	printf("Cannot create embree scene");
		return nullptr;
	}

	return SharedPtr(pScene);
}

EmbreeScene::EmbreeScene(std::shared_ptr<EmbreeDevice> pDevice): mpDevice(pDevice) {

}

EmbreeScene::~EmbreeScene() {
 	rtcReleaseScene(mEmbScene);
}

}  // namespace Falcor