project(cloudos-installer)
cmake_minimum_required(VERSION 2.8)
set( CMAKE_VERBOSE_MAKEFILE on )
set( CMAKE_CXX_FLAGS -std=c++0x )
#set( CMAKE_CXX_FLAGS -fsanitize=address )
set( CMAKE_BUILD_TYPE debug )

add_executable(cloudos-installer main.cpp
                                 src/Installer.cpp
                )
target_link_libraries(cloudos-installer cloudos
                                        boost_filesystem
					boost_system
					yui
					protobuf
					pthread
                        )

install(TARGETS cloudos-installer RUNTIME DESTINATION bin)

