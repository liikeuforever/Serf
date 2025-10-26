#!/bin/bash
echo "测试所有降采样数据集（排除原始数据集）"
echo "========================================"

datasets=(
    "Geolife_100k_with_timestamp_downsample_5x.csv,20000"
    "Geolife_100k_with_timestamp_downsample_10x.csv,10000"
    "Geolife_100k_with_timestamp_downsample_20x.csv,5000"
    "Geolife_100k_with_timestamp_downsample_40x.csv,2500"
    "Track_63530k_with_timestamp_downsample_5x.csv,12706"
    "Track_63530k_with_timestamp_downsample_10x.csv,6353"
    "Track_63530k_with_timestamp_downsample_20x.csv,3177"
    "Track_63530k_with_timestamp_downsample_40x.csv,1589"
)

for dataset in "${datasets[@]}"; do
    IFS=',' read -r file points <<< "$dataset"
    echo ""
    echo "测试: $file ($points 个点)"
    timeout 180 ./trajcompress_sp_test single "test/data_set/data_set_with_timestamp/$file" "$points" 1e-5 2>&1 | grep -E "(成功加载|TrajSP|Simple|压缩比|✓|✗)" | head -15
done

echo ""
echo "所有测试完成"
