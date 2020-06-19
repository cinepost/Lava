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
#include <iostream>
#include <chrono>
#include <ctime>  

#include "Falcor/stdafx.h"
#include "Falcor/Core/API/Vulkan/VKSmartHandle.h"
#include "Falcor/Core/API/Device.h"

namespace Falcor {

    template<> VkHandle<VkSwapchainKHR>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroySwapchainKHR(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkCommandPool>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyCommandPool(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkSemaphore>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroySemaphore(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkSampler>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE && gpDevice) vkDestroySampler(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkDescriptorSetLayout>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkPipeline>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyPipeline(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkShaderModule>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyShaderModule(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkPipelineLayout>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyPipelineLayout(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkDescriptorPool>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyDescriptorPool(gpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkQueryPool>::~VkHandle() { if (mApiHandle != VK_NULL_HANDLE && gpDevice) vkDestroyQueryPool(gpDevice->getApiHandle(), mApiHandle, nullptr); }

    VkDeviceData::~VkDeviceData() {
        if (mInstance != VK_NULL_HANDLE && mLogicalDevice != VK_NULL_HANDLE && mInstance != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
            vkDestroyDevice(mLogicalDevice, nullptr);
            vkDestroyInstance(mInstance, nullptr);
        }
    }

    template<>
    VkResource<VkImage, VkBuffer>::~VkResource() {
        if (!gpDevice) {
            // #VKTODO This is here because of the black texture in VkResourceViews.cpp
            LOG_DBG("gpDevice is NULL");
            return;
        }
        assert(mDeviceMem || mType == VkResourceType::Image);  // All of our resources are allocated with memory, except for the swap-chain backbuffers that we shouldn't release
        if (mDeviceMem) {
            switch (mType) {
                case VkResourceType::Image:
                    if (mImage) {
                        vkDestroyImage(gpDevice->getApiHandle(), mImage, nullptr);
                    } else {
                        //LOG_WARN("mImage is NULL");
                    }
                    break;
                case VkResourceType::Buffer:
                    if (mBuffer) {
                        vkDestroyBuffer(gpDevice->getApiHandle(), mBuffer, nullptr);
                    } else {
                        //LOG_WARN("mBuffer is NULL");
                    }
                    break;
                default:
                    should_not_get_here();
            }
            vkFreeMemory(gpDevice->getApiHandle(), mDeviceMem, nullptr);
            //fprintf(stderr, "_dbg_i %u\n", _dbg_i++);
            //std::cout << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << std::endl;
            //LOG_DBG("memory freed");
        }
    }

    template<>
    VkResource<VkImageView, VkBufferView>::~VkResource() {
        if (!gpDevice) {
            // #VKTODO This is here because of the black texture in VkResourceViews.cpp
            LOG_DBG("gpDevice is NULL");
            return;
        }
        switch (mType) {
            case VkResourceType::Image:
                if (mImage) {
                    vkDestroyImageView(gpDevice->getApiHandle(), mImage, nullptr);
                } else {
                    //LOG_WARN("mImage is NULL");
                }
                break;
            case VkResourceType::Buffer:
                if (mBuffer) {
                    vkDestroyBufferView(gpDevice->getApiHandle(), mBuffer, nullptr);
                } else {
                    //LOG_WARN("mBuffer is NULL");
                }
                break;
            default:
                should_not_get_here();
        }
    }

    VkFbo::~VkFbo() {
        vkDestroyRenderPass(gpDevice->getApiHandle(), mVkRenderPass, nullptr);
        vkDestroyFramebuffer(gpDevice->getApiHandle(), mVkFbo, nullptr);
    }

    VkRootSignature::~VkRootSignature() {
        vkDestroyPipelineLayout(gpDevice->getApiHandle(), mApiHandle, nullptr);
        for (auto& s : mSets) {
            vkDestroyDescriptorSetLayout(gpDevice->getApiHandle(), s, nullptr);
        }
    }


    // Force template instantiation
    /*
    template VkHandle<VkSwapchainKHR>::~VkHandle();
    template VkHandle<VkCommandPool>::~VkHandle();
    template VkHandle<VkSemaphore>::~VkHandle();
    template VkHandle<VkSampler>::~VkHandle();
    template VkHandle<VkDescriptorSetLayout>::~VkHandle();
    template VkHandle<VkPipeline>::~VkHandle();
    template VkHandle<VkShaderModule>::~VkHandle();
    template VkHandle<VkDescriptorPool>::~VkHandle();
    template VkHandle<VkQueryPool>::~VkHandle();

    template VkResource<VkImage, VkBuffer>::~VkResource();
    template VkResource<VkImageView, VkBufferView>::~VkResource();
    */
}  // namespace Falcor
