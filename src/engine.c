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

  @file src/engine.c
  @brief Graphics engine functions implementation.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include <stuffy/app.h>
#include <stuffy/vulkan.h>
#include <stuffy/window.h>

#include "moss/app_info.h"
#include "moss/engine.h"
#include "moss/result.h"
#include "moss/window_config.h"

#include "src/internal/app_info.h"
#include "src/internal/log.h"
#include "src/internal/shaders.h"
#include "src/internal/vk_instance_utils.h"
#include "src/internal/vk_physical_device_utils.h"
#include "src/internal/vk_shader_utils.h"
#include "src/internal/vk_swapchain_utils.h"
#include "src/internal/vk_validation_layers_utils.h"
#include "vulkan/vulkan_core.h"

/*=============================================================================
    ENGINE STATE
  =============================================================================*/

#define MAX_FRAMES_IN_FLIGHT 2

/*
  @brief Engine state.
*/
typedef struct
{
  /* Window. */
  StuffyWindow *window; /* Window handle. */

  /* Vulkan instance and surface. */
  VkInstance   api_instance; /* Vulkan instance. */
  VkSurfaceKHR surface;      /* Window surface. */

  /* Physical and logical device. */
  VkPhysicalDevice         physical_device;      /* Physical device. */
  VkDevice                 device;               /* Logical device. */
  Moss__QueueFamilyIndices queue_family_indices; /* Queue family indices. */
  VkQueue                  graphics_queue;       /* Graphics queue. */
  VkQueue                  present_queue;        /* Present queue. */

  /* Swap chain. */
  VkSwapchainKHR swapchain;              /* Swap chain. */
  VkImage       *swapchain_images;       /* Swap chain images. */
  uint32_t       swapchain_image_count;  /* Number of swap chain images. */
  VkFormat       swapchain_image_format; /* Swap chain image format. */
  VkExtent2D     swapchain_extent;       /* Swap chain extent. */
  VkImageView   *swapchain_image_views;  /* Swap chain image views. */
  VkFramebuffer *swapchain_framebuffers; /* Swap chain framebuffers. */
  bool           framebuffer_resized;

  /* Render pipeline. */
  VkRenderPass     render_pass;       /* Render pass. */
  VkPipelineLayout pipeline_layout;   /* Pipeline layout. */
  VkPipeline       graphics_pipeline; /* Graphics pipeline. */

  /* Command buffers. */
  VkCommandPool   command_pool;                            /* Command pool. */
  VkCommandBuffer command_buffers[ MAX_FRAMES_IN_FLIGHT ]; /* Command buffers. */

  /* Synchronization objects. */
  VkSemaphore
    image_available_semaphores[ MAX_FRAMES_IN_FLIGHT ]; /* Image available semaphores. */
  VkSemaphore
    render_finished_semaphores[ MAX_FRAMES_IN_FLIGHT ]; /* Render finished semaphores. */
  VkFence in_flight_fences[ MAX_FRAMES_IN_FLIGHT ];     /* In-flight fences. */

  /* Frame state. */
  uint32_t current_frame; /* Current frame index. */
} Moss__Engine;

/*
  @brief Global engine state.
*/
static Moss__Engine g_engine = {
  /* Window. */
  .window = NULL,

  /* Vulkan instance and surface. */
  .api_instance = VK_NULL_HANDLE,
  .surface      = VK_NULL_HANDLE,

  /* Physical and logical device. */
  .physical_device                                = VK_NULL_HANDLE,
  .device                                         = VK_NULL_HANDLE,
  .queue_family_indices.graphics_family           = 0,
  .queue_family_indices.present_family            = 0,
  .queue_family_indices.graphics_family_has_value = false,
  .queue_family_indices.present_family_has_value  = false,
  .graphics_queue                                 = VK_NULL_HANDLE,
  .present_queue                                  = VK_NULL_HANDLE,

  /* Swap chain. */
  .swapchain              = VK_NULL_HANDLE,
  .swapchain_images       = NULL,
  .swapchain_image_count  = 0,
  .swapchain_image_format = 0,
  .swapchain_extent       = (VkExtent2D) {.width = 0,     .height = 0   },
  .swapchain_image_views  = NULL,
  .swapchain_framebuffers = NULL,

  /* Render pipeline. */
  .render_pass       = VK_NULL_HANDLE,
  .pipeline_layout   = VK_NULL_HANDLE,
  .graphics_pipeline = VK_NULL_HANDLE,

  /* Command buffers. */
  .command_pool    = VK_NULL_HANDLE,
  .command_buffers = {VK_NULL_HANDLE, VK_NULL_HANDLE},

  /* Synchronization objects. */
  .image_available_semaphores = {VK_NULL_HANDLE, VK_NULL_HANDLE},
  .render_finished_semaphores = {VK_NULL_HANDLE, VK_NULL_HANDLE},
  .in_flight_fences           = {VK_NULL_HANDLE, VK_NULL_HANDLE},

  /* Frame state. */
  .current_frame = 0,
};

