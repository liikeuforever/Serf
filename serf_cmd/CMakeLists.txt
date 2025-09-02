cmake_minimum_required(VERSION 3.10)

project(sfz)

# Set C++ standard version
set(CMAKE_CXX_STANDARD 17)

add_executable(sfz cmd_interface.cc)

target_include_directories(sfz PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(sfz PRIVATE serf)