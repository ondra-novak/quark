cmake_minimum_required(VERSION 2.8)
add_compile_options(-std=c++14)
file(GLOB quark_purge_SRC "*.cpp")
add_executable (quark_purge ${quark_purge_SRC})
target_link_libraries (quark_purge LINK_PUBLIC quarklib couchit imtjson pthread)
 
install(TARGETS quark_purge 
        DESTINATION "bin"
        ) 
 
 
   