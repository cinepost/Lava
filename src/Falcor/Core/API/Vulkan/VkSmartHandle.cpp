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

//#define VMA_IMPLEMENTATION
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

namespace Falcor {

    template<> VkHandle<VkSwapchainKHR>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroySwapchainKHR(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkCommandPool>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyCommandPool(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkSemaphore>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroySemaphore(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkSampler>::~VkHandle() { if((mApiHandle != VK_NULL_HANDLE) && mpDevice) vkDestroySampler(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkDescriptorSetLayout>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkPipeline>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyPipeline(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkShaderModule>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyShaderModule(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkPipelineLayout>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyPipelineLayout(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkDescriptorPool>::~VkHandle() { if(mApiHandle != VK_NULL_HANDLE) vkDestroyDescriptorPool(mpDevice->getApiHandle(), mApiHandle, nullptr); }
    template<> VkHandle<VkQueryPool>::~VkHandle() { if ((mApiHandle != VK_NULL_HANDLE) && mpDevice) vkDestroyQueryPool(mpDevice->getApiHandle(), mApiHandle, nullptr); }

    VkDeviceData::~VkDeviceData() {
        if (mInstance != VK_NULL_HANDLE && mLogicalDevice != VK_NULL_HANDLE && mInstance != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
            vkDestroyDevice(mLogicalDevice, nullptr);
            vkDestroyInstance(mInstance, nullptr);
        }
    }

    template<>
    VkResource<VkImage, VkBuffer>::~VkResource() {
        if (!mpDevice) {
            // #VKTODO This is here because of the black texture in VkResourceViews.cpp
            return;
        }
        //assert(mDeviceMem || mType == VkResourceType::Image);  // All of our resources are allocated with memory, except for the swap-chain backbuffers that we shouldn't release
        
        const auto& allocator = mpDevice->allocator();
        VmaAllocationInfo info;
        vmaGetAllocationInfo(allocator, mAllocation, &info);
        
        //if (mDeviceMem) {
        if(info.deviceMemory != VK_NULL_HANDLE) {
            if(info.pMappedData) {
                vmaUnmapMemory(allocator, mAllocation);
            }

            switch (mType) {
                case VkResourceType::Image:
                    if (mImage) {
                        vmaDestroyImage(allocator, mImage, mAllocation);
                        //vkDestroyImage(mpDevice->getApiHandle(), mImage, nullptr);
                    }
                    break;
                case VkResourceType::Buffer:
                    if (mBuffer) {
                        vmaDestroyBuffer(allocator, mBuffer, mAllocation);
                        //vkDestroyBuffer(mpDevice->getApiHandle(), mBuffer, nullptr);
                    }
                    break;
                default:
                    should_not_get_here();
            }
        //    vkFreeMemory(mpDevice->getApiHandle(), mDeviceMem, nullptr);
        }
    }

    template<>
    VkResource<VkImageView, VkBufferView>::~VkResource() {
        if (!mpDevice) {
            // #VKTODO This is here because of the black texture in VkResourceViews.cpp
            return;
        }
        switch (mType) {
            case VkResourceType::Image:
                if (mImage) {
                    vkDestroyImageView(mpDevice->getApiHandle(), mImage, nullptr);
                }
                break;
            case VkResourceType::Buffer:
                if (mBuffer) {
                    vkDestroyBufferView(mpDevice->getApiHandle(), mBuffer, nullptr);
                }
                break;
            default:
                should_not_get_here();
        }
    }

    VkFbo::~VkFbo() {
        if (mpDevice) {
            vkDestroyRenderPass(mpDevice->getApiHandle(), mVkRenderPass, nullptr);
            vkDestroyFramebuffer(mpDevice->getApiHandle(), mVkFbo, nullptr);
        }
    }

    VkRootSignature::~VkRootSignature() {
        vkDestroyPipelineLayout(mpDevice->getApiHandle(), mApiHandle, nullptr);
        for (auto& s : mSets) {
            vkDestroyDescriptorSetLayout(mpDevice->getApiHandle(), s, nullptr);
        }
    }


    // Force template instantiation
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
}  // namespace Falcor