/*=============================================================================
    INTERNAL FUNCTION DECLARATIONS
  =============================================================================*/

/*
  @brief Creates Vulkan API instance.
  @param app_info A pointer to a native moss app info struct.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_api_instance (const MossAppInfo *app_info);

/*
  @brief Initializes stuffy app.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__init_stuffy_app (void);

/*
  @brief Deinitializes stuffy app.
*/
static void moss__deinit_stuffy_app (void);

/*
  @brief Creates window.
  @param window_config Window configuration.
  @param app_name Application name for window title.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult
moss__create_window (const MossWindowConfig *window_config, const char *app_name);

/*
  @brief Creates window surface.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_surface (void);

/*
  @brief Creates logical device and queues.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_logical_device (void);

/*
  @brief Creates swap chain.
  @param width Window width.
  @param height Window height.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_swapchain (uint32_t width, uint32_t height);

/*
  @brief Creates image views for swap chain images.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_image_views (void);

/*
  @brief Creates render pass.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_render_pass (void);

/*
  @brief Creates graphics pipeline.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_graphics_pipeline (void);

/*
  @brief Creates framebuffers.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_framebuffers (void);

/*
  @brief Creates command pool.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_command_pool (void);

/*
  @brief Creates command buffers.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_command_buffers (void);

/*
  @brief Creates image available semaphores.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_image_available_semaphores (void);

/*
  @brief Creates render finished semaphores.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_render_finished_semaphores (void);

/*
  @brief Creates in-flight fences.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_in_flight_fences (void);

/*
  @brief Creates synchronization objects (semaphores and fences).
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_synchronization_objects (void);

/*
  @brief Cleans up semaphores array.
  @param semaphores Array of semaphores to clean up.
*/
static void moss__cleanup_semaphores (VkSemaphore *semaphores);

/*
  @brief Cleans up fences array.
  @param fences Array of fences to clean up.
*/
static void moss__cleanup_fences (VkFence *fences);

/*
  @brief Cleans up image available semaphores.
*/
static void moss__cleanup_image_available_semaphores (void);

/*
  @brief Cleans up render finished semaphores.
*/
static void moss__cleanup_render_finished_semaphores (void);

/*
  @brief Cleans up in-flight fences.
*/
static void moss__cleanup_in_flight_fences (void);

/*
  @brief Cleans up synchronization objects.
*/
static void moss__cleanup_synchronization_objects (void);

/*
  @brief Records command buffer.
  @param command_buffer Command buffer to record.
  @param image_index Swap chain image index.
*/
static void
moss__record_command_buffer (VkCommandBuffer command_buffer, uint32_t image_index);

/*
  @brief Cleans up swap chain resources.
*/
static void moss__cleanup_swapchain (void);

/*
  @brief Recreates swap chain.
  @param width Window width.
  @param height Window height.
  @return Returns MOSS_RESULT_SUCCESS on success, error code otherwise.
*/
static MossResult moss__recreate_swapchain (uint32_t width, uint32_t height);

/*=============================================================================
    PUBLIC API FUNCTIONS IMPLEMENTATION
  =============================================================================*/

