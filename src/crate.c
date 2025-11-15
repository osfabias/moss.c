/*
  Copyright 2025 Osfabias

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  @file src/internal/crate.h
  @brief Crate abstraction functions for ease of work with Vulkan buffers
         implementation.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#include <stdint.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include "moss/result.h"

#include "src/internal/crate.h"
#include "src/internal/log.h"
#include "src/internal/memory_utils.h"

MossResult moss__create_crate (
  const VkPhysicalDevice           physical_device,
  const VkDevice                   device,
  const Moss__CrateCreateInfo *const info,
  Moss__Crate *const                 out_crate
)
{
  {  // Create buffer itself
    const VkBufferCreateInfo create_info = {
      .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size                  = info->size,
      .usage                 = info->usage,
      .sharingMode           = info->sharing_mode,
      .queueFamilyIndexCount = info->shared_queue_family_index_count,
      .pQueueFamilyIndices   = info->shared_queue_family_indices,
    };

    const VkResult result =
      vkCreateBuffer (device, &create_info, NULL, &out_crate->buffer);
    if (result != VK_SUCCESS)
    {
      moss__error ("Failed to create buffer: %d.", result);
      return MOSS_RESULT_ERROR;
    }
  }

  {  // Allocate buffer memory
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements (device, out_crate->buffer, &memory_requirements);

    // Save actual buffer size
    out_crate->size = memory_requirements.size;

    // Find suitable memory type index
    uint32_t suitable_memory_type_index;
    {
      const MossResult result = moss__select_suitable_memory_type (
        physical_device,
        memory_requirements.memoryTypeBits,
        info->memory_properties,
        &suitable_memory_type_index
      );
      if (result != MOSS_RESULT_SUCCESS)
      {
        vkDestroyBuffer (device, out_crate->buffer, NULL);
        moss__error ("Failed to find suitable memory type for buffer.");
        return MOSS_RESULT_ERROR;
      }
    }

    // Allocate memory
    const VkMemoryAllocateInfo alloc_info = {
      .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize  = memory_requirements.size,
      .memoryTypeIndex = suitable_memory_type_index,
    };
    const VkResult result =
      vkAllocateMemory (device, &alloc_info, NULL, &out_crate->memory);
    if (result != VK_SUCCESS)
    {
      vkDestroyBuffer (device, out_crate->buffer, NULL);
      moss__error ("Failed to allocate buffer memory: %d.", result);
      return MOSS_RESULT_ERROR;
    }
  }

  // After all bind memory to the buffer
  vkBindBufferMemory (device, out_crate->buffer, out_crate->memory, 0);

  // Save device handles for later cleanup
  out_crate->original_device         = device;
  out_crate->original_physical_device = physical_device;

  return MOSS_RESULT_SUCCESS;
}

MossResult moss__fill_crate (
  Moss__Crate *destination_crate,
  void        *source,
  VkDeviceSize size
)
{
  void *mapped_memory;
  vkMapMemory (
    destination_crate->original_device,
    destination_crate->memory,
    0,
    destination_crate->size,
    0,
    &mapped_memory
  );

  memcpy (mapped_memory, source, size);

  vkUnmapMemory (destination_crate->original_device, destination_crate->memory);

  return MOSS_RESULT_SUCCESS;
}

void moss__free_crate (Moss__Crate *crate)
{
  if (crate == NULL) { return; }

  if (crate->memory != VK_NULL_HANDLE)
  {
    vkFreeMemory (crate->original_device, crate->memory, NULL);
    crate->memory = VK_NULL_HANDLE;
  }

  if (crate->buffer != VK_NULL_HANDLE)
  {
    vkDestroyBuffer (crate->original_device, crate->buffer, NULL);
    crate->buffer = VK_NULL_HANDLE;
  }

  crate->size                    = 0;
  crate->original_device         = VK_NULL_HANDLE;
  crate->original_physical_device = VK_NULL_HANDLE;
}
