// vk-buffer.cpp

#include <string>

#include "vk-buffer.h"

#include "vk-util.h"
#if SLANG_WINDOWS_FAMILY
#    include <dxgi1_2.h>
#endif

#include "lava_utils_lib/logging.h"


namespace gfx {

using namespace Slang;

namespace vk {

VKBufferHandleRAII::~VKBufferHandleRAII() {
    if (m_api) {
        vmaDestroyBuffer(m_api->mVmaAllocator, m_buffer, mAllocation);
    }
}

Result VKBufferHandleRAII::init(
    const VulkanApi& api,
    Size bufferSize,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags reqMemoryProperties,
    bool isShared,
    VkExternalMemoryHandleTypeFlagsKHR extMemHandleType)
{
    assert(!isInitialized());

    m_api = &api;
    m_buffer = VK_NULL_HANDLE;

    mAllocationInfo = {};
    mAllocation = {};

    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkExternalMemoryBufferCreateInfo externalMemoryBufferCreateInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO };
    if (isShared) {
        externalMemoryBufferCreateInfo.handleTypes = extMemHandleType;
        bufferCreateInfo.pNext = &externalMemoryBufferCreateInfo;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.requiredFlags = reqMemoryProperties;

    if (usage & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR) {
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO; //VMA_MEMORY_USAGE_UNKNOWN;
    } else {
        allocInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
    }

    uint32_t memoryTypeIndex = 0;

    SLANG_VK_CHECK(vmaFindMemoryTypeIndexForBufferInfo(api.mVmaAllocator, &bufferCreateInfo, &allocInfo, &memoryTypeIndex));   

    //allocInfo.memoryTypeBits = 1u << memoryTypeIndex;

    SLANG_VK_CHECK(vmaCreateBuffer(api.mVmaAllocator, &bufferCreateInfo, &allocInfo, &m_buffer, &mAllocation, &mAllocationInfo));
    return SLANG_OK;
}

BufferResourceImpl::BufferResourceImpl(const IBufferResource::Desc& desc, DeviceImpl* renderer): Parent(desc), m_renderer(renderer) {
    assert(renderer);
}

BufferResourceImpl::~BufferResourceImpl() {
    if (sharedHandle.handleValue != 0) {
#if SLANG_WINDOWS_FAMILY
        CloseHandle((HANDLE)sharedHandle.handleValue);
#endif
    }
}

DeviceAddress BufferResourceImpl::getDeviceAddress() {
    if (!m_buffer.m_api->vkGetBufferDeviceAddress)
        return 0;

    VkBufferDeviceAddressInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = m_buffer.m_buffer;
    return (DeviceAddress)m_buffer.m_api->vkGetBufferDeviceAddress(m_buffer.m_api->m_device, &info);
}

Result BufferResourceImpl::getNativeResourceHandle(InteropHandle* outHandle) {
    outHandle->handleValue = (uint64_t)m_buffer.m_buffer;
    outHandle->api = InteropHandleAPI::Vulkan;
    return SLANG_OK;
}

Result BufferResourceImpl::getSharedHandle(InteropHandle* outHandle) {
    // Check if a shared handle already exists for this resource.
    if (sharedHandle.handleValue != 0) {
        *outHandle = sharedHandle;
        return SLANG_OK;
    }

    // If a shared handle doesn't exist, create one and store it.
#if SLANG_WINDOWS_FAMILY
    VkMemoryGetWin32HandleInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    info.pNext = nullptr;
    info.memory = m_buffer.mAllocationInfo.deviceMemory; //m_buffer.m_memory;
    info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    auto api = m_buffer.m_api;
    PFN_vkGetMemoryWin32HandleKHR vkCreateSharedHandle;
    vkCreateSharedHandle = api->vkGetMemoryWin32HandleKHR;
    if (!vkCreateSharedHandle) {
        return SLANG_FAIL;
    }
    SLANG_VK_RETURN_ON_FAIL( vkCreateSharedHandle(api->m_device, &info, (HANDLE*)&outHandle->handleValue));
#endif
    outHandle->api = InteropHandleAPI::Vulkan;
    return SLANG_OK;
}

Result BufferResourceImpl::map(MemoryRange* rangeToRead, void** outPointer) {
    SLANG_UNUSED(rangeToRead);
    auto api = m_buffer.m_api;
    SLANG_VK_RETURN_ON_FAIL( vmaMapMemory(api->mVmaAllocator, m_buffer.mAllocation, outPointer));
    return SLANG_OK;
}

Result BufferResourceImpl::unmap(MemoryRange* writtenRange) {
    SLANG_UNUSED(writtenRange);
    auto api = m_buffer.m_api;
    vmaUnmapMemory(api->mVmaAllocator, m_buffer.mAllocation);
    return SLANG_OK;
}

Result BufferResourceImpl::setDebugName(const char* name) {
    Parent::setDebugName(name);
    auto api = m_buffer.m_api;
    if (api->vkDebugMarkerSetObjectNameEXT) {
        LLOG_TRC << "Buffet debug name: " << std::string(name);
        VkDebugMarkerObjectNameInfoEXT nameDesc = {};
        nameDesc.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        nameDesc.object = (uint64_t)m_buffer.m_buffer;
        nameDesc.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
        nameDesc.pObjectName = name;
        api->vkDebugMarkerSetObjectNameEXT(api->m_device, &nameDesc);
    }
    return SLANG_OK;
}

} // namespace vk
} // namespace gfx
