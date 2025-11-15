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
#include "src/internal/vk_buffer_utils.h"

MossResult
moss__create_crate (const Moss__CrateCreateInfo *const info, Moss__Crate *const out_crate)
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
      vkCreateBuffer (info->device, &create_info, NULL, &out_crate->buffer);
    if (result != VK_SUCCESS)
    {
      moss__error ("Failed to create buffer: %d.", result);
      return MOSS_RESULT_ERROR;
    }
  }

  {  // Allocate buffer memory
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements (info->device, out_crate->buffer, &memory_requirements);

    // Save actual buffer size
    out_crate->size = memory_requirements.size;

    // Find suitable memory type index
    uint32_t suitable_memory_type_index;
    {
      const MossResult result = moss__select_suitable_memory_type (
        info->physical_device,
        memory_requirements.memoryTypeBits,
        info->memory_properties,
        &suitable_memory_type_index
      );
      if (result != MOSS_RESULT_SUCCESS)
      {
        moss__destroy_crate (out_crate);
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
      vkAllocateMemory (info->device, &alloc_info, NULL, &out_crate->memory);
    if (result != VK_SUCCESS)
    {
      moss__destroy_crate (out_crate);
      moss__error ("Failed to allocate buffer memory: %d.", result);
      return MOSS_RESULT_ERROR;
    }
  }

  // After all bind memory to the buffer
  vkBindBufferMemory (info->device, out_crate->buffer, out_crate->memory, 0);

  // Save device handles for later cleanup
  out_crate->original_device          = info->device;
  out_crate->original_physical_device = info->physical_device;

  // Save sharing mode and queue family indices
  out_crate->sharing_mode                    = info->sharing_mode;
  out_crate->shared_queue_family_index_count = info->shared_queue_family_index_count;
  if (info->shared_queue_family_index_count > 0 &&
      info->shared_queue_family_indices != NULL)
  {
    for (uint32_t i = 0; i < info->shared_queue_family_index_count && i < 2; ++i)
    {
      out_crate->shared_queue_family_indices[ i ] =
        info->shared_queue_family_indices[ i ];
    }
  }

  return MOSS_RESULT_SUCCESS;
}

MossResult moss__fill_crate (const Moss__FillCrateInfo *const info)
{
  Moss__Crate *const dst_crate = info->destination_crate;

  // Create staing crate
  Moss__Crate staging_crate;
  {
    const Moss__CrateCreateInfo create_info = {
      .size  = dst_crate->size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .memory_properties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

      .sharing_mode                    = dst_crate->sharing_mode,
      .shared_queue_family_index_count = dst_crate->shared_queue_family_index_count,
      .shared_queue_family_indices     = dst_crate->shared_queue_family_indices,
      .device                          = dst_crate->original_device,
      .physical_device                 = dst_crate->original_physical_device,
    };
    moss__create_crate (&create_info, &staging_crate);
  }

  // Copy data to the staging buffer
  void *mapped_memory;
  vkMapMemory (
    staging_crate.original_device,
    staging_crate.memory,
    0,
    staging_crate.size,
    0,
    &mapped_memory
  );

  memcpy (mapped_memory, info->source_memory, info->size);

  vkUnmapMemory (staging_crate.original_device, staging_crate.memory);

  {  // Copy data from staging buffer to the destination buffer
    const Moss__CopyVkBufferInfo copy_info = {
      .transfer_queue     = info->transfer_queue,
      .command_pool       = info->command_pool,
      .device             = dst_crate->original_device,
      .destination_buffer = dst_crate->buffer,
      .source_buffer      = staging_crate.buffer,
      .size               = staging_crate.size,
    };
    const MossResult result = moss__copy_vk_buffer (&copy_info);
    if (result != MOSS_RESULT_SUCCESS)
    {
      moss__error ("Failed to copy vulkan buffer.\n");
      return MOSS_RESULT_ERROR;
    }
  }

  moss__destroy_crate (&staging_crate);

  return MOSS_RESULT_SUCCESS;
}

void moss__destroy_crate (Moss__Crate *crate)
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

  crate->size                             = 0;
  crate->original_device                  = VK_NULL_HANDLE;
  crate->original_physical_device         = VK_NULL_HANDLE;
  crate->sharing_mode                     = VK_SHARING_MODE_EXCLUSIVE;
  crate->shared_queue_family_index_count  = 0;
  crate->shared_queue_family_indices[ 0 ] = 0;
  crate->shared_queue_family_indices[ 1 ] = 0;
}
