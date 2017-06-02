cmake_minimum_required(VERSION 2.8)
project (quark)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)
add_subdirectory (src/quark_lib)
add_subdirectory (src/quark_test)
add_subdirectory (src/quark)
add_compile_options(-std=c++11)

