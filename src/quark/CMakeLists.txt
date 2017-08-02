cmake_minimum_required(VERSION 2.8)
add_compile_options(-std=c++14)
file(GLOB quark_SRC "*.cpp")
set(designs designdocs/index.design.cpp designdocs/trades.design.cpp designdocs/positions.design.cpp)
add_custom_command(OUTPUT ${designs} COMMAND $(MAKE) -C designdocs all DEPENDS couchcpp) 
add_custom_target(buildDesigns COMMAND $(MAKE) -C designdocs all  DEPENDS couchcpp)
add_custom_target(cleanDesigns COMMAND $(MAKE) -C designdocs clean-designs)
add_executable (quark ${quark_SRC} ${designs})
add_dependencies(quark buildDesigns)  
target_link_libraries (quark LINK_PUBLIC quarklib couchit imtjson pthread)
 
install(TARGETS quark 
        DESTINATION "bin"
        ) 

install(FILES "../../conf/quark.conf" DESTINATION "etc/quark") 
 
 