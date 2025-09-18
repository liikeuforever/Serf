#!/usr/bin/env python3
"""
统计数据集中在不同精度抹除下连续相同数值的占比分布

该脚本会分析指定目录下的CSV数据集，在给定的max_diff精度抹除条件下，
统计连续相同数值的长度分布。

例如：数据序列 [1.121, 1.122, 1.123, 1.131]
在max_diff=0.01的情况下，会被视为 [1.12, 1.12, 1.12, 1.13]
连续相同值的分布为：长度3的连续段1个，长度1的连续段1个
"""

import os
import csv
import math
import collections
from typing import List, Dict, Tuple
import pandas as pd
import numpy as np


def apply_precision_erasure(value: float, max_diff: float) -> float:
    """
    应用精度抹除，将数值按照max_diff进行量化
    
    Args:
        value: 原始数值
        max_diff: 最大允许差异（精度参数）
    
    Returns:
        量化后的数值
    """
    if max_diff <= 0:
        return value
    
    # 将数值量化到max_diff的整数倍
    quantized = math.floor(value / max_diff) * max_diff
    return round(quantized, 10)  # 避免浮点精度问题


def count_consecutive_runs(values: List[float]) -> Dict[int, int]:
    """
    统计连续相同数值的长度分布
    
    Args:
        values: 数值序列
    
    Returns:
        字典，键为连续长度，值为该长度出现的次数
    """
    if not values:
        return {}
    
    run_lengths = []
    current_value = values[0]
    current_length = 1
    
    for i in range(1, len(values)):
        if abs(values[i] - current_value) < 1e-10:  # 考虑浮点精度
            current_length += 1
        else:
            run_lengths.append(current_length)
            current_value = values[i]
            current_length = 1
    
    # 添加最后一个连续段
    run_lengths.append(current_length)
    
    # 统计各长度的出现次数
    return dict(collections.Counter(run_lengths))


def analyze_dataset(file_path: str, max_diffs: List[float]) -> Dict[float, Dict[int, int]]:
    """
    分析单个数据集文件
    
    Args:
        file_path: 数据集文件路径
        max_diffs: 要测试的max_diff值列表
    
    Returns:
        嵌套字典，外层键为max_diff值，内层为连续长度分布
    """
    print(f"正在分析文件: {os.path.basename(file_path)}")
    
    # 读取数据
    values = []
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line:
                    # 假设数据格式为单列数值或者有简单分隔符
                    if ',' in line:
                        parts = line.split(',')
                        value = float(parts[-1])  # 取最后一列作为数值
                    else:
                        value = float(line)
                    values.append(value)
    except Exception as e:
        print(f"读取文件 {file_path} 时出错: {e}")
        return {}
    
    if not values:
        print(f"文件 {file_path} 中没有有效数据")
        return {}
    
    print(f"读取了 {len(values)} 个数据点")
    
    results = {}
    
    for max_diff in max_diffs:
        # 应用精度抹除
        quantized_values = [apply_precision_erasure(v, max_diff) for v in values]
        
        # 统计连续相同值的分布
        consecutive_counts = count_consecutive_runs(quantized_values)
        results[max_diff] = consecutive_counts
        
        # 输出简要统计信息
        total_runs = sum(consecutive_counts.values())
        total_values = len(values)
        avg_run_length = total_values / total_runs if total_runs > 0 else 0
        
        print(f"  max_diff={max_diff}: {total_runs}个连续段, 平均长度={avg_run_length:.2f}")
    
    return results


