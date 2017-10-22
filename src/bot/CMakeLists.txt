cmake_minimum_required(VERSION 2.8)
add_compile_options(-std=c++11)
file(GLOB quark_bot_SRC "*.cpp")
add_executable (quark_bot ${quark_bot_SRC} ../quark/logfile.cpp)
target_link_libraries (quark_bot LINK_PUBLIC  couchit jsonrpc_client imtjson pthread)
 
install(TARGETS quark_bot
        DESTINATION "bin"
        )   
        

