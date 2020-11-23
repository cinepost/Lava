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
#include <memory>

#include "Falcor/stdafx.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Resource.h"

#include "Falcor/Core/API/Vulkan/VKDevice.h"

namespace Falcor {
    VkDeviceMemory allocateDeviceMemory(std::shared_ptr<Device> pDevice, Device::MemoryType memType, uint32_t memoryTypeBits, size_t size);

    struct TextureApiData {
    };

    Texture::~Texture() {
        LOG_DBG("Deleting texture (resource id %zu )with source name %s", id(), mSourceFilename.c_str());

        // #VKTODO the `if` is here because of the black texture in VkResourceView.cpp
        if (mpDevice ) {

            for( auto pPage: mPages)
                pPage->release();

            for (auto bind : mOpaqueMemoryBinds)
                vkFreeMemory(mpDevice->getApiHandle(), bind.memory, nullptr);
        
            mpDevice->releaseResource(std::static_pointer_cast<VkBaseApiHandle>(mApiHandle));
        }
    }

    // Like getD3D12ResourceFlags but for Images specifically
    VkImageUsageFlags getVkImageUsageFlags(Resource::BindFlags bindFlags) {
        // Assume that every image can be updated/cleared, read from, and sampled
        VkImageUsageFlags vkFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        if (is_set(bindFlags, Resource::BindFlags::UnorderedAccess)) {
            vkFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }

        if (is_set(bindFlags, Resource::BindFlags::DepthStencil)) {
            vkFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }

        if (is_set(bindFlags, Resource::BindFlags::ShaderResource)) {
            // #VKTODO what does VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT mean?
            vkFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }

        if (is_set(bindFlags, Resource::BindFlags::RenderTarget)) {
            vkFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        // According to spec, must not be 0
        assert(vkFlags != 0);

        return vkFlags;
    }

    uint32_t getMaxMipCount(const VkExtent3D& size) {
        return 1 + uint32_t(glm::log2(static_cast<float>(glm::max(glm::max(size.width, size.height), size.depth))));
    }

    VkImageType getVkImageType(Texture::Type type) {
        switch (type) {
            case Texture::Type::Texture1D:
                return VK_IMAGE_TYPE_1D;

            case Texture::Type::Texture2D:
            case Texture::Type::Texture2DMultisample:
            case Texture::Type::TextureCube:
                return VK_IMAGE_TYPE_2D;

            case Texture::Type::Texture3D:
                return VK_IMAGE_TYPE_3D;
            default:
                should_not_get_here();
                return VK_IMAGE_TYPE_1D;
        }
    }

    static VkFormatFeatureFlags getFormatFeatureBitsFromUsage(VkImageUsageFlags usage) {
        VkFormatFeatureFlags bits = 0;
        if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) bits |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
        if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) bits |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) bits |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if (usage & VK_IMAGE_USAGE_STORAGE_BIT) bits |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) bits |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) bits |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        assert((usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) == 0);
        assert((usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) == 0);
        return bits;
    }

    static VkImageTiling getFormatImageTiling(std::shared_ptr<Device> device, VkFormat format, VkImageUsageFlags usage) {
        VkFormatProperties p;
        vkGetPhysicalDeviceFormatProperties(device->getApiHandle(), format, &p);
        auto featureBits = getFormatFeatureBitsFromUsage(usage);
        if ((p.optimalTilingFeatures & featureBits) == featureBits) return VK_IMAGE_TILING_OPTIMAL;
        if ((p.linearTilingFeatures & featureBits) == featureBits) return VK_IMAGE_TILING_LINEAR;

        should_not_get_here();
        return VkImageTiling(-1);
    }

    void Texture::updateSparseBindInfo() {
        assert(mImage != VK_NULL_HANDLE);
        // Update list of memory-backed sparse image memory binds
        
        mSparseImageMemoryBinds.clear();
        for (auto pPage : mPages) {
            mSparseImageMemoryBinds.push_back(pPage->mImageMemoryBind);
        }

        // Update sparse bind info
        mBindSparseInfo = {};
        mBindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        // todo: Semaphore for queue submission
        // bindSparseInfo.signalSemaphoreCount = 1;
        // bindSparseInfo.pSignalSemaphores = &bindSparseSemaphore;

        // Image memory binds
        mImageMemoryBindInfo = {};
        mImageMemoryBindInfo.image = mImage;
        mImageMemoryBindInfo.bindCount = static_cast<uint32_t>(mSparseImageMemoryBinds.size());
        mImageMemoryBindInfo.pBinds = mSparseImageMemoryBinds.data();

        mBindSparseInfo.imageBindCount = (mImageMemoryBindInfo.bindCount > 0) ? 1 : 0;
        mBindSparseInfo.pImageBinds = &mImageMemoryBindInfo;

        // Opaque image memory binds for the mip tail
        mOpaqueMemoryBindInfo.image = mImage;
        mOpaqueMemoryBindInfo.bindCount = static_cast<uint32_t>(mOpaqueMemoryBinds.size());
        mOpaqueMemoryBindInfo.pBinds = mOpaqueMemoryBinds.data();
        mBindSparseInfo.imageOpaqueBindCount = (mOpaqueMemoryBindInfo.bindCount > 0) ? 1 : 0;
        mBindSparseInfo.pImageOpaqueBinds = &mOpaqueMemoryBindInfo;
    }

    VirtualTexturePage::SharedPtr Texture::addPage(int3 offset, uint3 extent, const uint64_t size, const uint32_t mipLevel, uint32_t layer) {
        if (!mIsSparse) return nullptr;

        auto newPage = VirtualTexturePage::create(mpDevice);
        if (!newPage) return nullptr;

        newPage->mOffset = {offset[0], offset[1], offset[2]};
        newPage->mExtent = {extent[0], extent[1], extent[2]};
        newPage->mDevMemSize = size;
        newPage->mMipLevel = mipLevel;
        newPage->mLayer = layer;
        newPage->mIndex = static_cast<uint32_t>(mPages.size());
        newPage->mImageMemoryBind = {};
        newPage->mImageMemoryBind.offset = {offset[0], offset[1], offset[2]};
        newPage->mImageMemoryBind.extent = {extent[0], extent[1], extent[2]};
        mPages.push_back(newPage);
        return mPages.back();
    }

    uint32_t Texture::getTextureSizeInBytes() {
        if (mImage == VK_NULL_HANDLE)
            return 0; 

        return mMemRequirements.size;
    }

    void Texture::apiInit(const void* pData, bool autoGenMips) {
        if (mImage != VK_NULL_HANDLE) {
            LOG_WARN("Texture api already initialized !!!");
            return;
        }

        mMemRequirements.size = 0;
        VkImageCreateInfo imageInfo = {};

        imageInfo.arrayLayers = mArraySize;
        imageInfo.extent.depth = mDepth;
        imageInfo.extent.height = align_to(getFormatHeightCompressionRatio(mFormat), mHeight);
        imageInfo.extent.width = align_to(getFormatWidthCompressionRatio(mFormat), mWidth);
        imageInfo.format = getVkFormat(mFormat);
        imageInfo.imageType = getVkImageType(mType);
        
        imageInfo.initialLayout = (pData && !mIsSparse ) ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
        
        imageInfo.mipLevels = std::min(mMipLevels, getMaxMipCount(imageInfo.extent));
        imageInfo.pQueueFamilyIndices = nullptr;
        imageInfo.queueFamilyIndexCount = 0;
        imageInfo.samples = (VkSampleCountFlagBits)mSampleCount;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.usage = getVkImageUsageFlags(mBindFlags);
        imageInfo.tiling = getFormatImageTiling(mpDevice, imageInfo.format, imageInfo.usage);

        if (mType == Texture::Type::TextureCube) {
            imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            imageInfo.arrayLayers *= 6;
        }

        if(mIsSparse) {
            imageInfo.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
        }

        mState.global = (pData && !mIsSparse ) ? Resource::State::PreInitialized : Resource::State::Undefined;

        auto result = vkCreateImage(mpDevice->getApiHandle(), &imageInfo, nullptr, &mImage);
        if (VK_FAILED(result)) {
            mImage = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create texture.");
        }


        vkGetImageMemoryRequirements(mpDevice->getApiHandle(), mImage, &mMemRequirements);
        std::cout << "Image memory requirements:" << std::endl;
        std::cout << "\t Size: " << mMemRequirements.size << std::endl;
        std::cout << "\t Alignment: " << mMemRequirements.alignment << std::endl;

        if (mIsSparse) {
            // Check requested image size against hardware sparse limit            
            if (mMemRequirements.size > mpDevice->apiData()->properties.limits.sparseAddressSpaceSize) {
                std::cout << "Error: Requested sparse image size exceeds supports sparse address space size!" << std::endl;
                return;
            };

            // Get sparse memory requirements
            // Count
            uint32_t sparseMemoryReqsCount = 32;
            std::vector<VkSparseImageMemoryRequirements> sparseMemoryReqs(sparseMemoryReqsCount);
            vkGetImageSparseMemoryRequirements(mpDevice->getApiHandle(), mImage, &sparseMemoryReqsCount, sparseMemoryReqs.data());
            
            if (sparseMemoryReqsCount == 0) {
                std::cout << "Error: No memory requirements for the sparse image!" << std::endl;
                return;
            }
            sparseMemoryReqs.resize(sparseMemoryReqsCount);
            // Get actual requirements
            vkGetImageSparseMemoryRequirements(mpDevice->getApiHandle(), mImage, &sparseMemoryReqsCount, sparseMemoryReqs.data());

            std::cout << "Sparse image memory requirements: " << sparseMemoryReqsCount << std::endl;
            
            for (auto reqs : sparseMemoryReqs) {
                std::cout << "\t Image granularity: w = " << reqs.formatProperties.imageGranularity.width << " h = " << reqs.formatProperties.imageGranularity.height << " d = " << reqs.formatProperties.imageGranularity.depth << std::endl;
                std::cout << "\t Mip tail first LOD: " << reqs.imageMipTailFirstLod << std::endl;
                std::cout << "\t Mip tail size: " << reqs.imageMipTailSize << std::endl;
                std::cout << "\t Mip tail offset: " << reqs.imageMipTailOffset << std::endl;
                std::cout << "\t Mip tail stride: " << reqs.imageMipTailStride << std::endl;
                //todo:multiple reqs
                mMipTailStart = reqs.imageMipTailFirstLod;
            }

            // Get sparse image requirements for the color aspect
            VkSparseImageMemoryRequirements sparseMemoryReq;
            bool colorAspectFound = false;
            for (auto reqs : sparseMemoryReqs) {
                if (reqs.formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
                    sparseMemoryReq = reqs;
                    colorAspectFound = true;
                    break;
                }
            }
            if (!colorAspectFound) {
                std::cout << "Error: Could not find sparse image memory requirements for color aspect bit!" << std::endl;
                return;
            }

            // Calculate number of required sparse memory bindings by alignment
            assert((mMemRequirements.size % mMemRequirements.alignment) == 0);
            //mMemoryTypeIndex = vulkanDevice->getMemoryType(sparseImageMemoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            //mMemoryTypeIndex = mpDevice->getVkMemoryType(mMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        }

        // Allocate the GPU memory        
        VkDeviceMemory deviceMem = allocateDeviceMemory(mpDevice, Device::MemoryType::Default, mMemRequirements.memoryTypeBits, mMemRequirements.size);
        vkBindImageMemory(mpDevice->getApiHandle(), mImage, deviceMem, 0);
        mApiHandle = ApiHandle::create(mpDevice, mImage, deviceMem);
  
        if (pData && !mIsSparse) {
            uploadInitData(pData, autoGenMips);
        }
    }

}  // namespace Falcor
