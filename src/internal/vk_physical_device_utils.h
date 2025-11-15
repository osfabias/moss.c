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

  @file src/internal/vk_physical_device_utils.h
  @brief Vulkan physical device selection utility functions
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include <stuffy/vulkan.h>

#include "src/internal/log.h"

/*
  @brief Queue family indices.
  @details Stores the indices of queue families that are required for rendering.
*/
typedef struct
{
  uint32_t graphics_family;           /* Graphics queue family index. */
  uint32_t present_family;            /* Present queue family index. */
  bool     graphics_family_has_value; /* Whether graphics family index is valid. */
  bool     present_family_has_value;  /* Whether present family index is valid. */
} Moss__QueueFamilyIndices;

/*
  @brief Checks if device supports required extensions.
  @param device Physical device to check.
  @return True if all required extensions are supported.
*/
inline static bool moss__check_device_extension_support (VkPhysicalDevice device)
{
  const VkDeviceExtensions required_extensions = get_required_vk_device_extensions ( );

  uint32_t available_extension_count;
  vkEnumerateDeviceExtensionProperties (device, NULL, &available_extension_count, NULL);

  VkExtensionProperties available_extensions[ available_extension_count ];
  vkEnumerateDeviceExtensionProperties (
    device,
    NULL,
    &available_extension_count,
    available_extensions
  );

  for (uint32_t i = 0; i < required_extensions.count; ++i)
  {
    bool extension_found = false;
    for (uint32_t j = 0; j < available_extension_count; ++j)
    {
      if (strcmp (
            required_extensions.names[ i ],
            available_extensions[ j ].extensionName
          ) == 0)
      {
        extension_found = true;
        break;
      }
    }

    if (!extension_found) { return false; }
  }

  return true;
}

/*
  @brief Finds queue families for a physical device.
  @param device Physical device to query.
  @param surface Surface to check presentation support.
  @return Queue family indices structure.
*/
inline static Moss__QueueFamilyIndices
moss__find_queue_families (VkPhysicalDevice device, VkSurfaceKHR surface)
{
  Moss__QueueFamilyIndices indices = {
    .graphics_family_has_value = false,
    .present_family_has_value  = false,
  };

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties (device, &queue_family_count, NULL);

  VkQueueFamilyProperties queue_families[ queue_family_count ];
  vkGetPhysicalDeviceQueueFamilyProperties (device, &queue_family_count, queue_families);

  for (uint32_t i = 0; i < queue_family_count; ++i)
  {
    if (queue_families[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      indices.graphics_family           = i;
      indices.graphics_family_has_value = true;
    }

    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR (device, i, surface, &present_support);

    if (present_support)
    {
      indices.present_family           = i;
      indices.present_family_has_value = true;
    }

    if (indices.graphics_family_has_value && indices.present_family_has_value) { break; }
  }

  return indices;
}

/*
  @brief Checks if a physical device is suitable for our needs.
  @param device Physical device to check.
  @param surface Surface to check presentation support.
  @return True if device is suitable, false otherwise.
*/
inline static bool moss__is_physical_device_suitable (
  const VkPhysicalDevice device,
  const VkSurfaceKHR     surface
)
{
  const Moss__QueueFamilyIndices indices = moss__find_queue_families (device, surface);

  const bool extensions_supported = moss__check_device_extension_support (device);

  bool swap_chain_adequate = false;
  if (extensions_supported)
  {
    uint32_t format_count;
    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR (device, surface, &format_count, NULL);
    vkGetPhysicalDeviceSurfacePresentModesKHR (
      device,
      surface,
      &present_mode_count,
      NULL
    );

    swap_chain_adequate = (format_count > 0) && (present_mode_count > 0);
  }

  return indices.graphics_family_has_value && indices.present_family_has_value &&
         extensions_supported && swap_chain_adequate;
}

/*
  @brief Selects a suitable physical device from available devices.
  @param instance Vulkan instance.
  @param surface Surface to check presentation support.
  @param out_device Pointer to store selected physical device.
  @return VK_SUCCESS on success, error code otherwise.
*/
inline static VkResult moss__select_physical_device (
  VkInstance        instance,
  VkSurfaceKHR      surface,
  VkPhysicalDevice *out_device
)
{
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices (instance, &device_count, NULL);

  if (device_count == 0)
  {
    moss__error ("Failed to find GPUs with Vulkan support.\n");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  VkPhysicalDevice devices[ device_count ];
  vkEnumeratePhysicalDevices (instance, &device_count, devices);

  for (uint32_t i = 0; i < device_count; ++i)
  {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties (devices[ i ], &device_properties);

    const Moss__QueueFamilyIndices indices =
      moss__find_queue_families (devices[ i ], surface);
    const bool extensions_supported = moss__check_device_extension_support (devices[ i ]);

    bool swap_chain_adequate = false;
    if (extensions_supported)
    {
      uint32_t format_count;
      uint32_t present_mode_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR (devices[ i ], surface, &format_count, NULL);
      vkGetPhysicalDeviceSurfacePresentModesKHR (
        devices[ i ],
        surface,
        &present_mode_count,
        NULL
      );

      swap_chain_adequate = (format_count > 0) && (present_mode_count > 0);
    }

    if (moss__is_physical_device_suitable (devices[ i ], surface))
    {
      *out_device = devices[ i ];
      return VK_SUCCESS;
    }

    moss__error (
      "Device %u (%s) is not suitable:\n"
      "  Graphics queue: %s\n"
      "  Present queue: %s\n"
      "  Extensions supported: %s\n"
      "  Swap chain adequate: %s\n",
      i,
      device_properties.deviceName,
      indices.graphics_family_has_value ? "yes" : "no",
      indices.present_family_has_value ? "yes" : "no",
      extensions_supported ? "yes" : "no",
      swap_chain_adequate ? "yes" : "no"
    );
  }

  moss__error ("Failed to find a suitable GPU.\n");
  return VK_ERROR_INITIALIZATION_FAILED;
}
