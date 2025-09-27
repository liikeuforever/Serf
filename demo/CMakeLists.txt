cmake_minimum_required(VERSION 3.15)

project(SerfDemo)

# Set C++ standard version
set(CMAKE_CXX_STANDARD 17)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# GPS轨迹压缩演示程序
add_executable(gps_trajectory_compression_demo gps_trajectory_compression_demo.cc)
target_link_libraries(gps_trajectory_compression_demo PRIVATE serf)

# GPS轨迹压缩简单测试程序
add_executable(gps_simple_test gps_simple_test.cc)
target_link_libraries(gps_simple_test PRIVATE serf)

# 原有的轨迹演示程序（如果存在）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/trajectory_demo.cc)
    add_executable(trajectory_demo trajectory_demo.cc)
    target_link_libraries(trajectory_demo PRIVATE serf)
endif()

# 轨迹压缩演示程序（如果存在）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/trajectory_compression_demo.cc)
    add_executable(trajectory_compression_demo trajectory_compression_demo.cc)
    target_link_libraries(trajectory_compression_demo PRIVATE serf)
endif()
