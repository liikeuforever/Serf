#!/usr/bin/env python3
"""
分析GPS轨迹数据集的采样间隔分布
"""

import csv
from datetime import datetime
from collections import Counter
import statistics

def analyze_sampling_intervals(csv_file, dataset_name):
    """
    分析数据集的采样间隔分布
    """
    print(f"\n{'='*80}")
    print(f"【{dataset_name} 数据集采样间隔分析】")
    print(f"{'='*80}")
    
    # 读取数据
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        data = list(reader)
    
    print(f"\n总数据点数: {len(data):,}")
    
    # 解析时间戳并计算间隔
    timestamps = []
    for row in data:
        try:
            ts = datetime.strptime(row['timestamp'], '%Y-%m-%d %H:%M:%S')
            timestamps.append(ts)
        except:
            continue
    
    if len(timestamps) < 2:
        print("⚠️ 数据点不足，无法分析")
        return
    
    # 计算相邻点之间的时间间隔（秒）
    intervals = []
    for i in range(len(timestamps) - 1):
        interval = (timestamps[i+1] - timestamps[i]).total_seconds()
        intervals.append(interval)
    
    print(f"有效间隔数: {len(intervals):,}")
    
    # 基础统计
    print(f"\n{'─'*80}")
    print("基础统计")
    print(f"{'─'*80}")
    print(f"最小间隔: {min(intervals):.2f} 秒")
    print(f"最大间隔: {max(intervals):.2f} 秒")
    print(f"平均间隔: {statistics.mean(intervals):.2f} 秒")
    print(f"中位数:   {statistics.median(intervals):.2f} 秒")
    print(f"标准差:   {statistics.stdev(intervals):.2f} 秒")
    
    # 百分位数
    sorted_intervals = sorted(intervals)
    p25 = sorted_intervals[int(len(sorted_intervals) * 0.25)]
    p50 = sorted_intervals[int(len(sorted_intervals) * 0.50)]
    p75 = sorted_intervals[int(len(sorted_intervals) * 0.75)]
    p90 = sorted_intervals[int(len(sorted_intervals) * 0.90)]
    p95 = sorted_intervals[int(len(sorted_intervals) * 0.95)]
    p99 = sorted_intervals[int(len(sorted_intervals) * 0.99)]
    
    print(f"\n{'─'*80}")
    print("百分位数")
    print(f"{'─'*80}")
    print(f"P25:  {p25:.2f} 秒")
    print(f"P50:  {p50:.2f} 秒")
    print(f"P75:  {p75:.2f} 秒")
    print(f"P90:  {p90:.2f} 秒")
    print(f"P95:  {p95:.2f} 秒")
    print(f"P99:  {p99:.2f} 秒")
    
    # 统计分布（按秒分组）
    interval_counter = Counter()
    for interval in intervals:
        # 按整数秒分组
        interval_sec = int(round(interval))
        interval_counter[interval_sec] += 1
    
    print(f"\n{'─'*80}")
    print("间隔分布（前20个最常见的间隔）")
    print(f"{'─'*80}")
    print(f"{'间隔(秒)':<12} {'数量':<12} {'占比':<12} {'累计占比':<12}")
    print(f"{'─'*80}")
    
    cumulative = 0
    for interval_sec, count in interval_counter.most_common(20):
        percentage = count / len(intervals) * 100
        cumulative += percentage
        print(f"{interval_sec:<12} {count:<12,} {percentage:<11.2f}% {cumulative:<11.2f}%")
    
    # 分段统计
    print(f"\n{'─'*80}")
    print("间隔分段统计")
    print(f"{'─'*80}")
    
    ranges = [
        (0, 1, "0-1秒"),
        (1, 2, "1-2秒"),
        (2, 5, "2-5秒"),
        (5, 10, "5-10秒"),
        (10, 30, "10-30秒"),
        (30, 60, "30-60秒"),
        (60, 300, "1-5分钟"),
        (300, 3600, "5-60分钟"),
        (3600, float('inf'), ">1小时")
    ]
    
    for min_val, max_val, label in ranges:
        count = sum(1 for i in intervals if min_val <= i < max_val)
        if count > 0:
            percentage = count / len(intervals) * 100
            print(f"{label:<15} {count:>8,} 个 ({percentage:>6.2f}%)")
    
    # 异常间隔检测
    mean_interval = statistics.mean(intervals)
    std_interval = statistics.stdev(intervals)
    threshold = mean_interval + 3 * std_interval
    
    outliers = [i for i in intervals if i > threshold]
    if outliers:
        print(f"\n{'─'*80}")
        print(f"异常间隔检测（>均值+3σ，即>{threshold:.2f}秒）")
        print(f"{'─'*80}")
        print(f"异常间隔数: {len(outliers):,} ({len(outliers)/len(intervals)*100:.2f}%)")
        print(f"最大异常值: {max(outliers):.2f} 秒 ({max(outliers)/60:.2f} 分钟)")
    
    # 统计不同时间段的平均间隔
    print(f"\n{'─'*80}")
    print("时间段采样率分析（前1000点 vs 中间1000点 vs 最后1000点）")
    print(f"{'─'*80}")
    
    if len(intervals) >= 3000:
        first_1000 = intervals[:1000]
        middle_1000 = intervals[len(intervals)//2-500:len(intervals)//2+500]
        last_1000 = intervals[-1000:]
        
        print(f"前1000点:   平均 {statistics.mean(first_1000):.2f}秒, 中位数 {statistics.median(first_1000):.2f}秒")
        print(f"中间1000点: 平均 {statistics.mean(middle_1000):.2f}秒, 中位数 {statistics.median(middle_1000):.2f}秒")
        print(f"最后1000点: 平均 {statistics.mean(last_1000):.2f}秒, 中位数 {statistics.median(last_1000):.2f}秒")
    
    return {
        'total_points': len(data),
        'total_intervals': len(intervals),
        'min': min(intervals),
        'max': max(intervals),
        'mean': statistics.mean(intervals),
        'median': statistics.median(intervals),
        'std': statistics.stdev(intervals),
        'p25': p25,
        'p50': p50,
        'p75': p75,
        'p90': p90,
        'p95': p95,
        'p99': p99,
        'outliers': len(outliers) if outliers else 0
    }

def main():
    print("=" * 80)
    print("GPS轨迹数据集采样间隔分布分析")
    print("=" * 80)
    
    # 分析Geolife数据集
    geolife_stats = analyze_sampling_intervals(
        "test/data_set/Geolife_100k_with_timestamp.csv",
        "Geolife"
    )
    
    # 分析Track数据集
    track_stats = analyze_sampling_intervals(
        "test/data_set/Track_with_timestamp.csv",
        "Track"
    )
    
    # 对比总结
    print(f"\n{'='*80}")
    print("【两个数据集对比总结】")
    print(f"{'='*80}")
    
    print(f"\n{'指标':<20} {'Geolife':<20} {'Track':<20}")
    print(f"{'─'*60}")
    print(f"{'数据点数':<20} {geolife_stats['total_points']:<20,} {track_stats['total_points']:<20,}")
    print(f"{'平均间隔':<20} {geolife_stats['mean']:<20.2f} {track_stats['mean']:<20.2f}")
    print(f"{'中位数间隔':<20} {geolife_stats['median']:<20.2f} {track_stats['median']:<20.2f}")
    print(f"{'标准差':<20} {geolife_stats['std']:<20.2f} {track_stats['std']:<20.2f}")
    print(f"{'最小间隔':<20} {geolife_stats['min']:<20.2f} {track_stats['min']:<20.2f}")
    print(f"{'最大间隔':<20} {geolife_stats['max']:<20.2f} {track_stats['max']:<20.2f}")
    print(f"{'P95间隔':<20} {geolife_stats['p95']:<20.2f} {track_stats['p95']:<20.2f}")
    
    print(f"\n{'='*80}")
    print("主要发现:")
    print(f"{'='*80}")
    print(f"1. Geolife数据集:")
    print(f"   - 平均采样间隔约 {geolife_stats['mean']:.1f} 秒")
    print(f"   - 大部分点采样间隔稳定在 {geolife_stats['median']:.0f} 秒左右")
    print(f"   - 标准差 {geolife_stats['std']:.1f} 秒，说明采样率{'较稳定' if geolife_stats['std'] < 10 else '有一定波动'}")
    
    print(f"\n2. Track数据集:")
    print(f"   - 平均采样间隔约 {track_stats['mean']:.1f} 秒")
    print(f"   - 大部分点采样间隔稳定在 {track_stats['median']:.0f} 秒左右")
    print(f"   - 标准差 {track_stats['std']:.1f} 秒，说明采样率{'较稳定' if track_stats['std'] < 10 else '有一定波动'}")
    
    print(f"\n3. 对比:")
    if geolife_stats['mean'] > track_stats['mean']:
        print(f"   - Geolife采样间隔更大（{geolife_stats['mean']:.1f}秒 vs {track_stats['mean']:.1f}秒）")
        print(f"   - Track采样率更高，数据更密集")
    else:
        print(f"   - Track采样间隔更大（{track_stats['mean']:.1f}秒 vs {geolife_stats['mean']:.1f}秒）")
        print(f"   - Geolife采样率更高，数据更密集")
    
    print(f"\n{'='*80}")
    print("✅ 分析完成！")
    print(f"{'='*80}")

if __name__ == "__main__":
    main()


