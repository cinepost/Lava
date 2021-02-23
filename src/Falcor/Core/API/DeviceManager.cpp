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
#include "DeviceManager.h"

#include "Falcor/Utils/Debug/debug.h"


namespace py = pybind11;

namespace Falcor {

DeviceManager::DeviceManager() {

}

DeviceManager::~DeviceManager() {
    LOG_DBG("DeviceManager::~DeviceManager");
    for( auto& [id, pDevice]: mRenderingDevices ) {
        if (pDevice) {
            pDevice->cleanup();
            pDevice.reset();
        }
    }
    
    LOG_DBG("DeviceManager::~DeviceManager done");
}

//static
DeviceManager::SharedPtr DeviceManager::create() {
    auto pDeviceManager = new DeviceManager();

    if (!pDeviceManager->init()) {
        delete pDeviceManager;
        return nullptr;
    }

    pDeviceManager->printEnumeratedDevices();

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
    logWarning("Device not enumerated !");
    return false;
}

Device::SharedPtr DeviceManager::createRenderingDevice(uint8_t gpuId, const Device::Desc &desc) {
    if (!deviceEnumerated(gpuId)) {
        logError("Rendering device " + to_string(gpuId) + " not enumerated !!!");
        return nullptr;
    }

    Device::SharedPtr pDevice = renderingDevice(gpuId);
    if(pDevice) return pDevice;

    pDevice = Device::create(shared_from_this(), gpuId, desc);
    if (!pDevice) logError("Unable to create rendering device!");

    mRenderingDevices[gpuId] = pDevice;
    return pDevice;
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
    printf("Enumerated Vulkan physical devices...\n");
    for( auto &it: mDeviceNames) {
        printf("Device local uid: %u, name: %s\n", it.first, it.second.c_str());
    }
}

Device::SharedPtr DeviceManager::defaultRenderingDevice() const {
    return renderingDevice(mDefaultRenderingDeviceID);
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
