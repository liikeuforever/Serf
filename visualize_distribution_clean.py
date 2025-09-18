#!/usr/bin/env python3
"""
清晰直观的连续长度分布可视化

专注于生成简洁、直观、易读的图表来展示连续长度分布
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

# 设置matplotlib参数
plt.rcParams['font.family'] = 'DejaVu Sans'
plt.rcParams['font.size'] = 10
plt.rcParams['axes.titlesize'] = 12
plt.rcParams['axes.labelsize'] = 10
plt.rcParams['xtick.labelsize'] = 9
plt.rcParams['ytick.labelsize'] = 9
plt.rcParams['legend.fontsize'] = 8
plt.rcParams['figure.titlesize'] = 14

warnings.filterwarnings('ignore')


def apply_precision_erasure(value: float, max_diff: float) -> float:
    """Apply precision erasure by quantizing values to max_diff intervals"""
    if max_diff <= 0:
        return value
    
    quantized = math.floor(value / max_diff) * max_diff
    return round(quantized, 10)


def count_consecutive_runs(values: List[float]) -> Dict[int, int]:
    """Count consecutive runs of identical values"""
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
    """Analyze a single dataset file"""
    print(f"Analyzing: {os.path.basename(file_path)}")
    
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
        print(f"Error reading {file_path}: {e}")
        return {}
    
    if not values:
        return {}
    
    results = {}
    for max_diff in max_diffs:
        quantized_values = [apply_precision_erasure(v, max_diff) for v in values]
        consecutive_counts = count_consecutive_runs(quantized_values)
        results[max_diff] = consecutive_counts
    
    return results


def create_simple_histogram_comparison(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """Create simple histogram comparison for key datasets"""
    # Select a few representative datasets
    key_datasets = ['Air-pressure.csv', 'Motor-temp.csv', 'Stocks-USA.csv', 'T-drive.csv']
    available_datasets = [d for d in key_datasets if d in all_results]
    
    if not available_datasets:
        available_datasets = list(all_results.keys())[:4]
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    axes = axes.flatten()
    
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
    
    for i, dataset in enumerate(available_datasets[:4]):
        ax = axes[i]
        
        for j, max_diff in enumerate(max_diffs):
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                
                # Convert to arrays for plotting
                lengths = list(consecutive_counts.keys())
                counts = list(consecutive_counts.values())
                
                if lengths:
                    # Normalize counts to probabilities
                    total = sum(counts)
                    probabilities = [c/total for c in counts]
                    
                    ax.scatter(lengths, probabilities, alpha=0.7, s=30, 
                             label=f'max_diff={max_diff}', color=colors[j])
        
        ax.set_xlabel('Run Length')
        ax.set_ylabel('Probability')
        ax.set_title(f'{dataset.replace(".csv", "")}')
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.grid(True, alpha=0.3)
        ax.legend()
    
    plt.suptitle('Run Length Distribution Comparison', fontsize=16)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/clean_histogram_comparison.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_summary_heatmap(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """Create a clean summary heatmap"""
    datasets = list(all_results.keys())
    
    # Calculate key statistics
    avg_lengths = np.zeros((len(datasets), len(max_diffs)))
    median_lengths = np.zeros((len(datasets), len(max_diffs)))
    p90_lengths = np.zeros((len(datasets), len(max_diffs)))
    
    for i, dataset in enumerate(datasets):
        for j, max_diff in enumerate(max_diffs):
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                
                # Expand data for statistics
                run_lengths = []
                for length, count in consecutive_counts.items():
                    run_lengths.extend([length] * count)
                
                if run_lengths:
                    avg_lengths[i, j] = np.mean(run_lengths)
                    median_lengths[i, j] = np.median(run_lengths)
                    p90_lengths[i, j] = np.percentile(run_lengths, 90)
    
    # Create subplot figure
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 8))
    
    # Clean dataset names
    clean_datasets = [name.replace('.csv', '') for name in datasets]
    
    # Average length heatmap
    im1 = ax1.imshow(avg_lengths, cmap='viridis', aspect='auto')
    ax1.set_xticks(range(len(max_diffs)))
    ax1.set_xticklabels(max_diffs)
    ax1.set_yticks(range(len(datasets)))
    ax1.set_yticklabels(clean_datasets)
    ax1.set_xlabel('max_diff')
    ax1.set_title('Average Run Length')
    plt.colorbar(im1, ax=ax1, shrink=0.8)
    
    # Median length heatmap
    im2 = ax2.imshow(median_lengths, cmap='plasma', aspect='auto')
    ax2.set_xticks(range(len(max_diffs)))
    ax2.set_xticklabels(max_diffs)
    ax2.set_yticks(range(len(datasets)))
    ax2.set_yticklabels(clean_datasets)
    ax2.set_xlabel('max_diff')
    ax2.set_title('Median Run Length')
    plt.colorbar(im2, ax=ax2, shrink=0.8)
    
    # 90th percentile heatmap
    im3 = ax3.imshow(p90_lengths, cmap='inferno', aspect='auto')
    ax3.set_xticks(range(len(max_diffs)))
    ax3.set_xticklabels(max_diffs)
    ax3.set_yticks(range(len(datasets)))
    ax3.set_yticklabels(clean_datasets)
    ax3.set_xlabel('max_diff')
    ax3.set_title('90th Percentile Run Length')
    plt.colorbar(im3, ax=ax3, shrink=0.8)
    
    plt.suptitle('Run Length Statistics Summary', fontsize=16)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/clean_summary_heatmap.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_trend_analysis(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """Create trend analysis showing how distributions change with max_diff"""
    # Select representative datasets
    key_datasets = ['Air-pressure.csv', 'Motor-temp.csv', 'Stocks-USA.csv', 'T-drive.csv', 'Smart-grid.csv']
    available_datasets = [d for d in key_datasets if d in all_results]
    
    if not available_datasets:
        available_datasets = list(all_results.keys())[:5]
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    axes = axes.flatten()
    
    colors = plt.cm.Set1(np.linspace(0, 1, len(available_datasets)))
    
    # 1. Average run length trend
    ax = axes[0]
    for i, dataset in enumerate(available_datasets):
        avg_lengths = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                run_lengths = []
                for length, count in consecutive_counts.items():
                    run_lengths.extend([length] * count)
                avg_length = np.mean(run_lengths) if run_lengths else 0
                avg_lengths.append(avg_length)
            else:
                avg_lengths.append(0)
        
        ax.plot(max_diffs, avg_lengths, marker='o', label=dataset.replace('.csv', ''), 
                color=colors[i], linewidth=2)
    
    ax.set_xlabel('max_diff')
    ax.set_ylabel('Average Run Length')
    ax.set_title('Average Run Length vs max_diff')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # 2. Median run length trend
    ax = axes[1]
    for i, dataset in enumerate(available_datasets):
        median_lengths = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                run_lengths = []
                for length, count in consecutive_counts.items():
                    run_lengths.extend([length] * count)
                median_length = np.median(run_lengths) if run_lengths else 0
                median_lengths.append(median_length)
            else:
                median_lengths.append(0)
        
        ax.plot(max_diffs, median_lengths, marker='s', label=dataset.replace('.csv', ''), 
                color=colors[i], linewidth=2)
    
    ax.set_xlabel('max_diff')
    ax.set_ylabel('Median Run Length')
    ax.set_title('Median Run Length vs max_diff')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # 3. Single-run ratio trend
    ax = axes[2]
    for i, dataset in enumerate(available_datasets):
        single_ratios = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                total_runs = sum(consecutive_counts.values())
                single_runs = consecutive_counts.get(1, 0)
                ratio = single_runs / total_runs if total_runs > 0 else 0
                single_ratios.append(ratio)
            else:
                single_ratios.append(0)
        
        ax.plot(max_diffs, single_ratios, marker='^', label=dataset.replace('.csv', ''), 
                color=colors[i], linewidth=2)
    
    ax.set_xlabel('max_diff')
    ax.set_ylabel('Single Run Ratio')
    ax.set_title('Single Run Ratio vs max_diff')
    ax.set_xscale('log')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # 4. Total number of runs trend
    ax = axes[3]
    for i, dataset in enumerate(available_datasets):
        total_runs_list = []
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                total_runs = sum(consecutive_counts.values())
                total_runs_list.append(total_runs)
            else:
                total_runs_list.append(0)
        
        ax.plot(max_diffs, total_runs_list, marker='d', label=dataset.replace('.csv', ''), 
                color=colors[i], linewidth=2)
    
    ax.set_xlabel('max_diff')
    ax.set_ylabel('Total Number of Runs')
    ax.set_title('Total Runs vs max_diff')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.suptitle('Run Length Distribution Trends', fontsize=16)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/clean_trend_analysis.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def create_distribution_overview(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """Create an overview of distribution shapes"""
    # Focus on extreme cases: smallest and largest max_diff
    extreme_diffs = [max_diffs[0], max_diffs[-1]]  # 0.001 and 10.0
    
    # Select representative datasets
    key_datasets = ['Air-pressure.csv', 'Motor-temp.csv', 'Stocks-USA.csv', 'T-drive.csv']
    available_datasets = [d for d in key_datasets if d in all_results]
    
    if not available_datasets:
        available_datasets = list(all_results.keys())[:4]
    
    fig, axes = plt.subplots(2, 4, figsize=(20, 10))
    
    for i, max_diff in enumerate(extreme_diffs):
        for j, dataset in enumerate(available_datasets[:4]):
            ax = axes[i, j]
            
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                
                lengths = list(consecutive_counts.keys())
                counts = list(consecutive_counts.values())
                
                if lengths and counts:
                    # Sort by length for better visualization
                    sorted_data = sorted(zip(lengths, counts))
                    lengths, counts = zip(*sorted_data)
                    
                    # Convert to probabilities
                    total = sum(counts)
                    probabilities = [c/total for c in counts]
                    
                    # Create bar plot for better visibility
                    ax.bar(range(len(lengths)), probabilities, alpha=0.7)
                    
                    # Set x-axis labels (show only every nth label to avoid crowding)
                    step = max(1, len(lengths) // 10)
                    ax.set_xticks(range(0, len(lengths), step))
                    ax.set_xticklabels([str(lengths[k]) for k in range(0, len(lengths), step)], rotation=45)
            
            ax.set_xlabel('Run Length')
            ax.set_ylabel('Probability')
            
            if i == 0:
                ax.set_title(f'{dataset.replace(".csv", "")}')
            
            if j == 0:
                ax.text(-0.1, 0.5, f'max_diff = {max_diff}', rotation=90, 
                       transform=ax.transAxes, ha='right', va='center', fontsize=12)
            
            ax.set_yscale('log')
            ax.grid(True, alpha=0.3)
    
    plt.suptitle('Run Length Distribution Overview', fontsize=16)
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/clean_distribution_overview.png', 
                dpi=300, bbox_inches='tight')
    plt.show()


def main():
    """Main function"""
    data_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    max_diffs = [0.001, 0.01, 0.1, 1.0, 10.0]
    
    print("=" * 60)
    print("Clean Run Length Distribution Analysis")
    print("=" * 60)
    
    # Get all CSV files
    csv_files = []
    for filename in os.listdir(data_dir):
        if filename.endswith('.csv'):
            csv_files.append(os.path.join(data_dir, filename))
    
    csv_files.sort()
    print(f"Found {len(csv_files)} CSV files")
    
    # Analyze each file
    all_results = {}
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        results = analyze_dataset(file_path, max_diffs)
        all_results[filename] = results
    
    print("\nGenerating clean visualizations...")
    
    print("1. Simple histogram comparison...")
    create_simple_histogram_comparison(all_results, max_diffs)
    
    print("2. Summary heatmap...")
    create_summary_heatmap(all_results, max_diffs)
    
    print("3. Trend analysis...")
    create_trend_analysis(all_results, max_diffs)
    
    print("4. Distribution overview...")
    create_distribution_overview(all_results, max_diffs)
    
    print("\nAll clean visualizations completed!")
    print("Generated files:")
    print("- clean_histogram_comparison.png")
    print("- clean_summary_heatmap.png")
    print("- clean_trend_analysis.png")
    print("- clean_distribution_overview.png")


if __name__ == "__main__":
    main()
