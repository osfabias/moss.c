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

  @file example/main.c
  @brief Example program demonstrating library usage
  @author Ilya Buravov (ilburale@gmail.com)
*/

#include <stddef.h>
#include <stdlib.h>

#include <moss/engine.h>
#include <moss/result.h>
#include <moss/window_config.h>

static const MossAppInfo moss_app_info = {
  .app_name    = "Moss Example Application",
  .app_version = {0, 1, 0},
};

static const MossWindowConfig window_config = {
  .width  = 640,
  .height = 360,
};

int main (void)
{
  const MossEngineConfig moss_engine_config = {
    .app_info      = &moss_app_info,
    .window_config = &window_config,
  };

  if (moss_engine_init (&moss_engine_config) != MOSS_RESULT_SUCCESS)
  {
    return EXIT_FAILURE;
  }

  while (!moss_engine_should_close ( )) { moss_engine_draw_frame ( ); }

  moss_engine_deinit ( );

  return EXIT_SUCCESS;
}
