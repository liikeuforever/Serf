#!/usr/bin/env python3
"""
处理Trajtory数据集，提取10万个点，保留经纬度和时间戳
"""

import os
import glob
import csv
from datetime import datetime

def process_trajtory_data(trajtory_dir, output_file, max_points=100000):
    """
    处理Trajtory数据集
    
    CSV文件格式：
    time,latitude,longitude,altitude,...
    """
    all_points = []
    
    # 查找所有CSV文件
    csv_files = glob.glob(os.path.join(trajtory_dir, "*.csv"))
    csv_files.sort()
    
    print(f"找到 {len(csv_files)} 个Trajtory轨迹文件")
    
    for csv_file in csv_files:
        if len(all_points) >= max_points:
            break
            
        try:
            with open(csv_file, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    if len(all_points) >= max_points:
                        break
                    
                    if all(k in row for k in ['time', 'latitude', 'longitude']):
                        time_str = row['time']
                        latitude = row['latitude']
                        longitude = row['longitude']
                        
                        all_points.append({
                            'longitude': longitude,
                            'latitude': latitude,
                            'timestamp': time_str
                        })
        except Exception as e:
            print(f"处理文件出错 {os.path.basename(csv_file)}: {e}")
            continue
    
    print(f"\n提取了 {len(all_points)} 个Trajtory数据点")
    
    # 标准化时间戳格式（移除时区信息）
    print("标准化时间戳格式...")
    for point in all_points:
        # 移除时区信息 (+00:00)
        timestamp = point['timestamp']
        if '+' in timestamp:
            timestamp = timestamp.split('+')[0].strip()
        elif timestamp.endswith('Z'):
            timestamp = timestamp[:-1].strip()
        point['timestamp'] = timestamp
    
    # 按时间戳排序
    print("按时间戳排序数据...")
    try:
        all_points.sort(key=lambda x: datetime.strptime(x['timestamp'], '%Y-%m-%d %H:%M:%S'))
        print("✅ 排序完成")
    except Exception as e:
        print(f"⚠️ 排序失败: {e}，保持原始顺序")
    
    # 写入输出文件
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['longitude', 'latitude', 'timestamp'])
        for point in all_points:
            writer.writerow([point['longitude'], point['latitude'], point['timestamp']])
    
    print(f"✅ 数据已保存到: {output_file}")
    
    # 统计时间范围
    if len(all_points) > 0:
        print(f"\n数据时间范围:")
        print(f"  起始: {all_points[0]['timestamp']}")
        print(f"  结束: {all_points[-1]['timestamp']}")
        print(f"  起始位置: ({all_points[0]['longitude']}, {all_points[0]['latitude']})")
        print(f"  结束位置: ({all_points[-1]['longitude']}, {all_points[-1]['latitude']})")
    
    return len(all_points)

def analyze_sampling_interval(csv_file):
    """
    快速分析采样间隔
    """
    print(f"\n{'─'*80}")
    print("采样间隔快速分析")
    print(f"{'─'*80}")
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        data = list(reader)
    
    if len(data) < 100:
        print("数据点不足100个，跳过采样间隔分析")
        return
    
    # 分析前100个点的时间间隔
    timestamps = []
    for i, row in enumerate(data[:100]):
        try:
            ts = datetime.strptime(row['timestamp'], '%Y-%m-%d %H:%M:%S')
            timestamps.append(ts)
        except:
            continue
    
    if len(timestamps) < 2:
        return
    
    intervals = [(timestamps[i+1] - timestamps[i]).total_seconds() 
                 for i in range(len(timestamps)-1)]
    
    avg_interval = sum(intervals) / len(intervals)
    min_interval = min(intervals)
    max_interval = max(intervals)
    
    print(f"基于前100个点的统计:")
    print(f"  平均采样间隔: {avg_interval:.2f} 秒")
    print(f"  最小间隔: {min_interval:.2f} 秒")
    print(f"  最大间隔: {max_interval:.2f} 秒")
    print(f"  采样率: ~{1/avg_interval:.2f} Hz")

def main():
    print("=" * 80)
    print("处理Trajtory数据集")
    print("=" * 80)
    
    # 路径配置
    trajtory_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Trajtory"
    output_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    
    # 处理Trajtory数据集
    print(f"\n处理目录: {trajtory_dir}")
    print(f"目标点数: 100,000")
    print("")
    
    output_file = os.path.join(output_dir, "Trajtory_100k_with_timestamp.csv")
    count = process_trajtory_data(trajtory_dir, output_file, max_points=100000)
    
    # 快速分析采样间隔
    if count > 0:
        analyze_sampling_interval(output_file)
    
    # 汇总
    print("\n" + "=" * 80)
    print("✅ 处理完成！")
    print("=" * 80)
    print(f"输出文件: {output_file}")
    print(f"数据点数: {count:,}")
    
    # 显示样例数据
    print("\n" + "=" * 80)
    print("样例数据预览（前5行）")
    print("=" * 80)
    with open(output_file, 'r') as f:
        for i, line in enumerate(f):
            if i < 6:
                print(f"  {line.rstrip()}")

if __name__ == "__main__":
    main()

