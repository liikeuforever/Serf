#!/usr/bin/env python3
"""
生成实验结果对比表格

基于压缩比(CR)、压缩时间(CT)、解压时间(DT)数据生成类似实验论文的结果表格
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.colors import LinearSegmentedColormap
import seaborn as sns

# 设置matplotlib参数
plt.rcParams['font.family'] = 'DejaVu Sans'
plt.rcParams['font.size'] = 9
plt.rcParams['axes.titlesize'] = 12
plt.rcParams['figure.titlesize'] = 14

# 数据集名称映射
DATASET_NAMES = ['AP', 'BT', 'BW', 'CDT', 'CT', 'DT', 'IR', 'MT', 'PM10', 'SG', 'SUSA', 'TD', 'WS']

def load_experiment_data():
    """加载实验数据"""
    print("Loading experiment data...")
    
    # 读取压缩比数据
    cr_data = pd.read_csv('/Users/xuzihang/GitProject/GG/Serf/test/overall_cr_table.csv', header=None)
    cr_data = cr_data.iloc[:, :-1]  # 移除最后一列空列
    cr_data.columns = ['Algorithm'] + DATASET_NAMES
    cr_data = cr_data[cr_data['Algorithm'].notna()]  # 移除空行
    
    # 读取压缩时间数据
    ct_data = pd.read_csv('/Users/xuzihang/GitProject/GG/Serf/test/overall_ct_table.csv', header=None)
    ct_data = ct_data.iloc[:, :-1]
    ct_data.columns = ['Algorithm'] + DATASET_NAMES
    ct_data = ct_data[ct_data['Algorithm'].notna()]
    
    # 读取解压时间数据
    dt_data = pd.read_csv('/Users/xuzihang/GitProject/GG/Serf/test/overall_dt_table.csv', header=None)
    dt_data = dt_data.iloc[:, :-1]
    dt_data.columns = ['Algorithm'] + DATASET_NAMES
    dt_data = dt_data[dt_data['Algorithm'].notna()]
    
    return cr_data, ct_data, dt_data


def calculate_averages(data):
    """计算每个算法的平均值"""
    numeric_cols = [col for col in data.columns if col != 'Algorithm']
    data['Avg.'] = data[numeric_cols].mean(axis=1)
    return data


def create_experiment_table_visualization(cr_data, ct_data, dt_data):
    """创建实验结果表格可视化"""
    
    # 计算平均值
    cr_data = calculate_averages(cr_data.copy())
    ct_data = calculate_averages(ct_data.copy())
    dt_data = calculate_averages(dt_data.copy())
    
    # 创建图表
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(16, 20))
    
    # 算法分类和颜色
    algorithm_categories = {
        'Lossless': ['LZ77', 'Zstd', 'Snappy', 'SZ2', 'Machete', 'SimPiece', 'Deflate', 'LZ4'],
        'Streaming Lossless': ['FPC', 'Gorilla', 'Chimp128', 'Elf'],
        'Lossy': ['SerfQt', 'SerfXOR', 'SerfXOR_ZeroOpt', 'SerfXOR_FastSearch', 'SerfXOR_CombinedOpt']
    }
    
    category_colors = {
        'Lossless': '#E8F4FD',
        'Streaming Lossless': '#FFF2CC', 
        'Lossy': '#F8CECC'
    }
    
    def create_table(ax, data, title, value_format='{:.2f}', highlight_best='min'):
        """创建单个表格"""
        
        # 准备数据
        algorithms = data['Algorithm'].tolist()
        numeric_data = data.drop('Algorithm', axis=1)
        
        # 创建表格
        table_data = []
        for i, alg in enumerate(algorithms):
            row = [alg] + [value_format.format(val) for val in numeric_data.iloc[i]]
            table_data.append(row)
        
        # 列名
        columns = ['Algorithm'] + DATASET_NAMES + ['Avg.']
        
        # 创建表格
        table = ax.table(cellText=table_data,
                        colLabels=columns,
                        cellLoc='center',
                        loc='center',
                        bbox=[0, 0, 1, 1])
        
        # 设置表格样式
        table.auto_set_font_size(False)
        table.set_fontsize(8)
        table.scale(1, 2.5)
        
        # 设置标题行样式
        for i in range(len(columns)):
            table[(0, i)].set_facecolor('#4472C4')
            table[(0, i)].set_text_props(weight='bold', color='white')
            table[(0, i)].set_height(0.08)
        
        # 设置算法分类背景色和突出显示最佳结果
        for i, alg in enumerate(algorithms):
            row_idx = i + 1
            
            # 设置分类背景色
            category = None
            for cat, algs in algorithm_categories.items():
                if alg in algs:
                    category = cat
                    break
            
            if category:
                # 算法名称列
                table[(row_idx, 0)].set_facecolor(category_colors[category])
                table[(row_idx, 0)].set_text_props(weight='bold')
                
                # 数据列
                for j in range(1, len(columns)):
                    table[(row_idx, j)].set_facecolor(category_colors[category])
        
        # 突出显示最佳结果
        for j in range(1, len(columns)):
            col_values = [float(table_data[i][j]) for i in range(len(algorithms))]
            if highlight_best == 'min':
                best_idx = np.argmin(col_values)
            else:
                best_idx = np.argmax(col_values)
            
            # 加粗最佳结果
            table[(best_idx + 1, j)].set_text_props(weight='bold')
            table[(best_idx + 1, j)].set_facecolor('#90EE90')  # 浅绿色
        
        ax.set_title(title, fontsize=14, fontweight='bold', pad=20)
        ax.axis('off')
        
        return table
    
    # 创建三个表格
    print("Creating Compression Ratio table...")
    create_table(ax1, cr_data, 'Compression Ratio (Lower is Better)', '{:.2f}', 'min')
    
    print("Creating Compression Time table...")
    create_table(ax2, ct_data, 'Compression Time in ms (Lower is Better)', '{:.3f}', 'min')
    
    print("Creating Decompression Time table...")
    create_table(ax3, dt_data, 'Decompression Time in ms (Lower is Better)', '{:.3f}', 'min')
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/experiment_results_table.png', 
                dpi=300, bbox_inches='tight', facecolor='white')
    plt.show()


def create_summary_comparison():
    """创建汇总对比图"""
    cr_data, ct_data, dt_data = load_experiment_data()
    
    # 计算平均值
    cr_data = calculate_averages(cr_data.copy())
    ct_data = calculate_averages(ct_data.copy())
    dt_data = calculate_averages(dt_data.copy())
    
    # 提取平均值
    algorithms = cr_data['Algorithm'].tolist()
    cr_avg = cr_data['Avg.'].tolist()
    ct_avg = ct_data['Avg.'].tolist()
    dt_avg = dt_data['Avg.'].tolist()
    
    # 创建对比图
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    
    # 算法颜色映射
    colors = plt.cm.Set3(np.linspace(0, 1, len(algorithms)))
    
    # 压缩比对比
    bars1 = ax1.bar(range(len(algorithms)), cr_avg, color=colors)
    ax1.set_title('Average Compression Ratio', fontweight='bold')
    ax1.set_ylabel('Compression Ratio')
    ax1.set_xticks(range(len(algorithms)))
    ax1.set_xticklabels(algorithms, rotation=45, ha='right')
    ax1.grid(axis='y', alpha=0.3)
    
    # 压缩时间对比
    bars2 = ax2.bar(range(len(algorithms)), ct_avg, color=colors)
    ax2.set_title('Average Compression Time', fontweight='bold')
    ax2.set_ylabel('Time (ms)')
    ax2.set_xticks(range(len(algorithms)))
    ax2.set_xticklabels(algorithms, rotation=45, ha='right')
    ax2.set_yscale('log')
    ax2.grid(axis='y', alpha=0.3)
    
    # 解压时间对比
    bars3 = ax3.bar(range(len(algorithms)), dt_avg, color=colors)
    ax3.set_title('Average Decompression Time', fontweight='bold')
    ax3.set_ylabel('Time (ms)')
    ax3.set_xticks(range(len(algorithms)))
    ax3.set_xticklabels(algorithms, rotation=45, ha='right')
    ax3.set_yscale('log')
    ax3.grid(axis='y', alpha=0.3)
    
    # 综合性能散点图 (压缩比 vs 压缩时间)
    ax4.scatter(cr_avg, ct_avg, c=colors, s=100, alpha=0.7)
    for i, alg in enumerate(algorithms):
        ax4.annotate(alg, (cr_avg[i], ct_avg[i]), xytext=(5, 5), 
                    textcoords='offset points', fontsize=8)
    ax4.set_xlabel('Compression Ratio')
    ax4.set_ylabel('Compression Time (ms)')
    ax4.set_title('Compression Ratio vs Time Trade-off', fontweight='bold')
    ax4.set_yscale('log')
    ax4.grid(alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/experiment_summary_comparison.png', 
                dpi=300, bbox_inches='tight', facecolor='white')
    plt.show()


def create_serf_variants_focus():
    """专门对比SERF变体的性能"""
    cr_data, ct_data, dt_data = load_experiment_data()
    
    # 筛选SERF相关算法
    serf_algorithms = ['SerfQt', 'SerfXOR', 'SerfXOR_ZeroOpt', 'SerfXOR_FastSearch', 'SerfXOR_CombinedOpt']
    
    cr_serf = cr_data[cr_data['Algorithm'].isin(serf_algorithms)].copy()
    ct_serf = ct_data[ct_data['Algorithm'].isin(serf_algorithms)].copy()
    dt_serf = dt_data[dt_data['Algorithm'].isin(serf_algorithms)].copy()
    
    # 计算平均值
    cr_serf = calculate_averages(cr_serf)
    ct_serf = calculate_averages(ct_serf)
    dt_serf = calculate_averages(dt_serf)
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    
    # SERF变体性能热力图
    datasets = DATASET_NAMES + ['Avg.']
    
    # 压缩比热力图
    ax = axes[0, 0]
    cr_matrix = cr_serf.drop('Algorithm', axis=1).values
    im1 = ax.imshow(cr_matrix, cmap='RdYlGn_r', aspect='auto')
    ax.set_xticks(range(len(datasets)))
    ax.set_xticklabels(datasets, rotation=45)
    ax.set_yticks(range(len(serf_algorithms)))
    ax.set_yticklabels(serf_algorithms)
    ax.set_title('SERF Variants - Compression Ratio', fontweight='bold')
    plt.colorbar(im1, ax=ax, shrink=0.8)
    
    # 在热力图上添加数值
    for i in range(len(serf_algorithms)):
        for j in range(len(datasets)):
            ax.text(j, i, f'{cr_matrix[i, j]:.3f}', 
                   ha='center', va='center', color='black', fontsize=8)
    
    # 压缩时间热力图
    ax = axes[0, 1]
    ct_matrix = ct_serf.drop('Algorithm', axis=1).values
    im2 = ax.imshow(ct_matrix, cmap='RdYlGn_r', aspect='auto')
    ax.set_xticks(range(len(datasets)))
    ax.set_xticklabels(datasets, rotation=45)
    ax.set_yticks(range(len(serf_algorithms)))
    ax.set_yticklabels(serf_algorithms)
    ax.set_title('SERF Variants - Compression Time', fontweight='bold')
    plt.colorbar(im2, ax=ax, shrink=0.8)
    
    # 解压时间热力图
    ax = axes[1, 0]
    dt_matrix = dt_serf.drop('Algorithm', axis=1).values
    im3 = ax.imshow(dt_matrix, cmap='RdYlGn_r', aspect='auto')
    ax.set_xticks(range(len(datasets)))
    ax.set_xticklabels(datasets, rotation=45)
    ax.set_yticks(range(len(serf_algorithms)))
    ax.set_yticklabels(serf_algorithms)
    ax.set_title('SERF Variants - Decompression Time', fontweight='bold')
    plt.colorbar(im3, ax=ax, shrink=0.8)
    
    # SERF变体平均性能对比
    ax = axes[1, 1]
    x_pos = np.arange(len(serf_algorithms))
    width = 0.25
    
    ax.bar(x_pos - width, cr_serf['Avg.'], width, label='Compression Ratio', alpha=0.8)
    ax.bar(x_pos, ct_serf['Avg.'], width, label='Compression Time', alpha=0.8)
    ax.bar(x_pos + width, dt_serf['Avg.'], width, label='Decompression Time', alpha=0.8)
    
    ax.set_xlabel('SERF Variants')
    ax.set_ylabel('Performance Metrics')
    ax.set_title('SERF Variants - Average Performance', fontweight='bold')
    ax.set_xticks(x_pos)
    ax.set_xticklabels(serf_algorithms, rotation=45, ha='right')
    ax.legend()
    ax.set_yscale('log')
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('/Users/xuzihang/GitProject/GG/Serf/serf_variants_analysis.png', 
                dpi=300, bbox_inches='tight', facecolor='white')
    plt.show()


def print_performance_summary():
    """打印性能总结"""
    cr_data, ct_data, dt_data = load_experiment_data()
    
    cr_data = calculate_averages(cr_data.copy())
    ct_data = calculate_averages(ct_data.copy())
    dt_data = calculate_averages(dt_data.copy())
    
    print("\n" + "="*80)
    print("PERFORMANCE SUMMARY")
    print("="*80)
    
    print("\nBest Compression Ratio:")
    best_cr_idx = cr_data['Avg.'].idxmin()
    print(f"  {cr_data.iloc[best_cr_idx]['Algorithm']}: {cr_data.iloc[best_cr_idx]['Avg.']:.4f}")
    
    print("\nFastest Compression:")
    best_ct_idx = ct_data['Avg.'].idxmin()
    print(f"  {ct_data.iloc[best_ct_idx]['Algorithm']}: {ct_data.iloc[best_ct_idx]['Avg.']:.4f} ms")
    
    print("\nFastest Decompression:")
    best_dt_idx = dt_data['Avg.'].idxmin()
    print(f"  {dt_data.iloc[best_dt_idx]['Algorithm']}: {dt_data.iloc[best_dt_idx]['Avg.']:.4f} ms")
    
    print("\nSERF Variants Ranking (by Compression Ratio):")
    serf_algorithms = ['SerfQt', 'SerfXOR', 'SerfXOR_ZeroOpt', 'SerfXOR_FastSearch', 'SerfXOR_CombinedOpt']
    serf_cr = cr_data[cr_data['Algorithm'].isin(serf_algorithms)].sort_values('Avg.')
    for i, (_, row) in enumerate(serf_cr.iterrows(), 1):
        print(f"  {i}. {row['Algorithm']}: {row['Avg.']:.4f}")


def main():
    """主函数"""
    print("="*80)
    print("EXPERIMENT RESULTS VISUALIZATION")
    print("="*80)
    
    # 加载数据
    cr_data, ct_data, dt_data = load_experiment_data()
    
    print(f"Loaded data for {len(cr_data)} algorithms across {len(DATASET_NAMES)} datasets")
    
    print("\n1. Creating experiment results table...")
    create_experiment_table_visualization(cr_data, ct_data, dt_data)
    
    print("\n2. Creating summary comparison...")
    create_summary_comparison()
    
    print("\n3. Creating SERF variants analysis...")
    create_serf_variants_focus()
    
    print("\n4. Printing performance summary...")
    print_performance_summary()
    
    print("\n" + "="*80)
    print("All visualizations completed!")
    print("Generated files:")
    print("- experiment_results_table.png")
    print("- experiment_summary_comparison.png") 
    print("- serf_variants_analysis.png")
    print("="*80)


if __name__ == "__main__":
    main()
