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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include <stuffy/app.h>
#include <stuffy/vulkan.h>
#include <stuffy/window.h>

#include "moss/app_info.h"
#include "moss/engine.h"
#include "moss/result.h"
#include "moss/vertex.h"
#include "moss/window_config.h"

#include "src/internal/app_info.h"
#include "src/internal/crate.h"
#include "src/internal/log.h"
#include "src/internal/memory_utils.h"
#include "src/internal/shaders.h"
#include "src/internal/vertex.h"
#include "src/internal/vk_command_pool_utils.h"
#include "src/internal/vk_instance_utils.h"
#include "src/internal/vk_physical_device_utils.h"
#include "src/internal/vk_shader_utils.h"
#include "src/internal/vk_swapchain_utils.h"
#include "src/internal/vk_validation_layers_utils.h"

/*=============================================================================
    TEMPO
  =============================================================================*/

/* Vertex array just for implementing vertex buffers. */
static const MossVertex g_verticies[ 4 ] = {
  { { 0.0F, -0.5F }, { 1.0F, 0.0F, 0.0F } },
  {  { 0.5F, 0.5F }, { 0.0F, 1.0F, 0.0F } },
  { { -0.5F, 0.5F }, { 0.0F, 0.0F, 1.0F } },
};

/*=============================================================================
    ENGINE STATE
  =============================================================================*/

#define MAX_FRAMES_IN_FLIGHT (uint32_t)(2)

/* Max image count in swapchain. */
#define MAX_SWAPCHAIN_IMAGE_COUNT (uint32_t)(4)

/*
  @brief Engine state.
*/
typedef struct
{
  /* === Window === */
  /* Window handle. */
  StuffyWindow *window;

  /* === Vulkan instance and surface === */
  /* Vulkan instance. */
  VkInstance api_instance;
  /* Window surface. */
  VkSurfaceKHR surface;

  /* === Physical and logical device === */
  /* Physical device. */
  VkPhysicalDevice physical_device;
  /* Logical device. */
  VkDevice device;
  /* Queue family indices. */
  Moss__QueueFamilyIndices queue_family_indices;
  /* Graphics queue. */
  VkQueue graphics_queue;
  /* Present queue. */
  VkQueue present_queue;
  /* Transfer queue. */
  VkQueue transfer_queue;

  /* === Buffer sharing mode and queue family indices === */
  /* Buffer sharing mode for buffers shared between graphics and transfer queues. */
  VkSharingMode buffer_sharing_mode;
  /* Number of queue family indices that share buffers. */
  uint32_t shared_queue_family_index_count;
  /* Queue family indices that share buffers. */
  uint32_t shared_queue_family_indices[ 2 ];

  /* === Swap chain === */
  /* Swap chain. */
  VkSwapchainKHR swapchain;
  /* Swap chain images. */
  VkImage swapchain_images[ MAX_SWAPCHAIN_IMAGE_COUNT ];
  /* Number of swap chain images. */
  uint32_t swapchain_image_count;
  /* Swap chain image format. */
  VkFormat swapchain_image_format;
  /* Swap chain extent. */
  VkExtent2D swapchain_extent;
  /* Swap chain image views. */
  VkImageView swapchain_image_views[ MAX_SWAPCHAIN_IMAGE_COUNT ];
  /* Swap chain framebuffers. */
  VkFramebuffer swapchain_framebuffers[ MAX_SWAPCHAIN_IMAGE_COUNT ];
  /* Flag that shows, that the framebuffer resize was requested, but not performed yet. */
  bool framebuffer_resize_requsted;

  /* === Render pipeline === */
  /* Render pass. */
  VkRenderPass render_pass;
  /* Pipeline layout. */
  VkPipelineLayout pipeline_layout;
  /* Graphics pipeline. */
  VkPipeline graphics_pipeline;

  /* === Vertex buffers === */
  /* Vertex buffer. */
  Moss__Crate vertex_crate;

  /* === Command buffers === */
  /* General command pool. */
  VkCommandPool general_command_pool;
  /* Command buffers. */
  VkCommandBuffer general_command_buffers[ MAX_FRAMES_IN_FLIGHT ];
  /* Transfer command pool. */
  VkCommandPool transfer_command_pool;

  /* === Synchronization objects === */
  /* Image available semaphores. */
  VkSemaphore image_available_semaphores[ MAX_FRAMES_IN_FLIGHT ];
  /* Render finished semaphores. */
  VkSemaphore render_finished_semaphores[ MAX_FRAMES_IN_FLIGHT ];
  /* In-flight fences. */
  VkFence in_flight_fences[ MAX_FRAMES_IN_FLIGHT ];

  /* === Frame state === */
  /* Current frame index. */
  uint32_t current_frame;
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
  .physical_device                            = VK_NULL_HANDLE,
  .device                                     = VK_NULL_HANDLE,
  .graphics_queue                             = VK_NULL_HANDLE,
  .present_queue                              = VK_NULL_HANDLE,
  .transfer_queue = VK_NULL_HANDLE,
  .queue_family_indices = {
    .graphics_family       = 0,
    .present_family        = 0,
    .transfer_family       = 0,
    .graphics_family_found = false,
    .present_family_found  = false,
    .transfer_family_found = false,
  },
  .buffer_sharing_mode            = VK_SHARING_MODE_EXCLUSIVE,
  .shared_queue_family_index_count = 0,
  .shared_queue_family_indices     = {0, 0},

  /* Swap chain. */
  .swapchain                   = VK_NULL_HANDLE,
  .swapchain_images            = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE },
  .swapchain_image_count       = 0,
  .swapchain_image_format      = 0,
  .swapchain_extent            = (VkExtent2D) { .width = 0, .height = 0 },
  .swapchain_image_views       = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE },
  .swapchain_framebuffers      = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE },
  .framebuffer_resize_requsted = false,

  /* Render pipeline. */
  .render_pass       = VK_NULL_HANDLE,
  .pipeline_layout   = VK_NULL_HANDLE,
  .graphics_pipeline = VK_NULL_HANDLE,

  /* Vertex buffers. */
  .vertex_crate = {0},

  /* Command buffers. */
  .general_command_pool    = VK_NULL_HANDLE,
  .general_command_buffers = { VK_NULL_HANDLE, VK_NULL_HANDLE },

  /* Synchronization objects. */
  .image_available_semaphores = { VK_NULL_HANDLE, VK_NULL_HANDLE },
  .render_finished_semaphores = { VK_NULL_HANDLE, VK_NULL_HANDLE },
  .in_flight_fences           = { VK_NULL_HANDLE, VK_NULL_HANDLE },

  /* Frame state. */
  .current_frame = 0,
};

