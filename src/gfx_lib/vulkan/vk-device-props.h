#ifndef SRC_GFX_VULKAN_VK_DEVICE_PROPS_H_
#define SRC_GFX_VULKAN_VK_DEVICE_PROPS_H_

#include <cstdint>

namespace gfx {

namespace vk {

struct SubgroupSizeControlProperties {
	uint32_t minSubgroupSize;
	uint32_t maxSubgroupSize;
	uint32_t maxComputeWorkgroupSubgroups;
};

struct AccelerationStructureProperties {
	uint64_t maxGeometryCount;
	uint64_t maxInstanceCount;
	uint64_t maxPrimitiveCount;
	uint32_t maxPerStageDescriptorAccelerationStructures;
	uint32_t maxPerStageDescriptorUpdateAfterBindAccelerationStructures;
	uint32_t maxDescriptorSetAccelerationStructures;
	uint32_t maxDescriptorSetUpdateAfterBindAccelerationStructures;
	uint32_t minAccelerationStructureScratchOffsetAlignment;
};

	
} // namespace vk
} // namespace gfx

#endif // SRC_GFX_VULKAN_VK_DEVICE_PROPS_H_