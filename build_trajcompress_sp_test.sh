#!/bin/bash

echo "==============================================="
echo "编译 TrajCompress-SP 压缩测试程序"
echo "==============================================="

# 确保在项目根目录
if [ ! -f "CMakeLists.txt" ]; then
    echo "错误：请在项目根目录运行此脚本"
    exit 1
fi

# 创建临时编译目录
mkdir -p build_trajcompress_sp
cd build_trajcompress_sp

# 配置CMake以编译serf库
echo "步骤 1/3: 配置CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "CMake配置失败"
    exit 1
fi

# 编译serf库（如果需要）
echo "步骤 2/3: 编译serf库..."
make serf -j4 > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "警告：serf库编译失败，尝试继续..."
fi

cd ..

# 使用g++直接编译测试程序
echo "步骤 3/3: 编译测试程序..."
g++ -std=c++17 -O3 \
    -I./src \
    -o trajcompress_sp_test \
    trajcompress_sp_test.cc \
    src/compressor/trajcompress_sp_compressor.cc \
    src/compressor/serf_qt_compressor.cc \
    src/utils/output_bit_stream.cc \
    src/utils/input_bit_stream.cc \
    src/utils/elias_gamma_codec.cc \
    -lm

if [ $? -eq 0 ]; then
    echo "✓ 编译成功！可执行文件: trajcompress_sp_test"
    echo ""
    echo "==============================================="
    echo "运行测试程序"
    echo "==============================================="
    echo ""
    
    # 运行测试程序
    ./trajcompress_sp_test
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✓ 测试完成！"
    else
        echo ""
        echo "✗ 测试运行失败"
        exit 1
    fi
else
    echo "✗ 编译失败！"
    exit 1
fi

echo ""
echo "==============================================="
echo "使用说明"
echo "==============================================="
echo "直接运行: ./trajcompress_sp_test"
echo "指定参数: ./trajcompress_sp_test <数据集路径> <测试点数> <误差阈值>"
echo ""
echo "示例："
echo "  ./trajcompress_sp_test test/data_set/Geolife_100k_longitude_latitude.csv 50000 1e-5"
echo ""

