cmake_minimum_required(VERSION 2.8)
project (quark)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)

include_directories(BEFORE src/imtjson/src src/couchit/src src)
add_compile_options(-std=c++11)


add_subdirectory (src/couchcpp EXCLUDE_FROM_ALL)
add_subdirectory (src/imtjson/src/imtjson EXCLUDE_FROM_ALL)
add_subdirectory (src/couchit/src/couchit EXCLUDE_FROM_ALL)
add_subdirectory (src/quark_lib EXCLUDE_FROM_ALL)
add_subdirectory (src/quark_test EXCLUDE_FROM_ALL)


add_subdirectory (src/quark)
add_subdirectory (src/rpc)
add_subdirectory (src/create)
add_dependencies(quark couchcpp) 
 