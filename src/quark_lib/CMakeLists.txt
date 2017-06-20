cmake_minimum_required(VERSION 2.8) 
add_compile_options(-std=c++11 -fPIC)
file(GLOB quarklib_SRC "*.cpp")
file(GLOB quarklib_HDR "*.h" "*.tcc")
add_library (quarklib ${quarklib_SRC})

install(TARGETS quarklib
        DESTINATION "lib"
        ) 
INSTALL(FILES ${quarklib_HDR}
        DESTINATION "include/quarklib" 
        )    