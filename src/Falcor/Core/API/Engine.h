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
#ifndef SRC_FALCOR_CORE_API_ENGINE_H_
#define SRC_FALCOR_CORE_API_ENGINE_H_

#include <list>
#include <string>
#include <memory>
#include <queue>
#include <vector>

#include "Falcor/Core/Window.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/FBO.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/API/LowLevelContextData.h"
#include "Falcor/Core/API/DescriptorPool.h"
#include "Falcor/Core/API/GpuMemoryHeap.h"
#include "Falcor/Core/API/QueryHeap.h"


namespace Falcor {

struct DeviceApiData;

class dlldecl Engine {
 public:
    using SharedPtr = std::shared_ptr<Engine>;
    using SharedConstPtr = std::shared_ptr<const Engine>;

    /** Create a rendering engine.
        \param[in] pWindow a previously-created window object
        \param[in] desc Device configuration descriptor.
        \return nullptr if the function failed, otherwise a new device object
    */
    static SharedPtr create(Window::SharedPtr& pWindow, const Device::Desc& desc);
    
    bool init();

    /** Get the FBO object associated with the swap-chain.
        This can change each frame, depending on the API used
    */
    Fbo::SharedPtr getSwapChainFbo() const;

    /** Get the FBO object used for headless rendering.
        This can change each frame, depending on the API used
    */
    Fbo::SharedPtr getOffscreenFbo() const;

    Device::SharedPtr device() { return mpDevice; };

 private:
    Engine(Device::SharedPtr device);

    void releaseFboData();
    
    bool updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);
    bool updateOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);

    static constexpr uint32_t kSwapChainBuffersCount = 5;

    uint32_t mCurrentBackBufferIndex;
    Fbo::SharedPtr mpSwapChainFbos[kSwapChainBuffersCount];
    Fbo::SharedPtr mpOffscreenFbo;

    Device::SharedPtr mpDevice;

    bool headless = false;


};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_ENGINE_H_