/*=============================================================================
    INTERNAL CALLBACK FUNCTION DECLARATIONS
  =============================================================================*/

/*
  @brief Callback for window resize event.
  @note Satisfies @ref StuffyWindowResizeCallback signature.
*/
static void moss__window_resize_callback (StuffyWindow *window, StuffyWindowRect rect);

/*=============================================================================
    INTERNAL FUNCTION DECLARATIONS
  =============================================================================*/

/*
  @brief Creates Vulkan API instance.
  @param app_info A pointer to a native moss app info struct.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_api_instance (const MossAppInfo *app_info);

/*
  @brief Initializes stuffy app.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__init_stuffy_app (void);

/*
  @brief Deinitializes stuffy app.
*/
inline static void moss__deinit_stuffy_app (void);

/*
  @brief Creates stuffy window config out of moss window config.
  @param window_config A pointer to a moss window cofig.
  @return Returns stuffy window config that corresponds to the passed moss config.
*/
inline static StuffyWindowConfig
moss__create_stuffy_window_config (const MossWindowConfig *window_config);

/*
  @brief Creates window.
  @param window_config Window configuration.
  @param app_name Application name for window title.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__open_window (const MossWindowConfig *window_config);

/*
  @brief Creates window surface.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_surface (void);

/*
  @brief Creates logical device and queues.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_logical_device (void);

/*
  @brief Initializes buffer sharing mode and queue family indices.
  @details Determines whether graphics and transfer queue families are the same,
           and sets up the appropriate sharing mode and queue family indices
           for buffer creation. This should be called after logical device creation.
*/
inline static void moss__init_buffer_sharing_mode (void);

