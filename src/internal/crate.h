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

  @file src/internal/crate.h
  @brief Crate abstraction functions for ease of work with Vulkan buffers.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "moss/result.h"
#include "vulkan/vulkan_core.h"

/*
  @brief Crate - a self-contained GPU buffer abstraction.
  @details A crate is a wrapper structure that encapsulates a Vulkan buffer along with
           its associated device memory and metadata. It provides a convenient way to
           manage GPU buffers by bundling together all the necessary Vulkan handles
           and device references required for buffer operations and cleanup.

  @par Lifecycle:
  - Create: Use @ref moss__create_crate to allocate and bind buffer memory
  - Use: Access the buffer via the @c buffer field for Vulkan operations
  - Fill: Use @ref moss__fill_crate to upload data to the buffer
  - Destroy: Use @ref moss__free_crate to release all resources

  @par Example:
  @code
  Moss__CrateCreateInfo create_info = {
    .size = 1024,
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    // ... other fields
  };
  Moss__Crate crate;
  moss__create_crate(physical_device, device, &create_info, &crate);
  // Use crate.buffer in Vulkan commands
  moss__free_crate(&crate);
  @endcode
*/
typedef struct
{
  /* Physical device where the buffer's memory is allocated.
     @details This is the physical device that was used during crate creation to
              determine memory type requirements. Stored for reference and potential
              future operations that may require physical device information.
     @note This field is set automatically by @ref moss__create_crate and should
           not be modified manually. */
  VkPhysicalDevice original_physical_device;

  /* Logical device where the buffer and its memory were created.
     @details This device handle is required for all buffer and memory operations,
              including mapping, unmapping, and cleanup. Stored to enable proper
              resource management without requiring external device references.
     @note This field is set automatically by @ref moss__create_crate and should
           not be modified manually. */
  VkDevice original_device;

  /* Vulkan buffer handle.
     @details The actual Vulkan buffer object that can be used in Vulkan commands
              such as vkCmdBindVertexBuffers, vkCmdBindIndexBuffer, or as a source
              or destination in transfer operations. The buffer is created with the
              usage flags and sharing mode specified in the creation info.
     @note This handle is valid only after successful crate creation and becomes
           invalid after calling @ref moss__free_crate. */
  VkBuffer buffer;

  /* Device memory handle bound to the buffer.
     @details The memory object that was allocated and bound to the buffer during
              crate creation. This memory has the properties specified in the
              creation info (e.g., host-visible, device-local). The memory is
              automatically bound to the buffer at offset 0 during creation.
     @note This handle is valid only after successful crate creation and becomes
           invalid after calling @ref moss__free_crate. */
  VkDeviceMemory memory;

  /* Actual size of the allocated buffer memory in bytes.
     @details This is the actual size of memory allocated by Vulkan, which may be
              larger than the requested size due to memory alignment requirements
              and device-specific constraints. This value is obtained from
              vkGetBufferMemoryRequirements and represents the true allocation size.
     @note This size should be used when mapping memory or performing operations
           that require the actual allocated size rather than the requested size. */
  VkDeviceSize size;

  /* Buffer sharing mode between queue families.
     @details The sharing mode that was used when creating this buffer. This is
              either VK_SHARING_MODE_EXCLUSIVE or VK_SHARING_MODE_CONCURRENT.
     @note This field is set automatically by @ref moss__create_crate and should
           not be modified manually. */
  VkSharingMode sharing_mode;

  /* Number of queue family indices that share this buffer.
     @details The number of queue families in the @c shared_queue_family_indices
              array. This is 0 for exclusive mode, or 2 for concurrent mode when
              sharing between graphics and transfer queues.
     @note This field is set automatically by @ref moss__create_crate and should
           not be modified manually. */
  uint32_t shared_queue_family_index_count;

  /* Queue family indices that share this buffer.
     @details An array of queue family indices that can access this buffer when
              using concurrent sharing mode. Only valid when @c sharing_mode is
              VK_SHARING_MODE_CONCURRENT and @c shared_queue_family_index_count > 0.
     @note This field is set automatically by @ref moss__create_crate and should
           not be modified manually. */
  uint32_t shared_queue_family_indices[ 2 ];
} Moss__Crate;

/*
  @brief Crate creation information structure.
  @details This structure contains all the parameters needed to create a crate.
           It specifies the buffer's size, usage, sharing mode, and memory
           requirements. All fields must be properly initialized before passing
           to @ref moss__create_crate.

           @note The actual allocated memory size may differ from the requested
                 @c size due to alignment requirements. The actual size is stored
                 in the created crate's @c size field.
*/
typedef struct
{
  /* Physical device to allocate memory on. */
  VkPhysicalDevice physical_device;

  /* Logical device to create buffer itself on. */
  VkDevice device;

  /* Desired size of the buffer in bytes. */
  VkDeviceSize size;

  /* Buffer usage flags specifying how the buffer will be used. */
  VkBufferUsageFlags usage;

  /* Buffer sharing mode between queue families. */
  VkSharingMode sharing_mode;

  /* Number of queue family indices that will share the buffer. */
  uint32_t shared_queue_family_index_count;

  /* Array of queue family indices that will share the buffer. */
  const uint32_t *shared_queue_family_indices;

  /* Required memory properties for the buffer's backing memory. */
  VkMemoryPropertyFlags memory_properties;
} Moss__CrateCreateInfo;

/*
  @brief Required information for crate filling operation.
*/
typedef struct
{
  /* Destination crate to write data to. */
  Moss__Crate *destination_crate;

  /* Source memory to read data from. */
  void *source_memory;

  /* Number of bytes to read and write. */
  VkDeviceSize size;

  /* Queue to perform transfer operation on. */
  VkQueue transfer_queue;

  /* Command pool to perform commands in. */
  VkCommandPool command_pool;
} Moss__FillCrateInfo;

/*
  @brief Creates moss crate.
  @param info Required info for crate creation.
  @param out_crate Ouput variable where crate handle will be written to.
  @return MOSS_RESULT_SUCCESS on success, otherwise MOSS_RESULT_ERROR.
*/
MossResult moss__create_crate (const Moss__CrateCreateInfo *info, Moss__Crate *out_crate);

/*
  @brief Fill moss crate.
  @details Creates temporary staging crate in order to store actual data on GPU
           in the most optimized format.
  @param info Required information for crate fill operation.
  @return MOSS_RESULT_SUCCESS on success, otherwise MOSS_RESULT_ERROR.
*/
MossResult moss__fill_crate (const Moss__FillCrateInfo *info);

/*
  @brief Free moss crate.
  @details Destroys the buffer and frees its memory. After calling this function,
           the crate handle becomes invalid and should not be used.
  @param crate Crate to free. Must not be NULL.
  @note This function safely handles NULL handles in the crate structure.
*/
void moss__destroy_crate (Moss__Crate *crate);
