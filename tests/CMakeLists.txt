#=============================================================================
# TESTS CONFIGURATION
#=============================================================================

# Add testing dependencies
add_subdirectory(vendor)

# Find all test source files
file(GLOB TEST_SOURCES "*.c")

# Create one executable per test source and wire it up
set(_ALL_TEST_TARGETS "")
foreach(_SRC IN LISTS TEST_SOURCES)
  get_filename_component(_NAME "${_SRC}" NAME_WE)

  add_executable(${_NAME} "${_SRC}")

  # Link with the main library and Check
  target_link_libraries(${_NAME} PRIVATE
    moss
    Check::check
  )

  # Apply same compile options as the main library
  if(MOSS_COMPILE_OPTIONS)
    target_compile_options(${_NAME} PRIVATE ${MOSS_COMPILE_OPTIONS})
  endif()

  # Add test to CTest
  add_test(NAME ${_NAME} COMMAND ${_NAME})

  list(APPEND _ALL_TEST_TARGETS ${_NAME})
endforeach()

# Enable code coverage for tests by default (when Debug build)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    # Enable coverage for test executables
    foreach(_TGT IN LISTS _ALL_TEST_TARGETS)
      target_compile_options(${_TGT} PRIVATE --coverage)
      target_link_libraries(${_TGT} PRIVATE --coverage)
    endforeach()
    
    # Enable coverage for the library target to measure coverage
    target_compile_options(moss PRIVATE --coverage)
    target_link_libraries(moss PRIVATE --coverage)
    
    message(STATUS "Code coverage enabled for tests and library")
  endif()
endif()

