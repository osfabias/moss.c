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

  @file src/internal/vk_command_pool_utils.h
  @brief Vulkan command pool utility functions.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "moss/result.h"
#include "src/internal/log.h"

inline static MossResult moss__create_command_pool (
  VkDevice             device,
  const uint32_t       queue_family_index,
  VkCommandPool *const out_command_pool
)
{
  const VkCommandPoolCreateInfo pool_info = {
    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = queue_family_index,
  };

  const VkResult result =
    vkCreateCommandPool (device, &pool_info, NULL, out_command_pool);

  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create command pool. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}
