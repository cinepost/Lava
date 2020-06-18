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

#include <typeinfo>

#include "Falcor/stdafx.h"
#include "Falcor/Core/API/FBO.h"
#include "Falcor/Core/API/Device.h"
#include "VKState.h"

namespace Falcor {

    Fbo::Fbo() {
        mColorAttachments.resize(getMaxColorTargetCount());
    }

    Fbo::~Fbo() {
        gpDevice->releaseResource(std::static_pointer_cast<VkBaseApiHandle>(mApiHandle));
    }

    const Fbo::ApiHandle& Fbo::getApiHandle() const {
        finalize();
        return mApiHandle;
    }

    uint32_t Fbo::getMaxColorTargetCount() {
        int count = gpDevice->getPhysicalDeviceLimits().maxFragmentOutputAttachments;
        return count;
    }

    void Fbo::initApiHandle() const {
        // Bind the color buffers
        uint32_t arraySize = -1;
        std::vector<VkImageView> attachments(Fbo::getMaxColorTargetCount() + 1);  // 1 is for the depth
        LOG_DBG("vector size: %u", attachments.size());

        uint32_t rtCount = 0;
        for (uint32_t i = 0; i < Fbo::getMaxColorTargetCount(); i++) {
            if (mColorAttachments[i].pTexture) {
                LOG_DBG("texture width: %u", mColorAttachments[i].pTexture->getWidth());

                LOG_DBG("getRenderTargetView getType: %s", typeid(getRenderTargetView(i)->getApiHandle().getType()).name());

                assert(arraySize == -1 || arraySize == getRenderTargetView(i)->getViewInfo().arraySize);
                arraySize = getRenderTargetView(i)->getViewInfo().arraySize;
                LOG_DBG("arraySize: %u", arraySize);
                auto o = getRenderTargetView(i)->getApiHandle().getImage();
                std::string s = typeid(o).name();
                LOG_DBG("o: %s", s.c_str());
                LOG_DBG("ok");
                attachments[rtCount] = getRenderTargetView(i)->getApiHandle();
                LOG_DBG("attachments ok");
                rtCount++;
            }
        }
        // Bind the depth buffer
        if (mDepthStencil.pTexture) {
            assert(arraySize == -1 || arraySize == getDepthStencilView()->getViewInfo().arraySize);
            if (arraySize == -1) {
                arraySize = getDepthStencilView()->getViewInfo().arraySize;
            }
            auto test = getDepthStencilView();
            if (!test) {
                printf("ERROR getDepthStencilView returned NULL\n");
            }
            auto test2 = test->getApiHandle();
            if (!test2) {
                printf("ERROR getDepthStencilView->getApiHandle() returned NULL\n");
            }
            attachments[rtCount] = getDepthStencilView()->getApiHandle();
            rtCount++;
        }

        // Render Pass
        RenderPassCreateInfo renderPassInfo;
        initVkRenderPassInfo(*mpDesc, renderPassInfo);
        VkRenderPass pass;
        vkCreateRenderPass(gpDevice->getApiHandle(), &renderPassInfo.info, nullptr, &pass);

        // Framebuffer
        VkFramebufferCreateInfo frameBufferInfo = {};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass = pass;
        frameBufferInfo.attachmentCount = rtCount;
        frameBufferInfo.pAttachments = attachments.data();
        frameBufferInfo.width = rtCount ? getWidth() : 1;
        frameBufferInfo.height = rtCount ? getHeight() : 1;
        frameBufferInfo.layers = rtCount ? arraySize : 1;

        VkFramebuffer frameBuffer;

        vkCreateFramebuffer(gpDevice->getApiHandle(), &frameBufferInfo, nullptr, &frameBuffer);

        if (mApiHandle) gpDevice->releaseResource(std::static_pointer_cast<VkBaseApiHandle>(mApiHandle));
        mApiHandle = ApiHandle::create(pass, frameBuffer);
    }

    void Fbo::applyColorAttachment(uint32_t rtIndex) {}

    void Fbo::applyDepthAttachment() {}

    RenderTargetView::SharedPtr Fbo::getRenderTargetView(uint32_t rtIndex) const {
        const auto& rt = mColorAttachments[rtIndex];
        if (rt.pTexture) {
            printf("gRTV-1\n");
            return rt.pTexture->getRTV(rt.mipLevel, rt.firstArraySlice, rt.arraySize);
        } else {
            printf("gRTV-2\n");
            return RenderTargetView::getNullView();
        }
    }

    DepthStencilView::SharedPtr Fbo::getDepthStencilView() const {
        if (mDepthStencil.pTexture) {
            return mDepthStencil.pTexture->getDSV(mDepthStencil.mipLevel, mDepthStencil.firstArraySlice, mDepthStencil.arraySize);
        } else {
            return DepthStencilView::getNullView();
        }
    }
}  // namespace Falcor
