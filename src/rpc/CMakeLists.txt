cmake_minimum_required(VERSION 2.8)
add_compile_options(-std=c++11)
file(GLOB rpc_SRC "*.cpp")
add_executable (quark_rpc ${rpc_SRC})
target_link_libraries (quark_rpc LINK_PUBLIC quarklib couchit imtjson pthread)
 
install(TARGETS quark_rpc
        DESTINATION "bin"
        )   
        
install(FILES "../../conf/rpc.conf" DESTINATION "etc/quark") 
install(FILES "../../conf/quark.xinetd" DESTINATION "/etc/xinetd.d" RENAME "quark")
install(CODE "MESSAGE(\"Please restart xinetd: service xinetd restart\")")
