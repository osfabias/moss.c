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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include <stuffy/vulkan.h>

#include "src/internal/log.h"
#include "vulkan/vulkan_core.h"

/*
  @brief Queue family indices.
  @details Stores the indices of queue families that are required for rendering.
*/
typedef struct
{
  uint32_t graphics_family;       /* Graphics queue family index. */
  uint32_t present_family;        /* Present queue family index. */
  uint32_t transfer_family;       /* Transfer queue family index. */
  bool     graphics_family_found; /* Whether graphics family index is valid. */
  bool     present_family_found;  /* Whether present family index is valid. */
  bool     transfer_family_found; /* Whether present family index is valid. */
} Moss__QueueFamilyIndices;

/*
  @brief Vulkan physical device extensions.
*/
typedef struct
{
  const char *const *names; /* Extension names. */
  const uint32_t     count; /* Extension count. */
} Moss__VkPhysicalDeviceExtensions;

inline static Moss__VkPhysicalDeviceExtensions
moss__get_required_vk_device_extensions (void)
{
#ifdef __APPLE__
  static const char *const extension_names[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_portability_subset",
  };
#else
#  error \
    "Vulkan physical device extensions aren't specified for the current target platform."
#endif

  static const Moss__VkPhysicalDeviceExtensions extensions = {
    .names = extension_names,
    .count = sizeof (extension_names) / sizeof (extension_names[ 0 ]),
  };

  return extensions;
}

/*
  @brief Finds queue families for a physical device.
  @param device Physical device to query.
  @param surface Surface to check presentation support.
  @return Queue family indices structure.
*/
inline static Moss__QueueFamilyIndices
moss__find_queue_families (const VkPhysicalDevice device, const VkSurfaceKHR surface)
{
  Moss__QueueFamilyIndices indices = {
    .graphics_family_found = false,
    .present_family_found  = false,
    .transfer_family_found = false,
  };

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties (device, &queue_family_count, NULL);

  VkQueueFamilyProperties queue_families[ queue_family_count ];
  vkGetPhysicalDeviceQueueFamilyProperties (device, &queue_family_count, queue_families);

  for (uint32_t i = 0; i < queue_family_count; ++i)
  {
    VkQueueFlags queue_family_flags = queue_families[ i ].queueFlags;

    if ((queue_family_flags & VK_QUEUE_TRANSFER_BIT) &&
        !(queue_family_flags & VK_QUEUE_GRAPHICS_BIT))
    {
      indices.transfer_family       = i;
      indices.transfer_family_found = true;
    }

    if (queue_family_flags & VK_QUEUE_GRAPHICS_BIT)
    {
      indices.graphics_family       = i;
      indices.graphics_family_found = true;
    }

    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR (device, i, surface, &present_support);

    if (present_support)
    {
      indices.present_family       = i;
      indices.present_family_found = true;
    }

    if (indices.transfer_family_found && indices.graphics_family_found &&
        indices.present_family_found)
    {
      break;
    }
  }

  // If transfer queue family not found, set it to be the same as the graphics family
  if (!indices.transfer_family_found)
  {
    indices.transfer_family       = indices.graphics_family;
    indices.transfer_family_found = true;
  }

  return indices;
}


/*
  @brief Checks if device supports required queues.
  @param device Physical device to check.
  @return True if all required queues are supported, otherwise false.
*/
inline static bool moss__check_device_queues_support (
  const VkPhysicalDevice device,
  const VkSurfaceKHR     surface
)
{
#ifndef NDEBUG
  VkPhysicalDeviceProperties device_properties;
  vkGetPhysicalDeviceProperties (device, &device_properties);
#endif

  const Moss__QueueFamilyIndices indices = moss__find_queue_families (device, surface);

  if (!indices.present_family_found)
  {
    moss__info (
      "%s device do not support required present queue family.\n",
      device_properties.deviceName
    );
    return false;
  }

  if (!indices.graphics_family_found)
  {
    moss__info (
      "%s device do not support required graphics queue family.\n",
      device_properties.deviceName
    );
    return false;
  }

  return true;
}

/*
  @brief Checks if device supports required extensions.
  @param device Physical device to check.
  @return True if all required extensions are supported, otherwise false.
*/
inline static bool moss__check_device_extension_support (const VkPhysicalDevice device)
{
#ifndef NDEBUG
  VkPhysicalDeviceProperties device_properties;
  vkGetPhysicalDeviceProperties (device, &device_properties);
#endif

  const Moss__VkPhysicalDeviceExtensions required_extensions =
    moss__get_required_vk_device_extensions ( );

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

    if (!extension_found)
    {
      moss__info (
        "%s device doesn't support required \"%s\" extension.\n",
        device_properties.deviceName,
        required_extensions.names[ i ]
      );
      return false;
    }
  }

  return true;
}


/*
  @brief Checks if device supports required formats.
  @param device Physical device to check.
  @return True if all required formats are supported, otherwise false.
*/
inline static bool moss__check_device_format_support (
  const VkPhysicalDevice device,
  const VkSurfaceKHR     surface
)
{
  uint32_t format_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR (device, surface, &format_count, NULL);

  uint32_t present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR (device, surface, &present_mode_count, NULL);

  return (format_count > 0) && (present_mode_count > 0);
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
  if (!moss__check_device_queues_support (device, surface)) { return false; }
  if (!moss__check_device_extension_support (device)) { return false; }
  if (!moss__check_device_format_support (device, surface)) { return false; }

  return true;
}

/*
  @brief Selects a suitable physical device from available devices.
  @param instance Vulkan instance.
  @param surface Surface to check presentation support.
  @param out_device Pointer to store selected physical device.
  @return VK_SUCCESS on success, error code otherwise.
*/
inline static VkResult moss__select_physical_device (
  const VkInstance        instance,
  const VkSurfaceKHR      surface,
  VkPhysicalDevice *const out_device
)
{
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices (instance, &device_count, NULL);

  if (device_count == 0)
  {
    moss__error ("Failed to find GPUs with Vulkan support.\n");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  assert (device_count < 16);

  VkPhysicalDevice devices[ device_count ];
  vkEnumeratePhysicalDevices (instance, &device_count, devices);

  for (uint32_t i = 0; i < device_count; ++i)
  {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties (devices[ i ], &device_properties);

    if (moss__is_physical_device_suitable (devices[ i ], surface))
    {
      *out_device = devices[ i ];

      moss__info ("Selected %s as the target GPU.\n", device_properties.deviceName);
      return VK_SUCCESS;
    }
  }

  moss__error ("Failed to find a suitable GPU.\n");
  return VK_ERROR_INITIALIZATION_FAILED;
}