/*
  @brief Initializes engine instance.
  @param config Engine configuration.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
MossResult moss_engine_init (const MossEngineConfig *const config)
{
  if (moss__init_stuffy_app ( ) != MOSS_RESULT_SUCCESS) { return MOSS_RESULT_ERROR; }

  if (moss__create_window (config->window_config, config->app_info->app_name) !=
      MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_api_instance (config->app_info) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_surface ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__select_physical_device (
        g_engine.api_instance,
        g_engine.surface,
        &g_engine.physical_device
      ) != VK_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  g_engine.queue_family_indices =
    moss__find_queue_families (g_engine.physical_device, g_engine.surface);

  if (moss__create_logical_device ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  vkGetDeviceQueue (
    g_engine.device,
    g_engine.queue_family_indices.graphics_family,
    0,
    &g_engine.graphics_queue
  );
  vkGetDeviceQueue (
    g_engine.device,
    g_engine.queue_family_indices.present_family,
    0,
    &g_engine.present_queue
  );

  if (moss__create_swapchain (
        config->window_config->width,
        config->window_config->height
      ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_image_views ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_render_pass ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_graphics_pipeline ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_framebuffers ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_command_pool ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_command_buffers ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_synchronization_objects ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  g_engine.current_frame = 0;

  return MOSS_RESULT_SUCCESS;
}

/*
  @brief Destroys engine instance.
  @details Cleans up all reserved memory and destroys all GraphicsAPI objects.
*/
void moss_engine_deinit (void)
{
  if (g_engine.device != VK_NULL_HANDLE) { vkDeviceWaitIdle (g_engine.device); }

  moss__cleanup_swapchain ( );
  moss__cleanup_synchronization_objects ( );

  if (g_engine.device != VK_NULL_HANDLE)
  {
    if (g_engine.command_pool != VK_NULL_HANDLE)
    {
      vkDestroyCommandPool (g_engine.device, g_engine.command_pool, NULL);
      g_engine.command_pool = VK_NULL_HANDLE;
    }

    if (g_engine.graphics_pipeline != VK_NULL_HANDLE)
    {
      vkDestroyPipeline (g_engine.device, g_engine.graphics_pipeline, NULL);
      g_engine.graphics_pipeline = VK_NULL_HANDLE;
    }

    if (g_engine.pipeline_layout != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout (g_engine.device, g_engine.pipeline_layout, NULL);
      g_engine.pipeline_layout = VK_NULL_HANDLE;
    }

    if (g_engine.render_pass != VK_NULL_HANDLE)
    {
      vkDestroyRenderPass (g_engine.device, g_engine.render_pass, NULL);
      g_engine.render_pass = VK_NULL_HANDLE;
    }

    vkDestroyDevice (g_engine.device, NULL);
    g_engine.device                                         = VK_NULL_HANDLE;
    g_engine.physical_device                                = VK_NULL_HANDLE;
    g_engine.graphics_queue                                 = VK_NULL_HANDLE;
    g_engine.present_queue                                  = VK_NULL_HANDLE;
    g_engine.queue_family_indices.graphics_family           = 0;
    g_engine.queue_family_indices.present_family            = 0;
    g_engine.queue_family_indices.graphics_family_has_value = false;
    g_engine.queue_family_indices.present_family_has_value  = false;
  }

  if (g_engine.surface != VK_NULL_HANDLE)
  {
    vkDestroySurfaceKHR (g_engine.api_instance, g_engine.surface, NULL);
    g_engine.surface = VK_NULL_HANDLE;
  }

  if (g_engine.api_instance != VK_NULL_HANDLE)
  {
    vkDestroyInstance (g_engine.api_instance, NULL);
    g_engine.api_instance = VK_NULL_HANDLE;
  }

  if (g_engine.window != NULL)
  {
    stuffy_window_close (g_engine.window);
    g_engine.window = NULL;
  }

  moss__deinit_stuffy_app ( );

  g_engine.current_frame = 0;
}

/*
  @brief Checks if the window should close.
  @return Returns true if window should close, false otherwise.
*/
bool moss_engine_should_close (void)
{
  if (g_engine.window == NULL) { return true; }
  stuffy_app_update ( );
  return stuffy_window_should_close (g_engine.window);
}

/*
  @brief Draws a frame.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
MossResult moss_engine_draw_frame (void)
{
  const VkFence     in_flight_fence = g_engine.in_flight_fences[ g_engine.current_frame ];
  const VkSemaphore image_available_semaphore =
    g_engine.image_available_semaphores[ g_engine.current_frame ];
  const VkSemaphore render_finished_semaphore =
    g_engine.render_finished_semaphores[ g_engine.current_frame ];
  const VkCommandBuffer command_buffer =
    g_engine.command_buffers[ g_engine.current_frame ];

  vkWaitForFences (g_engine.device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
  vkResetFences (g_engine.device, 1, &in_flight_fence);

  uint32_t current_image_index;
  VkResult result = vkAcquireNextImageKHR (
    g_engine.device,
    g_engine.swapchain,
    UINT64_MAX,
    image_available_semaphore,
    VK_NULL_HANDLE,
    &current_image_index
  );

  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // Swap chain is out of date, need to recreate before we can acquire image
    const StuffyWindowRect window_rect = stuffy_window_get_rect (g_engine.window);
    return moss__recreate_swapchain (window_rect.width, window_rect.height);
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
  {
    moss__error ("Failed to acquire swap chain image.\n");
    return MOSS_RESULT_ERROR;
  }

  vkResetCommandBuffer (command_buffer, 0);
  moss__record_command_buffer (command_buffer, current_image_index);

  const VkSemaphore wait_semaphores[] = {image_available_semaphore};
  const size_t      wait_semaphore_count =
    sizeof (wait_semaphores) / sizeof (wait_semaphores[ 0 ]);

  const VkSemaphore signal_semaphores[] = {render_finished_semaphore};
  const size_t      signal_semaphore_count =
    sizeof (signal_semaphores) / sizeof (signal_semaphores[ 0 ]);

  const VkSubmitInfo submit_info = {
    .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = wait_semaphore_count,
    .pWaitSemaphores    = wait_semaphores,
    .pWaitDstStageMask =
      (const VkPipelineStageFlags[]) {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
    .commandBufferCount   = 1,
    .pCommandBuffers      = &command_buffer,
    .signalSemaphoreCount = signal_semaphore_count,
    .pSignalSemaphores    = signal_semaphores,
  };

  if (vkQueueSubmit (g_engine.graphics_queue, 1, &submit_info, in_flight_fence) !=
      VK_SUCCESS)
  {
    moss__error ("Failed to submit draw command buffer.\n");
    return MOSS_RESULT_ERROR;
  }

  const VkPresentInfoKHR present_info = {
    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = signal_semaphore_count,
    .pWaitSemaphores    = signal_semaphores,
    .swapchainCount     = 1,
    .pSwapchains        = &g_engine.swapchain,
    .pImageIndices      = &g_engine.current_frame,
  };

  result = vkQueuePresentKHR (g_engine.present_queue, &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
  {
    // Swap chain is out of date or suboptimal, need to recreate
    const StuffyWindowRect window_rect = stuffy_window_get_rect (g_engine.window);
    if (moss__recreate_swapchain (window_rect.width, window_rect.height) !=
        MOSS_RESULT_SUCCESS)
    {
      return MOSS_RESULT_ERROR;
    }
  }
  else if (result != VK_SUCCESS)
  {
    moss__error ("Failed to present swap chain image.\n");
    return MOSS_RESULT_ERROR;
  }

  g_engine.current_frame = (g_engine.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

  return MOSS_RESULT_SUCCESS;
}

/*=============================================================================
    INTERNAL FUNCTIONS IMPLEMENTATION
  =============================================================================*/

