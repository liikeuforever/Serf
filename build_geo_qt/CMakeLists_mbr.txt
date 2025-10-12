cmake_minimum_required(VERSION 3.10)
project(MbrGeoQtTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(src)

# 源文件
set(UTILS_SOURCES
    src/utils/geohash_codec.cc
    src/utils/mbr_geohash_codec.cc
    src/utils/leb128_codec.cc
    src/utils/elias_gamma_codec.cc
    src/utils/input_bit_stream.cc
    src/utils/output_bit_stream.cc
)

set(COMPRESSOR_SOURCES
    src/compressor/geo_qt_compressor.cc
    src/compressor/mbr_geo_qt_compressor.cc
    src/compressor/serf_qt_compressor.cc
)

set(DECOMPRESSOR_SOURCES
    src/decompressor/geo_qt_decompressor.cc
    src/decompressor/mbr_geo_qt_decompressor.cc
    src/decompressor/serf_qt_decompressor.cc
)

# MBR 优化版测试程序
add_executable(mbr_geo_qt_test
    mbr_geo_qt_test.cc
    ${UTILS_SOURCES}
    ${COMPRESSOR_SOURCES}
    ${DECOMPRESSOR_SOURCES}
)

# 编译选项
target_compile_options(mbr_geo_qt_test PRIVATE -O2 -Wall)

# 链接数学库
target_link_libraries(mbr_geo_qt_test m)
