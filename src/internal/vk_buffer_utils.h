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

  @file src/internal/vk_buffer_utils.h
  @brief Vulkan buffers utility functions.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "moss/result.h"
#include "src/internal/log.h"
#include "vulkan/vulkan_core.h"

/*
  @brief Info required to perform copy operation on two buffers.
*/
typedef struct
{
  VkDevice      device;             /* Logical device. */
  VkBuffer      destination_buffer; /* Destination buffer to copy data to. */
  VkBuffer      source_buffer;      /* Source buffer to copy data from. */
  VkDeviceSize  size;               /* Amount of bytes to copy. */
  VkCommandPool command_pool;       /* Command pool to perfom operation with. */
  VkQueue       transfer_queue;     /* Queue to be used as a transfer queue. */
} Moss__CopyVkBufferInfo;

/*
  @brief Copies data from one Vulkan buffer to another.
  @param info Required info for operation execution.
  @return Return MOSS_RESULT_SUCCESS on success, otherwise MOSS_RESULT_ERROR.
*/
inline static MossResult moss__copy_vk_buffer (const Moss__CopyVkBufferInfo *info)
{
  // Allocate command buffer
  VkCommandBuffer command_buffer;
  {
    const VkCommandBufferAllocateInfo alloc_info = {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandPool        = info->command_pool,
      .commandBufferCount = 1,
    };
    const VkResult result =
      vkAllocateCommandBuffers (info->device, &alloc_info, &command_buffer);
    if (result != VK_SUCCESS)
    {
      moss__error ("Failed to allocate command buffer: %d.\n", result);
      return MOSS_RESULT_ERROR;
    }
  }

  {  // Begin command buffer
    const VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    const VkResult result = vkBeginCommandBuffer (command_buffer, &begin_info);
    if (result != VK_SUCCESS)
    {
      vkFreeCommandBuffers (info->device, info->command_pool, 1, &command_buffer);
      moss__error ("Failed to begin command buffer: %d.\n", result);
      return MOSS_RESULT_ERROR;
    }
  }

  // Add copy command
  const VkBufferCopy copy_region = {
    .size      = info->size,
    .srcOffset = 0,
    .dstOffset = 0,
  };
  vkCmdCopyBuffer (
    command_buffer,
    info->source_buffer,
    info->destination_buffer,
    1,
    &copy_region
  );

  vkEndCommandBuffer (command_buffer);

  {  // Submit command buffer
    const VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                       .commandBufferCount = 1,
                                       .pCommandBuffers    = &command_buffer };

    const VkResult result =
      vkQueueSubmit (info->transfer_queue, 1, &submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS)
    {
      vkFreeCommandBuffers (info->device, info->command_pool, 1, &command_buffer);
      moss__error ("Failed to submit queue: %d.\n", result);
      return MOSS_RESULT_ERROR;
    }
  }

  {  // Wait until queue is idle
    const VkResult result = vkQueueWaitIdle (info->transfer_queue);
    if (result != VK_SUCCESS)
    {
      vkFreeCommandBuffers (info->device, info->command_pool, 1, &command_buffer);
      moss__error ("Failed to try wait for queue idle: %d.\n", result);
      return MOSS_RESULT_ERROR;
    }
  }

  // Clean up
  vkFreeCommandBuffers (info->device, info->command_pool, 1, &command_buffer);

  return MOSS_RESULT_SUCCESS;
}
