#!/usr/bin/env python3
"""
分析GPS轨迹数据集的采样间隔分布（清洗版本）
过滤掉轨迹切换导致的异常间隔
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
    all_intervals = []
    for i in range(len(timestamps) - 1):
        interval = (timestamps[i+1] - timestamps[i]).total_seconds()
        all_intervals.append(interval)
    
    print(f"总间隔数: {len(all_intervals):,}")
    
    # 过滤掉异常间隔（负值和超过1小时的，这些通常是轨迹切换）
    normal_intervals = [i for i in all_intervals if 0 <= i <= 3600]
    abnormal_count = len(all_intervals) - len(normal_intervals)
    
    print(f"正常间隔: {len(normal_intervals):,} ({len(normal_intervals)/len(all_intervals)*100:.2f}%)")
    print(f"异常间隔: {abnormal_count:,} ({abnormal_count/len(all_intervals)*100:.2f}%) - 可能是轨迹切换")
    
    if len(normal_intervals) < 10:
        print("⚠️ 正常间隔数据不足，无法进行详细分析")
        return
    
    # 基础统计（仅使用正常间隔）
    intervals = normal_intervals
    print(f"\n{'─'*80}")
    print("基础统计（仅统计正常间隔：0-3600秒）")
    print(f"{'─'*80}")
    print(f"最小间隔: {min(intervals):.2f} 秒")
    print(f"最大间隔: {max(intervals):.2f} 秒")
    print(f"平均间隔: {statistics.mean(intervals):.2f} 秒")
    print(f"中位数:   {statistics.median(intervals):.2f} 秒")
    print(f"标准差:   {statistics.stdev(intervals):.2f} 秒")
    
    # 百分位数
    sorted_intervals = sorted(intervals)
    p10 = sorted_intervals[int(len(sorted_intervals) * 0.10)]
    p25 = sorted_intervals[int(len(sorted_intervals) * 0.25)]
    p50 = sorted_intervals[int(len(sorted_intervals) * 0.50)]
    p75 = sorted_intervals[int(len(sorted_intervals) * 0.75)]
    p90 = sorted_intervals[int(len(sorted_intervals) * 0.90)]
    p95 = sorted_intervals[int(len(sorted_intervals) * 0.95)]
    p99 = sorted_intervals[int(len(sorted_intervals) * 0.99)]
    
    print(f"\n{'─'*80}")
    print("百分位数")
    print(f"{'─'*80}")
    print(f"P10:  {p10:.2f} 秒")
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
    print("间隔分布（前15个最常见的间隔）")
    print(f"{'─'*80}")
    print(f"{'间隔(秒)':<12} {'数量':<12} {'占比':<12} {'累计占比':<12} {'可视化':<20}")
    print(f"{'─'*80}")
    
    cumulative = 0
    for interval_sec, count in interval_counter.most_common(15):
        percentage = count / len(intervals) * 100
        cumulative += percentage
        bar = '█' * int(percentage / 2)  # 每2%一个方块
        print(f"{interval_sec:<12} {count:<12,} {percentage:<11.2f}% {cumulative:<11.2f}% {bar}")
    
    # 分段统计
    print(f"\n{'─'*80}")
    print("间隔分段统计")
    print(f"{'─'*80}")
    print(f"{'分段':<20} {'数量':<15} {'占比':<12} {'可视化':<30}")
    print(f"{'─'*80}")
    
    ranges = [
        (0, 1, "0-1秒（瞬时）"),
        (1, 2, "1-2秒"),
        (2, 3, "2-3秒"),
        (3, 5, "3-5秒"),
        (5, 10, "5-10秒"),
        (10, 30, "10-30秒"),
        (30, 60, "30-60秒"),
        (60, 300, "1-5分钟"),
        (300, 600, "5-10分钟"),
        (600, 3600, "10-60分钟")
    ]
    
    for min_val, max_val, label in ranges:
        count = sum(1 for i in intervals if min_val <= i < max_val)
        if count > 0:
            percentage = count / len(intervals) * 100
            bar = '█' * int(percentage / 2)
            print(f"{label:<20} {count:>8,} 个   {percentage:>6.2f}%  {bar}")
    
    # 主要采样率识别
    print(f"\n{'─'*80}")
    print("主要采样率识别")
    print(f"{'─'*80}")
    
    # 识别占比超过10%的间隔
    main_intervals = [(sec, count) for sec, count in interval_counter.most_common() 
                      if count / len(intervals) * 100 >= 5]
    
    if main_intervals:
        print("占比≥5%的主要采样间隔:")
        for sec, count in main_intervals:
            percentage = count / len(intervals) * 100
            print(f"  - {sec}秒: {count:,}个 ({percentage:.2f}%)")
    
    # 采样率一致性分析
    print(f"\n{'─'*80}")
    print("采样率一致性分析")
    print(f"{'─'*80}")
    
    # 计算间隔的变异系数（CV = std/mean）
    cv = statistics.stdev(intervals) / statistics.mean(intervals)
    print(f"变异系数(CV): {cv:.4f}")
    
    if cv < 0.3:
        consistency = "高度一致"
    elif cv < 0.5:
        consistency = "较一致"
    elif cv < 1.0:
        consistency = "中等变化"
    else:
        consistency = "变化较大"
    
    print(f"采样率一致性: {consistency}")
    print(f"说明: CV越小说明采样率越稳定")
    
    return {
        'total_points': len(data),
        'total_intervals': len(all_intervals),
        'normal_intervals': len(normal_intervals),
        'abnormal_intervals': abnormal_count,
        'min': min(intervals),
        'max': max(intervals),
        'mean': statistics.mean(intervals),
        'median': statistics.median(intervals),
        'std': statistics.stdev(intervals),
        'p10': p10,
        'p25': p25,
        'p50': p50,
        'p75': p75,
        'p90': p90,
        'p95': p95,
        'p99': p99,
        'cv': cv,
        'main_intervals': main_intervals[:3]  # 前3个主要间隔
    }

def main():
    print("=" * 80)
    print("GPS轨迹数据集采样间隔分布分析（清洗版）")
    print("=" * 80)
    print("注：已过滤轨迹切换导致的异常间隔（负值和>1小时）")
    
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
    
    print(f"\n{'指标':<25} {'Geolife':<25} {'Track':<25}")
    print(f"{'─'*75}")
    print(f"{'数据点数':<25} {geolife_stats['total_points']:<25,} {track_stats['total_points']:<25,}")
    print(f"{'正常间隔数':<25} {geolife_stats['normal_intervals']:<25,} {track_stats['normal_intervals']:<25,}")
    print(f"{'异常间隔数':<25} {geolife_stats['abnormal_intervals']:<25,} {track_stats['abnormal_intervals']:<25,}")
    print(f"{'─'*75}")
    print(f"{'平均间隔':<25} {geolife_stats['mean']:<25.2f} {track_stats['mean']:<25.2f}")
    print(f"{'中位数间隔':<25} {geolife_stats['median']:<25.2f} {track_stats['median']:<25.2f}")
    print(f"{'标准差':<25} {geolife_stats['std']:<25.2f} {track_stats['std']:<25.2f}")
    print(f"{'变异系数(CV)':<25} {geolife_stats['cv']:<25.4f} {track_stats['cv']:<25.4f}")
    print(f"{'─'*75}")
    print(f"{'最小间隔':<25} {geolife_stats['min']:<25.2f} {track_stats['min']:<25.2f}")
    print(f"{'P50间隔':<25} {geolife_stats['p50']:<25.2f} {track_stats['p50']:<25.2f}")
    print(f"{'P95间隔':<25} {geolife_stats['p95']:<25.2f} {track_stats['p95']:<25.2f}")
    print(f"{'最大间隔':<25} {geolife_stats['max']:<25.2f} {track_stats['max']:<25.2f}")
    
    print(f"\n{'='*80}")
    print("核心发现:")
    print(f"{'='*80}")
    
    print(f"\n1️⃣  Geolife数据集特征:")
    print(f"   ✓ 主要采样间隔: {', '.join([f'{s}秒' for s, c in geolife_stats['main_intervals']])}")
    print(f"   ✓ 中位数间隔: {geolife_stats['median']:.1f}秒")
    print(f"   ✓ 平均间隔: {geolife_stats['mean']:.1f}秒")
    print(f"   ✓ 采样率一致性: CV={geolife_stats['cv']:.3f} ({'稳定' if geolife_stats['cv'] < 0.5 else '有波动'})")
    
    print(f"\n2️⃣  Track数据集特征:")
    print(f"   ✓ 主要采样间隔: {', '.join([f'{s}秒' for s, c in track_stats['main_intervals']])}")
    print(f"   ✓ 中位数间隔: {track_stats['median']:.1f}秒")
    print(f"   ✓ 平均间隔: {track_stats['mean']:.1f}秒")
    print(f"   ✓ 采样率一致性: CV={track_stats['cv']:.3f} ({'稳定' if track_stats['cv'] < 0.5 else '有波动'})")
    
    print(f"\n3️⃣  对比结论:")
    ratio = geolife_stats['median'] / track_stats['median']
    print(f"   • Geolife的采样间隔是Track的 {ratio:.1f}倍")
    print(f"   • Track采样率更高（约{1/track_stats['median']:.1f}Hz），数据更密集")
    print(f"   • Geolife采样率较低（约{1/geolife_stats['median']:.2f}Hz），适合长时间轨迹记录")
    
    print(f"\n{'='*80}")
    print("✅ 分析完成！")
    print(f"{'='*80}")
    
    # 导出CSV报告
    with open("sampling_intervals_statistics.csv", 'w', newline='', encoding='utf-8-sig') as f:
        import csv
        writer = csv.writer(f)
        writer.writerow(["指标", "Geolife", "Track"])
        writer.writerow(["数据点数", geolife_stats['total_points'], track_stats['total_points']])
        writer.writerow(["正常间隔数", geolife_stats['normal_intervals'], track_stats['normal_intervals']])
        writer.writerow(["异常间隔数", geolife_stats['abnormal_intervals'], track_stats['abnormal_intervals']])
        writer.writerow([])
        writer.writerow(["平均间隔(秒)", f"{geolife_stats['mean']:.2f}", f"{track_stats['mean']:.2f}"])
        writer.writerow(["中位数间隔(秒)", f"{geolife_stats['median']:.2f}", f"{track_stats['median']:.2f}"])
        writer.writerow(["标准差(秒)", f"{geolife_stats['std']:.2f}", f"{track_stats['std']:.2f}"])
        writer.writerow(["变异系数", f"{geolife_stats['cv']:.4f}", f"{track_stats['cv']:.4f}"])
        writer.writerow([])
        writer.writerow(["P10(秒)", f"{geolife_stats['p10']:.2f}", f"{track_stats['p10']:.2f}"])
        writer.writerow(["P25(秒)", f"{geolife_stats['p25']:.2f}", f"{track_stats['p25']:.2f}"])
        writer.writerow(["P50(秒)", f"{geolife_stats['p50']:.2f}", f"{track_stats['p50']:.2f}"])
        writer.writerow(["P75(秒)", f"{geolife_stats['p75']:.2f}", f"{track_stats['p75']:.2f}"])
        writer.writerow(["P90(秒)", f"{geolife_stats['p90']:.2f}", f"{track_stats['p90']:.2f}"])
        writer.writerow(["P95(秒)", f"{geolife_stats['p95']:.2f}", f"{track_stats['p95']:.2f}"])
        writer.writerow(["P99(秒)", f"{geolife_stats['p99']:.2f}", f"{track_stats['p99']:.2f}"])
    
    print(f"\n📊 统计数据已导出到: sampling_intervals_statistics.csv")

if __name__ == "__main__":
    main()


