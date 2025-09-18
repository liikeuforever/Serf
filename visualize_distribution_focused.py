#!/usr/bin/env python3
"""
专注于连续长度分布的可视化分析

该脚本重点展示连续长度的分布特征，而不是简单的平均值：
1. 直方图展示分布形状
2. 小提琴图展示分布密度
3. 累积分布函数(CDF)
4. 概率密度函数(PDF)
5. 分位数分布
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
import warnings

# 设置中文字体支持
plt.rcParams['font.sans-serif'] = ['SimHei', 'DejaVu Sans', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['figure.figsize'] = [12, 8]

warnings.filterwarnings('ignore')


def apply_precision_erasure(value: float, max_diff: float) -> float:
    """应用精度抹除，将数值按照max_diff进行量化"""
    if max_diff <= 0:
        return value
    
    quantized = math.floor(value / max_diff) * max_diff
    return round(quantized, 10)


def count_consecutive_runs(values: List[float]) -> Dict[int, int]:
    """统计连续相同数值的长度分布"""
    if not values:
        return {}
    
    run_lengths = []
    current_value = values[0]
    current_length = 1
    
    for i in range(1, len(values)):
        if abs(values[i] - current_value) < 1e-10:
            current_length += 1
        else:
            run_lengths.append(current_length)
            current_value = values[i]
            current_length = 1
    
    run_lengths.append(current_length)
    return dict(collections.Counter(run_lengths))


def analyze_dataset(file_path: str, max_diffs: List[float]) -> Dict[float, Dict[int, int]]:
    """分析单个数据集文件"""
    print(f"正在分析文件: {os.path.basename(file_path)}")
    
    values = []
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line:
                    if ',' in line:
                        parts = line.split(',')
                        value = float(parts[-1])
                    else:
                        value = float(line)
                    values.append(value)
    except Exception as e:
        print(f"读取文件 {file_path} 时出错: {e}")
        return {}
    
    if not values:
        return {}
    
    results = {}
    for max_diff in max_diffs:
        quantized_values = [apply_precision_erasure(v, max_diff) for v in values]
        consecutive_counts = count_consecutive_runs(quantized_values)
        results[max_diff] = consecutive_counts
    
    return results


def prepare_distribution_data(all_results: Dict[str, Dict[float, Dict[int, int]]]) -> pd.DataFrame:
    """准备分布数据，展开每个连续长度的实际出现次数"""
    data_rows = []
    
    for dataset_name, dataset_results in all_results.items():
        clean_name = dataset_name.replace('.csv', '')
        
        for max_diff, consecutive_counts in dataset_results.items():
            # 为每个连续长度创建对应数量的记录
            for run_length, count in consecutive_counts.items():
                for _ in range(count):
                    data_rows.append({
                        'dataset': clean_name,
                        'max_diff': max_diff,
                        'run_length': run_length,
                        'log_run_length': np.log10(run_length)
                    })
    
    return pd.DataFrame(data_rows)


def create_histogram_matrix(df: pd.DataFrame, max_diffs: List[float]):
    """创建直方图矩阵展示连续长度分布"""
    n_diffs = len(max_diffs)
    fig, axes = plt.subplots(2, 3, figsize=(20, 12))
    axes = axes.flatten()
    
    datasets = df['dataset'].unique()
    colors = plt.cm.Set3(np.linspace(0, 1, len(datasets)))
    
    for i, max_diff in enumerate(max_diffs):
        ax = axes[i]
        df_subset = df[df['max_diff'] == max_diff]
        
        # 为每个数据集绘制直方图
        for j, dataset in enumerate(datasets):
            dataset_data = df_subset[df_subset['dataset'] == dataset]
            if not dataset_data.empty:
                run_lengths = dataset_data['run_length'].values
                
                # 使用对数分箱以更好展示分布
                max_length = max(run_lengths)
                if max_length > 1:
                    bins = np.logspace(0, np.log10(max_length), 50)
                else:
                    bins = 50
                
                ax.hist(run_lengths, bins=bins, alpha=0.6, 
                       label=dataset, color=colors[j], density=True)
        
        ax.set_xlabel('连续长度')
        ax.set_ylabel('概率密度')
        ax.set_title(f'max_diff = {max_diff}')
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.grid(True, alpha=0.3)
        
        if i == 0:
            ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    
    plt.suptitle('连续长度分布直方图矩阵', fontsize=16, y=0.98)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/distribution_histogram_matrix.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_violin_plot_comparison(df: pd.DataFrame, max_diffs: List[float]):
    """创建小提琴图比较不同条件下的分布"""
    fig, axes = plt.subplots(2, 3, figsize=(20, 12))
    axes = axes.flatten()
    
    for i, max_diff in enumerate(max_diffs):
        ax = axes[i]
        df_subset = df[df['max_diff'] == max_diff]
        
        # 准备小提琴图数据
        violin_data = []
        labels = []
        
        for dataset in df['dataset'].unique():
            dataset_data = df_subset[df_subset['dataset'] == dataset]
            if not dataset_data.empty:
                # 限制数据量避免内存问题
                run_lengths = dataset_data['run_length'].values
                if len(run_lengths) > 10000:
                    run_lengths = np.random.choice(run_lengths, 10000, replace=False)
                
                violin_data.append(run_lengths)
                labels.append(dataset)
        
        if violin_data:
            parts = ax.violinplot(violin_data, positions=range(len(labels)), 
                                showmeans=True, showmedians=True)
            
            # 美化小提琴图
            for pc in parts['bodies']:
                pc.set_facecolor('lightblue')
                pc.set_alpha(0.7)
            
            ax.set_xticks(range(len(labels)))
            ax.set_xticklabels(labels, rotation=45, ha='right')
            ax.set_ylabel('连续长度')
            ax.set_title(f'max_diff = {max_diff}')
            ax.set_yscale('log')
            ax.grid(True, alpha=0.3)
    
    plt.suptitle('连续长度分布小提琴图', fontsize=16, y=0.98)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/distribution_violin_plots.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_cdf_comparison(df: pd.DataFrame, max_diffs: List[float]):
    """创建累积分布函数(CDF)比较图"""
    fig, axes = plt.subplots(2, 3, figsize=(20, 12))
    axes = axes.flatten()
    
    datasets = df['dataset'].unique()
    colors = plt.cm.tab10(np.linspace(0, 1, len(datasets)))
    
    for i, max_diff in enumerate(max_diffs):
        ax = axes[i]
        df_subset = df[df['max_diff'] == max_diff]
        
        for j, dataset in enumerate(datasets):
            dataset_data = df_subset[df_subset['dataset'] == dataset]
            if not dataset_data.empty:
                run_lengths = dataset_data['run_length'].values
                
                # 计算CDF
                sorted_lengths = np.sort(run_lengths)
                y_values = np.arange(1, len(sorted_lengths) + 1) / len(sorted_lengths)
                
                ax.plot(sorted_lengths, y_values, label=dataset, 
                       color=colors[j], linewidth=2)
        
        ax.set_xlabel('连续长度')
        ax.set_ylabel('累积概率')
        ax.set_title(f'max_diff = {max_diff}')
        ax.set_xscale('log')
        ax.grid(True, alpha=0.3)
        
        if i == 0:
            ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    
    plt.suptitle('连续长度累积分布函数(CDF)', fontsize=16, y=0.98)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/distribution_cdf_comparison.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_quantile_heatmap(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """创建分位数热力图"""
    datasets = list(all_results.keys())
    quantiles = [0.5, 0.75, 0.9, 0.95, 0.99]  # 中位数、75%、90%、95%、99%分位数
    
    fig, axes = plt.subplots(1, len(quantiles), figsize=(25, 6))
    
    for q_idx, quantile in enumerate(quantiles):
        ax = axes[q_idx]
        quantile_matrix = np.zeros((len(datasets), len(max_diffs)))
        
        for i, dataset in enumerate(datasets):
            for j, max_diff in enumerate(max_diffs):
                if max_diff in all_results[dataset]:
                    consecutive_counts = all_results[dataset][max_diff]
                    
                    # 展开数据计算分位数
                    run_lengths = []
                    for length, count in consecutive_counts.items():
                        run_lengths.extend([length] * count)
                    
                    if run_lengths:
                        quantile_value = np.percentile(run_lengths, quantile * 100)
                        quantile_matrix[i, j] = quantile_value
        
        # 创建热力图
        im = ax.imshow(quantile_matrix, cmap='viridis', aspect='auto')
        
        ax.set_xticks(range(len(max_diffs)))
        ax.set_xticklabels(max_diffs)
        ax.set_yticks(range(len(datasets)))
        ax.set_yticklabels([name.replace('.csv', '') for name in datasets])
        ax.set_xlabel('max_diff')
        ax.set_title(f'{quantile*100:.0f}%分位数')
        
        # 添加数值标注
        for i in range(len(datasets)):
            for j in range(len(max_diffs)):
                if quantile_matrix[i, j] > 0:
                    ax.text(j, i, f'{quantile_matrix[i, j]:.0f}', 
                           ha='center', va='center', color='white', fontsize=8)
        
        plt.colorbar(im, ax=ax, label='连续长度')
    
    plt.suptitle('连续长度分位数热力图', fontsize=16)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/distribution_quantile_heatmap.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_distribution_comparison_by_dataset(df: pd.DataFrame, max_diffs: List[float]):
    """按数据集比较不同max_diff下的分布"""
    datasets = df['dataset'].unique()
    n_datasets = len(datasets)
    
    # 计算子图布局
    n_cols = 4
    n_rows = (n_datasets + n_cols - 1) // n_cols
    
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(20, 5*n_rows))
    if n_rows == 1:
        axes = axes.reshape(1, -1)
    
    colors = plt.cm.viridis(np.linspace(0, 1, len(max_diffs)))
    
    for i, dataset in enumerate(datasets):
        row = i // n_cols
        col = i % n_cols
        ax = axes[row, col]
        
        dataset_data = df[df['dataset'] == dataset]
        
        for j, max_diff in enumerate(max_diffs):
            max_diff_data = dataset_data[dataset_data['max_diff'] == max_diff]
            if not max_diff_data.empty:
                run_lengths = max_diff_data['run_length'].values
                
                # 使用直方图密度绘制分布曲线
                if len(run_lengths) > 1:
                    # 计算直方图
                    max_length = max(run_lengths)
                    if max_length > 1:
                        bins = np.logspace(0, np.log10(max_length), 30)
                    else:
                        bins = 30
                    
                    hist, bin_edges = np.histogram(run_lengths, bins=bins, density=True)
                    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
                    
                    ax.plot(bin_centers, hist, label=f'max_diff={max_diff}', 
                           color=colors[j], linewidth=2, marker='o', markersize=4)
        
        ax.set_xlabel('连续长度')
        ax.set_ylabel('密度')
        ax.set_title(dataset)
        ax.set_xscale('log')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.3)
    
    # 隐藏多余的子图
    for i in range(n_datasets, n_rows * n_cols):
        row = i // n_cols
        col = i % n_cols
        axes[row, col].set_visible(False)
    
    plt.suptitle('各数据集在不同max_diff下的连续长度分布密度', fontsize=16)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/distribution_by_dataset.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_distribution_statistics_summary(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """创建分布统计信息汇总"""
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    
    datasets = list(all_results.keys())
    
    # 1. 中位数变化
    for dataset in datasets:
        medians = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                run_lengths = []
                for length, count in consecutive_counts.items():
                    run_lengths.extend([length] * count)
                
                if run_lengths:
                    median = np.median(run_lengths)
                    medians.append(median)
                else:
                    medians.append(0)
            else:
                medians.append(0)
        
        ax1.plot(max_diffs, medians, marker='o', label=dataset.replace('.csv', ''))
    
    ax1.set_xlabel('max_diff')
    ax1.set_ylabel('中位数连续长度')
    ax1.set_title('连续长度中位数变化')
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax1.grid(True, alpha=0.3)
    
    # 2. 90%分位数变化
    for dataset in datasets:
        percentile_90s = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                run_lengths = []
                for length, count in consecutive_counts.items():
                    run_lengths.extend([length] * count)
                
                if run_lengths:
                    p90 = np.percentile(run_lengths, 90)
                    percentile_90s.append(p90)
                else:
                    percentile_90s.append(0)
            else:
                percentile_90s.append(0)
        
        ax2.plot(max_diffs, percentile_90s, marker='s', label=dataset.replace('.csv', ''))
    
    ax2.set_xlabel('max_diff')
    ax2.set_ylabel('90%分位数连续长度')
    ax2.set_title('连续长度90%分位数变化')
    ax2.set_xscale('log')
    ax2.set_yscale('log')
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax2.grid(True, alpha=0.3)
    
    # 3. 标准差变化
    for dataset in datasets:
        stds = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                run_lengths = []
                for length, count in consecutive_counts.items():
                    run_lengths.extend([length] * count)
                
                if run_lengths:
                    std = np.std(run_lengths)
                    stds.append(std)
                else:
                    stds.append(0)
            else:
                stds.append(0)
        
        ax3.plot(max_diffs, stds, marker='^', label=dataset.replace('.csv', ''))
    
    ax3.set_xlabel('max_diff')
    ax3.set_ylabel('标准差')
    ax3.set_title('连续长度标准差变化')
    ax3.set_xscale('log')
    ax3.set_yscale('log')
    ax3.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax3.grid(True, alpha=0.3)
    
    # 4. 偏度变化
    for dataset in datasets:
        skewnesses = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                run_lengths = []
                for length, count in consecutive_counts.items():
                    run_lengths.extend([length] * count)
                
                if len(run_lengths) > 2:
                    # 计算偏度（使用numpy实现）
                    mean_val = np.mean(run_lengths)
                    std_val = np.std(run_lengths)
                    if std_val > 0:
                        skewness = np.mean(((run_lengths - mean_val) / std_val) ** 3)
                        skewnesses.append(skewness)
                    else:
                        skewnesses.append(0)
                else:
                    skewnesses.append(0)
            else:
                skewnesses.append(0)
        
        ax4.plot(max_diffs, skewnesses, marker='d', label=dataset.replace('.csv', ''))
    
    ax4.set_xlabel('max_diff')
    ax4.set_ylabel('偏度')
    ax4.set_title('连续长度分布偏度变化')
    ax4.set_xscale('log')
    ax4.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
    ax4.grid(True, alpha=0.3)
    
    plt.suptitle('连续长度分布统计特征变化', fontsize=16)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/distribution_statistics_summary.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def main():
    """主函数"""
    data_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    max_diffs = [0.001, 0.01, 0.1, 1.0, 10.0]
    
    print("=" * 60)
    print("连续长度分布专项可视化分析")
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
    
    print("\n开始生成分布可视化图表...")
    
    # 准备分布数据
    df = prepare_distribution_data(all_results)
    print(f"准备了 {len(df)} 个分布数据点")
    
    # 生成各种分布可视化图表
    print("1. 生成直方图矩阵...")
    create_histogram_matrix(df, max_diffs)
    
    print("2. 生成小提琴图比较...")
    create_violin_plot_comparison(df, max_diffs)
    
    print("3. 生成CDF比较图...")
    create_cdf_comparison(df, max_diffs)
    
    print("4. 生成分位数热力图...")
    create_quantile_heatmap(all_results, max_diffs)
    
    print("5. 生成按数据集的分布比较...")
    create_distribution_comparison_by_dataset(df, max_diffs)
    
    print("6. 生成分布统计特征汇总...")
    create_distribution_statistics_summary(all_results, max_diffs)
    
    print("\n所有分布可视化图表已生成完成！")
    print("生成的图片文件：")
    print("- distribution_histogram_matrix.png")
    print("- distribution_violin_plots.png")
    print("- distribution_cdf_comparison.png")
    print("- distribution_quantile_heatmap.png")
    print("- distribution_by_dataset.png")
    print("- distribution_statistics_summary.png")


if __name__ == "__main__":
    main()
