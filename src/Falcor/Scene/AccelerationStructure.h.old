#ifndef FALCOR_SCENE_ACCELERATION_STRUCTURE_H_
#define FALCOR_SCENE_ACCELERATION_STRUCTURE_H_

#include <glm/mat4x3.hpp>
#include <memory>
#include <vector>

#include <Falcor/Core/API/Vulkan/FalcorVK.h>
#include <Falcor/Core/API/Device.h>
#include <Falcor/Core/API/Buffer.h>


template<typename T>
inline T align_up(T value, T align) {
    return (value + align - T(1)) / align * align;
}

inline size_t align(size_t size, size_t min = 0) {
    if (min == 0)
        return align_up(size, sizeof(void*));

    return align_up((size + min - 1) & ~(min - 1), sizeof(void*));
}

template<typename T>
inline size_t align(size_t min = 0) {
    return align(sizeof(T), min);
}

inline void* alloc_data(size_t size, size_t alignment = sizeof(int8_t)) {
#if _WIN32
    return _aligned_malloc(size, alignment);
#else
    return aligned_alloc(alignment, size);
#endif
}

inline void free_data(void* data) {
#if _WIN32
    _aligned_free(data);
#else
    free(data);
#endif
}

inline void* realloc_data(void* data, size_t size, size_t alignment) {
#if _WIN32
    return _aligned_realloc(data, size, alignment);
#else
    return realloc(data, align(size, alignment));
#endif
}

struct no_copy_no_move {
    /**
     * @brief Construct a new object
     */
    no_copy_no_move() = default;

    /**
     * @brief No copy
     */
    no_copy_no_move(no_copy_no_move const&) = delete;

    /**
     * @brief No move
     */
    void operator=(no_copy_no_move const&) = delete;
};

struct memory : no_copy_no_move {
    static memory& get() {
        static memory memory;
        return memory;
    }

    static VkAllocationCallbacks* alloc() {
        if (get().use_custom_cpu_callbacks)
            return &get().vk_callbacks;

        return nullptr;
    }

    static uint32_t find_type_with_properties(VkPhysicalDeviceMemoryProperties properties, uint32_t type_bits,
                                          VkMemoryPropertyFlags required_properties);

    static uint32_t find_type(VkPhysicalDevice gpu, VkMemoryPropertyFlags properties, uint32_t type_bits);

    void set_callbacks(VkAllocationCallbacks const& callbacks) {
        vk_callbacks = callbacks;
    }

  private:
    memory();

    /// Use custom cpu callbacks
    bool use_custom_cpu_callbacks = true;

    /// @see https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkAllocationCallbacks.html
    VkAllocationCallbacks vk_callbacks = {};

};

namespace Falcor {

struct AccelerationStructure {
    using SharedPtr = std::shared_ptr<AccelerationStructure>;

    virtual ~AccelerationStructure() {
        destroy();
    }

    const VkPhysicalDeviceAccelerationStructurePropertiesKHR& getProperties() const {
        return mProperties;
    }

    virtual bool createAccelerationStructure(VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) = 0;
    virtual void destroy();

    // TODO host command versions of build and compact

    bool build(VkCommandBuffer cmd_buf, VkDeviceAddress scratch_buffer);
    bool update(VkCommandBuffer cmd_buf, VkDeviceAddress scratch_buffer) {
        return mBuilt ? build(cmd_buf, scratch_buffer) : false;
    }
    AccelerationStructure::SharedPtr compact(VkCommandBuffer cmd_buf);

    VkAccelerationStructureKHR getVkHandle() const { return mHandle; }

    Device::SharedPtr getDevice() { return mpDevice; }

    VkDeviceAddress getVkAddress() { return mVkAddress; }
    VkDeviceAddress getVkAddress() const { return mVkAddress; }

    VkDeviceSize scratchBufferSize() const;

  protected:
    AccelerationStructure(Device::SharedPtr pDevice);

    Device::SharedPtr mpDevice = nullptr;

    VkPhysicalDeviceAccelerationStructurePropertiesKHR mProperties;

    VkAccelerationStructureCreateInfoKHR mCreateInfo;
    mutable VkAccelerationStructureBuildGeometryInfoKHR mBuildInfo;

    VkAccelerationStructureKHR mHandle = VK_NULL_HANDLE;
    VkDeviceAddress mVkAddress = 0;

    //VkQueryPool query_pool = VK_NULL_HANDLE;
    QueryHeap::SharedPtr mpQueryHeap = nullptr;

