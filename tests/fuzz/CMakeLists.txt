# Only built with -DFH_FUZZ=ON (and clang): see the root CMakeLists.txt.
add_executable(fuzz_handler
  fuzz_handler.cpp)
target_link_libraries(fuzz_handler PRIVATE fh_nasdaq)
target_compile_options(fuzz_handler PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_handler PRIVATE -fsanitize=fuzzer,address,undefined)
