#!/bin/bash

echo "编译空间填充曲线压缩测试程序..."

# 创建临时编译目录
mkdir -p build_sfc_test
cd build_sfc_test

# 配置CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译新添加的源文件
make -j$(nproc)

# 编译测试程序
g++ -std=c++17 -O3 \
    -I../src \
    -o ../space_filling_curve_test \
    ../space_filling_curve_compression_test.cc \
    ../src/compressor/space_filling_curve.cc \
    ../src/compressor/sfc_compressor.cc \
    ../src/utils/output_bit_stream.cc \
    ../src/utils/elias_gamma_codec.cc \
    ../src/utils/zig_zag_codec.cc \
    ../src/utils/double.cc

if [ $? -eq 0 ]; then
    echo "编译成功! 可执行文件: space_filling_curve_test"
    cd ..
    echo "运行测试程序..."
    ./space_filling_curve_test
else
    echo "编译失败!"
    cd ..
    exit 1
fi


