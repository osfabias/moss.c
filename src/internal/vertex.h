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

  @file src/internal/vertex.h
  @brief Internal vertex utility function declarations.
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

#include "moss/vertex.h"
#include "vulkan/vulkan_core.h"

/* VkVertexBindingDescription pack. */
typedef struct
{
  /* Binding descriptions. */
  const VkVertexInputBindingDescription *const descriptions;

  /* Description count. */
  const size_t count;
} Moss__VkVertexInputBindingDescriptionPack;

/* VkVertexAttributeDescription pack. */
typedef struct
{
  /* Attribute descriptions. */
  const VkVertexInputAttributeDescription *const descriptions;

  /* Description count. */
  const size_t count;
} Moss__VkVertexInputAttributeDescriptionPack;

/*
  @brief Returns Vulkan input binding description that corresponds to the @ref MossVertex.
  @return Vulkan input binding description.
*/
inline static Moss__VkVertexInputBindingDescriptionPack
moss__get_vk_vertex_input_binding_description (void)
{
  static const VkVertexInputBindingDescription binding_descriptions[] = {
    {
     .binding   = 0,
     .stride    = sizeof (MossVertex),
     .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
     },
  };

  static const Moss__VkVertexInputBindingDescriptionPack descriptions_pack = {
    .count        = sizeof (binding_descriptions) / sizeof (binding_descriptions[ 0 ]),
    .descriptions = binding_descriptions,
  };

  return descriptions_pack;
}

/*
  @brief Returns Vulkan input attribute descipritions that corresponds to the @ref
  MossVertex fields.
  @return Vulkan input attribute descriptions.
*/
inline static Moss__VkVertexInputAttributeDescriptionPack
moss__get_vk_vertex_input_attribute_description (void)
{
  static const VkVertexInputAttributeDescription attribute_descriptions[] = {
    {
     .binding  = 0,
     .location = 0,
     .format   = VK_FORMAT_R32G32_SFLOAT,
     .offset   = offsetof (MossVertex, position),
     },
    {
     .binding  = 0,
     .location = 1,
     .format   = VK_FORMAT_R32G32B32_SFLOAT,
     .offset   = offsetof (MossVertex,    color),
     }
  };

  static const Moss__VkVertexInputAttributeDescriptionPack descriptions_pack = {
    .descriptions = attribute_descriptions,
    .count = sizeof (attribute_descriptions) / sizeof (attribute_descriptions[ 0 ]),
  };

  return descriptions_pack;
}
