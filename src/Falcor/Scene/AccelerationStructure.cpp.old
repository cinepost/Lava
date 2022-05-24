#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include "AccelerationStructure.h"
#include "Falcor/Utils/Debug/debug.h"



#define LAVA_CUSTOM_CPU_ALLOCATION_CALLBACK_USER_DATA (void*) (intptr_t) 20180208

static void* VKAPI_PTR custom_cpu_allocation(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
    assert(user_data == LAVA_CUSTOM_CPU_ALLOCATION_CALLBACK_USER_DATA);
    return alloc_data(size, alignment);
}

static void* VKAPI_PTR custom_cpu_reallocation(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
    assert(user_data == LAVA_CUSTOM_CPU_ALLOCATION_CALLBACK_USER_DATA);
    return realloc_data(original, size, alignment);
}

static void VKAPI_PTR custom_cpu_free(void* user_data, void* memory) {
    assert(user_data == LAVA_CUSTOM_CPU_ALLOCATION_CALLBACK_USER_DATA);
    free_data(memory);
}

memory::memory() {
    if (!use_custom_cpu_callbacks) {
        return;
    }

    vk_callbacks.pUserData = LAVA_CUSTOM_CPU_ALLOCATION_CALLBACK_USER_DATA;
    vk_callbacks.pfnAllocation = reinterpret_cast<PFN_vkAllocationFunction>(&custom_cpu_allocation);
    vk_callbacks.pfnReallocation = reinterpret_cast<PFN_vkReallocationFunction>(&custom_cpu_reallocation);
    vk_callbacks.pfnFree = reinterpret_cast<PFN_vkFreeFunction>(&custom_cpu_free);
}

