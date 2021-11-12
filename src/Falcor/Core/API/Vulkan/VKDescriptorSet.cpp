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
#include "Falcor/Core/API/DescriptorSet.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Buffer.h"
#include "Falcor/Utils/Debug/debug.h"

#include "VKDescriptorData.h"


namespace Falcor {

    VkDescriptorSetLayout createDescriptorSetLayout(std::shared_ptr<Device> pDevice, const DescriptorSet::Layout& layout);
    VkDescriptorType falcorToVkDescType(DescriptorPool::Type type);

    void DescriptorSet::apiInit() {
        auto layout = createDescriptorSetLayout(mpDevice, mLayout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mpPool->getApiHandle(0);
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;
        vk_call(vkAllocateDescriptorSets(mpPool->device()->getApiHandle(), &allocInfo, &mApiHandle));
        mpApiData = std::make_shared<DescriptorSetApiData>(mpPool->device(), layout, mpPool->getApiHandle(0), mApiHandle);
    }

    DescriptorSet::CpuHandle DescriptorSet::getCpuHandle(uint32_t rangeIndex, uint32_t descInRange) const {
        UNSUPPORTED_IN_VULKAN("DescriptorSet::getCpuHandle");
        return nullptr;
    }

    DescriptorSet::GpuHandle DescriptorSet::getGpuHandle(uint32_t rangeIndex, uint32_t descInRange) const {
        UNSUPPORTED_IN_VULKAN("DescriptorSet::getGpuHandle");
        return nullptr;
    }

    template<bool isUav, typename ViewType>
    static void setSrvUavCommon(Device::SharedPtr device, VkDescriptorSet set, uint32_t bindIndex, uint32_t arrayIndex, const ViewType* pView, DescriptorPool::Type type) {
        //LOG_DBG("setSrvUavCommon descriptor type %s", to_string(type).c_str());
        
        assert(pView);

        VkWriteDescriptorSet write = {};
        VkDescriptorImageInfo image;
        VkDescriptorBufferInfo buffer;
        typename ViewType::ApiHandle handle = pView->getApiHandle();
        VkBufferView texelBufferView = {};
        VkWriteDescriptorSetAccelerationStructureKHR descriptorSetAccelerationStructure = {};

        auto descriptorCount = 1;

        if (handle.getType() == VkResourceType::Buffer) {
            Buffer* pBuffer = dynamic_cast<Buffer*>(pView->getResource());

            //LOG_DBG("Buffer %zu update descriptor set bindFlags %s", pBuffer->id(),to_string(pBuffer->getBindFlags()).c_str());

            if (pBuffer) {

                if (pBuffer->isTyped()) {
                    texelBufferView = pBuffer->getUAV()->getApiHandle();
                    write.pTexelBufferView = &texelBufferView;
                    write.pBufferInfo = nullptr;
                } else {
                    buffer.buffer = pBuffer->getApiHandle();
                    buffer.offset = pBuffer->getGpuAddressOffset();
                    buffer.range = pBuffer->getSize();
                    write.pBufferInfo = &buffer;
                    write.pTexelBufferView = nullptr;
                }
            } else {
                if (type == DescriptorPool::Type::AccelerationStructureSrv ) {
                    // Empty acceleration structure view
                    descriptorSetAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                    descriptorSetAccelerationStructure.pNext = NULL;
                    descriptorSetAccelerationStructure.accelerationStructureCount = 0;
                    descriptorSetAccelerationStructure.pAccelerationStructures = VK_NULL_HANDLE;

                    write.pNext = &descriptorSetAccelerationStructure;
                    descriptorCount = 0;
                }
            }
            write.pImageInfo = nullptr;
        } else {
            assert(handle.getType() == VkResourceType::Image);
            
            Texture* pTexture = dynamic_cast<Texture*>(pView->getResource());
            //LOG_DBG("Texture %zu update descriptor set bindFlags %s", pTexture->id(),to_string(pTexture->getBindFlags()).c_str());

            image.imageLayout = isUav ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image.imageView = handle;
            image.sampler = nullptr;
            write.pImageInfo = &image;
        }

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = falcorToVkDescType(type);
        write.dstSet = set;
        write.dstBinding = bindIndex;
        write.dstArrayElement = arrayIndex;
        write.descriptorCount = descriptorCount;

        //LOG_DBG("vkUpdateDescriptorSets 1");
        vkUpdateDescriptorSets(device->getApiHandle(), 1, &write, 0, nullptr);
    }

    void DescriptorSet::setSrv(uint32_t rangeIndex, uint32_t descIndex, const ShaderResourceView* pSrv) {
        assert(pSrv && "ShaderResourceView pointer is NULL");
        setSrvUavCommon<false>(mpPool->device(), mApiHandle, mLayout.getRange(rangeIndex).baseRegIndex, descIndex, pSrv, mLayout.getRange(rangeIndex).type);
    }

    void DescriptorSet::setUav(uint32_t rangeIndex, uint32_t descIndex, const UnorderedAccessView* pUav) {
        assert(pUav && "UnorderedAccessView pointer is NULL");
        setSrvUavCommon<true>(mpPool->device(), mApiHandle, mLayout.getRange(rangeIndex).baseRegIndex, descIndex, pUav, mLayout.getRange(rangeIndex).type);
    }

    void DescriptorSet::setSampler(uint32_t rangeIndex, uint32_t descIndex, const Sampler* pSampler) {
        VkDescriptorImageInfo info;
        info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        info.imageView = nullptr;
        info.sampler = pSampler->getApiHandle();

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = mApiHandle;
        write.dstBinding = mLayout.getRange(rangeIndex).baseRegIndex;
        write.dstArrayElement = descIndex;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &info;

        //LOG_DBG("vkUpdateDescriptorSets 2");
        vkUpdateDescriptorSets(mpPool->device()->getApiHandle(), 1, &write, 0, nullptr);
        //LOG_DBG("vkUpdateDescriptorSets 2 done");
    }

    void DescriptorSet::setCbv(uint32_t rangeIndex, uint32_t descIndex, ConstantBufferView* pView) {
        //LOG_DBG("setCbv rangeIndex %u, descIndex %u, range type %s", rangeIndex, descIndex, to_string((DescriptorPool::Type)mLayout.getRange(rangeIndex).type).c_str());
        assert(pView && "ConstantBufferView pointer is NULL");
        VkDescriptorBufferInfo info;

        const auto& pBuffer = dynamic_cast<const Buffer*>(pView->getResource());
        assert(pBuffer);
        info.buffer = pBuffer->getApiHandle();
        info.offset = pBuffer->getGpuAddressOffset();
        info.range = pBuffer->getSize();

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = mApiHandle;
        write.dstBinding = mLayout.getRange(rangeIndex).baseRegIndex;
        write.dstArrayElement = descIndex;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &info;

        //LOG_DBG("Buffer %zu update descriptor set bindFlags %s", pBuffer->id(),to_string(pBuffer->getBindFlags()).c_str());
        //LOG_DBG("vkUpdateDescriptorSets 3");
        vkUpdateDescriptorSets(mpPool->device()->getApiHandle(), 1, &write, 0, nullptr);
        //LOG_DBG("vkUpdateDescriptorSets 3 done");
    }

    template<bool forGraphics>
    static void bindCommon(DescriptorSet::ApiHandle set, CopyContext* pCtx, const RootSignature* pRootSig, uint32_t bindLocation) {
        VkPipelineBindPoint bindPoint = forGraphics ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE;
        VkDescriptorSet vkSet = set;
        vkCmdBindDescriptorSets(pCtx->getLowLevelData()->getCommandList(), bindPoint, pRootSig->getApiHandle(), bindLocation, 1, &vkSet, 0, nullptr);
    }

    void DescriptorSet::bindForGraphics(CopyContext* pCtx, const RootSignature* pRootSig, uint32_t rootIndex) {
        bindCommon<true>(mApiHandle, pCtx, pRootSig, rootIndex);
    }

    void DescriptorSet::bindForCompute(CopyContext* pCtx, const RootSignature* pRootSig, uint32_t rootIndex) {
        bindCommon<false>(mApiHandle, pCtx, pRootSig, rootIndex);
    }

}  // namespace Falcor
