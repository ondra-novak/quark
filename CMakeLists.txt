cmake_minimum_required(VERSION 2.8)
project (quark)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)

include_directories(BEFORE src/imtjson/src src/couchit/src src)


add_subdirectory (src/quark)
add_subdirectory (src/couchcpp)
add_dependencies(quark couchcpp) 
add_subdirectory (src/imtjson/src/imtjson)
add_subdirectory (src/couchit/src/couchit)
add_subdirectory (src/quark_lib)
add_subdirectory (src/quark_test)
add_compile_options(-std=c++11)
