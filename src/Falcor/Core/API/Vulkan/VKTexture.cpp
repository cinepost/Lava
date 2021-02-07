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
#include "Falcor/Core/API/SparseResourceManager.h"

#include "Falcor/Core/API/Vulkan/VKDevice.h"

namespace Falcor {
    VkDeviceMemory allocateDeviceMemory(std::shared_ptr<Device> pDevice, Device::MemoryType memType, uint32_t memoryTypeBits, size_t size);

    glm::uvec3 alignedDivision(const VkExtent3D& extent, const VkExtent3D& granularity) {
        glm::uvec3 res;
        res.x = extent.width / granularity.width + ((extent.width % granularity.width) ? 1u : 0u);
        res.y = extent.height / granularity.height + ((extent.height % granularity.height) ? 1u : 0u);
        res.z = extent.depth / granularity.depth + ((extent.depth % granularity.depth) ? 1u : 0u);
        return res;
    }

    struct TextureApiData {
    };

    Texture::~Texture() {
        LOG_DBG("Deleting texture (resource id %zu )with source name %s", id(), mSourceFilename.c_str());

        // #VKTODO the `if` is here because of the black texture in VkResourceView.cpp
        if (mpDevice ) {
            //if (mImage != VK_NULL_HANDLE)
            //    vkDestroyImage(mpDevice->getApiHandle(), mImage, nullptr);
            
            if (mBindSparseSemaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(mpDevice->getApiHandle(), mBindSparseSemaphore, nullptr);

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

    uint32_t getMaxMipCountVK(const VkExtent3D& size) {
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

        if (!mIsSparse) {
            LOG_ERR("Unable to sparse bind non sparse texture !!!");
            return;
        }
        
        // Update list of memory-backed sparse image memory binds
        mSparseImageMemoryBinds.clear();
        for (auto pPage : mPages) {
            //if ( pPage->isResident())
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
        LOG_WARN("Texture mOpaqueMemoryBinds.size() %zu", mOpaqueMemoryBinds.size());
        mOpaqueMemoryBindInfo.image = mImage;
        mOpaqueMemoryBindInfo.bindCount = static_cast<uint32_t>(mOpaqueMemoryBinds.size());
        mOpaqueMemoryBindInfo.pBinds = mOpaqueMemoryBinds.data();
        mBindSparseInfo.imageOpaqueBindCount = (mOpaqueMemoryBindInfo.bindCount > 0) ? 1 : 0;
        mBindSparseInfo.pImageOpaqueBinds = &mOpaqueMemoryBindInfo;


        // --------------------
        // Bind to queue
        auto queue = mpDevice->getCommandQueueHandle(LowLevelContextData::CommandQueueType::Direct, 0);

        // todo: in draw?
        vkQueueBindSparse(queue, 1, &mBindSparseInfo, VK_NULL_HANDLE);
        
        //todo: use sparse bind semaphore
        vkQueueWaitIdle(queue);
    }

    size_t Texture::getTextureSizeInBytes() {
        if (mImage == VK_NULL_HANDLE)
            return 0;

        return mIsSparse ? static_cast<size_t>(mSparseResidentMemSize) : mMemRequirements.size;
    }

    void Texture::apiInit(const void* pData, bool autoGenMips) {
        if (mImage != VK_NULL_HANDLE) {
            LOG_WARN("Texture api already initialized !!!");
            return;
        }

        mMemRequirements.size = 0;
        VkImageCreateInfo imageCreateInfo = {};

        imageCreateInfo.arrayLayers = mArraySize;
        imageCreateInfo.extent.depth = mDepth;
        imageCreateInfo.extent.height = align_to(getFormatHeightCompressionRatio(mFormat), mHeight);
        imageCreateInfo.extent.width = align_to(getFormatWidthCompressionRatio(mFormat), mWidth);
        imageCreateInfo.format = getVkFormat(mFormat);
        imageCreateInfo.imageType = getVkImageType(mType);
        
        imageCreateInfo.initialLayout = (pData && !mIsSparse ) ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
        
        imageCreateInfo.mipLevels = std::min(mMipLevels, getMaxMipCountVK(imageCreateInfo.extent));
        imageCreateInfo.pQueueFamilyIndices = nullptr;
        imageCreateInfo.queueFamilyIndexCount = 0;
        imageCreateInfo.samples = (VkSampleCountFlagBits)mSampleCount;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.usage = getVkImageUsageFlags(mBindFlags);
        imageCreateInfo.tiling = getFormatImageTiling(mpDevice, imageCreateInfo.format, imageCreateInfo.usage);

        if (mType == Texture::Type::TextureCube) {
            imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            imageCreateInfo.arrayLayers *= 6;
        }

        if(mIsSparse) {
            imageCreateInfo.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
        }

        mState.global = (pData && !mIsSparse ) ? Resource::State::PreInitialized : Resource::State::Undefined;

        auto result = vkCreateImage(mpDevice->getApiHandle(), &imageCreateInfo, nullptr, &mImage);
        if (VK_FAILED(result)) {
            mImage = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create texture.");
        }

        mApiHandle = ApiHandle::create(mpDevice, mImage, VK_NULL_HANDLE);
        
        if (mIsSparse) {
            // transition layout
            auto pCtx = mpDevice->getRenderContext();
            pCtx->textureBarrier(this, Resource::State::ShaderResource);
            pCtx->flush(true);
        }

        vkGetImageMemoryRequirements(mpDevice->getApiHandle(), mImage, &mMemRequirements);
        std::cout << "Image memory requirements:" << std::endl;
        std::cout << "\t Size: " << mMemRequirements.size << std::endl;
        std::cout << "\t Alignment: " << mMemRequirements.alignment << std::endl;

        if (mIsSparse) {
            std::cout << "Sparse address space size: " << mpDevice->apiData()->properties.limits.sparseAddressSpaceSize << std::endl;
            // Check requested image size against hardware sparse limit            
            if (mMemRequirements.size > mpDevice->apiData()->properties.limits.sparseAddressSpaceSize) {
                LOG_ERR("Error: Requested sparse image size exceeds supports sparse address space size !!!");
                return;
            };

            // Get sparse memory requirements
            // Count
            uint32_t sparseMemoryReqsCount = 32;
            std::vector<VkSparseImageMemoryRequirements> sparseMemoryReqs(sparseMemoryReqsCount);
            vkGetImageSparseMemoryRequirements(mpDevice->getApiHandle(), mImage, &sparseMemoryReqsCount, sparseMemoryReqs.data());
            
            if (sparseMemoryReqsCount == 0) {
                LOG_ERR("Error: No memory requirements for the sparse image !!!");
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
                LOG_ERR("Error: Could not find sparse image memory requirements for color aspect bit !!!");
                return;
            }

            // Calculate number of required sparse memory bindings by alignment
            assert((mMemRequirements.size % mMemRequirements.alignment) == 0);
            //mMemoryTypeIndex = vulkanDevice->getMemoryType(sparseImageMemoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            mMemoryTypeIndex = mpDevice->getVkMemoryTypeNative(mMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            // Get sparse bindings
            uint32_t sparseBindsCount = static_cast<uint32_t>(mMemRequirements.size / mMemRequirements.alignment);
            std::vector<VkSparseMemoryBind> sparseMemoryBinds(sparseBindsCount);

            mSparseImageMemoryRequirements = sparseMemoryReq;

            mSparsePageRes = {
                sparseMemoryReq.formatProperties.imageGranularity.width,
                sparseMemoryReq.formatProperties.imageGranularity.width,
                sparseMemoryReq.formatProperties.imageGranularity.width
            };

            LOG_DBG("mSparsePageRes %u %u %u", mSparsePageRes.x, mSparsePageRes.y, mSparsePageRes.z);
            
            // The mip tail contains all mip levels > sparseMemoryReq.imageMipTailFirstLod
            // Check if the format has a single mip tail for all layers or one mip tail for each layer
            // @todo: Comment
            mMipTailInfo.singleMipTail = sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
            mMipTailInfo.alignedMipSize = sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_ALIGNED_MIP_SIZE_BIT;

            auto& sparseResourceManager = SparseResourceManager::instance();

            uint32_t pageIndex = 0;
            // Sparse bindings for each mip level of all layers outside of the mip tail
            for (uint32_t layer = 0; layer < mArraySize; layer++) {

                // sparseMemoryReq.imageMipTailFirstLod is the first mip level that's stored inside the mip tail
                uint32_t currentMipBase = 0;
                for (uint32_t mipLevel = 0; mipLevel < sparseMemoryReq.imageMipTailFirstLod; mipLevel++) {
                    VkExtent3D extent;
                    extent.width = std::max(imageCreateInfo.extent.width >> mipLevel, 1u);
                    extent.height = std::max(imageCreateInfo.extent.height >> mipLevel, 1u);
                    extent.depth = std::max(imageCreateInfo.extent.depth >> mipLevel, 1u);

                    LOG_DBG("Mip level width %u height %u ...", extent.width, extent.height);

                    // Aligned sizes by image granularity
                    VkExtent3D imageGranularity = sparseMemoryReq.formatProperties.imageGranularity;
                    glm::uvec3 sparseBindCounts = alignedDivision(extent, imageGranularity);
                    glm::uvec3 lastBlockExtent;
                    lastBlockExtent.x = (extent.width % imageGranularity.width) ? extent.width % imageGranularity.width : imageGranularity.width;
                    lastBlockExtent.y = (extent.height % imageGranularity.height) ? extent.height % imageGranularity.height : imageGranularity.height;
                    lastBlockExtent.z = (extent.depth % imageGranularity.depth) ? extent.depth % imageGranularity.depth : imageGranularity.depth;

                    LOG_DBG("apiInit mip level %u sparse binds count: %u %u %u", mipLevel, sparseBindCounts.x, sparseBindCounts.y, sparseBindCounts.z);

                    // @todo: Comment
                    for (uint32_t z = 0; z < sparseBindCounts.z; z++) {
                        for (uint32_t y = 0; y < sparseBindCounts.y; y++) {
                            for (uint32_t x = 0; x < sparseBindCounts.x; x++) {
                                // Offset
                                VkOffset3D offset;
                                offset.x = x * imageGranularity.width;
                                offset.y = y * imageGranularity.height;
                                offset.z = z * imageGranularity.depth;
                                // Size of the page
                                VkExtent3D extent;
                                extent.width = (x == sparseBindCounts.x - 1) ? lastBlockExtent.x : imageGranularity.width;
                                extent.height = (y == sparseBindCounts.y - 1) ? lastBlockExtent.y : imageGranularity.height;
                                extent.depth = (z == sparseBindCounts.z - 1) ? lastBlockExtent.z : imageGranularity.depth;

                                // Add new virtual page
                                VirtualTexturePage::SharedPtr pPage = sparseResourceManager.addTexturePage(shared_from_this(), pageIndex++, {offset.x, offset.y, offset.z}, {extent.width, extent.height, extent.depth}, mMemRequirements.alignment, mipLevel, layer);
                                mPages.push_back(pPage);

                                //LOG_DBG("Tile level %u size %u %u", mipLevel, extent.width, extent.height);

                                //pageIndex++;
                            }
                        }
                    }
                    mMipBases[mipLevel] = currentMipBase;
                    
                    mSparsePagesCount += sparseBindCounts.x * sparseBindCounts.y * sparseBindCounts.z;
                    currentMipBase = mSparsePagesCount;
                }

                // @todo: proper comment
                // @todo: store in mip tail and properly release
                // @todo: Only one block for single mip tail

                
                if ((!mMipTailInfo.singleMipTail) && (sparseMemoryReq.imageMipTailFirstLod < mMipLevels)) {
                    std::cout << "Layer " << layer << "single mip tail" << std::endl;
                    // Allocate memory for the layer mip tail
                    VkMemoryAllocateInfo memAllocInfo = {};
                    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                    memAllocInfo.pNext = NULL;
                    memAllocInfo.allocationSize = sparseMemoryReq.imageMipTailSize;
                    memAllocInfo.memoryTypeIndex = mMemoryTypeIndex;

                    VkDeviceMemory deviceMemory;
                    if ( VK_FAILED(vkAllocateMemory(mpDevice->getApiHandle(), &memAllocInfo, nullptr, &deviceMemory)) ) {
                        LOG_ERR("Could not allocate memory !!!");
                        return;
                    }

                    // (Opaque) sparse memory binding
                    VkSparseMemoryBind sparseMemoryBind{};
                    sparseMemoryBind.resourceOffset = sparseMemoryReq.imageMipTailOffset + layer * sparseMemoryReq.imageMipTailStride;
                    sparseMemoryBind.size = sparseMemoryReq.imageMipTailSize;
                    sparseMemoryBind.memory = deviceMemory;

                    mOpaqueMemoryBinds.push_back(sparseMemoryBind);

                    mMipBases[sparseMemoryReq.imageMipTailFirstLod] = mSparsePagesCount;                    
                    //mSparsePagesCount += 1;
                }

            } // end layers and mips

            // Check if format has one mip tail for all layers
            if ((sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && (sparseMemoryReq.imageMipTailFirstLod < mMipLevels)) {
                std::cout << "One mip tail for all mip layers " << std::endl;
                // Allocate memory for the mip tail
                VkMemoryAllocateInfo memAllocInfo = {};
                memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                memAllocInfo.pNext = NULL;
                memAllocInfo.allocationSize = sparseMemoryReq.imageMipTailSize;
                memAllocInfo.memoryTypeIndex = mMemoryTypeIndex;

                VkDeviceMemory deviceMemory;
                if ( VK_FAILED(vkAllocateMemory(mpDevice->getApiHandle(), &memAllocInfo, nullptr, &deviceMemory)) ) {
                    LOG_ERR("Could not allocate memory !!!");
                    return;
                }

                // (Opaque) sparse memory binding
                VkSparseMemoryBind sparseMemoryBind{};
                sparseMemoryBind.resourceOffset = sparseMemoryReq.imageMipTailOffset;
                sparseMemoryBind.size = sparseMemoryReq.imageMipTailSize;
                sparseMemoryBind.memory = deviceMemory;

                mOpaqueMemoryBinds.push_back(sparseMemoryBind);
            }

            std::cout << "Texture info:" << std::endl;
            std::cout << "\tDim: " << mWidth << " x " << mHeight << std::endl;
            std::cout << "\tVirtual pages: " << mPages.size() << std::endl;
            std::cout << "\tSingle mip tail: " << (mMipTailInfo.singleMipTail ? "Yes" : "No") << std::endl;
            std::cout << "\tMip tail start: " << sparseMemoryReq.imageMipTailFirstLod << std::endl;
            std::cout << "\tMip tail size: " << sparseMemoryReq.imageMipTailSize << std::endl;

            // Create signal semaphore for sparse binding
            VkSemaphoreCreateInfo semaphoreCreateInfo = {};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreCreateInfo.pNext = NULL;
            semaphoreCreateInfo.flags = 0;

            if ( VK_FAILED(vkCreateSemaphore(mpDevice->getApiHandle(), &semaphoreCreateInfo, nullptr, &mBindSparseSemaphore)) ) {
                LOG_ERR("Could not create semaphore !!!");
                return;
            }

            // Prepare bind sparse info for reuse in queue submission
            updateSparseBindInfo();
        }

        // Allocate the GPU memory        
        if (!mIsSparse) {
            VkDeviceMemory deviceMem = VK_NULL_HANDLE;
            deviceMem = allocateDeviceMemory(mpDevice, Device::MemoryType::Default, mMemRequirements.memoryTypeBits, mMemRequirements.size);
            vkBindImageMemory(mpDevice->getApiHandle(), mImage, deviceMem, 0);
        }
        
        if (pData && !mIsSparse) {
            uploadInitData(pData, autoGenMips);
        }
    }

}  // namespace Falcor