def calculate_statistics(consecutive_counts: Dict[int, int]) -> Dict[str, float]:
    """
    计算统计指标
    
    Args:
        consecutive_counts: 连续长度分布
    
    Returns:
        统计指标字典
    """
    if not consecutive_counts:
        return {}
    
    # 计算总连续段数和总数据点数
    total_runs = sum(consecutive_counts.values())
    total_points = sum(length * count for length, count in consecutive_counts.items())
    
    # 计算平均连续长度
    avg_length = total_points / total_runs if total_runs > 0 else 0
    
    # 计算最大连续长度
    max_length = max(consecutive_counts.keys()) if consecutive_counts else 0
    
    # 计算长度为1的连续段占比（即非连续值的占比）
    single_runs = consecutive_counts.get(1, 0)
    single_ratio = single_runs / total_runs if total_runs > 0 else 0
    
    # 计算连续段长度的中位数
    lengths = []
    for length, count in consecutive_counts.items():
        lengths.extend([length] * count)
    lengths.sort()
    n = len(lengths)
    median_length = lengths[n//2] if n > 0 else 0
    
    return {
        'total_runs': total_runs,
        'total_points': total_points,
        'avg_length': avg_length,
        'max_length': max_length,
        'single_ratio': single_ratio,
        'median_length': median_length
    }


def main():
    """主函数"""
    # 配置参数
    data_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    max_diffs = [0.001, 0.01, 0.1, 1.0, 10.0]  # 不同的精度抹除参数
    
    print("=" * 60)
    print("数据集连续相同数值分布分析")
    print("=" * 60)
    
    # 获取所有CSV文件
    csv_files = []
    for filename in os.listdir(data_dir):
        if filename.endswith('.csv'):
            csv_files.append(os.path.join(data_dir, filename))
    
    csv_files.sort()
    print(f"找到 {len(csv_files)} 个CSV文件")
    print()
    
    # 分析每个文件
    all_results = {}
    
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        results = analyze_dataset(file_path, max_diffs)
        all_results[filename] = results
        print()
    
    # 输出详细结果
    print("=" * 60)
    print("详细统计结果")
    print("=" * 60)
    
    for filename, file_results in all_results.items():
        print(f"\n文件: {filename}")
        print("-" * 40)
        
        for max_diff in max_diffs:
            if max_diff in file_results:
                consecutive_counts = file_results[max_diff]
                stats = calculate_statistics(consecutive_counts)
                
                print(f"\nmax_diff = {max_diff}:")
                print(f"  总连续段数: {stats.get('total_runs', 0)}")
                print(f"  总数据点数: {stats.get('total_points', 0)}")
                print(f"  平均连续长度: {stats.get('avg_length', 0):.2f}")
                print(f"  最大连续长度: {stats.get('max_length', 0)}")
                print(f"  单点连续段占比: {stats.get('single_ratio', 0):.2%}")
                print(f"  连续长度中位数: {stats.get('median_length', 0)}")
                
                # 输出连续长度分布（前10个最常见的长度）
                sorted_counts = sorted(consecutive_counts.items(), 
                                     key=lambda x: x[1], reverse=True)
                print("  连续长度分布 (长度: 次数):")
                for length, count in sorted_counts[:10]:
                    percentage = count / stats.get('total_runs', 1) * 100
                    print(f"    长度{length}: {count}次 ({percentage:.1f}%)")
                
                if len(sorted_counts) > 10:
                    print(f"    ... 还有{len(sorted_counts) - 10}种其他长度")
    
    # 生成汇总报告
    print("\n" + "=" * 60)
    print("汇总报告")
    print("=" * 60)
    
    for max_diff in max_diffs:
        print(f"\nmax_diff = {max_diff} 的汇总:")
        print("-" * 30)
        
        total_avg_lengths = []
        total_single_ratios = []
        
        for filename, file_results in all_results.items():
            if max_diff in file_results:
                stats = calculate_statistics(file_results[max_diff])
                total_avg_lengths.append(stats.get('avg_length', 0))
                total_single_ratios.append(stats.get('single_ratio', 0))
        
        if total_avg_lengths:
            print(f"  平均连续长度 - 均值: {np.mean(total_avg_lengths):.2f}, "
                  f"中位数: {np.median(total_avg_lengths):.2f}")
            print(f"  单点连续段占比 - 均值: {np.mean(total_single_ratios):.2%}, "
                  f"中位数: {np.median(total_single_ratios):.2%}")


if __name__ == "__main__":
    main()
