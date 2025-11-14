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

  @file include/moss/apidef.h
  @brief API declaration macro for C++ compatibility
  @author Ilya Buravov (ilburale@gmail.com)
  @details This header provides the @ref __MOSS_API__ macro for declaring
           public library functions with proper C linkage when used from C++.
*/

#pragma once

/*
  @brief API declaration macro for public library functions
  @details This macro ensures that library functions can be called from C++
           code without name mangling issues. When included in C++ code, it
           expands to `extern "C"` to provide C linkage. In C code, it expands
           to `extern`.

  @par Behavior:
  - In C++ code: expands to `extern "C"`
  - In C code: expands to `extern`

  @par Usage:
  Place this macro before the return type of any function that is part of the
  public library API. It should be used in both header file declarations and
  source file definitions.

  @par Example:
  @code
    // In header file (include/moss/main.h):
    __MOSS_API__ moss_status_t moss_init(void);
    __MOSS_API__ moss_status_t moss_process_data(
        const void* data, size_t size, moss_callback_t callback, void* user_data
    );

    // In source file (src/main.c):
    __MOSS_API__ moss_status_t moss_init(void) {
        return MOSS_SUCCESS;
    }
  @endcode

  @note Use this macro for all public API functions to ensure C++ compatibility.
  @note The macro is a no-op in C code but essential for C++ interoperability.
*/
#ifdef __cplusplus
#  define __MOSS_API__ extern "C"
#else
#  define __MOSS_API__ extern
#endif
