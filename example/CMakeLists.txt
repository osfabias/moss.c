add_executable(example main.c)
target_link_libraries(example PRIVATE moss)
target_compile_options(example PRIVATE ${MOSS_COMPILE_OPTIONS})
