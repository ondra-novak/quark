cmake_minimum_required(VERSION 2.8)
add_compile_options(-std=c++11)
add_executable (quark main.cpp) 
target_link_libraries (quark LINK_PUBLIC quarklib imtjson) 