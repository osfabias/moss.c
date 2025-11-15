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

  @file src/internal/memory_utils.h
  @brief Utility functions for GPU memory management.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include "moss/result.h"
#include <stdint.h>

#include <vulkan/vulkan.h>

/*
  @brief Searches for the suitable memory type that satisfies passed filter and
  properties.
  @param device Physical device to search memory type on.
  @param type_filter Memory type filter.
  @param properties Required memory properties.
  @param out_memory_type Ouput variables that found memory type will be written to.
  @return Return MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__select_suitable_memory_type (
  VkPhysicalDevice            device,
  const uint32_t              type_filter,
  const VkMemoryPropertyFlags properties,
  uint32_t                   *out_memory_type
)
{
  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties (device, &memory_properties);

  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
  {
    const VkMemoryType memory_type = memory_properties.memoryTypes[ i ];

    if (type_filter & (1 << i) && (memory_type.propertyFlags & properties) == properties)
    {
      *out_memory_type = i;
      return MOSS_RESULT_SUCCESS;
    }
  }

  return MOSS_RESULT_ERROR;
}
