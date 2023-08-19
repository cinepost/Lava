/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include <pybind11/pybind11.h>

#include "Falcor/stdafx.h"

#include "Falcor/Utils/Debug/debug.h"
#include "lava_utils_lib/logging.h"

#include "DeviceManager.h"


namespace py = pybind11;

namespace Falcor {

DeviceManager::DeviceManager(): mInitialized(false), mEnableValidationLayer(false) {

}

DeviceManager::~DeviceManager() {
    for( auto& [id, pDevice]: mRenderingDevices ) {
        if (pDevice) {
            pDevice->cleanup();
            pDevice.reset();
        }
    }
}

//static
DeviceManager::SharedPtr DeviceManager::create(bool enableValidationLayer) {
    auto pDeviceManager = new DeviceManager();
    pDeviceManager->mEnableValidationLayer = enableValidationLayer;

    if (!pDeviceManager->init()) {
        delete pDeviceManager;
        return nullptr;
    }

#ifdef _DEBUG
    pDeviceManager->printEnumeratedDevices();
#endif

    return SharedPtr(pDeviceManager);
}

std::vector<Device::SharedPtr> DeviceManager::renderingDevices() const {
    std::vector<Device::SharedPtr> devices;
    for( auto& device: mRenderingDevices ) {
        devices.push_back(device.second);
    }
    return devices;
}

bool DeviceManager::deviceEnumerated(uint8_t gpuId) const {
    if(mDeviceNames.find(gpuId) != mDeviceNames.end()) {
        return true;
    }
    LLOG_ERR << "Device " << std::to_string(static_cast<uint16_t>(gpuId)) << "not enumerated !";
    return false;
}

Device::SharedPtr DeviceManager::renderingDevice(uint8_t gpuId) const {
    if (!deviceEnumerated(gpuId)) return nullptr;

    auto it = mRenderingDevices.find(gpuId);

    if (it == mRenderingDevices.end()) {
        // device not created yet ...
        return nullptr;
    }
    return it->second;
}


void DeviceManager::printEnumeratedDevices() const {
    LLOG_INF << "Enumerated physical devices...";
    for( auto &it: mDeviceNames) {
        LLOG_INF << "Physical device id: " << std::to_string(static_cast<uint16_t>(it.first)) << ", name: " << it.second;
    }
}

Device::SharedPtr DeviceManager::defaultRenderingDevice() const {
    return renderingDevice(mDefaultRenderingDeviceID);
}

Device::SharedPtr DeviceManager::defaultDisplayDevice() const {
    return renderingDevice(mDefaultDisplayDeviceID);
}

void DeviceManager::setDefaultRenderingDevice(uint8_t gpuId) {
    if(gpuId < mRenderingDevices.size())
        mDefaultRenderingDeviceID = gpuId;
}

#ifdef SCRIPTING
SCRIPT_BINDING(DeviceManager) {
    pybind11::class_<DeviceManager, Device::SharedPtr> deviceManagerClass(m, "DeviceManager");
    deviceManagerClass.def_static("create", [](){ return Device::SharedPtr<DeviceManager, py::nodelete>(&DeviceManager::create()); });
    deviceManagerClass.def("listDevices", &DeviceManager::listDevices);
    deviceManagerClass.def("defaultRenderingDevice", &DeviceManager::defaultRenderingDevice);
}
#endif

}  // namespace Falcor
