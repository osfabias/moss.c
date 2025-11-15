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

  @file src/internal/vk_instance_utils.c
  @brief Vulkan instance utility functions.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdint.h>

#include <vulkan/vulkan.h>

/*
  @brief Vulkan instance extensions.
*/
typedef struct
{
  const char *const *names; /* Extension names. */
  const size_t       count; /* Extension count. */
} Moss__VkInstanceExtensions;

/*
  @brief Returns a list of required Vulkan instance extensions.
  @return Instance extensions struct.
*/
inline static Moss__VkInstanceExtensions moss__get_required_vk_instance_extensions (void)
{
#ifdef __APPLE__
  static const char *const extension_names[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    "VK_EXT_metal_surface",
  };
#else
#  error "Vulkan instance extensions are not specified for the current target platform."
#endif

  static const Moss__VkInstanceExtensions extensions = {
    .names = extension_names,
    .count = sizeof (extension_names) / sizeof (extension_names[ 0 ]),
  };

  return extensions;
}

/*
  @brief Returns required Vulkan instance flags.
  @return Vulkan instance flags value.
*/
inline static uint32_t moss__get_required_vk_instance_flags (void)
{
#ifdef __APPLE__
  return VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#else
  return 0;
#endif
}
