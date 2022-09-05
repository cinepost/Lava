// vk-query.cpp

#include "FalcorVK.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Core/API/RtQueryPool.h"

using namespace Slang;

namespace Falcor {

Slang::Result RtQueryPool::RtQueryPool(Device::SharedPtr pDevice, const Desc& desc): mpDevice(pDevice) {
    VkQueryPool pool = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.queryCount = (uint32_t)desc.count;
    switch (desc.type) {
        case QueryType::Timestamp:
            createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            break;
        case QueryType::CompactedSize:
            createInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
            break;
        case QueryType::SerializationSize:
            createInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR;
            break;
        case QueryType::CurrentSize:
            // Vulkan does not support CurrentSize query, will not create actual pools here.
            return SLANG_OK;
        default:
            return SLANG_E_INVALID_ARG;
    }
    SLANG_VK_RETURN_ON_FAIL(vkCreateQueryPool(mpDevice->getApiHandle(), &createInfo, nullptr, &pool));
    mApiHandle = ApiHandle::create(pDevice, pool);
}

RtQueryPool::~RtQueryPool(){
    vkDestroyQueryPool(mpDevice->getApiHandle(), mApiHandle, nullptr);
}

int32_t RtQueryPool::getResult(int index, int count, uint64_t* data) {
    if (!mApiHandle) {
        // Vulkan does not support CurrentSize query, return 0 here.
        for (SlangInt i = 0; i < count; i++)
            data[i] = 0;
        return SLANG_OK;
    }

    SLANG_VK_RETURN_ON_FAIL(vkGetQueryPoolResults(
        mpDevice->getApiHandle(),
        mApiHandle,
        (uint32_t)index,
        (uint32_t)count,
        sizeof(uint64_t) * count,
        data,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    return SLANG_OK;
}

void _writeTimestamp(VkCommandBuffer vkCmdBuffer, IQueryPool* queryPool, SlangInt index) {
    auto queryPoolImpl = static_cast<QueryPoolImpl*>(queryPool);
    vkCmdResetQueryPool(vkCmdBuffer, mApiHandle, (uint32_t)index, 1);
    vkCmdWriteTimestamp(vkCmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mApiHandle, (uint32_t)index);
}

}  // namespace Falcor
