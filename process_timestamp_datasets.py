#!/usr/bin/env python3
"""
处理带时间戳的轨迹数据集
1. 确保原始数据集最多10万点
2. 生成降采样版本（5x, 10x, 20x, 40x）
"""

import pandas as pd
import os
from pathlib import Path

def process_dataset(input_file, output_dir, dataset_name, max_points=100000):
    """
    处理单个数据集
    
    Args:
        input_file: 输入文件路径
        output_dir: 输出目录
        dataset_name: 数据集名称（用于生成输出文件名）
        max_points: 最大点数
    """
    print(f"\n{'='*60}")
    print(f"处理数据集: {dataset_name}")
    print(f"{'='*60}")
    
    # 读取数据
    print(f"读取文件: {input_file}")
    df = pd.read_csv(input_file)
    original_count = len(df)
    print(f"原始点数: {original_count}")
    
    # 确保不超过max_points
    if original_count > max_points:
        print(f"截取前{max_points}个点...")
        df = df.head(max_points)
    
    actual_count = len(df)
    print(f"处理后点数: {actual_count}")
    
    # 保存原始数据集（如果需要）
    output_original = os.path.join(output_dir, f"{dataset_name}_with_timestamp.csv")
    df.to_csv(output_original, index=False)
    print(f"✓ 保存原始数据: {output_original} ({actual_count} 点)")
    
    # 生成降采样版本
    downsample_factors = [5, 10, 20, 40]
    
    for factor in downsample_factors:
        # 降采样：每factor个点取1个
        df_downsampled = df.iloc[::factor].copy()
        downsampled_count = len(df_downsampled)
        
        output_file = os.path.join(
            output_dir, 
            'downsampled',
            f"{dataset_name}_with_timestamp_downsample_{factor}x.csv"
        )
        
        df_downsampled.to_csv(output_file, index=False)
        print(f"✓ 降采样 {factor}x: {output_file} ({downsampled_count} 点)")
    
    print(f"\n数据集 {dataset_name} 处理完成！")
    return actual_count

def main():
    # 设置路径
    base_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    output_dir = base_dir
    downsampled_dir = os.path.join(output_dir, 'downsampled')
    
    # 确保降采样目录存在
    os.makedirs(downsampled_dir, exist_ok=True)
    
    # 数据集配置
    datasets = [
        {
            'name': 'Geolife_100k',
            'input': os.path.join(base_dir, 'Geolife_100k_with_timestamp.csv'),
            'max_points': 100000
        },
        {
            'name': 'Track_63530k',
            'input': os.path.join(base_dir, 'Track_with_timestamp.csv'),
            'max_points': 100000  # 保持原有63530点
        },
        {
            'name': 'Trajtory_100k',
            'input': os.path.join(base_dir, 'Trajtory_100k_with_timestamp.csv'),
            'max_points': 100000
        }
    ]
    
    print("="*60)
    print("轨迹数据集处理工具（带时间戳版本）")
    print("="*60)
    print(f"输出目录: {output_dir}")
    print(f"降采样目录: {downsampled_dir}")
    
    # 处理每个数据集
    results = {}
    for dataset in datasets:
        if not os.path.exists(dataset['input']):
            print(f"\n⚠ 警告: 文件不存在 {dataset['input']}，跳过")
            continue
        
        count = process_dataset(
            dataset['input'],
            output_dir,
            dataset['name'],
            dataset['max_points']
        )
        results[dataset['name']] = count
    
    # 打印总结
    print("\n" + "="*60)
    print("处理完成总结")
    print("="*60)
    for name, count in results.items():
        print(f"{name:20s}: {count:6d} 点")
        for factor in [5, 10, 20, 40]:
            downsampled_count = count // factor
            print(f"  └─ 降采样 {factor:2d}x: {downsampled_count:6d} 点")
    
    print("\n✓ 所有数据集处理完成！")

if __name__ == '__main__':
    main()

