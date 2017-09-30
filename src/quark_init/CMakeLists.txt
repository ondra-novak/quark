cmake_minimum_required(VERSION 2.8)
add_compile_options(-std=c++14)
file(GLOB quark_init_SRC "*.cpp")
add_executable (quark_init ${quark_init_SRC})
target_link_libraries (quark_init LINK_PUBLIC quarklib couchit imtjson pthread)
 
install(TARGETS quark_init 
        DESTINATION "bin"
        ) 
 
 
   