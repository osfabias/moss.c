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

  @file src/internal/log.h
  @brief Internal log macro
  @author Ilya Buravov (ilburale@gmail.com)
*/

#pragma once

#include <stdio.h>

#ifdef NDEBUG
#  define moss__info(...)
#else
/*
  @brief Prints info message in stdin.
  @param ... Arguments to be passed to printf.
  @note Expands only in debug builds.
*/
#  define moss__info(...) printf ("moss [info]: " __VA_ARGS__)
#endif

/*
  @brief Prints warning message in stderr.
  @param ... Arguments to be passed to printf.
  @note Expands only in debug builds.
*/
#define moss__warning(...) printf ("moss [warning]: " __VA_ARGS__);

/*
  @brief Prints error message in stderr.
  @param ... Arguments to be passed to printf.
  @note Expands only in debug builds.
*/
#define moss__error(...) fprintf (stderr, "moss [error]: " __VA_ARGS__);
