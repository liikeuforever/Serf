#!/bin/bash

# 测试Trajtory数据集的所有降采样版本

echo "================================================================================"
echo "Trajtory数据集降采样压缩测试"
echo "================================================================================"
echo ""

# 确保编译最新版本
echo "编译测试程序..."
g++ -std=c++17 -O3 -I./src -o trajcompress_sp_test trajcompress_sp_test.cc \
    src/compressor/trajcompress_sp_compressor.cc \
    src/compressor/serf_qt_compressor.cc \
    src/utils/output_bit_stream.cc \
    src/utils/input_bit_stream.cc \
    src/utils/elias_gamma_codec.cc \
    src/utils/elias_delta_codec.cc \
    -lm

if [ $? -ne 0 ]; then
    echo "编译失败！"
    exit 1
fi

echo "✅ 编译成功"
echo ""

DATA_DIR="./test/data_set"
DOWNSAMPLED_DIR="${DATA_DIR}/downsampled"
EPSILON="1e-5"

# 测试配置
declare -a DATASETS=(
    "${DATA_DIR}/Trajtory_97k_longitude_latitude.csv|97009|原始 (1/1)"
    "${DOWNSAMPLED_DIR}/Trajtory_97k_longitude_latitude_downsample_5x.csv|20000|降采样 (1/5)"
    "${DOWNSAMPLED_DIR}/Trajtory_97k_longitude_latitude_downsample_10x.csv|10000|降采样 (1/10)"
    "${DOWNSAMPLED_DIR}/Trajtory_97k_longitude_latitude_downsample_20x.csv|5000|降采样 (1/20)"
    "${DOWNSAMPLED_DIR}/Trajtory_97k_longitude_latitude_downsample_40x.csv|3000|降采样 (1/40)"
)

# 运行每个测试
for dataset_info in "${DATASETS[@]}"; do
    IFS='|' read -r filepath max_points label <<< "$dataset_info"
    
    echo "================================================================================"
    echo "测试: Trajtory ${label}"
    echo "================================================================================"
    echo "文件: $(basename $filepath)"
    echo "测试点数上限: ${max_points}"
    echo ""
    
    # 运行测试
    ./trajcompress_sp_test "${filepath}" "${max_points}" "${EPSILON}"
    
    echo ""
    echo ""
done

echo "================================================================================"
echo "✅ 所有测试完成！"
echo "================================================================================"
echo ""
echo "CSV结果文件已生成在当前目录，文件名格式："
echo "  test_results_Trajtory_97k_longitude_latitude_<点数>_<时间戳>.csv"
echo ""

