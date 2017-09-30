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
add_subdirectory (src/quark_init) 
add_subdirectory (src/rpc)
add_subdirectory (src/jsonrpc_netcat)
add_subdirectory (src/bot)
add_subdirectory (src/purgedb)
add_dependencies(quark couchcpp)  

# set_target_properties(couchcpp  jsonrpc_netcat  quark  quark_bot  quark_init  quark_rpc PROPERTIES LINK_FLAGS_RELEASE "-static" LINK_FLAGS_RELWITHDEBINFO "-static")
