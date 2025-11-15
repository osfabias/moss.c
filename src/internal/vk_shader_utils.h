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

  @file src/internal/vk_shader_utils.h
  @brief Vulkan shader utility functions
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

#include <vulkan/vulkan.h>

#include "src/internal/log.h"

/*
  @brief Creates a shader module from SPIR-V code.
  @param device Logical device.
  @param code Pointer to SPIR-V code.
  @param code_size Size of SPIR-V code in bytes.
  @param out_shader_module Pointer to store created shader module.
  @return VK_SUCCESS on success, error code otherwise.
*/
inline static VkResult moss__create_shader_module (
  VkDevice device, const uint32_t *code, size_t code_size, VkShaderModule *out_shader_module
)
{
  const VkShaderModuleCreateInfo create_info = {
    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = code_size,
    .pCode    = code,
  };

  const VkResult result = vkCreateShaderModule (device, &create_info, NULL, out_shader_module);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create shader module. Error code: %d.\n", result);
  }

  return result;
}

