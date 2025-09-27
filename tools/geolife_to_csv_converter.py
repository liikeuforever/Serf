#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Geolife数据集转换工具
将Geolife数据集转换为T-Drive格式的CSV文件

Geolife数据格式：
- 目录结构: Data/用户ID/Trajectory/轨迹文件.plt
- PLT文件格式: 前6行是头部信息，从第7行开始是数据
- 数据格式: 纬度,经度,0,高度,时间戳,日期,时间

输出格式：
- CSV文件: 经度,纬度 (与T-Drive格式一致)
"""

import os
import sys
import csv
import glob
from datetime import datetime
import argparse

class GeolifeConverter:
    def __init__(self, data_dir, output_dir):
        self.data_dir = data_dir
        self.output_dir = output_dir
        self.total_points = 0
        self.total_files = 0
        self.processed_users = 0
        
    def parse_plt_file(self, plt_file):
        """解析单个PLT文件，提取经纬度数据"""
        points = []
        
        try:
            with open(plt_file, 'r', encoding='utf-8') as f:
                lines = f.readlines()
                
                # 跳过前6行头部信息
                data_lines = lines[6:]
                
                for line in data_lines:
                    line = line.strip()
                    if not line:
                        continue
                        
                    parts = line.split(',')
                    if len(parts) >= 7:
                        try:
                            latitude = float(parts[0])
                            longitude = float(parts[1])
                            
                            # 基本数据验证
                            if -90 <= latitude <= 90 and -180 <= longitude <= 180:
                                points.append((longitude, latitude))
                                
                        except ValueError:
                            continue
                            
        except Exception as e:
            print(f"警告: 无法解析文件 {plt_file}: {e}")
            
        return points
    
    def process_user_data(self, user_id):
        """处理单个用户的轨迹数据"""
        user_dir = os.path.join(self.data_dir, user_id)
        trajectory_dir = os.path.join(user_dir, 'Trajectory')
        
        if not os.path.exists(trajectory_dir):
            return []
            
        # 获取所有PLT文件并按时间排序
        plt_files = glob.glob(os.path.join(trajectory_dir, '*.plt'))
        plt_files.sort()  # 按文件名排序，文件名包含时间戳
        
        all_points = []
        
        for plt_file in plt_files:
            points = self.parse_plt_file(plt_file)
            all_points.extend(points)
            self.total_files += 1
            
            if len(points) > 0:
                print(f"  处理文件: {os.path.basename(plt_file)} -> {len(points)} 个GPS点")
        
        return all_points
    
    def convert_all_users(self, max_users=None, max_points_per_file=50000):
        """转换所有用户数据"""
        # 获取所有用户目录
        user_dirs = [d for d in os.listdir(self.data_dir) 
                    if os.path.isdir(os.path.join(self.data_dir, d)) and d.isdigit()]
        user_dirs.sort()
        
        if max_users:
            user_dirs = user_dirs[:max_users]
            
        print(f"发现 {len(user_dirs)} 个用户目录")
        
        # 创建输出目录
        os.makedirs(self.output_dir, exist_ok=True)
        
        all_trajectory_points = []
        
        for user_id in user_dirs:
            print(f"\n处理用户 {user_id}...")
            
            user_points = self.process_user_data(user_id)
            
            if user_points:
                all_trajectory_points.extend(user_points)
                self.total_points += len(user_points)
                self.processed_users += 1
                
                print(f"  用户 {user_id}: {len(user_points)} 个GPS点")
                
                # 如果积累的点数过多，分批写入文件
                if len(all_trajectory_points) >= max_points_per_file:
                    self.save_trajectory_batch(all_trajectory_points, len(glob.glob(os.path.join(self.output_dir, 'geolife_*.csv'))))
                    all_trajectory_points = []
        
        # 保存剩余的轨迹点
        if all_trajectory_points:
            self.save_trajectory_batch(all_trajectory_points, len(glob.glob(os.path.join(self.output_dir, 'geolife_*.csv'))))
        
        self.create_summary_files()
        
    def save_trajectory_batch(self, points, batch_num):
        """保存一批轨迹数据到CSV文件"""
        if not points:
            return
            
        # 主文件：经度,纬度格式
        main_file = os.path.join(self.output_dir, f'geolife_longitude_latitude_batch_{batch_num:03d}.csv')
        
        with open(main_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            for lon, lat in points:
                writer.writerow([f"{lon:.6f}", f"{lat:.6f}"])
                
        print(f"保存批次 {batch_num}: {len(points)} 个点 -> {main_file}")
        
        # 分别保存经度和纬度文件
        lon_file = os.path.join(self.output_dir, f'geolife_longitude_batch_{batch_num:03d}.csv')
        lat_file = os.path.join(self.output_dir, f'geolife_latitude_batch_{batch_num:03d}.csv')
        
        with open(lon_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            for lon, lat in points:
                writer.writerow([f"{lon:.6f}"])
                
        with open(lat_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            for lon, lat in points:
                writer.writerow([f"{lat:.6f}"])
    
    def create_summary_files(self):
        """合并所有批次文件，创建汇总文件"""
        print(f"\n创建汇总文件...")
        
        # 合并所有批次的主文件
        main_files = glob.glob(os.path.join(self.output_dir, 'geolife_longitude_latitude_batch_*.csv'))
        main_files.sort()
        
        if main_files:
            summary_file = os.path.join(self.output_dir, 'geolife_longitude_latitude.csv')
            with open(summary_file, 'w', newline='', encoding='utf-8') as outf:
                for batch_file in main_files:
                    with open(batch_file, 'r', encoding='utf-8') as inf:
                        outf.write(inf.read())
            print(f"创建汇总文件: {summary_file}")
        
        # 合并经度文件
        lon_files = glob.glob(os.path.join(self.output_dir, 'geolife_longitude_batch_*.csv'))
        lon_files.sort()
        
        if lon_files:
            summary_lon_file = os.path.join(self.output_dir, 'geolife_longitude.csv')
            with open(summary_lon_file, 'w', newline='', encoding='utf-8') as outf:
                for batch_file in lon_files:
                    with open(batch_file, 'r', encoding='utf-8') as inf:
                        outf.write(inf.read())
            print(f"创建经度汇总文件: {summary_lon_file}")
        
        # 合并纬度文件
        lat_files = glob.glob(os.path.join(self.output_dir, 'geolife_latitude_batch_*.csv'))
        lat_files.sort()
        
        if lat_files:
            summary_lat_file = os.path.join(self.output_dir, 'geolife_latitude.csv')
            with open(summary_lat_file, 'w', newline='', encoding='utf-8') as outf:
                for batch_file in lat_files:
                    with open(batch_file, 'r', encoding='utf-8') as inf:
                        outf.write(inf.read())
            print(f"创建纬度汇总文件: {summary_lat_file}")
    
    def print_summary(self):
        """打印处理摘要"""
        print(f"\n" + "="*60)
        print(f"Geolife数据转换完成！")
        print(f"="*60)
        print(f"处理用户数: {self.processed_users}")
        print(f"处理文件数: {self.total_files}")
        print(f"总GPS轨迹点数: {self.total_points:,}")
        print(f"输出目录: {self.output_dir}")
        print(f"="*60)

def main():
    parser = argparse.ArgumentParser(description='将Geolife数据集转换为T-Drive格式的CSV文件')
    parser.add_argument('--data_dir', default='/Users/xuzihang/GitProject/GG/Serf/test/Geolife/Data',
                       help='Geolife数据目录路径')
    parser.add_argument('--output_dir', default='/Users/xuzihang/GitProject/GG/Serf/test/data_set',
                       help='输出CSV文件目录')
    parser.add_argument('--max_users', type=int, default=20,
                       help='最大处理用户数 (默认: 20)')
    parser.add_argument('--max_points', type=int, default=50000,
                       help='每个批次文件的最大点数 (默认: 50000)')
    
    args = parser.parse_args()
    
    print("🚀 Geolife数据集转换工具")
    print("="*60)
    print(f"输入目录: {args.data_dir}")
    print(f"输出目录: {args.output_dir}")
    print(f"最大用户数: {args.max_users}")
    print(f"批次大小: {args.max_points:,} 点")
    print("="*60)
    
    if not os.path.exists(args.data_dir):
        print(f"❌ 错误: 输入目录不存在: {args.data_dir}")
        sys.exit(1)
    
    converter = GeolifeConverter(args.data_dir, args.output_dir)
    
    try:
        converter.convert_all_users(max_users=args.max_users, max_points_per_file=args.max_points)
        converter.print_summary()
        
        print(f"\n✅ 转换完成！可以使用以下文件进行测试:")
        print(f"   主文件: {os.path.join(args.output_dir, 'geolife_longitude_latitude.csv')}")
        print(f"   经度文件: {os.path.join(args.output_dir, 'geolife_longitude.csv')}")
        print(f"   纬度文件: {os.path.join(args.output_dir, 'geolife_latitude.csv')}")
        
    except KeyboardInterrupt:
        print(f"\n⚠️  用户中断了转换过程")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ 转换过程中发生错误: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
