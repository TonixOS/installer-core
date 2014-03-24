project(cloudos-installer)
cmake_minimum_required(VERSION 2.8)
set( CMAKE_VERBOSE_MAKEFILE on )
#set( CMAKE_CXX_FLAGS "-fsanitize=address -std=c++11" )
set( CMAKE_CXX_FLAGS "-std=c++11" )
set( CMAKE_BUILD_TYPE debug )
add_definitions(-DBOOST_FILESYSTEM_NO_DEPRECATED)
add_definitions(-DBOOST_ALL_DYN_LINK)

add_executable(cloudos-installer main.cpp
                                 src/Installer.cpp
                )
target_link_libraries(cloudos-installer cloudos
                                        boost_filesystem
					boost_system
					boost_log
					boost_log_setup
					boost_thread
					boost_regex
					yui
					protobuf
					pthread
                        )

install(TARGETS cloudos-installer RUNTIME DESTINATION bin)

