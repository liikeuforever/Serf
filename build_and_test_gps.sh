#!/bin/bash

# GPS轨迹压缩算法编译和测试脚本

echo "=== GPS轨迹压缩算法编译和测试 ==="

# 检查是否在正确的目录
if [ ! -f "CMakeLists.txt" ]; then
    echo "错误：请在项目根目录运行此脚本"
    exit 1
fi

# 创建并进入build目录
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

echo "开始编译..."

# 配置CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "CMake配置失败"
    exit 1
fi

# 编译项目
make -j4

if [ $? -ne 0 ]; then
    echo "编译失败"
    exit 1
fi

echo "编译成功！"

# 运行GPS测试
echo ""
echo "=== 运行GPS轨迹压缩测试 ==="

if [ -f "test/serf_qt_gps_test" ]; then
    echo "运行单元测试..."
    ./test/serf_qt_gps_test
    
    if [ $? -eq 0 ]; then
        echo "✓ 单元测试通过"
    else
        echo "✗ 单元测试失败"
    fi
else
    echo "警告：未找到GPS测试程序"
fi

# 运行简单演示
echo ""
echo "=== 运行GPS轨迹压缩演示 ==="

if [ -f "demo/gps_simple_test" ]; then
    echo "运行简单演示..."
    ./demo/gps_simple_test
    
    if [ $? -eq 0 ]; then
        echo "✓ 演示程序运行成功"
    else
        echo "✗ 演示程序运行失败"
    fi
else
    echo "警告：未找到GPS演示程序"
fi

echo ""
echo "=== 测试完成 ==="
echo "如果要运行完整的演示程序，请执行："
echo "  ./demo/gps_trajectory_compression_demo ../test/data_set/Geolife_100k_longitude_latitude.csv"
