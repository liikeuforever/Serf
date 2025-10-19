#!/usr/bin/env python3
"""
为Geolife和Track数据集添加时间戳信息
"""

import os
import glob
from datetime import datetime, timedelta
import csv

def process_geolife_data(geolife_dir, output_file, max_points=100000):
    """
    处理Geolife数据集，提取经纬度和时间戳
    
    Geolife PLT文件格式（跳过前6行）：
    Latitude,Longitude,0,Altitude,Days,Date,Time
    """
    all_points = []
    
    # 查找所有.plt文件
    plt_files = glob.glob(os.path.join(geolife_dir, "**/*.plt"), recursive=True)
    plt_files.sort()
    
    print(f"找到 {len(plt_files)} 个Geolife轨迹文件")
    
    for plt_file in plt_files:
        if len(all_points) >= max_points:
            break
            
        try:
            with open(plt_file, 'r') as f:
                # 跳过前6行头部信息
                for _ in range(6):
                    f.readline()
                
                # 读取数据行
                for line in f:
                    if len(all_points) >= max_points:
                        break
                    
                    parts = line.strip().split(',')
                    if len(parts) >= 7:
                        latitude = parts[0]
                        longitude = parts[1]
                        date = parts[5]  # YYYY-MM-DD
                        time = parts[6]  # HH:MM:SS
                        
                        # 合并日期和时间
                        timestamp = f"{date} {time}"
                        
                        all_points.append({
                            'longitude': longitude,
                            'latitude': latitude,
                            'timestamp': timestamp
                        })
        except Exception as e:
            print(f"处理文件出错 {plt_file}: {e}")
            continue
    
    print(f"提取了 {len(all_points)} 个Geolife数据点")
    
    # 写入输出文件
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['longitude', 'latitude', 'timestamp'])
        for point in all_points:
            writer.writerow([point['longitude'], point['latitude'], point['timestamp']])
    
    print(f"✅ Geolife数据已保存到: {output_file}")
    return len(all_points)

def process_track_data(track_dir, output_file, max_points=70000):
    """
    处理Track数据集，提取经纬度和时间戳
    
    Track CSV文件格式：
    ,Date,Time,Latitude,Longitude,...
    """
    all_points = []
    
    # 查找所有CSV文件
    csv_files = glob.glob(os.path.join(track_dir, "**/*.csv"), recursive=True)
    csv_files.sort()
    
    print(f"找到 {len(csv_files)} 个Track轨迹文件")
    
    for csv_file in csv_files:
        if len(all_points) >= max_points:
            break
            
        try:
            with open(csv_file, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    if len(all_points) >= max_points:
                        break
                    
                    if 'Latitude' in row and 'Longitude' in row and 'Date' in row and 'Time' in row:
                        latitude = row['Latitude']
                        longitude = row['Longitude']
                        date = row['Date']  # M/D/YYYY
                        time = row['Time']  # HH:MM:SS
                        
                        # 转换日期格式为标准格式
                        try:
                            date_obj = datetime.strptime(date, '%m/%d/%Y')
                            formatted_date = date_obj.strftime('%Y-%m-%d')
                            timestamp = f"{formatted_date} {time}"
                        except:
                            timestamp = f"{date} {time}"
                        
                        all_points.append({
                            'longitude': longitude,
                            'latitude': latitude,
                            'timestamp': timestamp
                        })
        except Exception as e:
            print(f"处理文件出错 {csv_file}: {e}")
            continue
    
    print(f"提取了 {len(all_points)} 个Track数据点")
    
    # 写入输出文件
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['longitude', 'latitude', 'timestamp'])
        for point in all_points:
            writer.writerow([point['longitude'], point['latitude'], point['timestamp']])
    
    print(f"✅ Track数据已保存到: {output_file}")
    return len(all_points)

def main():
    print("=" * 80)
    print("为Geolife和Track数据集添加时间戳")
    print("=" * 80)
    
    # 路径配置
    geolife_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife"
    track_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Track/Processed Data"
    output_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    
    # 处理Geolife数据集
    print("\n" + "-" * 80)
    print("处理Geolife数据集...")
    print("-" * 80)
    geolife_output = os.path.join(output_dir, "Geolife_100k_with_timestamp.csv")
    geolife_count = process_geolife_data(geolife_dir, geolife_output, max_points=100000)
    
    # 处理Track数据集
    print("\n" + "-" * 80)
    print("处理Track数据集...")
    print("-" * 80)
    track_output = os.path.join(output_dir, "Track_with_timestamp.csv")
    track_count = process_track_data(track_dir, track_output, max_points=70000)
    
    # 汇总
    print("\n" + "=" * 80)
    print("✅ 处理完成！")
    print("=" * 80)
    print(f"Geolife: {geolife_count} 点 -> {geolife_output}")
    print(f"Track:   {track_count} 点 -> {track_output}")
    
    # 显示样例数据
    print("\n" + "=" * 80)
    print("样例数据预览")
    print("=" * 80)
    
    print("\nGeolife (前5行):")
    with open(geolife_output, 'r') as f:
        for i, line in enumerate(f):
            if i < 6:
                print(f"  {line.rstrip()}")
    
    print("\nTrack (前5行):")
    with open(track_output, 'r') as f:
        for i, line in enumerate(f):
            if i < 6:
                print(f"  {line.rstrip()}")

if __name__ == "__main__":
    main()