static MossResult moss__create_api_instance (const MossAppInfo *const app_info)
{
  // Set up validation layers
#ifdef NDEBUG
  const bool enable_validation_layers = false;
#else
  const bool enable_validation_layers = true;
#endif

  uint32_t           validation_layer_count = 0;
  const char *const *validation_layer_names = NULL;

  if (enable_validation_layers)
  {
    if (moss__check_vk_validation_layer_support ( ))
    {
      const Moss__VkValidationLayers validation_layers =
        moss__get_vk_validation_layers ( );
      validation_layer_count = validation_layers.count;
      validation_layer_names = validation_layers.names;
    }
    else {
      moss__warning (
        "Validation layers are enabled but not supported. Disabling validation layers."
      );
    }
  }

  // Set up other required info
  const VkApplicationInfo          vk_app_info = moss__create_vk_app_info (app_info);
  const Moss__VkInstanceExtensions extensions =
    moss__get_required_vk_instance_extensions ( );

  // Make instance create info
  const VkInstanceCreateInfo instance_create_info = {
    .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo        = &vk_app_info,
    .ppEnabledExtensionNames = extensions.names,
    .enabledExtensionCount   = extensions.count,
    .enabledLayerCount       = validation_layer_count,
    .ppEnabledLayerNames     = validation_layer_names,
    .flags                   = moss__get_required_vk_instance_flags ( ),
  };

  const VkResult result =
    vkCreateInstance (&instance_create_info, NULL, &g_engine.api_instance);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create Vulkan instance. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__init_stuffy_app (void)
{
  if (stuffy_app_init ( ) != 0)
  {
    moss__error ("Failed to initialize stuffy app.\n");
    return MOSS_RESULT_ERROR;
  }
  return MOSS_RESULT_SUCCESS;
}

static void moss__deinit_stuffy_app (void) { stuffy_app_deinit ( ); }

static MossResult
moss__create_window (const MossWindowConfig *window_config, const char *app_name)
{
  const StuffyWindowConfig stuffy_window_config = {
    .rect =
      {
             .x      = 128,
             .y      = 128,
             .width  = window_config->width,
             .height = window_config->height,
             },
    .title      = app_name,
    .style_mask = STUFFY_WINDOW_STYLE_TITLED_BIT | STUFFY_WINDOW_STYLE_CLOSABLE_BIT |
                  STUFFY_WINDOW_STYLE_RESIZABLE_BIT | STUFFY_WINDOW_STYLE_ICONIFIABLE_BIT,
  };

  g_engine.window = stuffy_window_open (&stuffy_window_config);
  if (g_engine.window == NULL)
  {
    moss__error ("Failed to create window.\n");
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_surface (void)
{
  const StuffyVkSurfaceCreateInfo create_info = {
    .window    = g_engine.window,
    .instance  = g_engine.api_instance,
    .allocator = NULL,
  };

  const VkResult result = stuffy_vk_create_surface (&create_info, &g_engine.surface);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create window surface. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_logical_device (void)
{
  const Moss__VkPhysicalDeviceExtensions extensions =
    moss__get_required_vk_device_extensions ( );

  uint32_t                queue_create_info_count = 0;
  VkDeviceQueueCreateInfo queue_create_infos[ 2 ];
  const float             queue_priority = 1.0F;

  const VkDeviceQueueCreateInfo graphics_queue_create_info = {
    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = g_engine.queue_family_indices.graphics_family,
    .queueCount       = 1,
    .pQueuePriorities = &queue_priority,
  };
  queue_create_infos[ queue_create_info_count++ ] = graphics_queue_create_info;

  if (g_engine.queue_family_indices.graphics_family !=
      g_engine.queue_family_indices.present_family)
  {
    VkDeviceQueueCreateInfo present_queue_create_info = {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = g_engine.queue_family_indices.present_family,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priority,
    };
    queue_create_infos[ queue_create_info_count++ ] = present_queue_create_info;
  }

  VkPhysicalDeviceFeatures device_features = {0};

  const VkDeviceCreateInfo create_info = {
    .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount    = queue_create_info_count,
    .pQueueCreateInfos       = queue_create_infos,
    .enabledExtensionCount   = extensions.count,
    .ppEnabledExtensionNames = extensions.names,
    .pEnabledFeatures        = &device_features,
  };

  const VkResult result =
    vkCreateDevice (g_engine.physical_device, &create_info, NULL, &g_engine.device);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create logical device. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_swapchain (uint32_t width, uint32_t height)
{
  Moss__SwapChainSupportDetails swapchain_support =
    moss__query_swapchain_support (g_engine.physical_device, g_engine.surface);

  const VkSurfaceFormatKHR surface_format = moss__choose_swap_surface_format (
    swapchain_support.formats,
    swapchain_support.format_count
  );
  const VkPresentModeKHR present_mode = moss__choose_swap_present_mode (
    swapchain_support.present_modes,
    swapchain_support.present_mode_count
  );
  const VkExtent2D extent =
    moss__choose_swap_extent (&swapchain_support.capabilities, width, height);

  VkSwapchainCreateInfoKHR create_info = {
    .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface          = g_engine.surface,
    .minImageCount    = swapchain_support.capabilities.minImageCount,
    .imageFormat      = surface_format.format,
    .imageColorSpace  = surface_format.colorSpace,
    .imageExtent      = extent,
    .imageArrayLayers = 1,
    .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform     = swapchain_support.capabilities.currentTransform,
    .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode      = present_mode,
    .clipped          = VK_TRUE,
    .oldSwapchain     = VK_NULL_HANDLE,
  };

  uint32_t queue_family_indices[] = {
    g_engine.queue_family_indices.graphics_family,
    g_engine.queue_family_indices.present_family,
  };

  if (g_engine.queue_family_indices.graphics_family !=
      g_engine.queue_family_indices.present_family)
  {
    create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices   = queue_family_indices;
  }
  else {
    create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices   = NULL;
  }

  const VkResult result =
    vkCreateSwapchainKHR (g_engine.device, &create_info, NULL, &g_engine.swapchain);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create swap chain. Error code: %d.\n", result);
    moss__free_swapchain_support_details (&swapchain_support);
    return MOSS_RESULT_ERROR;
  }

  vkGetSwapchainImagesKHR (
    g_engine.device,
    g_engine.swapchain,
    &g_engine.swapchain_image_count,
    NULL
  );
  g_engine.swapchain_images =
    (VkImage *)malloc (g_engine.swapchain_image_count * sizeof (VkImage));
  vkGetSwapchainImagesKHR (
    g_engine.device,
    g_engine.swapchain,
    &g_engine.swapchain_image_count,
    g_engine.swapchain_images
  );

  g_engine.swapchain_image_format = surface_format.format;
  g_engine.swapchain_extent       = extent;

  moss__free_swapchain_support_details (&swapchain_support);

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_image_views (void)
{
  g_engine.swapchain_image_views =
    (VkImageView *)malloc (g_engine.swapchain_image_count * sizeof (VkImageView));

  for (uint32_t i = 0; i < g_engine.swapchain_image_count; ++i)
  {
    const VkImageViewCreateInfo create_info = {
      .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image    = g_engine.swapchain_images[ i ],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format   = g_engine.swapchain_image_format,
      .components =
        {
                     .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                     },
      .subresourceRange = {
                     .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                     .baseMipLevel   = 0,
                     .levelCount     = 1,
                     .baseArrayLayer = 0,
                     .layerCount     = 1,
                     },
    };

    if (vkCreateImageView (
          g_engine.device,
          &create_info,
          NULL,
          &g_engine.swapchain_image_views[ i ]
        ) != VK_SUCCESS)
    {
      moss__error ("Failed to create image view %u.\n", i);
      for (uint32_t j = 0; j < i; ++j)
      {
        vkDestroyImageView (g_engine.device, g_engine.swapchain_image_views[ j ], NULL);
      }
      free (g_engine.swapchain_image_views);
      g_engine.swapchain_image_views = NULL;
      return MOSS_RESULT_ERROR;
    }
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_render_pass (void)
{
  const VkAttachmentDescription color_attachment = {
    .format         = g_engine.swapchain_image_format,
    .samples        = VK_SAMPLE_COUNT_1_BIT,
    .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  const VkAttachmentReference color_attachment_ref = {
    .attachment = 0,
    .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  const VkSubpassDescription subpass = {
    .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments    = &color_attachment_ref,
  };

  const VkSubpassDependency dependency = {
    .srcSubpass    = VK_SUBPASS_EXTERNAL,
    .dstSubpass    = 0,
    .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  const VkRenderPassCreateInfo render_pass_info = {
    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments    = &color_attachment,
    .subpassCount    = 1,
    .pSubpasses      = &subpass,
    .dependencyCount = 1,
    .pDependencies   = &dependency,
  };

  const VkResult result =
    vkCreateRenderPass (g_engine.device, &render_pass_info, NULL, &g_engine.render_pass);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create render pass. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_graphics_pipeline (void)
{
  VkShaderModule vert_shader_module;
  VkShaderModule frag_shader_module;

  if (moss__create_shader_module_from_file (
        g_engine.device,
        MOSS__VERT_SHADER_PATH,
        &vert_shader_module
      ) != VK_SUCCESS)
  {
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_shader_module_from_file (
        g_engine.device,
        MOSS__FRAG_SHADER_PATH,
        &frag_shader_module
      ) != VK_SUCCESS)
  {
    vkDestroyShaderModule (g_engine.device, vert_shader_module, NULL);
    return MOSS_RESULT_ERROR;
  }

  const VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage  = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vert_shader_module,
    .pName  = "main",
  };

  const VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = frag_shader_module,
    .pName  = "main",
  };

  const VkPipelineShaderStageCreateInfo shader_stages[] = {
    vert_shader_stage_info,
    frag_shader_stage_info
  };

  const VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount   = 0,
    .pVertexBindingDescriptions      = NULL,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions    = NULL,
  };

  const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  const VkViewport viewport = {
    .x        = 0.0F,
    .y        = 0.0F,
    .width    = (float)g_engine.swapchain_extent.width,
    .height   = (float)g_engine.swapchain_extent.height,
    .minDepth = 0.0F,
    .maxDepth = 1.0F,
  };

  const VkRect2D scissor = {
    .offset = {0, 0},
    .extent = g_engine.swapchain_extent,
  };

  const VkPipelineViewportStateCreateInfo viewport_state = {
    .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports    = &viewport,
    .scissorCount  = 1,
    .pScissors     = &scissor,
  };

  const VkPipelineRasterizationStateCreateInfo rasterizer = {
    .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable        = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode             = VK_POLYGON_MODE_FILL,
    .lineWidth               = 1.0F,
    .cullMode                = VK_CULL_MODE_BACK_BIT,
    .frontFace               = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable         = VK_FALSE,
  };

  const VkPipelineMultisampleStateCreateInfo multisampling = {
    .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable  = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  const VkPipelineColorBlendAttachmentState color_blend_attachment = {
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable = VK_FALSE,
  };

  const VkPipelineColorBlendStateCreateInfo color_blending = {
    .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable   = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments    = &color_blend_attachment,
  };

  const VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount         = 0,
    .pSetLayouts            = NULL,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges    = NULL,
  };

  if (vkCreatePipelineLayout (
        g_engine.device,
        &pipeline_layout_info,
        NULL,
        &g_engine.pipeline_layout
      ) != VK_SUCCESS)
  {
    moss__error ("Failed to create pipeline layout.\n");
    vkDestroyShaderModule (g_engine.device, frag_shader_module, NULL);
    vkDestroyShaderModule (g_engine.device, vert_shader_module, NULL);
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  const VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount          = 2,
    .pStages             = shader_stages,
    .pVertexInputState   = &vertex_input_info,
    .pInputAssemblyState = &input_assembly,
    .pViewportState      = &viewport_state,
    .pRasterizationState = &rasterizer,
    .pMultisampleState   = &multisampling,
    .pColorBlendState    = &color_blending,
    .layout              = g_engine.pipeline_layout,
    .renderPass          = g_engine.render_pass,
    .subpass             = 0,
    .basePipelineHandle  = VK_NULL_HANDLE,
    .basePipelineIndex   = -1,
  };

  const VkResult result = vkCreateGraphicsPipelines (
    g_engine.device,
    VK_NULL_HANDLE,
    1,
    &pipeline_info,
    NULL,
    &g_engine.graphics_pipeline
  );

  vkDestroyShaderModule (g_engine.device, frag_shader_module, NULL);
  vkDestroyShaderModule (g_engine.device, vert_shader_module, NULL);

  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create graphics pipeline. Error code: %d.\n", result);
    vkDestroyPipelineLayout (g_engine.device, g_engine.pipeline_layout, NULL);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_framebuffers (void)
{
  g_engine.swapchain_framebuffers =
    (VkFramebuffer *)malloc (g_engine.swapchain_image_count * sizeof (VkFramebuffer));

  for (uint32_t i = 0; i < g_engine.swapchain_image_count; ++i)
  {
    const VkImageView attachments[] = {g_engine.swapchain_image_views[ i ]};

    const VkFramebufferCreateInfo framebuffer_info = {
      .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass      = g_engine.render_pass,
      .attachmentCount = 1,
      .pAttachments    = attachments,
      .width           = g_engine.swapchain_extent.width,
      .height          = g_engine.swapchain_extent.height,
      .layers          = 1,
    };

    if (vkCreateFramebuffer (
          g_engine.device,
          &framebuffer_info,
          NULL,
          &g_engine.swapchain_framebuffers[ i ]
        ) != VK_SUCCESS)
    {
      moss__error ("Failed to create framebuffer %u.\n", i);
      for (uint32_t j = 0; j < i; ++j)
      {
        vkDestroyFramebuffer (
          g_engine.device,
          g_engine.swapchain_framebuffers[ j ],
          NULL
        );
      }
      free ((void *)g_engine.swapchain_framebuffers);
      g_engine.swapchain_framebuffers = NULL;
      return MOSS_RESULT_ERROR;
    }
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_command_pool (void)
{
  const VkCommandPoolCreateInfo pool_info = {
    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = g_engine.queue_family_indices.graphics_family,
  };

  const VkResult result =
    vkCreateCommandPool (g_engine.device, &pool_info, NULL, &g_engine.command_pool);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to create command pool. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static MossResult moss__create_command_buffers (void)
{
  const VkCommandBufferAllocateInfo alloc_info = {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = g_engine.command_pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  const VkResult result =
    vkAllocateCommandBuffers (g_engine.device, &alloc_info, g_engine.command_buffers);
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to allocate command buffers. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

static void
moss__record_command_buffer (VkCommandBuffer command_buffer, uint32_t image_index)
{
  const VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  if (vkBeginCommandBuffer (command_buffer, &begin_info) != VK_SUCCESS)
  {
    moss__error ("Failed to begin recording command buffer.\n");
    return;
  }

  const VkRenderPassBeginInfo render_pass_info = {
    .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass  = g_engine.render_pass,
    .framebuffer = g_engine.swapchain_framebuffers[ image_index ],
    .renderArea =
      {
                   .offset = {0, 0},
                   .extent = g_engine.swapchain_extent,
                   },
    .clearValueCount = 1,
    .pClearValues    = (const VkClearValue[]) {
                   {.color = {{0.0F, 0.0F, 0.0F, 1.0F}}},
                   },
  };

  vkCmdBeginRenderPass (command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline (
    command_buffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    g_engine.graphics_pipeline
  );

  vkCmdDraw (command_buffer, 3, 1, 0, 0);

  vkCmdEndRenderPass (command_buffer);

  if (vkEndCommandBuffer (command_buffer) != VK_SUCCESS)
  {
    moss__error ("Failed to record command buffer.\n");
  }
}

static void moss__cleanup_swapchain (void)
{
  if (g_engine.device == VK_NULL_HANDLE) { return; }

  if (g_engine.swapchain_framebuffers != NULL)
  {
    for (uint32_t i = 0; i < g_engine.swapchain_image_count; ++i)
    {
      vkDestroyFramebuffer (g_engine.device, g_engine.swapchain_framebuffers[ i ], NULL);
    }
    free (g_engine.swapchain_framebuffers);
    g_engine.swapchain_framebuffers = NULL;
  }

  if (g_engine.swapchain_image_views != NULL)
  {
    for (uint32_t i = 0; i < g_engine.swapchain_image_count; ++i)
    {
      vkDestroyImageView (g_engine.device, g_engine.swapchain_image_views[ i ], NULL);
    }
    free (g_engine.swapchain_image_views);
    g_engine.swapchain_image_views = NULL;
  }

  if (g_engine.swapchain_images != NULL)
  {
    free (g_engine.swapchain_images);
    g_engine.swapchain_images = NULL;
  }

  if (g_engine.swapchain != VK_NULL_HANDLE)
  {
    vkDestroySwapchainKHR (g_engine.device, g_engine.swapchain, NULL);
    g_engine.swapchain = VK_NULL_HANDLE;
  }

  g_engine.swapchain_images        = NULL;
  g_engine.swapchain_image_count   = 0;
  g_engine.swapchain_image_format  = 0;
  g_engine.swapchain_extent.width  = 0;
  g_engine.swapchain_extent.height = 0;
  g_engine.swapchain_image_views   = NULL;
  g_engine.swapchain_framebuffers  = NULL;
}

static MossResult moss__recreate_swapchain (uint32_t width, uint32_t height)
{
  vkDeviceWaitIdle (g_engine.device);

  moss__cleanup_swapchain ( );

  if (moss__create_swapchain (width, height) != MOSS_RESULT_SUCCESS)
  {
    return MOSS_RESULT_ERROR;
  }
  if (moss__create_image_views ( ) != MOSS_RESULT_SUCCESS) { return MOSS_RESULT_ERROR; }
  if (moss__create_framebuffers ( ) != MOSS_RESULT_SUCCESS) { return MOSS_RESULT_ERROR; }

  return MOSS_RESULT_SUCCESS;
}

/*
  @brief Creates image available semaphores.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_image_available_semaphores (void)
{
  if (g_engine.device == VK_NULL_HANDLE) { return MOSS_RESULT_ERROR; }

  const VkSemaphoreCreateInfo semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
  {
    const VkResult result = vkCreateSemaphore (
      g_engine.device,
      &semaphore_info,
      NULL,
      &g_engine.image_available_semaphores[ i ]
    );
    if (result == VK_SUCCESS) { continue; }

    moss__error ("Failed to create image available semaphore for frame %u.\n", i);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

/*
  @brief Creates render finished semaphores.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_render_finished_semaphores (void)
{
  if (g_engine.device == VK_NULL_HANDLE) { return MOSS_RESULT_ERROR; }

  const VkSemaphoreCreateInfo semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
  {
    const VkResult result = vkCreateSemaphore (
      g_engine.device,
      &semaphore_info,
      NULL,
      &g_engine.render_finished_semaphores[ i ]
    );
    if (result == VK_SUCCESS) { continue; }

    moss__error ("Failed to create render finished semaphore for frame %u.\n", i);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

/*
  @brief Creates in-flight fences.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_in_flight_fences (void)
{
  if (g_engine.device == VK_NULL_HANDLE) { return MOSS_RESULT_ERROR; }

  const VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
  {
    const VkResult result =
      vkCreateFence (g_engine.device, &fence_info, NULL, &g_engine.in_flight_fences[ i ]);
    if (result == VK_SUCCESS) { continue; }

    moss__error ("Failed to create in-flight fence for frame %u.\n", i);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

/*
  @brief Creates synchronization objects (semaphores and fences).
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
static MossResult moss__create_synchronization_objects (void)
{
  if (moss__create_image_available_semaphores ( ) != MOSS_RESULT_SUCCESS)
  {
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_render_finished_semaphores ( ) != MOSS_RESULT_SUCCESS)
  {
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_in_flight_fences ( ) != MOSS_RESULT_SUCCESS)
  {
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

/*
  @brief Cleans up semaphores array.
  @param semaphores Array of semaphores to clean up.
*/
static void moss__cleanup_semaphores (VkSemaphore *semaphores)
{
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
  {
    if (semaphores[ i ] == VK_NULL_HANDLE) { continue; }

    vkDestroySemaphore (g_engine.device, semaphores[ i ], NULL);
    semaphores[ i ] = VK_NULL_HANDLE;
  }
}

/*
  @brief Cleans up fences array.
  @param fences Array of fences to clean up.
*/
static void moss__cleanup_fences (VkFence *fences)
{
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
  {
    if (fences[ i ] == VK_NULL_HANDLE) { continue; }

    vkDestroyFence (g_engine.device, fences[ i ], NULL);
    fences[ i ] = VK_NULL_HANDLE;
  }
}

/*
  @brief Cleans up image available semaphores.
*/
static void moss__cleanup_image_available_semaphores (void)
{
  moss__cleanup_semaphores (g_engine.image_available_semaphores);
}

/*
  @brief Cleans up render finished semaphores.
*/
static void moss__cleanup_render_finished_semaphores (void)
{
  moss__cleanup_semaphores (g_engine.render_finished_semaphores);
}

/*
  @brief Cleans up in-flight fences.
*/
static void moss__cleanup_in_flight_fences (void)
{
  moss__cleanup_fences (g_engine.in_flight_fences);
}

/*
  @brief Cleans up synchronization objects.
*/
static void moss__cleanup_synchronization_objects (void)
{
  moss__cleanup_in_flight_fences ( );
  moss__cleanup_render_finished_semaphores ( );
  moss__cleanup_image_available_semaphores ( );
}
