project(zoe)

cmake_minimum_required(VERSION 3.15)

set(cc_directory ${CMAKE_SOURCE_DIR}/../../ChakraCore)
set(cc_build_directory ${cc_directory}/Build/VcBuild/bin/x64_debug)

include_directories(
  ${cc_directory}/lib/jsrt
)

add_executable(
  zoe
  main.cpp
)

target_link_libraries(
  zoe
  ${cc_build_directory}/ChakraCore.lib
)

configure_file(
  ${cc_build_directory}/ChakraCore.dll
  ${CMAKE_CURRENT_BINARY_DIR}/ChakraCore.dll
  COPYONLY
)