    Buffer::SharedPtr mpASBuffer = nullptr;

    std::vector<VkAccelerationStructureGeometryKHR> mGeometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> mRanges;

    // this is set on the newly created acceleration structure by compact()
    VkDeviceSize mCompactSize = 0;

    bool mBuilt = false;

    bool createInternal(VkBuildAccelerationStructureFlagsKHR flags);
    void addGeometry(const VkAccelerationStructureGeometryDataKHR& geometry_data, VkGeometryTypeKHR type, const VkAccelerationStructureBuildRangeInfoKHR& range, VkGeometryFlagsKHR flags = 0);
    VkAccelerationStructureBuildSizesInfoKHR getSizes() const;
};

struct BottomLevelAccelerationStructure : AccelerationStructure {
    using SharedPtr = std::shared_ptr<BottomLevelAccelerationStructure>;
    using map = std::map<uint32_t, SharedPtr>;
    using list = std::vector<SharedPtr>;

    static SharedPtr create(Device::SharedPtr pDevice);

    virtual bool createAccelerationStructure(VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) override;

    void addGeometry(const VkAccelerationStructureGeometryTrianglesDataKHR& triangles, const VkAccelerationStructureBuildRangeInfoKHR& range, VkGeometryFlagsKHR flags = 0) {
        VkAccelerationStructureGeometryDataKHR geometryData = {};
        geometryData.triangles = triangles;
        AccelerationStructure::addGeometry(geometryData, VK_GEOMETRY_TYPE_TRIANGLES_KHR, range, flags);
    }
    void addGeometry(const VkAccelerationStructureGeometryAabbsDataKHR& aabbs, const VkAccelerationStructureBuildRangeInfoKHR& range, VkGeometryFlagsKHR flags = 0) {
        AccelerationStructure::addGeometry(VkAccelerationStructureGeometryDataKHR{ .aabbs = aabbs }, VK_GEOMETRY_TYPE_AABBS_KHR, range, flags);
    }

    void clearGeometries();

  protected:
    BottomLevelAccelerationStructure(Device::SharedPtr pDevice);
};

struct TopLevelAccelerationStructure : AccelerationStructure {
    using SharedPtr = std::shared_ptr<TopLevelAccelerationStructure>;
    using map = std::map<uint32_t, SharedPtr>;
    using list = std::vector<SharedPtr>;

    static SharedPtr create(Device::SharedPtr pDevice);

    virtual bool createAccelerationStructure(VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) override;
    virtual void destroy() override;

    const VkWriteDescriptorSetAccelerationStructureKHR* getDescriptorInfo() const {
        return &mDescriptor;
    };

    void addInstance(const VkAccelerationStructureInstanceKHR& instance);
    void addInstance(BottomLevelAccelerationStructure::SharedPtr pBlas);
    void addInstances(const std::vector<VkAccelerationStructureInstanceKHR>& instances);

    void updateInstance(uint32_t i, const VkAccelerationStructureInstanceKHR& instance);
    void updateInstance(uint32_t i, BottomLevelAccelerationStructure::SharedPtr pBlas);

    void setInstanceTransform(uint32_t i, const glm::mat4x3& transform);

    void clearInstances();

  protected:
    TopLevelAccelerationStructure(Device::SharedPtr pDevice);

  private:
    std::vector<VkAccelerationStructureInstanceKHR> mInstances;
    Buffer::SharedPtr mpInstancesBuffer = nullptr;
    VkWriteDescriptorSetAccelerationStructureKHR mDescriptor;
};

inline std::string to_string(VkStructureType vkType ) {
    std::string s = "VkStructureType: ";
    #define type_2_string(a) case a: return std::string("VkStructureType: ") + #a;
    switch(vkType) {
        type_2_string(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);
        default:
            should_not_get_here();
            return "VkStructureType: UNKNOWN";
    }
    #undef type_2_string
}

inline std::string to_string(const VkAccelerationStructureGeometryKHR &geometry) {
    std::string s = "VkAccelerationStructureGeometryKHR:";
    s += "\n sType: " + to_string(geometry.sType);
    s += "\n pNext: " + reinterpret_cast<std::size_t>(geometry.pNext);
    //s += ",map type " + to_string(buff->getType());
    return s;
}

} // namespace Falcor

#endif  // FALCOR_SCENE_ACCELERATION_STRUCTURE_H_