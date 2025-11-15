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

  @file src/internal/shaders.h
  @brief Embedded shader code for triangle rendering
  @author Ilya Buravov (ilburale@gmail.com)
  @details This file contains SPIR-V bytecode for vertex and fragment shaders
           used to render a simple triangle.
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

/*
  @brief Vertex shader SPIR-V code.
  @details Simple vertex shader that outputs a triangle with positions and colors.
           Shader code:
           #version 450
           layout(location = 0) in vec2 inPosition;
           layout(location = 1) in vec3 inColor;
           layout(location = 0) out vec3 fragColor;
           void main() {
             gl_Position = vec4(inPosition, 0.0, 1.0);
             fragColor = inColor;
           }
*/
extern const uint32_t moss__vert_shader_spv[];
extern const size_t   moss__vert_shader_spv_size;

/*
  @brief Fragment shader SPIR-V code.
  @details Simple fragment shader that outputs the interpolated color.
           Shader code:
           #version 450
           layout(location = 0) in vec3 fragColor;
           layout(location = 0) out vec4 outColor;
           void main() {
             outColor = vec4(fragColor, 1.0);
           }
*/
extern const uint32_t moss__frag_shader_spv[];
extern const size_t   moss__frag_shader_spv_size;

