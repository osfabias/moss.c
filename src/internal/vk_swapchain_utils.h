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

  @file src/internal/vk_swapchain_utils.h
  @brief Vulkan swap chain utility functions
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdint.h>
#include <stdlib.h>

#include <vulkan/vulkan.h>

#include "src/internal/log.h"
#include "vulkan/vulkan_core.h"

/* Max number of available Vulkan formats. */
#define MAX_VULKAN_FORMAT_COUNT (uint32_t)(265)

/* Max number of available Vulkan present modes . */
#define MAX_VULKAN_PRESENT_MODE_COUNT (uint32_t)(265)

/*
  @brief Swap chain support details.
  @details Contains information about swap chain capabilities.
*/
typedef struct
{
  /* Surface capabilities. */
  VkSurfaceCapabilitiesKHR capabilities;

  /* Available surface formats. */
  VkSurfaceFormatKHR formats[ MAX_VULKAN_FORMAT_COUNT ];

  /* Number of formats. */
  uint32_t format_count;

  /* Available present modes. */
  VkPresentModeKHR present_modes[ MAX_VULKAN_PRESENT_MODE_COUNT ];

  /* Number of present modes. */
  uint32_t present_mode_count;
} Moss__SwapChainSupportDetails;

/*
  @brief Query swap chain support details for a physical device.
  @param device Physical device.
  @param surface Surface to query.
  @return Swap chain support details. Caller must free formats and present_modes arrays.
*/
inline static Moss__SwapChainSupportDetails
moss__query_swapchain_support (VkPhysicalDevice device, VkSurfaceKHR surface)
{
  Moss__SwapChainSupportDetails details = {0};

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR (device, surface, &details.capabilities);

  vkGetPhysicalDeviceSurfaceFormatsKHR (device, surface, &details.format_count, NULL);

  if (details.format_count <= MAX_VULKAN_FORMAT_COUNT)
  {
    vkGetPhysicalDeviceSurfaceFormatsKHR (
      device,
      surface,
      &details.format_count,
      details.formats
    );
  }
  else {
    moss__error (
      "Format count exceeded the limit (%d > %d). No formats saved.",
      details.format_count,
      MAX_VULKAN_FORMAT_COUNT
    );
  }

  vkGetPhysicalDeviceSurfacePresentModesKHR (
    device,
    surface,
    &details.present_mode_count,
    NULL
  );

  if (details.present_mode_count <= MAX_VULKAN_PRESENT_MODE_COUNT)
  {
    vkGetPhysicalDeviceSurfacePresentModesKHR (
      device,
      surface,
      &details.present_mode_count,
      details.present_modes
    );
  }
  else {
    moss__error (
      "Present mode count exceeded the limit (%d > %d). No formats saved.",
      details.present_mode_count,
      MAX_VULKAN_PRESENT_MODE_COUNT
    );
  }

  return details;
}

/*
  @brief Choose swap surface format.
  @param available_formats Available formats array.
  @param format_count Number of available formats.
  @return Selected surface format.
*/
inline static VkSurfaceFormatKHR moss__choose_swap_surface_format (
  const VkSurfaceFormatKHR *available_formats,
  uint32_t                  format_count
)
{
  for (uint32_t i = 0; i < format_count; ++i)
  {
    if (available_formats[ i ].format == VK_FORMAT_B8G8R8A8_SRGB &&
        available_formats[ i ].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      return available_formats[ i ];
    }
  }

  return available_formats[ 0 ];
}

/*
  @brief Choose swap present mode.
  @param available_present_modes Available present modes array.
  @param present_mode_count Number of available present modes.
  @return Selected present mode.
*/
inline static VkPresentModeKHR moss__choose_swap_present_mode (
  const VkPresentModeKHR *available_present_modes,
  uint32_t                present_mode_count
)
{
  for (uint32_t i = 0; i < present_mode_count; ++i)
  {
    if (available_present_modes[ i ] == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      return available_present_modes[ i ];
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

/*
  @brief Choose swap extent.
  @param capabilities Surface capabilities.
  @param width Desired width.
  @param height Desired height.
  @return Selected extent.
*/
inline static VkExtent2D moss__choose_swap_extent (
  const VkSurfaceCapabilitiesKHR *capabilities,
  uint32_t                        width,
  uint32_t                        height
)
{
  if (capabilities->currentExtent.width != UINT32_MAX)
  {
    return capabilities->currentExtent;
  }

  VkExtent2D actual_extent = {width, height};

  if (actual_extent.width < capabilities->minImageExtent.width)
  {
    actual_extent.width = capabilities->minImageExtent.width;
  }
  if (actual_extent.width > capabilities->maxImageExtent.width)
  {
    actual_extent.width = capabilities->maxImageExtent.width;
  }

  if (actual_extent.height < capabilities->minImageExtent.height)
  {
    actual_extent.height = capabilities->minImageExtent.height;
  }
  if (actual_extent.height > capabilities->maxImageExtent.height)
  {
    actual_extent.height = capabilities->maxImageExtent.height;
  }

  return actual_extent;
}
