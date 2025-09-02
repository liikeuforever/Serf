cmake_minimum_required(VERSION 3.15)

project(Sprintz)

# Set C++ standard version
set(CMAKE_CXX_STANDARD 11)

# -O3 Optimization for release version
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Set parallel compilation level as 4
set(CMAKE_BUILD_PARALLEL_LEVEL 4)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Scan and collect all source code file
file(GLOB_RECURSE LIB_SRC *.cpp *.cc)

# Check if we're on x86/x64 architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i386|i686")
    # On x86, include all files
    set(SPRINTZ_SOURCES ${LIB_SRC})
else()
    # On non-x86 architectures, exclude files with heavy AVX2 usage
    set(SPRINTZ_SOURCES)
    foreach(SRC_FILE ${LIB_SRC})
        get_filename_component(FILENAME ${SRC_FILE} NAME)
        # Skip files that heavily use AVX2 instructions
        if(NOT FILENAME MATCHES "sprintz_xff.*|sprintz_delta.*|delta\\.cpp|predict\\.cpp")
            list(APPEND SPRINTZ_SOURCES ${SRC_FILE})
        endif()
    endforeach()
endif()

add_library(sprintz STATIC ${SPRINTZ_SOURCES})

# Only apply x86-specific optimizations on x86/x64 architectures
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i386|i686")
    target_compile_options(sprintz PRIVATE -march=native -mavx2 -mbmi -mbmi2)
else()
    # For ARM and other architectures, use generic optimizations
    target_compile_options(sprintz PRIVATE -O3)
endif()
