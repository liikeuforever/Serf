#!/usr/bin/env python3
"""
连续长度分布概览 - 最终简化版本

展示所有数据集在所有max_diff参数下的连续长度分布概览
"""

import os
import csv
import math
import collections
from typing import List, Dict, Tuple
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import warnings

# 设置matplotlib参数
plt.rcParams['font.family'] = 'DejaVu Sans'
plt.rcParams['font.size'] = 8
plt.rcParams['axes.titlesize'] = 10
plt.rcParams['axes.labelsize'] = 8
plt.rcParams['xtick.labelsize'] = 7
plt.rcParams['ytick.labelsize'] = 7
plt.rcParams['legend.fontsize'] = 6
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


def create_comprehensive_distribution_overview(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """Create comprehensive distribution overview showing all datasets and all max_diff values"""
    
    datasets = list(all_results.keys())
    n_datasets = len(datasets)
    n_max_diffs = len(max_diffs)
    
    # Create a large subplot grid: datasets as rows, max_diff as columns
    # Increase figure size and adjust spacing
    fig, axes = plt.subplots(n_datasets, n_max_diffs, figsize=(5*n_max_diffs, 3.5*n_datasets))
    
    # Handle case where we have only one row or column
    if n_datasets == 1:
        axes = axes.reshape(1, -1)
    elif n_max_diffs == 1:
        axes = axes.reshape(-1, 1)
    elif n_datasets == 1 and n_max_diffs == 1:
        axes = np.array([[axes]])
    
    # Color scheme for different max_diff values
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
    
    for i, dataset in enumerate(datasets):
        for j, max_diff in enumerate(max_diffs):
            ax = axes[i, j]
            
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                
                if consecutive_counts:
                    # Prepare data for plotting
                    lengths = list(consecutive_counts.keys())
                    counts = list(consecutive_counts.values())
                    
                    # Sort by length
                    sorted_data = sorted(zip(lengths, counts))
                    lengths, counts = zip(*sorted_data)
                    
                    # Convert to probabilities
                    total = sum(counts)
                    probabilities = [c/total for c in counts]
                    
                    # Create bar plot
                    bars = ax.bar(range(len(lengths)), probabilities, 
                                 alpha=0.7, color=colors[j % len(colors)])
                    
                    # Set x-axis labels (show only key points to avoid crowding)
                    if len(lengths) <= 10:
                        ax.set_xticks(range(len(lengths)))
                        ax.set_xticklabels([str(l) for l in lengths], rotation=45)
                    else:
                        # Show every nth label
                        step = max(1, len(lengths) // 5)
                        tick_positions = range(0, len(lengths), step)
                        ax.set_xticks(tick_positions)
                        ax.set_xticklabels([str(lengths[k]) for k in tick_positions], rotation=45)
                    
                    # Set y-axis to log scale if there's a wide range
                    if max(probabilities) / min(probabilities) > 100:
                        ax.set_yscale('log')
                else:
                    # No data case
                    ax.text(0.5, 0.5, 'No Data', ha='center', va='center', 
                           transform=ax.transAxes, fontsize=10)
            else:
                # Missing data case
                ax.text(0.5, 0.5, 'Missing', ha='center', va='center', 
                       transform=ax.transAxes, fontsize=10)
            
            # Set labels and title
            if i == n_datasets - 1:  # Bottom row
                ax.set_xlabel('Run Length', fontsize=8)
            if j == 0:  # Left column
                ax.set_ylabel('Probability', fontsize=8)
            
            # Set title for top row (max_diff values)
            if i == 0:
                ax.set_title(f'max_diff = {max_diff}', fontsize=10, pad=15)
            
            # Set dataset name on the left with better positioning
            if j == 0:
                ax.text(-0.25, 0.5, dataset.replace('.csv', ''), 
                       rotation=90, transform=ax.transAxes, 
                       ha='center', va='center', fontsize=9, weight='bold')
            
            ax.grid(True, alpha=0.3)
            
            # Adjust tick label size
            ax.tick_params(axis='both', which='major', labelsize=6)
    
    # Add main title
    plt.suptitle('Run Length Distribution Overview - All Datasets vs All max_diff Values', 
                 fontsize=16, y=0.98)
    
    # Adjust layout with more space for labels
    plt.tight_layout()
    plt.subplots_adjust(left=0.12, right=0.98, top=0.94, bottom=0.08, hspace=0.4, wspace=0.35)
    
    # Save with high DPI for clarity
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/final_distribution_overview.png', 
                dpi=300, bbox_inches='tight', facecolor='white')
    plt.show()
    
    print(f"Generated comprehensive overview: {n_datasets} datasets × {n_max_diffs} max_diff values")


def print_summary_statistics(all_results: Dict[str, Dict[float, Dict[int, int]]], max_diffs: List[float]):
    """Print summary statistics for quick reference"""
    print("\n" + "="*80)
    print("SUMMARY STATISTICS")
    print("="*80)
    
    datasets = list(all_results.keys())
    
    for dataset in datasets:
        print(f"\n{dataset.replace('.csv', '')}:")
        print("-" * 40)
        
        for max_diff in max_diffs:
            if max_diff in all_results[dataset]:
                consecutive_counts = all_results[dataset][max_diff]
                
                if consecutive_counts:
                    # Calculate key statistics
                    run_lengths = []
                    for length, count in consecutive_counts.items():
                        run_lengths.extend([length] * count)
                    
                    avg_length = np.mean(run_lengths)
                    median_length = np.median(run_lengths)
                    max_length = max(run_lengths)
                    total_runs = len(run_lengths)
                    single_runs = consecutive_counts.get(1, 0)
                    single_ratio = single_runs / sum(consecutive_counts.values()) * 100
                    
                    print(f"  max_diff={max_diff:5.3f}: avg={avg_length:6.1f}, median={median_length:4.0f}, "
                          f"max={max_length:5.0f}, single_ratio={single_ratio:5.1f}%")


def main():
    """Main function"""
    data_dir = "/Users/xuzihang/GitProject/GG/Serf/test/data_set"
    max_diffs = [0.001, 0.01, 0.1, 1.0, 10.0]
    
    print("=" * 80)
    print("FINAL RUN LENGTH DISTRIBUTION OVERVIEW")
    print("=" * 80)
    
    # Get all CSV files
    csv_files = []
    for filename in os.listdir(data_dir):
        if filename.endswith('.csv'):
            csv_files.append(os.path.join(data_dir, filename))
    
    csv_files.sort()
    print(f"Found {len(csv_files)} CSV files")
    print(f"Will analyze with {len(max_diffs)} max_diff values: {max_diffs}")
    
    # Analyze each file
    all_results = {}
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        results = analyze_dataset(file_path, max_diffs)
        all_results[filename] = results
    
    print(f"\nGenerating comprehensive distribution overview...")
    print(f"Grid size: {len(csv_files)} datasets × {len(max_diffs)} max_diff values")
    
    # Create the comprehensive overview
    create_comprehensive_distribution_overview(all_results, max_diffs)
    
    # Print summary statistics
    print_summary_statistics(all_results, max_diffs)
    
    print(f"\nFinal visualization completed!")
    print("Generated file: final_distribution_overview.png")
    print(f"This shows all {len(csv_files)} datasets across all {len(max_diffs)} max_diff values")


if __name__ == "__main__":
    main()
