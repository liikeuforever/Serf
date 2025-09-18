#!/usr/bin/env python3
"""
可视化不同max_diff和数据集下的连续长度分布

该脚本会生成散点图来展示：
1. 不同max_diff参数下各数据集的连续长度分布
2. 多维度的统计信息可视化
3. 交互式图表便于深入分析
"""

import os
import csv
import math
import collections
from typing import List, Dict, Tuple
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.colors import LogNorm
import warnings

# 设置中文字体支持
plt.rcParams['font.sans-serif'] = ['SimHei', 'DejaVu Sans', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False

warnings.filterwarnings('ignore')


def apply_precision_erasure(value: float, max_diff: float) -> float:
    """应用精度抹除，将数值按照max_diff进行量化"""
    if max_diff <= 0:
        return value
    
    # 将数值量化到max_diff的整数倍
    quantized = math.floor(value / max_diff) * max_diff
    return round(quantized, 10)  # 避免浮点精度问题


def count_consecutive_runs(values: List[float]) -> Dict[int, int]:
    """统计连续相同数值的长度分布"""
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
    """分析单个数据集文件"""
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
    
    return results


def prepare_visualization_data(all_results: Dict[str, Dict[float, Dict[int, int]]]) -> pd.DataFrame:
    """准备可视化数据"""
    data_rows = []
    
    for dataset_name, dataset_results in all_results.items():
        # 清理数据集名称（去掉.csv后缀）
        clean_name = dataset_name.replace('.csv', '')
        
        for max_diff, consecutive_counts in dataset_results.items():
            for run_length, count in consecutive_counts.items():
                # 为每个连续长度和出现次数创建多行数据（用于散点图密度表示）
                for _ in range(min(count, 1000)):  # 限制最大点数避免图表过密
                    data_rows.append({
                        'dataset': clean_name,
                        'max_diff': max_diff,
                        'run_length': run_length,
                        'count': count,
                        'log_count': np.log10(count + 1),
                        'log_run_length': np.log10(run_length)
                    })
    
    return pd.DataFrame(data_rows)


def create_scatter_plot_matrix(df: pd.DataFrame, max_diffs: List[float]):
    """创建散点图矩阵展示不同max_diff下的分布"""
    n_diffs = len(max_diffs)
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    datasets = df['dataset'].unique()
    colors = plt.cm.tab10(np.linspace(0, 1, len(datasets)))
    
    for i, max_diff in enumerate(max_diffs):
        ax = axes[i]
        df_subset = df[df['max_diff'] == max_diff]
        
        for j, dataset in enumerate(datasets):
            dataset_data = df_subset[df_subset['dataset'] == dataset]
            if not dataset_data.empty:
                ax.scatter(dataset_data['run_length'], dataset_data['count'], 
                          alpha=0.6, s=20, color=colors[j], label=dataset)
        
        ax.set_xlabel('连续长度')
        ax.set_ylabel('出现次数')
        ax.set_title(f'max_diff = {max_diff}')
        ax.set_yscale('log')
        ax.set_xscale('log')
        ax.grid(True, alpha=0.3)
        
        if i == 0:  # 只在第一个子图显示图例
            ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/consecutive_distribution_scatter_matrix.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_heatmap_visualization(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """创建热力图展示平均连续长度"""
    datasets = list(all_results.keys())
    avg_lengths = np.zeros((len(datasets), len(max_diffs)))
    single_ratios = np.zeros((len(datasets), len(max_diffs)))
    
    for i, dataset in enumerate(datasets):
        for j, max_diff in enumerate(max_diffs):
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                total_runs = sum(consecutive_counts.values())
                total_points = sum(length * count for length, count in consecutive_counts.items())
                avg_length = total_points / total_runs if total_runs > 0 else 0
                single_ratio = consecutive_counts.get(1, 0) / total_runs if total_runs > 0 else 0
                
                avg_lengths[i, j] = avg_length
                single_ratios[i, j] = single_ratio
    
    # 创建热力图
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8))
    
    # 平均连续长度热力图
    clean_datasets = [name.replace('.csv', '') for name in datasets]
    im1 = ax1.imshow(avg_lengths, cmap='viridis', aspect='auto', norm=LogNorm())
    ax1.set_xticks(range(len(max_diffs)))
    ax1.set_xticklabels(max_diffs)
    ax1.set_yticks(range(len(datasets)))
    ax1.set_yticklabels(clean_datasets)
    ax1.set_xlabel('max_diff')
    ax1.set_ylabel('数据集')
    ax1.set_title('平均连续长度')
    
    # 添加数值标注
    for i in range(len(datasets)):
        for j in range(len(max_diffs)):
            if avg_lengths[i, j] > 0:
                ax1.text(j, i, f'{avg_lengths[i, j]:.1f}', 
                        ha='center', va='center', color='white', fontsize=8)
    
    plt.colorbar(im1, ax=ax1, label='平均连续长度')
    
    # 单点连续段占比热力图
    im2 = ax2.imshow(single_ratios, cmap='plasma', aspect='auto', vmin=0, vmax=1)
    ax2.set_xticks(range(len(max_diffs)))
    ax2.set_xticklabels(max_diffs)
    ax2.set_yticks(range(len(datasets)))
    ax2.set_yticklabels(clean_datasets)
    ax2.set_xlabel('max_diff')
    ax2.set_ylabel('数据集')
    ax2.set_title('单点连续段占比')
    
    # 添加数值标注
    for i in range(len(datasets)):
        for j in range(len(max_diffs)):
            ax2.text(j, i, f'{single_ratios[i, j]:.2f}', 
                    ha='center', va='center', color='white', fontsize=8)
    
    plt.colorbar(im2, ax=ax2, label='单点连续段占比')
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/consecutive_distribution_heatmap.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_3d_scatter_plot(df: pd.DataFrame):
    """创建3D散点图展示三维关系"""
    from mpl_toolkits.mplot3d import Axes3D
    
    fig = plt.figure(figsize=(15, 10))
    ax = fig.add_subplot(111, projection='3d')
    
    datasets = df['dataset'].unique()
    colors = plt.cm.tab10(np.linspace(0, 1, len(datasets)))
    
    for i, dataset in enumerate(datasets):
        dataset_data = df[df['dataset'] == dataset]
        # 采样数据避免图表过密
        if len(dataset_data) > 5000:
            dataset_data = dataset_data.sample(5000)
        
        ax.scatter(dataset_data['max_diff'], 
                  dataset_data['log_run_length'], 
                  dataset_data['log_count'],
                  alpha=0.6, s=20, color=colors[i], label=dataset)
    
    ax.set_xlabel('max_diff')
    ax.set_ylabel('log(连续长度)')
    ax.set_zlabel('log(出现次数)')
    ax.set_title('连续长度分布三维可视化')
    ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/consecutive_distribution_3d.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_box_plot_by_maxdiff(df: pd.DataFrame, max_diffs: List[float]):
    """创建箱线图展示不同max_diff下连续长度的分布"""
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    for i, max_diff in enumerate(max_diffs):
        ax = axes[i]
        df_subset = df[df['max_diff'] == max_diff]
        
        # 为每个数据集创建箱线图数据
        box_data = []
        labels = []
        
        for dataset in df['dataset'].unique():
            dataset_data = df_subset[df_subset['dataset'] == dataset]
            if not dataset_data.empty:
                # 展开数据用于箱线图
                run_lengths = []
                for _, row in dataset_data.iterrows():
                    run_lengths.extend([row['run_length']] * min(row['count'], 100))
                
                if run_lengths:
                    box_data.append(run_lengths)
                    labels.append(dataset.replace('.csv', ''))
        
        if box_data:
            ax.boxplot(box_data, labels=labels)
            ax.set_ylabel('连续长度')
            ax.set_title(f'max_diff = {max_diff}')
            ax.set_yscale('log')
            ax.tick_params(axis='x', rotation=45)
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/consecutive_distribution_boxplot.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_summary_statistics_plot(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """创建汇总统计图表"""
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    
    datasets = list(all_results.keys())
    clean_datasets = [name.replace('.csv', '') for name in datasets]
    
    # 1. 平均连续长度随max_diff变化
    for dataset in datasets:
        avg_lengths = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                total_runs = sum(consecutive_counts.values())
                total_points = sum(length * count for length, count in consecutive_counts.items())
                avg_length = total_points / total_runs if total_runs > 0 else 0
                avg_lengths.append(avg_length)
            else:
                avg_lengths.append(0)
        
        ax1.plot(max_diffs, avg_lengths, marker='o', label=dataset.replace('.csv', ''))
    
    ax1.set_xlabel('max_diff')
    ax1.set_ylabel('平均连续长度')
    ax1.set_title('平均连续长度随精度参数变化')
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax1.grid(True, alpha=0.3)
    
    # 2. 单点连续段占比随max_diff变化
    for dataset in datasets:
        single_ratios = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                total_runs = sum(consecutive_counts.values())
                single_ratio = consecutive_counts.get(1, 0) / total_runs if total_runs > 0 else 0
                single_ratios.append(single_ratio)
            else:
                single_ratios.append(0)
        
        ax2.plot(max_diffs, single_ratios, marker='s', label=dataset.replace('.csv', ''))
    
    ax2.set_xlabel('max_diff')
    ax2.set_ylabel('单点连续段占比')
    ax2.set_title('单点连续段占比随精度参数变化')
    ax2.set_xscale('log')
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax2.grid(True, alpha=0.3)
    
    # 3. 最大连续长度随max_diff变化
    for dataset in datasets:
        max_lengths = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                max_length = max(consecutive_counts.keys()) if consecutive_counts else 0
                max_lengths.append(max_length)
            else:
                max_lengths.append(0)
        
        ax3.plot(max_diffs, max_lengths, marker='^', label=dataset.replace('.csv', ''))
    
    ax3.set_xlabel('max_diff')
    ax3.set_ylabel('最大连续长度')
    ax3.set_title('最大连续长度随精度参数变化')
    ax3.set_xscale('log')
    ax3.set_yscale('log')
    ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax3.grid(True, alpha=0.3)
    
    # 4. 连续段总数随max_diff变化
    for dataset in datasets:
        total_runs = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                total_run = sum(consecutive_counts.values())
                total_runs.append(total_run)
            else:
                total_runs.append(0)
        
        ax4.plot(max_diffs, total_runs, marker='d', label=dataset.replace('.csv', ''))
    
    ax4.set_xlabel('max_diff')
    ax4.set_ylabel('连续段总数')
    ax4.set_title('连续段总数随精度参数变化')
    ax4.set_xscale('log')
    ax4.set_yscale('log')
    ax4.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax4.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/consecutive_distribution_summary.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def main():
    """主函数"""
    # 配置参数
    data_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    max_diffs = [0.001, 0.01, 0.1, 1.0, 10.0]  # 不同的精度抹除参数
    
    print("=" * 60)
    print("连续相同数值分布可视化分析")
    print("=" * 60)
    
    # 获取所有CSV文件
    csv_files = []
    for filename in os.listdir(data_dir):
        if filename.endswith('.csv'):
            csv_files.append(os.path.join(data_dir, filename))
    
    csv_files.sort()
    print(f"找到 {len(csv_files)} 个CSV文件")
    
    # 分析每个文件
    all_results = {}
    
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        results = analyze_dataset(file_path, max_diffs)
        all_results[filename] = results
        print(f"完成分析: {filename}")
    
    print("\n开始生成可视化图表...")
    
    # 准备可视化数据
    df = prepare_visualization_data(all_results)
    print(f"准备了 {len(df)} 个数据点用于可视化")
    
    # 生成各种可视化图表
    print("1. 生成散点图矩阵...")
    create_scatter_plot_matrix(df, max_diffs)
    
    print("2. 生成热力图...")
    create_heatmap_visualization(all_results, max_diffs)
    
    print("3. 生成3D散点图...")
    create_3d_scatter_plot(df)
    
    print("4. 生成箱线图...")
    create_box_plot_by_maxdiff(df, max_diffs)
    
    print("5. 生成汇总统计图表...")
    create_summary_statistics_plot(all_results, max_diffs)
    
    print("\n所有可视化图表已生成完成！")
    print("生成的图片文件：")
    print("- consecutive_distribution_scatter_matrix.png")
    print("- consecutive_distribution_heatmap.png")
    print("- consecutive_distribution_3d.png")
    print("- consecutive_distribution_boxplot.png")
    print("- consecutive_distribution_summary.png")


if __name__ == "__main__":
    main()
