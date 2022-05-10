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

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/Vulkan/FalcorVK.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Debug/debug.h"

//#define VMA_IMPLEMENTATION
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include "Falcor/Core/API/Buffer.h"


#define GFX_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT ( 256 )
#define GFX_TEXTURE_DATA_PLACEMENT_ALIGNMENT ( 512 )

namespace Falcor {

void* mapBufferApi(std::shared_ptr<Device> pDevice, const Buffer::ApiHandle& apiHandle, size_t size) {
    void* pData;
    vk_call(vmaMapMemory(pDevice->allocator(), apiHandle.allocation(), &pData));
    return pData;
}

VkBufferUsageFlags getBufferUsageFlag(Buffer::BindFlags bindFlags) {
    // Assume every buffer can be read from and written into
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    auto setBit = [&flags, &bindFlags](Buffer::BindFlags f, VkBufferUsageFlags vkBit) {
        if (is_set(bindFlags, f)) {
            flags |= vkBit;
        }
    };
    
    setBit(Buffer::BindFlags::Vertex,           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    setBit(Buffer::BindFlags::Index,            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

    //setBit(Buffer::BindFlags::Vertex,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    //setBit(Buffer::BindFlags::Index, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

    setBit(Buffer::BindFlags::UnorderedAccess,  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    setBit(Buffer::BindFlags::ShaderResource,   VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    
    setBit(Buffer::BindFlags::IndirectArg,      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    setBit(Buffer::BindFlags::Constant,         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    setBit(Buffer::BindFlags::AccelerationStructure, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    setBit(Buffer::BindFlags::AccelerationStructureScratch, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR);

    setBit(Buffer::BindFlags::AccelerationStructureBuild, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

    setBit(Buffer::BindFlags::AccelerationStructureInput, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

    //setBit(Buffer::BindFlags::ShaderBindingTable, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR);

    return flags;
}

// VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, false, VMA_MEMORY_USAGE_CPU_TO_GPU

size_t getBufferDataAlignment(const Buffer* pBuffer) {
    //VkMemoryRequirements reqs;
    //vkGetBufferMemoryRequirements(pBuffer->device()->getApiHandle(), pBuffer->getApiHandle(), &reqs);
    //return reqs.alignment;

    // This in order of the alignment size
    const auto& bindFlags = pBuffer->getBindFlags();
    if (is_set(bindFlags, Buffer::BindFlags::Constant)) return GFX_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    if (is_set(bindFlags, Buffer::BindFlags::Index)) return sizeof(uint32_t); // This actually depends on the size of the index, but we can handle losing 2 bytes

    return GFX_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
}

Buffer::ApiHandle createBuffer(Device::SharedPtr pDevice, size_t size, Buffer::BindFlags bindFlags, GpuMemoryHeap::Type memType) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.flags = 0;
    bufferInfo.size = size;
    bufferInfo.usage = getBufferUsageFlag(bindFlags);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.queueFamilyIndexCount = 0;
    bufferInfo.pQueueFamilyIndices = nullptr;
    
    VkBuffer buffer;

    VmaAllocationCreateInfo allocInfo = {};

    switch(memType) {
        case GpuMemoryHeap::Type::Readback:
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            break;
        case GpuMemoryHeap::Type::Upload:
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            break;
        //case GpuMemoryHeap::Type::Default:
        //    allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
        //    break;
        default:
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
    }

    VmaAllocation allocation;
    vmaCreateBuffer(pDevice->allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

    VmaStats stats = {};
    vmaCalculateStats(pDevice->allocator(), &stats);

    Buffer::ApiHandle apiHandle = Buffer::ApiHandle::create(pDevice, buffer, allocation);
    return apiHandle;
}

void Buffer::apiInit(bool hasInitData) {
    if (mBindFlags == BindFlags::Constant) {
        mSize = align_to((size_t)GFX_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, mSize);
    }

    if (mCpuAccess == CpuAccess::Write) {
        mDynamicData = mpDevice->getUploadHeap()->allocate(mSize);
        mApiHandle = mDynamicData.pResourceHandle;
    } else {
        if (mCpuAccess == CpuAccess::Read && mBindFlags == BindFlags::None) {
            mApiHandle = createBuffer(mpDevice, mSize, mBindFlags, Device::MemoryType::Readback);
        } else {
            mApiHandle = createBuffer(mpDevice, mSize, mBindFlags, Device::MemoryType::Default);
        }
    }
}

uint64_t Buffer::getGpuAddress() const {
    //UNSUPPORTED_IN_VULKAN(__FUNCTION__);
    //return 0;
    VkBufferDeviceAddressInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = mApiHandle.getBuffer();

    return vkGetBufferDeviceAddressKHR(mpDevice->getApiHandle(), &info);
}

void Buffer::unmap() {
    if (mpStagingResource) {
        mpStagingResource->unmap();
        mpStagingResource = nullptr;
    } else if (mDynamicData.pData == nullptr && mBindFlags == BindFlags::None) {
        // We only unmap staging buffers
        assert(mCpuAccess == CpuAccess::Read);
        //vkUnmapMemory(mpDevice->getApiHandle(), mApiHandle);
        vmaUnmapMemory(mpDevice->allocator(), mAllocation);
    }
}

/*
uint64_t Buffer::makeResident(Buffer::GpuAccessFlags flags) const {
    UNSUPPORTED_IN_VULKAN(__FUNCTION__);
    return 0;
}

void Buffer::evict() const {
    UNSUPPORTED_IN_VULKAN(__FUNCTION__);
}
*/
}  // namespace Falcor
