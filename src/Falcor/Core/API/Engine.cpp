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
#include "Falcor/stdafx.h"
#include "Engine.h"

#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Texture.h"

#include "Falcor/Utils/Debug/debug.h"

namespace Falcor {

Engine::SharedPtr gpEngine;
Engine::SharedPtr gpEngineHeadless;    

Engine::Engine(Device::SharedPtr device) : mpDevice(device) {
    headless = mpDevice->isHeadless();
}

Engine::SharedPtr Engine::create(Window::SharedPtr& pWindow, const Device::Desc& desc) {
    if(pWindow) {
        // Swapchain enabled engine
        if (gpEngine) {
            logError("Falcor only supports a single engine");
            return nullptr;
        }
        auto pDevice = Device::SharedPtr(new Device(pWindow, desc));
        gpEngine = SharedPtr(new Engine(pDevice));
        if (gpEngine->init() == false) { 
            logError("Error creating rendering engine");
            return nullptr; 
        }
        return gpEngine;
    } else {
        // Headless engine
        if (gpEngineHeadless) {
            logError("Falcor only supports a single headless engine");
            return nullptr;
        }
        auto pDevice = Device::SharedPtr(new Device(pWindow, desc));
        gpEngineHeadless = SharedPtr(new Engine(pDevice));
        if (gpEngineHeadless->init() == false) { 
            logError("Error creating rendering engine");
            return nullptr; 
        }
        return gpEngineHeadless;
    }
}

/**
 * Initialize engine
 */
bool Engine::init() {
    if(!mpDevice->init()){
        return false;
    }

    // Update the FBOs
    if(!headless) {
        if (updateDefaultFBO(mpDevice->getWindow()->getClientAreaSize().x, mpDevice->getWindow()->getClientAreaSize().y, mpDevice->getDesc().colorFormat, mpDevice->getDesc().depthFormat) == false) {
            return false;
        }
    } else {
        if (updateOffscreenFBO( mpDevice->getDesc().width,  mpDevice->getDesc().height, mpDevice->getDesc().colorFormat, mpDevice->getDesc().depthFormat) == false) {
            return false;
        }
    }
    return true;
}

void Engine::releaseFboData() {
    // First, delete all FBOs
    if (!headless) {
        // Delete swapchain FBOs
        for (auto& pFbo : mpSwapChainFbos) {
            pFbo->attachColorTarget(nullptr, 0);
            pFbo->attachDepthStencilTarget(nullptr);
        }
    } else {
        // Delete headless FBO
        mpOffscreenFbo->attachColorTarget(nullptr, 0);
        mpOffscreenFbo->attachDepthStencilTarget(nullptr);
    }

    // Now execute all deferred releases
    mpDevice->release();
}

bool Engine::updateOffscreenFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat) {
    // Create a texture object
    auto pColorTex = Texture::SharedPtr(new Texture(width, height, 1, 1, 1, 1, colorFormat, Texture::Type::Texture2D, Texture::BindFlags::RenderTarget));
    //pColorTex->mApiHandle = apiHandles[i];
    
    // Create the FBO if it's required
    if (mpOffscreenFbo == nullptr) mpOffscreenFbo = Fbo::create();
    mpOffscreenFbo->attachColorTarget(pColorTex, 0);

    // Create a depth texture
    if (depthFormat != ResourceFormat::Unknown) {
        auto pDepth = Texture::create2D(width, height, depthFormat, 1, 1, nullptr, Texture::BindFlags::DepthStencil);
        mpOffscreenFbo->attachDepthStencilTarget(pDepth);
    }

    return true;
}

bool Engine::updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat) {
    ResourceHandle apiHandles[kSwapChainBuffersCount] = {};
    getApiFboData(width, height, colorFormat, depthFormat, apiHandles, mCurrentBackBufferIndex);

    for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) {
        // Create a texture object
        auto pColorTex = Texture::SharedPtr(new Texture(width, height, 1, 1, 1, 1, colorFormat, Texture::Type::Texture2D, Texture::BindFlags::RenderTarget));
        pColorTex->mApiHandle = apiHandles[i];
        // Create the FBO if it's required
        if (mpSwapChainFbos[i] == nullptr) mpSwapChainFbos[i] = Fbo::create();
        mpSwapChainFbos[i]->attachColorTarget(pColorTex, 0);

        // Create a depth texture
        if (depthFormat != ResourceFormat::Unknown) {
            auto pDepth = Texture::create2D(width, height, depthFormat, 1, 1, nullptr, Texture::BindFlags::DepthStencil);
            mpSwapChainFbos[i]->attachDepthStencilTarget(pDepth);
        }
    }
    return true;
}

Fbo::SharedPtr Engine::getSwapChainFbo() const {
    assert(!headless);
    return mpSwapChainFbos[mCurrentBackBufferIndex];
}

Fbo::SharedPtr Engine::getOffscreenFbo() const {
    assert(headless);
    return mpOffscreenFbo;
}

}  // namespace Falcor
