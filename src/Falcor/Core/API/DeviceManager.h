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
#ifndef SRC_FALCOR_CORE_API_DEVICEMANAGER_H_
#define SRC_FALCOR_CORE_API_DEVICEMANAGER_H_

#include <list>
#include <string>
#include <memory>
#include <queue>
#include <vector>
#include <atomic>
#include <unordered_map>

#include "Falcor/Core/API/Device.h"

namespace Falcor {

#ifdef _DEBUG
    #define DEFAULT_ENABLE_DEBUG_LAYER true
#else
    #define DEFAULT_ENABLE_DEBUG_LAYER false
#endif

class dlldecl DeviceManager {
 public:
    using DeviceLocalUID = Device::DeviceLocalUID;

    static DeviceManager& instance() {
        static DeviceManager instance;      // Guaranteed to be destroyed.
        return instance;                    // Instantiated on first use.
    }

    const std::unordered_map<DeviceLocalUID, std::string>& listDevices() { return mDeviceNames; }
    std::vector<Device::SharedPtr> displayDevices();
    std::vector<Device::SharedPtr> renderingDevices();

    Device::SharedPtr displayDevice(DeviceLocalUID luid);
    Device::SharedPtr renderingDevice(DeviceLocalUID luid);

    Device::SharedPtr createDisplayDevice(DeviceLocalUID luid, Falcor::Window::SharedPtr pWindow, const Device::Desc &desc);
    Device::SharedPtr createRenderingDevice(DeviceLocalUID luid, const Device::Desc &desc);

    Device::SharedPtr defaultDisplayDevice();
    Device::SharedPtr defaultRenderingDevice();

    void printEnumeratedDevices();

 public:
    DeviceManager(DeviceManager const&)     = delete;
    void operator=(DeviceManager const&)    = delete;

 private:
    DeviceManager();

    bool init();
    bool deviceEnumerated(DeviceLocalUID luid);
    void enumerateDevices();

    bool initialized;
    std::unordered_map<DeviceLocalUID, std::string> mDeviceNames;
    std::unordered_map<DeviceLocalUID, Device::SharedPtr> mDisplayDevices;
    std::unordered_map<DeviceLocalUID, Device::SharedPtr> mRenderingDevices;    
};


}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_DEVICEMANAGER_H_
