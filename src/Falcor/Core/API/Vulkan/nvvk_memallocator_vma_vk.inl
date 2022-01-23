#include "VulkanMemoryAllocator/vk_mem_alloc.h"
#include "nvvk/memallocator_vma_vk.hpp"
#include "nvvk/error_vk.hpp"

namespace nvvk {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lava version of NVVK ResourceAllocatorVma. 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline LavaResourceAllocatorVma::LavaResourceAllocatorVma(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize stagingBlockSize, VmaAllocator vma )
{
  init(instance, device, physicalDevice, stagingBlockSize, vma);
}

inline LavaResourceAllocatorVma::~LavaResourceAllocatorVma()
{
  deinit();
}

inline void LavaResourceAllocatorVma::init(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize stagingBlockSize, VmaAllocator vma)
{

  assert(vma);

  //VmaAllocatorCreateInfo allocatorInfo = {};
  //allocatorInfo.physicalDevice         = physicalDevice;
  //allocatorInfo.device                 = device;
  //allocatorInfo.instance               = instance;
  //allocatorInfo.flags                  = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  //vmaCreateAllocator(&allocatorInfo, &m_vma);
  m_vma = vma;

  m_memAlloc.reset(new VMAMemoryAllocator(device, physicalDevice, m_vma));
  ResourceAllocator::init(device, physicalDevice, m_memAlloc.get(), stagingBlockSize);
}

inline void LavaResourceAllocatorVma::deinit()
{
  ResourceAllocator::deinit();

  m_memAlloc.reset();
  //vmaDestroyAllocator(m_vma);
  m_vma = nullptr;
}

}  // namespace nvvk