/*
  @brief Creates swap chain.
  @param width Window width.
  @param height Window height.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_swapchain (uint32_t width, uint32_t height);

/*
  @brief Creates image views for swap chain images.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_image_views (void);

/*
  @brief Creates render pass.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_render_pass (void);

/*
  @brief Returns Vulkan pipeline vertex input state info.
  @return Vulkan pipeline vertex input state info.
*/
inline static VkPipelineVertexInputStateCreateInfo
moss__create_vk_pipeline_vertex_input_state_info (void);

/*
  @brief Creates graphics pipeline.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_graphics_pipeline (void);

/*
  @brief Creates framebuffers.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_framebuffers (void);

/*
  @brief Creates vertex buffer.
  @return Returns MOSS_RESULT_SUCCESS on successs, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_vertex_crate (void);

/*
  @brief Fills vertex crate with vertex data.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__fill_vertex_crate (void);

/*
  @brief Creates command buffers.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_general_command_buffers (void);

/*
  @brief Creates image available semaphores.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_image_available_semaphores (void);

/*
  @brief Creates render finished semaphores.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_render_finished_semaphores (void);

/*
  @brief Creates in-flight fences.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_in_flight_fences (void);

/*
  @brief Creates synchronization objects (semaphores and fences).
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
inline static MossResult moss__create_synchronization_objects (void);

/*
  @brief Cleans up semaphores array.
  @param semaphores Array of semaphores to clean up.
*/
inline static void moss__cleanup_semaphores (VkSemaphore *semaphores);

/*
  @brief Cleans up fences array.
  @param fences Array of fences to clean up.
*/
inline static void moss__cleanup_fences (VkFence *fences);

/*
  @brief Cleans up image available semaphores.
*/
inline static void moss__cleanup_image_available_semaphores (void);

/*
  @brief Cleans up render finished semaphores.
*/
inline static void moss__cleanup_render_finished_semaphores (void);

/*
  @brief Cleans up in-flight fences.
*/
inline static void moss__cleanup_in_flight_fences (void);

/*
  @brief Cleans up synchronization objects.
*/
inline static void moss__cleanup_synchronization_objects (void);

/*
  @brief Records command buffer.
  @param command_buffer Command buffer to record.
  @param image_index Swap chain image index.
*/
inline static void
moss__record_command_buffer (VkCommandBuffer command_buffer, uint32_t image_index);

/*
  @brief Cleans up swapchain framebuffers.
*/
inline static void moss__cleanup_swapchain_framebuffers (void);

/*
  @brief Cleans up swapchain image views.
*/
inline static void moss__cleanup_swapchain_image_views (void);

/*
  @brief Cleans up swapchain handle.
*/
inline static void moss__cleanup_swapchain_handle (void);

/*
  @brief Cleans up swap chain resources.
*/
inline static void moss__cleanup_swapchain (void);

/*
  @brief Waits until window gets maximized.
*/
inline static void moss__wait_while_window_is_minimized (void);