namespace Falcor {

AccelerationStructure::AccelerationStructure(Device::SharedPtr pDevice): mpDevice(pDevice) {
  mProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
  
  mCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  mCreateInfo.pNext = NULL;

  mBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  mBuildInfo.pNext = NULL;

  mBuilt = false;
}

bool AccelerationStructure::createInternal(VkBuildAccelerationStructureFlagsKHR flags) {
    LOG_WARN("AccelerationStructure::createInternal");

    VkPhysicalDeviceProperties2 properties2 = { 
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, 
        .pNext = &mProperties 
    };
    
   // vkGetPhysicalDeviceProperties2(mpDevice->getApiHandle(), &properties2);

    mBuildInfo.type = mCreateInfo.type;
    mBuildInfo.flags = flags;

    printf("AccelerationStructure::createInternal 1\n");
    if (mCompactSize > 0) {
        // set by compact() before calling create() on the new AS
        mCreateInfo.size = mCompactSize;
    } else {
        mCreateInfo.size = getSizes().accelerationStructureSize;
    }

    printf("AccelerationStructure::createInternal 2\n");
    //as_buffer = make_buffer();

    mpASBuffer = Buffer::create(mpDevice, mCreateInfo.size, Buffer::BindFlags::AccelerationStructure, Buffer::CpuAccess::None);

    //if (!as_buffer->create(device, nullptr, mCreateInfo.size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))
    //    return false;

    printf("AccelerationStructure::createInternal 3\n");
    mCreateInfo.buffer = mpASBuffer->getApiHandle();
    mCreateInfo.offset = 0;
    mCreateInfo.deviceAddress = 0;
    mCreateInfo.createFlags = 0x00000000;

    printf("AccelerationStructure::createInternal 4\n");
    if (VK_FAILED(vkCreateAccelerationStructureKHR(mpDevice->getApiHandle(), &mCreateInfo, memory::alloc(), &mHandle))) {
        LOG_ERR("Error creating acceleration structure !!!");
        return false;
    }

    assert(mHandle != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR address_info = {};
        address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        address_info.accelerationStructure = mHandle;

    printf("AccelerationStructure::createInternal 5\n");
    mVkAddress = vkGetAccelerationStructureDeviceAddressKHR(mpDevice->getApiHandle(), &address_info);

    //const VkQueryPoolCreateInfo pool_info = {
    //    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    //    .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
    //    .queryCount = 1
    //};

    const auto queryCount = 1;
    mpQueryHeap = QueryHeap::create(mpDevice, QueryHeap::Type::AccelerationStructureCompactedSize, queryCount);

    //vk_call(vkCreateQueryPool(pDevice->getApiHandle(), &pool_info, memory::alloc(), &query_pool));

    LOG_WARN("AccelerationStructure::createInternal done!");
    return true;
}

void AccelerationStructure::destroy() {
    if (mHandle != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(mpDevice->getApiHandle(), mHandle, memory::alloc());
        mHandle = VK_NULL_HANDLE;
        mVkAddress = 0;
    }

    //if (mpQueryHeap) {
    //    vkDestroyQueryPool(mpDevice->getApiHandle(), query_pool, memory::alloc());
    //    query_pool = VK_NULL_HANDLE;
    //}

    //if (mpASBuffer) {
    //    as_buffer->destroy();
    //    mpASBuffer = nullptr;
    //}

    mGeometries.clear();
    mRanges.clear();

    mBuilt = false;
}

VkDeviceSize AccelerationStructure::scratchBufferSize() const {
    const VkAccelerationStructureBuildSizesInfoKHR sizes = getSizes();
    return std::max(sizes.buildScratchSize, sizes.updateScratchSize);
}

bool AccelerationStructure::build(VkCommandBuffer cmd_buf, VkDeviceAddress scratch_buffer) {
    LOG_WARN("AccelerationStructure::build");

    assert(scratch_buffer);

    if (mHandle == VK_NULL_HANDLE) {
        LOG_ERR("AccelerationStructure::build mHandle == VK_NULL_HANDLE");
        return false;
    }
    
    if (mBuilt && !(mBuildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR)) {
        LOG_ERR("AccelerationStructure::build mBuildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR");
        return false;
    }

    mBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    mBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    mBuildInfo.dstAccelerationStructure = mHandle;
    mBuildInfo.geometryCount = uint32_t(mGeometries.size());
    mBuildInfo.pGeometries = mGeometries.data();
    mBuildInfo.scratchData.deviceAddress = scratch_buffer;

    if (mBuilt) {
        mBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
        mBuildInfo.srcAccelerationStructure = mHandle;
    }

    const VkAccelerationStructureBuildRangeInfoKHR* buildRanges = mRanges.data();

    //

    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

    if (srcStageMask != dstStageMask) {
        VkBufferMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VkAccessFlagBits(0);
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.buffer = mpASBuffer->getApiHandle();
        barrier.offset = mpASBuffer->getGpuAddressOffset();
        barrier.size = mpASBuffer->getSize();

        vkCmdPipelineBarrier(cmd_buf, srcStageMask, dstStageMask, 0, 0, nullptr, 1, &barrier, 0, nullptr);

        //mpASBuffer->setGlobalState(Resource::State::AccelStructWrite);
        //return true;
    }
    //return false;

    //

    vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &mBuildInfo, &buildRanges);
    mBuilt = true;

    if (mBuildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) {
        LOG_WARN("Get compacted structure size");
        VkMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

        LOG_WARN("11");
        vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        //vkCmdResetQueryPool(cmd_buf, query_pool, 0, 1);
        vkCmdResetQueryPool(cmd_buf, mpQueryHeap->getApiHandle(), 0, 1);
        vkCmdWriteAccelerationStructuresPropertiesKHR(cmd_buf, 1, &mHandle, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, mpQueryHeap->getApiHandle(), 0);
        LOG_WARN("Get compacted structure size done");
    }

    LOG_WARN("AccelerationStructure::build done");
    return true;
}

AccelerationStructure::SharedPtr AccelerationStructure::compact(VkCommandBuffer cmd_buf) {
    LOG_WARN("AccelerationStructure::compact");

    if (!mBuilt) {
        LOG_ERR("AccelerationStructure::compact acceleration structure not built !!!");
        return nullptr;
    }

    if (!(mBuildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)) {
        LOG_ERR("AccelerationStructure::compact no compaction allowed !!!");
        return nullptr;
    }

    AccelerationStructure::SharedPtr new_structure = nullptr;

    if (mBuildInfo.type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR) {
        new_structure = BottomLevelAccelerationStructure::create(mpDevice);
    } else if (mBuildInfo.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR) {
        new_structure = TopLevelAccelerationStructure::create(mpDevice);
    } else {
        LOG_ERR("AccelerationStructure::compact no compaction allowed !!!");
        return nullptr;
    }

    new_structure->mBuildInfo = mBuildInfo;
    new_structure->mGeometries = mGeometries;
    new_structure->mRanges = mRanges;
    new_structure->mBuilt = mBuilt;

    LOG_WARN("AccelerationStructure::compact 1");
    assert(mpQueryHeap);

    VkDeviceSize compactSize = 0;
    //vk_call(vkGetQueryPoolResults(mpDevice->getApiHandle(), mpQueryHeap->getApiHandle(), 0, 1, sizeof(VkDeviceSize), &new_structure->mCompactSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT));
    vk_call(vkGetQueryPoolResults(mpDevice->getApiHandle(), mpQueryHeap->getApiHandle(), 0, 1, sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT));
    
    LOG_WARN("AccelerationStructure::compact 1.1");

    if (!new_structure->createAccelerationStructure()) {
        LOG_ERR("AccelerationStructure::compact error creating new structure !!!");
        return nullptr;
    }

    VkCopyAccelerationStructureInfoKHR copy_info = {};
    copy_info.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
    copy_info.src = mHandle;
    copy_info.dst = new_structure->mHandle;
    copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
    
    LOG_WARN("AccelerationStructure::compact 2");

    vkCmdCopyAccelerationStructureKHR(cmd_buf, &copy_info);

    LOG_WARN("AccelerationStructure::compact done!");
    return new_structure;
}

void AccelerationStructure::addGeometry(const VkAccelerationStructureGeometryDataKHR& geometryData, VkGeometryTypeKHR geometryType, const VkAccelerationStructureBuildRangeInfoKHR& range, VkGeometryFlagsKHR flags) {
    LOG_DBG("AccelerationStructure::addGeometry");
    if (mBuilt) {
        LOG_ERR("Attempting to add geometry to already built acceleration structure!!!");
        return;
    }

    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.pNext = NULL;
    geometry.geometryType = geometryType;
    geometry.geometry = geometryData;
    geometry.flags = flags; 

    std::cout << to_string(geometry) << std::endl;

    mGeometries.push_back(geometry);
    mRanges.push_back(range);

    LOG_DBG("AccelerationStructure::addGeometry done!");
}

VkAccelerationStructureBuildSizesInfoKHR AccelerationStructure::getSizes() const {
    LOG_DBG("AccelerationStructure::getSizes");

    assert(mGeometries.size() > 0);
    assert(mGeometries.size() == mRanges.size());

    auto buildInfo = mBuildInfo;

    if(!mBuilt) {
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
    } else {
        buildInfo.srcAccelerationStructure = mHandle;
    }

    if (mBuildInfo.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR) {
        LOG_WARN("Getting build sizes for TLAS");
    } else {
        LOG_WARN("Getting build sizes for BLAS");
    }

    buildInfo.pGeometries = mGeometries.data();
    buildInfo.geometryCount = uint32_t(mGeometries.size());

    const VkAccelerationStructureBuildTypeKHR buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
    std::vector<uint32_t> primitive_counts(mRanges.size());
    
    std::transform(mRanges.begin(), mRanges.end(), primitive_counts.begin(), [](const VkAccelerationStructureBuildRangeInfoKHR& r) { return r.primitiveCount; });

    size_t i = 0;
    printf("primitive_counts\n");
    for(i = 0; i < primitive_counts.size(); i++) {
        printf("%zu - %u\n", i, primitive_counts[i]);
    }

    VkAccelerationStructureBuildSizesInfoKHR sizesInfo = {};
    sizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    sizesInfo.pNext = NULL;
    
    vkGetAccelerationStructureBuildSizesKHR(mpDevice->getApiHandle(), buildType, &buildInfo, primitive_counts.data(), &sizesInfo);
    
    printf("accelerationStructureSize: %zu \n", sizesInfo.accelerationStructureSize);
    printf("updateScratchSize: %zu \n", sizesInfo.updateScratchSize);
    printf("buildScratchSize: %zu \n", sizesInfo.buildScratchSize);

    LOG_DBG("AccelerationStructure::getSizes done !");
    return sizesInfo;
}

BottomLevelAccelerationStructure::SharedPtr BottomLevelAccelerationStructure::create(Device::SharedPtr pDevice) {
    return SharedPtr(new BottomLevelAccelerationStructure(pDevice)); 
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(Device::SharedPtr pDevice): AccelerationStructure::AccelerationStructure(pDevice) {
    mBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
}

bool BottomLevelAccelerationStructure::createAccelerationStructure(VkBuildAccelerationStructureFlagsKHR flags) {
    LOG_WARN("BottomLevelAccelerationStructure::createAccelerationStructure");
    mCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    bool result = createInternal(flags);

    LOG_WARN("BottomLevelAccelerationStructure::createAccelerationStructure done !");
    return result;
}

void BottomLevelAccelerationStructure::clearGeometries() {
    mGeometries.clear();
    mRanges.clear();
}

TopLevelAccelerationStructure::SharedPtr TopLevelAccelerationStructure::create(Device::SharedPtr pDevice) {
    return SharedPtr(new TopLevelAccelerationStructure(pDevice)); 
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure(Device::SharedPtr pDevice): AccelerationStructure::AccelerationStructure(pDevice) {
    mBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkWriteDescriptorSetAccelerationStructureKHR descriptor = {};
    descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptor.accelerationStructureCount = 1;
    descriptor.pAccelerationStructures = &mHandle;

    mDescriptor = descriptor;
}

bool TopLevelAccelerationStructure::createAccelerationStructure(VkBuildAccelerationStructureFlagsKHR flags) {
    mCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    printf("TLAS::createAccelerationStructure 1\n");

    mpInstancesBuffer = Buffer::create(mpDevice, sizeof(VkAccelerationStructureInstanceKHR) * mInstances.size(), 
        Buffer::BindFlags::AccelerationStructureBuild, Buffer::CpuAccess::Write, mInstances.data());

    printf("TLAS::createAccelerationStructure 2\n");
    VkDeviceOrHostAddressConstKHR data = {};
    data.deviceAddress = mpInstancesBuffer->getGpuAddress();
    data.hostAddress = nullptr;

    VkAccelerationStructureGeometryDataKHR geometry = {};

    VkAccelerationStructureGeometryInstancesDataKHR instances = {};
    instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instances.pNext = NULL;
    instances.arrayOfPointers = VK_FALSE;
    instances.data = data;

    geometry.instances = instances;
    
    VkAccelerationStructureBuildRangeInfoKHR range = {};
    range.primitiveCount = uint32_t(mInstances.size());
    range.primitiveOffset = 0;
    range.firstVertex = 0;
    range.transformOffset = 0;

    printf("TLAS::createAccelerationStructure 3\n");
    addGeometry(geometry, VK_GEOMETRY_TYPE_INSTANCES_KHR, range);

    printf("TLAS::createAccelerationStructure 4\n");
    return createInternal(flags);
}

void TopLevelAccelerationStructure::destroy() {
    mInstances.clear();
    //instance_buffer.destroy();
    AccelerationStructure::destroy();
}

void TopLevelAccelerationStructure::addInstance(const VkAccelerationStructureInstanceKHR& instance) {
    if (mBuilt) {
        return;
    }

    mInstances.push_back(instance);
}

void TopLevelAccelerationStructure::addInstances(const std::vector<VkAccelerationStructureInstanceKHR>& instances) {
    printf("Adding %zu instances to TLAS\n", instances.size());
    if (mBuilt) {
        LOG_ERR("Error adding instances !!!");
        return;
    }

    //mInstances.insert( mInstances.end(), instances.begin(), instances.end() );

    for(auto &instance: instances) {
        mInstances.push_back(instance);
    }

    printf("TLAS instances count after addition is %zu\n", mInstances.size());
}

void TopLevelAccelerationStructure::addInstance(BottomLevelAccelerationStructure::SharedPtr pBlas) {
    assert(pBlas);

    if (mBuilt) {
        return;
    }

    VkAccelerationStructureInstanceKHR instance = {}; 
    instance.transform = *reinterpret_cast<const VkTransformMatrixKHR*>(glm::value_ptr(glm::identity<glm::mat4x3>()));
    instance.mask = 0xff;
    instance.accelerationStructureReference = pBlas->getVkAddress();

    mInstances.push_back(instance);
}

void TopLevelAccelerationStructure::updateInstance(uint32_t i, const VkAccelerationStructureInstanceKHR& instance) {
    if (i < mInstances.size()) {
        mInstances[i] = instance;

        if (mpInstancesBuffer) {
            VkAccelerationStructureInstanceKHR* buffer_instances = static_cast<VkAccelerationStructureInstanceKHR*>(mpInstancesBuffer->map(Buffer::MapType::Write));
            buffer_instances[i] = instance;
            mpInstancesBuffer->unmap();
        }
    }
}

void TopLevelAccelerationStructure::updateInstance(uint32_t i, BottomLevelAccelerationStructure::SharedPtr pBlas) {
    assert(pBlas);

    if (i < mInstances.size()) {
        mInstances[i].accelerationStructureReference = pBlas->getVkAddress();
        
        if (mpInstancesBuffer) {
            VkAccelerationStructureInstanceKHR* buffer_instances = static_cast<VkAccelerationStructureInstanceKHR*>(mpInstancesBuffer->map(Buffer::MapType::Write));
            buffer_instances[i].accelerationStructureReference = pBlas->getVkAddress();
            mpInstancesBuffer->unmap();
        }
    }
}

void TopLevelAccelerationStructure::setInstanceTransform(uint32_t i, const glm::mat4x3& transform) {
    static_assert(sizeof(glm::mat4x3) == sizeof(VkTransformMatrixKHR::matrix));
    if (i < mInstances.size()) {
        const glm::mat3x4 transposed = glm::transpose(transform);
        const VkTransformMatrixKHR& transform_ref = *reinterpret_cast<const VkTransformMatrixKHR*>(glm::value_ptr(transposed));
        mInstances[i].transform = transform_ref;
        if (mpInstancesBuffer) {
            VkAccelerationStructureInstanceKHR* buffer_instances = static_cast<VkAccelerationStructureInstanceKHR*>(mpInstancesBuffer->map(Buffer::MapType::Write));
            buffer_instances[i].transform = transform_ref;
            mpInstancesBuffer->unmap();
        }
    }
}

void TopLevelAccelerationStructure::clearInstances() {
    mGeometries.clear();
    mRanges.clear();
    mInstances.clear();
}

} // namespace Falcor