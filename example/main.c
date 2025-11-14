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

#include <stdlib.h>

#include <stuffy/app.h>
#include <stuffy/window.h>

static const WindowConf window_config = {
  .rect       = {.x = 128, .y = 128, .width = 640, .height = 360},
  .title      = "Moss Example Application",
  .style_mask = WINDOW_STYLE_TITLED_BIT | WINDOW_STYLE_CLOSABLE_BIT |
                WINDOW_STYLE_RESIZABLE_BIT | WINDOW_STYLE_ICONIFIABLE_BIT,
};

int main (void)
{
  init_app ( );


  Window *const window = open_window (&window_config);

  while (!should_window_close (window)) { update_app ( ); }

  deinit_app ( );

  return EXIT_SUCCESS;
}
