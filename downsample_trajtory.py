#!/usr/bin/env python3
"""
对Trajtory数据集进行降采样
采样率: 1/5, 1/10, 1/20, 1/40
"""

import pandas as pd
import os

def downsample_csv(input_filepath, output_dir, downsample_rates):
    """
    对CSV文件进行降采样
    
    Args:
        input_filepath: 输入CSV文件路径
        output_dir: 输出目录
        downsample_rates: 降采样率列表 [5, 10, 20, 40]
    """
    print(f"处理文件: {input_filepath}")
    
    # 读取数据
    df = pd.read_csv(input_filepath)
    base_name = os.path.basename(input_filepath).replace('.csv', '')
    
    print(f"原始数据点数: {len(df):,}")
    print(f"原始数据列: {list(df.columns)}")
    
    # 对每个采样率进行降采样
    for rate in downsample_rates:
        # 每rate个点取一个
        sampled_df = df.iloc[::rate, :].copy()
        
        # 生成输出文件名
        output_filename = f"{base_name}_downsample_{rate}x.csv"
        output_filepath = os.path.join(output_dir, output_filename)
        
        # 保存文件（保留表头）
        sampled_df.to_csv(output_filepath, index=False)
        
        print(f"  采样率 1/{rate}: {len(sampled_df):,} 点 -> {output_filepath}")
    
    print()

def main():
    print("=" * 80)
    print("Trajtory数据集降采样")
    print("=" * 80)
    print()
    
    # 配置
    input_file = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Trajtory_100k_with_timestamp.csv"
    output_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/downsampled"
    downsample_rates = [5, 10, 20, 40]
    
    # 确保输出目录存在
    os.makedirs(output_dir, exist_ok=True)
    
    # 降采样
    downsample_csv(input_file, output_dir, downsample_rates)
    
    # 显示输出目录内容
    print("=" * 80)
    print("✅ 降采样完成！")
    print("=" * 80)
    print(f"\n输出目录: {output_dir}")
    print("\nTrajtory降采样文件:")
    
    trajtory_files = [f for f in os.listdir(output_dir) if 'Trajtory' in f and 'downsample' in f]
    trajtory_files.sort()
    
    for f in trajtory_files:
        filepath = os.path.join(output_dir, f)
        size_mb = os.path.getsize(filepath) / 1024 / 1024
        print(f"  • {f} ({size_mb:.2f} MB)")
    
    # 显示所有降采样文件的汇总
    print("\n所有数据集降采样文件汇总:")
    all_files = sorted([f for f in os.listdir(output_dir) if f.endswith('.csv')])
    
    datasets = {}
    for f in all_files:
        if 'Geolife' in f:
            dataset_name = 'Geolife'
        elif 'Track' in f and 'Trajtory' not in f:
            dataset_name = 'Track'
        elif 'Trajtory' in f:
            dataset_name = 'Trajtory'
        else:
            continue
        
        if dataset_name not in datasets:
            datasets[dataset_name] = []
        datasets[dataset_name].append(f)
    
    for dataset_name in ['Geolife', 'Track', 'Trajtory']:
        if dataset_name in datasets:
            print(f"\n  【{dataset_name}】")
            for f in sorted(datasets[dataset_name]):
                print(f"    - {f}")

if __name__ == "__main__":
    main()

