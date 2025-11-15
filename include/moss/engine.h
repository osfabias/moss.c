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

  @file include/moss/engine.h
  @brief Graphics engine functions
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdbool.h>

#include "moss/apidef.h"
#include "moss/app_info.h"
#include "moss/result.h"
#include "moss/window_config.h"

/*
  @brief Moss engine configuration.
*/
typedef struct
{
  const MossAppInfo      *app_info;      /* Application info. */
  const MossWindowConfig *window_config; /* Window configuration. */
} MossEngineConfig;

/*
  @brief Initializes graphics engine.
  @param config Engine configuration.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
__MOSS_API__ MossResult moss_engine_init (const MossEngineConfig *config);

/*
  @brief Deinitializes engine instance.
  @details Cleans up all reserved memory and destroys all GraphicsAPI objects.
*/
__MOSS_API__ void moss_engine_deinit (void);

/*
  @brief Draws a frame.
  @details Renders the current frame to the swap chain.
  @return Returns MOSS_RESULT_SUCCESS on success, MOSS_RESULT_ERROR otherwise.
*/
__MOSS_API__ MossResult moss_engine_draw_frame (void);

/*
  @brief Checks if the window should close.
  @return Returns true if window should close, false otherwise.
*/
__MOSS_API__ bool moss_engine_should_close (void);
