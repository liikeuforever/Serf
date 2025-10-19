#!/usr/bin/env python3
"""
基于现有的经纬度文件添加时间戳信息
通过在原始数据中查找匹配的经纬度来获取对应的时间戳
"""

import os
import glob
import csv
from collections import defaultdict

def build_geolife_timestamp_index(geolife_dir):
    """
    构建Geolife数据的经纬度到时间戳的映射
    """
    print("构建Geolife时间戳索引...")
    timestamp_map = {}
    
    plt_files = glob.glob(os.path.join(geolife_dir, "**/*.plt"), recursive=True)
    
    for plt_file in plt_files:
        try:
            with open(plt_file, 'r') as f:
                # 跳过前6行
                for _ in range(6):
                    f.readline()
                
                for line in f:
                    parts = line.strip().split(',')
                    if len(parts) >= 7:
                        latitude = parts[0]
                        longitude = parts[1]
                        date = parts[5]
                        time = parts[6]
                        
                        key = f"{longitude},{latitude}"
                        timestamp = f"{date} {time}"
                        
                        # 如果已存在，保留第一次出现的时间戳
                        if key not in timestamp_map:
                            timestamp_map[key] = timestamp
        except Exception as e:
            continue
    
    print(f"  索引了 {len(timestamp_map)} 个唯一坐标点")
    return timestamp_map

def build_track_timestamp_index(track_dir):
    """
    构建Track数据的经纬度到时间戳的映射
    """
    print("构建Track时间戳索引...")
    timestamp_map = {}
    
    csv_files = glob.glob(os.path.join(track_dir, "**/*.csv"), recursive=True)
    
    for csv_file in csv_files:
        try:
            with open(csv_file, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    if all(k in row for k in ['Latitude', 'Longitude', 'Date', 'Time']):
                        latitude = row['Latitude']
                        longitude = row['Longitude']
                        date = row['Date']
                        time = row['Time']
                        
                        key = f"{longitude},{latitude}"
                        
                        # 转换日期格式
                        from datetime import datetime
                        try:
                            date_obj = datetime.strptime(date, '%m/%d/%Y')
                            formatted_date = date_obj.strftime('%Y-%m-%d')
                            timestamp = f"{formatted_date} {time}"
                        except:
                            timestamp = f"{date} {time}"
                        
                        if key not in timestamp_map:
                            timestamp_map[key] = timestamp
        except Exception as e:
            continue
    
    print(f"  索引了 {len(timestamp_map)} 个唯一坐标点")
    return timestamp_map

def add_timestamps_to_file(input_file, output_file, timestamp_map, dataset_name):
    """
    为经纬度文件添加时间戳
    """
    print(f"\n处理 {dataset_name} 数据集...")
    
    with open(input_file, 'r') as f:
        lines = f.readlines()
    
    total = len(lines)
    matched = 0
    unmatched = 0
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['longitude', 'latitude', 'timestamp'])
        
        for i, line in enumerate(lines):
            lon, lat = line.strip().split(',')
            key = f"{lon},{lat}"
            
            if key in timestamp_map:
                timestamp = timestamp_map[key]
                matched += 1
            else:
                # 如果找不到精确匹配，使用估算的时间戳
                # 假设采样率为5秒（Geolife）或2秒（Track）
                if dataset_name == "Geolife":
                    # Geolife数据集，假设起始时间和5秒间隔
                    from datetime import datetime, timedelta
                    base_time = datetime(2008, 10, 23, 2, 53, 0)
                    estimated_time = base_time + timedelta(seconds=i * 5)
                    timestamp = estimated_time.strftime('%Y-%m-%d %H:%M:%S')
                else:
                    # Track数据集，假设起始时间和2秒间隔
                    from datetime import datetime, timedelta
                    base_time = datetime(2022, 5, 4, 16, 1, 0)
                    estimated_time = base_time + timedelta(seconds=i * 2)
                    timestamp = estimated_time.strftime('%Y-%m-%d %H:%M:%S')
                unmatched += 1
            
            writer.writerow([lon, lat, timestamp])
    
    print(f"  总计: {total} 点")
    print(f"  匹配: {matched} 点 ({matched/total*100:.1f}%)")
    print(f"  估算: {unmatched} 点 ({unmatched/total*100:.1f}%)")
    print(f"  ✅ 已保存到: {output_file}")

def main():
    print("=" * 80)
    print("为现有数据文件添加时间戳")
    print("=" * 80)
    
    # 路径配置
    geolife_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife"
    track_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Track/Processed Data"
    
    # 构建时间戳索引
    print("\n" + "-" * 80)
    geolife_ts_map = build_geolife_timestamp_index(geolife_dir)
    track_ts_map = build_track_timestamp_index(track_dir)
    
    # 为Geolife添加时间戳
    print("\n" + "-" * 80)
    add_timestamps_to_file(
        "test/data_set/Geolife_100k_longitude_latitude.csv",
        "test/data_set/Geolife_100k_with_timestamp.csv",
        geolife_ts_map,
        "Geolife"
    )
    
    # 为Track添加时间戳
    print("\n" + "-" * 80)
    add_timestamps_to_file(
        "test/data_set/Track_63530k_longitude_latitude.csv",
        "test/data_set/Track_with_timestamp.csv",
        track_ts_map,
        "Track"
    )
    
    # 显示样例
    print("\n" + "=" * 80)
    print("样例数据预览")
    print("=" * 80)
    
    print("\nGeolife (前5行):")
    with open("test/data_set/Geolife_100k_with_timestamp.csv", 'r') as f:
        for i, line in enumerate(f):
            if i < 6:
                print(f"  {line.rstrip()}")
    
    print("\nTrack (前5行):")
    with open("test/data_set/Track_with_timestamp.csv", 'r') as f:
        for i, line in enumerate(f):
            if i < 6:
                print(f"  {line.rstrip()}")
    
    print("\n" + "=" * 80)
    print("✅ 完成！")
    print("=" * 80)

if __name__ == "__main__":
    main()


