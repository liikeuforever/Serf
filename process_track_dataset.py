#!/usr/bin/env python3
"""
处理Track数据集，合并所有轨迹文件并生成经纬度格式
参考Geolife_100k_longitude_latitude.csv的格式
"""

import os
import csv
import glob
import random
from pathlib import Path

def process_track_dataset():
    """处理Track数据集，合并所有轨迹文件"""
    
    # 输入和输出路径
    input_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Track/Processed Data"
    output_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    
    # 创建输出目录
    os.makedirs(output_dir, exist_ok=True)
    
    # 收集所有CSV文件
    csv_files = []
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith('.csv'):
                csv_files.append(os.path.join(root, file))
    
    print(f"找到 {len(csv_files)} 个轨迹文件")
    
    # 存储所有轨迹点
    all_points = []
    
    # 处理每个CSV文件
    for i, csv_file in enumerate(csv_files):
        print(f"处理文件 {i+1}/{len(csv_files)}: {os.path.basename(csv_file)}")
        
        try:
            with open(csv_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                
                for row in reader:
                    try:
                        # 提取经纬度
                        longitude = float(row['Longitude'])
                        latitude = float(row['Latitude'])
                        
                        # 检查数据有效性
                        if -180 <= longitude <= 180 and -90 <= latitude <= 90:
                            all_points.append((longitude, latitude))
                            
                    except (ValueError, KeyError) as e:
                        # 跳过无效行
                        continue
                        
        except Exception as e:
            print(f"  警告: 无法处理文件 {csv_file}: {e}")
            continue
    
    print(f"总共收集到 {len(all_points)} 个有效轨迹点")
    
    if len(all_points) == 0:
        print("错误: 没有找到有效的轨迹点")
        return
    
    # 随机采样100k个点（如果数据足够）
    if len(all_points) > 100000:
        print("数据点超过100k，随机采样100k个点")
        all_points = random.sample(all_points, 100000)
    else:
        print(f"使用全部 {len(all_points)} 个数据点")
    
    # 生成输出文件名
    output_file = os.path.join(output_dir, f"Track_{len(all_points)}k_longitude_latitude.csv")
    
    # 写入合并后的数据
    print(f"保存到: {output_file}")
    with open(output_file, 'w', newline='') as f:
        for lon, lat in all_points:
            f.write(f"{lon},{lat}\n")
    
    print(f"✅ 成功处理Track数据集!")
    print(f"   输出文件: {output_file}")
    print(f"   数据点数: {len(all_points)}")
    
    # 显示前几个点作为验证
    print("\n前5个数据点:")
    for i, (lon, lat) in enumerate(all_points[:5]):
        print(f"  {i+1}: {lon}, {lat}")

if __name__ == "__main__":
    process_track_dataset()

