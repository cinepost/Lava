#include "EmbreeDevice.h"

namespace Falcor {

static void errorFunction(void* userPtr, enum RTCError error, const char* str) {
  printf("Embree error %d: %s\n", error, str);
}

EmbreeDevice::SharedPtr EmbreeDevice::create() {
	auto pDevice = new EmbreeDevice();

	pDevice->mEmbDevice = rtcNewDevice(NULL);
	if (!pDevice->mEmbDevice) {
    	printf("error %d: cannot create device\n", rtcGetDeviceError(NULL));
		return nullptr;
	}

  rtcSetDeviceErrorFunction(mEmbDevice, errorFunction, NULL);
  return SharedPtr(pDevice);
}

EmbreeDevice::EmbreeDevice() {

}

EmbreeDevice::~EmbreeDevice() {
 	rtcReleaseDevice(mEmbDevice);
}

}  // namespace Falcor