/*
  @brief Recreates swap chain.
  @param width Window width.
  @param height Window height.
  @return Returns MOSS_RESULT_SUCCESS on success, error code otherwise.
*/
inline static MossResult moss__recreate_swapchain (uint32_t width, uint32_t height);

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

  if (moss__open_window (config->window_config) != MOSS_RESULT_SUCCESS)
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

  vkGetDeviceQueue (
    g_engine.device,
    g_engine.queue_family_indices.transfer_family,
    0,
    &g_engine.transfer_queue
  );

  moss__init_buffer_sharing_mode ( );

  const StuffyExtent2D framebuffer_size =
    stuffy_window_get_framebuffer_size (g_engine.window);
  if (moss__create_swapchain (framebuffer_size.width, framebuffer_size.height) !=
      MOSS_RESULT_SUCCESS)
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

  // Create general command pool
  if (moss__create_command_pool (
        g_engine.device,
        g_engine.queue_family_indices.graphics_family,
        &g_engine.general_command_pool
      ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  // Create transfer command pool
  if (moss__create_command_pool (
        g_engine.device,
        g_engine.queue_family_indices.transfer_family,
        &g_engine.transfer_command_pool
      ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_vertex_crate ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__fill_vertex_crate ( ) != MOSS_RESULT_SUCCESS)
  {
    moss_engine_deinit ( );
    return MOSS_RESULT_ERROR;
  }

  if (moss__create_general_command_buffers ( ) != MOSS_RESULT_SUCCESS)
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
    if (g_engine.transfer_command_pool != VK_NULL_HANDLE)
    {
      vkDestroyCommandPool (g_engine.device, g_engine.transfer_command_pool, NULL);
      g_engine.transfer_command_pool = VK_NULL_HANDLE;
    }

    if (g_engine.general_command_pool != VK_NULL_HANDLE)
    {
      vkDestroyCommandPool (g_engine.device, g_engine.general_command_pool, NULL);
      g_engine.general_command_pool = VK_NULL_HANDLE;
    }

    moss__destroy_crate (&g_engine.vertex_crate);

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
    g_engine.device                                     = VK_NULL_HANDLE;
    g_engine.physical_device                            = VK_NULL_HANDLE;
    g_engine.graphics_queue                             = VK_NULL_HANDLE;
    g_engine.transfer_queue                             = VK_NULL_HANDLE;
    g_engine.present_queue                              = VK_NULL_HANDLE;
    g_engine.queue_family_indices.graphics_family       = 0;
    g_engine.queue_family_indices.present_family        = 0;
    g_engine.queue_family_indices.transfer_family       = 0;
    g_engine.queue_family_indices.graphics_family_found = false;
    g_engine.queue_family_indices.present_family_found  = false;
    g_engine.queue_family_indices.transfer_family_found = false;
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
    g_engine.general_command_buffers[ g_engine.current_frame ];

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
    const StuffyExtent2D framebuffer_size =
      stuffy_window_get_framebuffer_size (g_engine.window);
    return moss__recreate_swapchain (framebuffer_size.width, framebuffer_size.height);
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
  {
    moss__error ("Failed to acquire swap chain image.\n");
    return MOSS_RESULT_ERROR;
  }

  vkResetCommandBuffer (command_buffer, 0);
  moss__record_command_buffer (command_buffer, current_image_index);

  const VkSemaphore wait_semaphores[] = { image_available_semaphore };
  const size_t      wait_semaphore_count =
    sizeof (wait_semaphores) / sizeof (wait_semaphores[ 0 ]);

  const VkSemaphore signal_semaphores[] = { render_finished_semaphore };
  const size_t      signal_semaphore_count =
    sizeof (signal_semaphores) / sizeof (signal_semaphores[ 0 ]);

  const VkSubmitInfo submit_info = {
    .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = wait_semaphore_count,
    .pWaitSemaphores    = wait_semaphores,
    .pWaitDstStageMask =
      (const VkPipelineStageFlags[]) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
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
    .pImageIndices      = &current_image_index,
  };

  result = vkQueuePresentKHR (g_engine.present_queue, &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      g_engine.framebuffer_resize_requsted)
  {
    g_engine.framebuffer_resize_requsted = false;

    // Swap chain is out of date or suboptimal, need to recreate
    const StuffyExtent2D framebuffer_size =
      stuffy_window_get_framebuffer_size (g_engine.window);
    if (moss__recreate_swapchain (framebuffer_size.width, framebuffer_size.height) !=
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
    INTERNAL CALLBACK FUNCTIONS IMPLENTATION
  =============================================================================*/

static void
moss__window_resize_callback (StuffyWindow *const window, StuffyWindowRect rect)
{
  (void)(window);
  (void)(rect);

  g_engine.framebuffer_resize_requsted = true;
}

/*=============================================================================
    INTERNAL FUNCTIONS IMPLEMENTATION
  =============================================================================*/

inline static MossResult moss__create_api_instance (const MossAppInfo *const app_info)
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

inline static MossResult moss__init_stuffy_app (void)
{
  if (stuffy_app_init ( ) != 0)
  {
    moss__error ("Failed to initialize stuffy app.\n");
    return MOSS_RESULT_ERROR;
  }
  return MOSS_RESULT_SUCCESS;
}

inline static void moss__deinit_stuffy_app (void) { stuffy_app_deinit ( ); }

inline static StuffyWindowConfig
moss__create_stuffy_window_config (const MossWindowConfig *const window_config)
{
  const StuffyWindowRect window_rect = {
    .x      = 128,
    .y      = 128,
    .width  = window_config->width,
    .height = window_config->height,
  };

  const StuffyWindowConfig result_config = {
    .rect       = window_rect,
    .title      = window_config->title,
    .style_mask = STUFFY_WINDOW_STYLE_TITLED_BIT | STUFFY_WINDOW_STYLE_CLOSABLE_BIT |
                  STUFFY_WINDOW_STYLE_RESIZABLE_BIT | STUFFY_WINDOW_STYLE_ICONIFIABLE_BIT,
  };

  return result_config;
}

inline static MossResult moss__open_window (const MossWindowConfig *window_config)
{
  const StuffyWindowConfig stuffy_window_config =
    moss__create_stuffy_window_config (window_config);

  g_engine.window = stuffy_window_open (&stuffy_window_config);
  if (g_engine.window == NULL)
  {
    moss__error ("Failed to create window.\n");
    return MOSS_RESULT_ERROR;
  }

  stuffy_window_set_resize_callback (g_engine.window, moss__window_resize_callback);

  return MOSS_RESULT_SUCCESS;
}

inline static MossResult moss__create_surface (void)
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

inline static MossResult moss__create_logical_device (void)
{
  const Moss__VkPhysicalDeviceExtensions extensions =
    moss__get_required_vk_device_extensions ( );

  uint32_t                queue_create_info_count = 0;
  VkDeviceQueueCreateInfo queue_create_infos[ 3 ];
  const float             queue_priority = 1.0F;

  // Add graphics queue create info
  {
    const VkDeviceQueueCreateInfo create_info = {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = g_engine.queue_family_indices.graphics_family,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priority,
    };
    queue_create_infos[ queue_create_info_count++ ] = create_info;
  }

  // Add present queue create info
  if (g_engine.queue_family_indices.graphics_family !=
      g_engine.queue_family_indices.present_family)
  {
    const VkDeviceQueueCreateInfo create_info = {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = g_engine.queue_family_indices.present_family,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priority,
    };
    queue_create_infos[ queue_create_info_count++ ] = create_info;
  }

  // Add transfer queue create info
  if (g_engine.queue_family_indices.graphics_family !=
      g_engine.queue_family_indices.transfer_family)
  {
    const VkDeviceQueueCreateInfo create_info = {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = g_engine.queue_family_indices.transfer_family,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priority,
    };
    queue_create_infos[ queue_create_info_count++ ] = create_info;
  }

  VkPhysicalDeviceFeatures device_features = { 0 };

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

inline static void moss__init_buffer_sharing_mode (void)
{
  if (g_engine.queue_family_indices.graphics_family ==
      g_engine.queue_family_indices.transfer_family)
  {
    g_engine.buffer_sharing_mode             = VK_SHARING_MODE_EXCLUSIVE;
    g_engine.shared_queue_family_index_count = 0;
  }
  else {
    g_engine.buffer_sharing_mode = VK_SHARING_MODE_CONCURRENT;
    g_engine.shared_queue_family_indices[ 0 ] =
      g_engine.queue_family_indices.graphics_family;
    g_engine.shared_queue_family_indices[ 1 ] =
      g_engine.queue_family_indices.transfer_family;
    g_engine.shared_queue_family_index_count = 2;
  }
}

inline static MossResult
moss__create_swapchain (const uint32_t width, const uint32_t height)
{
  const Moss__SwapChainSupportDetails swapchain_support =
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
    return MOSS_RESULT_ERROR;
  }

  vkGetSwapchainImagesKHR (
    g_engine.device,
    g_engine.swapchain,
    &g_engine.swapchain_image_count,
    NULL
  );

  if (g_engine.swapchain_image_count > MAX_SWAPCHAIN_IMAGE_COUNT)
  {
    moss__error (
      "Real swapchain image count is bigger than expected. (%d > %d)",
      g_engine.swapchain_image_count,
      MAX_SWAPCHAIN_IMAGE_COUNT
    );
    return MOSS_RESULT_ERROR;
  }

  vkGetSwapchainImagesKHR (
    g_engine.device,
    g_engine.swapchain,
    &g_engine.swapchain_image_count,
    g_engine.swapchain_images
  );

  g_engine.swapchain_image_format = surface_format.format;
  g_engine.swapchain_extent       = extent;

  return MOSS_RESULT_SUCCESS;
}

inline static MossResult moss__create_image_views (void)
{
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
      return MOSS_RESULT_ERROR;
    }
  }

  return MOSS_RESULT_SUCCESS;
}

inline static MossResult moss__create_render_pass (void)
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

inline static VkPipelineVertexInputStateCreateInfo
moss__create_vk_pipeline_vertex_input_state_info (void)
{
  const Moss__VkVertexInputBindingDescriptionPack binding_descriptions_pack =
    moss__get_vk_vertex_input_binding_description ( );

  const Moss__VkVertexInputAttributeDescriptionPack attribute_descriptions_pack =
    moss__get_vk_vertex_input_attribute_description ( );

  const VkPipelineVertexInputStateCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount   = binding_descriptions_pack.count,
    .pVertexBindingDescriptions      = binding_descriptions_pack.descriptions,
    .vertexAttributeDescriptionCount = attribute_descriptions_pack.count,
    .pVertexAttributeDescriptions    = attribute_descriptions_pack.descriptions,
  };

  return info;
}

inline static MossResult moss__create_graphics_pipeline (void)
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

  const VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info,
                                                            frag_shader_stage_info };

  const VkPipelineVertexInputStateCreateInfo vertex_input_info =
    moss__create_vk_pipeline_vertex_input_state_info ( );

  const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  const VkPipelineViewportStateCreateInfo viewport_state = {
    .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports    = NULL,  // Dynamic viewport
    .scissorCount  = 1,
    .pScissors     = NULL,  // Dynamic scissor
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

  const VkDynamicState dynamic_states[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  const VkPipelineDynamicStateCreateInfo dynamic_state = {
    .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = sizeof (dynamic_states) / sizeof (dynamic_states[ 0 ]),
    .pDynamicStates    = dynamic_states,
  };

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
    .pDynamicState       = &dynamic_state,
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

inline static MossResult moss__create_framebuffers (void)
{
  for (uint32_t i = 0; i < g_engine.swapchain_image_count; ++i)
  {
    const VkImageView attachments[] = { g_engine.swapchain_image_views[ i ] };

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
      return MOSS_RESULT_ERROR;
    }
  }

  return MOSS_RESULT_SUCCESS;
}

inline static MossResult moss__create_vertex_crate (void)
{
  Moss__CrateCreateInfo create_info = {
    .size  = sizeof (g_verticies),
    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .memory_properties               = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    .sharing_mode                    = g_engine.buffer_sharing_mode,
    .shared_queue_family_index_count = g_engine.shared_queue_family_index_count,
    .shared_queue_family_indices     = g_engine.shared_queue_family_indices,
  };

  const MossResult result = moss__create_crate (
    g_engine.physical_device,
    g_engine.device,
    &create_info,
    &g_engine.vertex_crate
  );

  if (result != MOSS_RESULT_SUCCESS) { moss__error ("Failed to create vertex crate.\n"); }

  return result;
}

inline static MossResult moss__fill_vertex_crate (void)
{
  const Moss__FillCrateInfo fill_info = {
    .destination_crate = &g_engine.vertex_crate,
    .source_memory     = (void *)g_verticies,
    .size              = sizeof (g_verticies),
    .transfer_queue    = g_engine.transfer_queue,
    .command_pool      = g_engine.transfer_command_pool,
  };

  return moss__fill_crate (&fill_info);
}

inline static MossResult moss__create_general_command_buffers (void)
{
  const VkCommandBufferAllocateInfo alloc_info = {
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool        = g_engine.general_command_pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  const VkResult result = vkAllocateCommandBuffers (
    g_engine.device,
    &alloc_info,
    g_engine.general_command_buffers
  );
  if (result != VK_SUCCESS)
  {
    moss__error ("Failed to allocate command buffers. Error code: %d.\n", result);
    return MOSS_RESULT_ERROR;
  }

  return MOSS_RESULT_SUCCESS;
}

inline static void
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

  const VkViewport viewport = {
    .x        = 0.0F,
    .y        = 0.0F,
    .width    = (float)g_engine.swapchain_extent.width,
    .height   = (float)g_engine.swapchain_extent.height,
    .minDepth = 0.0F,
    .maxDepth = 1.0F,
  };

  const VkRect2D scissor = {
    .offset = { 0, 0 },
    .extent = g_engine.swapchain_extent,
  };

  const VkBuffer     vertex_buffers[]        = { g_engine.vertex_crate.buffer };
  const VkDeviceSize vertex_buffer_offsets[] = { 0 };

  vkCmdSetViewport (command_buffer, 0, 1, &viewport);
  vkCmdSetScissor (command_buffer, 0, 1, &scissor);

  vkCmdBindVertexBuffers (command_buffer, 0, 1, vertex_buffers, vertex_buffer_offsets);

  vkCmdDraw (command_buffer, sizeof (g_verticies) / sizeof (g_verticies[ 0 ]), 1, 0, 0);

  vkCmdEndRenderPass (command_buffer);

  if (vkEndCommandBuffer (command_buffer) != VK_SUCCESS)
  {
    moss__error ("Failed to record command buffer.\n");
  }
}

inline static void moss__cleanup_swapchain_framebuffers (void)
{
  for (uint32_t i = 0; i < g_engine.swapchain_image_count; ++i)
  {
    if (g_engine.swapchain_framebuffers[ i ] == VK_NULL_HANDLE) { continue; }

    vkDestroyFramebuffer (g_engine.device, g_engine.swapchain_framebuffers[ i ], NULL);
    g_engine.swapchain_framebuffers[ i ] = VK_NULL_HANDLE;
  }
}

inline static void moss__cleanup_swapchain_image_views (void)
{
  for (uint32_t i = 0; i < g_engine.swapchain_image_count; ++i)
  {
    if (g_engine.swapchain_image_views[ i ] == VK_NULL_HANDLE) { continue; }

    vkDestroyImageView (g_engine.device, g_engine.swapchain_image_views[ i ], NULL);
    g_engine.swapchain_image_views[ i ] = VK_NULL_HANDLE;
  }
}

inline static void moss__cleanup_swapchain_handle (void)
{
  if (g_engine.swapchain != VK_NULL_HANDLE)
  {
    vkDestroySwapchainKHR (g_engine.device, g_engine.swapchain, NULL);
    g_engine.swapchain = VK_NULL_HANDLE;
  }
}

inline static void moss__cleanup_swapchain (void)
{
  moss__cleanup_swapchain_framebuffers ( );
  moss__cleanup_swapchain_image_views ( );
  moss__cleanup_swapchain_handle ( );

  g_engine.swapchain_image_count  = 0;
  g_engine.swapchain_image_format = 0;
  g_engine.swapchain_extent       = (VkExtent2D) { .width = 0, .height = 0 };
}

inline static void moss__wait_while_window_is_minimized (void)
{
  StuffyWindowRect rect = stuffy_window_get_rect (g_engine.window);

  while (rect.width == 0 || rect.height == 0)
  {
    rect = stuffy_window_get_rect (g_engine.window);
    stuffy_app_update ( );
  }
}

inline static MossResult moss__recreate_swapchain (uint32_t width, uint32_t height)
{
  moss__wait_while_window_is_minimized ( );

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
inline static MossResult moss__create_image_available_semaphores (void)
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
inline static MossResult moss__create_render_finished_semaphores (void)
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
inline static MossResult moss__create_in_flight_fences (void)
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
inline static MossResult moss__create_synchronization_objects (void)
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
inline static void moss__cleanup_semaphores (VkSemaphore *semaphores)
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
inline static void moss__cleanup_fences (VkFence *fences)
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
inline static void moss__cleanup_image_available_semaphores (void)
{
  moss__cleanup_semaphores (g_engine.image_available_semaphores);
}

/*
  @brief Cleans up render finished semaphores.
*/
inline static void moss__cleanup_render_finished_semaphores (void)
{
  moss__cleanup_semaphores (g_engine.render_finished_semaphores);
}

/*
  @brief Cleans up in-flight fences.
*/
inline static void moss__cleanup_in_flight_fences (void)
{
  moss__cleanup_fences (g_engine.in_flight_fences);
}

/*
  @brief Cleans up synchronization objects.
*/
inline static void moss__cleanup_synchronization_objects (void)
{
  moss__cleanup_in_flight_fences ( );
  moss__cleanup_render_finished_semaphores ( );
  moss__cleanup_image_available_semaphores ( );
}
