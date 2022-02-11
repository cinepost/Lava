/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#ifndef NVVK_MEMALLOCATOR_VMA_H_INCLUDED
#define NVVK_MEMALLOCATOR_VMA_H_INCLUDED

#include "nvvk/memallocator_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

namespace nvvk {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lava version of NVVK ResourceAllocatorVma. 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 \class nvvk::ResourceAllocatorVMA
 nvvk::ResourceAllocatorVMA is a convencience class creating, initializing and owning a nvvk::VmaAllocator
 and associated nvvk::MemAllocator object. 
*/
class LavaResourceAllocatorVma : public ResourceAllocator
{
public:
  LavaResourceAllocatorVma() = default;
  LavaResourceAllocatorVma(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize stagingBlockSize = NVVK_DEFAULT_STAGING_BLOCKSIZE, VmaAllocator vma = nullptr);
  virtual ~LavaResourceAllocatorVma();

  void init(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize stagingBlockSize = NVVK_DEFAULT_STAGING_BLOCKSIZE, VmaAllocator vma = nullptr);
  void deinit();

protected:
  VmaAllocator                  m_vma{nullptr};
  std::unique_ptr<MemAllocator> m_memAlloc;
};

}  // namespace nvvk

#include "Falcor/Core/API/Vulkan/nvvk_memallocator_vma_vk.inl"

#endif  // NVVK_MEMALLOCATOR_VMA_H_INCLUDED
