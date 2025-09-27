#include "compressor/serf_qt_gps_stats_compressor.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include <iostream>
#include <iomanip>

SerfQtGpsStatsCompressor::SerfQtGpsStatsCompressor(int block_size, double e_max, 
                                                  double epsilon_v, double epsilon_theta)
    : SerfQtGpsConfigurableCompressor(block_size, e_max, epsilon_v, epsilon_theta) {
}

void SerfQtGpsStatsCompressor::EncodeStrategy(CorrectionFlag strategy, int64_t qv, int64_t qtheta) {
    // 记录策略使用前的比特数
    int bits_before = compressed_size_in_bits_;
    
    // 调用父类方法进行实际编码
    SerfQtGpsConfigurableCompressor::EncodeStrategy(strategy, qv, qtheta);
    
    // 计算本次编码使用的比特数
    int bits_after = compressed_size_in_bits_;
    int bits_used = bits_after - bits_before;
    
    // 更新统计信息
    switch (strategy) {
        case FLAG_ZERO_CORR:
            stats_.zero_corr_count++;
            stats_.total_strategy_bits += 1; // 零校正使用1个比特
            break;
            
        case FLAG_V_ONLY:
            stats_.v_only_count++;
            stats_.total_strategy_bits += 2; // 速度校正使用2个比特
            stats_.total_quantization_bits += (bits_used - 2); // 剩余为量化数据
            break;
            
        case FLAG_THETA_ONLY:
            stats_.theta_only_count++;
            stats_.total_strategy_bits += 3; // 角度校正使用3个比特
            stats_.total_quantization_bits += (bits_used - 3); // 剩余为量化数据
            break;
            
        case FLAG_BOTH:
            stats_.both_count++;
            stats_.total_strategy_bits += 3; // 完全校正使用3个比特
            stats_.total_quantization_bits += (bits_used - 3); // 剩余为量化数据
            break;
    }
}

void SerfQtGpsStatsCompressor::StrategyStats::PrintStats() const {
    int total_points = GetTotalPoints();
    if (total_points == 0) {
        std::cout << "没有策略统计数据" << std::endl;
        return;
    }
    
    std::cout << "\n=== 策略使用分布统计 ===" << std::endl;
    std::cout << "总处理点数: " << total_points << std::endl;
    
    std::cout << "\n各策略使用次数和比例:" << std::endl;
    std::cout << "  FLAG_ZERO_CORR (零校正):   " << std::setw(8) << zero_corr_count 
              << " (" << std::fixed << std::setprecision(2) 
              << (100.0 * zero_corr_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_V_ONLY (速度校正):    " << std::setw(8) << v_only_count 
              << " (" << (100.0 * v_only_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_THETA_ONLY (角度校正): " << std::setw(8) << theta_only_count 
              << " (" << (100.0 * theta_only_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_BOTH (完全校正):      " << std::setw(8) << both_count 
              << " (" << (100.0 * both_count / total_points) << "%)" << std::endl;
    
    // 编码成本分析
    int total_bits = total_strategy_bits + total_quantization_bits;
    std::cout << "\n编码成本分析:" << std::endl;
    std::cout << "  策略标志总比特: " << total_strategy_bits << " bits (" 
              << std::setprecision(1) << (100.0 * total_strategy_bits / total_bits) << "%)" << std::endl;
    std::cout << "  量化数据总比特: " << total_quantization_bits << " bits (" 
              << (100.0 * total_quantization_bits / total_bits) << "%)" << std::endl;
    std::cout << "  总编码比特数: " << total_bits << " bits" << std::endl;
    
    // 平均成本分析
    std::cout << "\n平均编码成本:" << std::endl;
    std::cout << "  平均每点总成本: " << std::setprecision(2) 
              << (static_cast<double>(total_bits) / total_points) << " bits/点" << std::endl;
    std::cout << "  平均策略成本: " 
              << (static_cast<double>(total_strategy_bits) / total_points) << " bits/点" << std::endl;
    std::cout << "  平均量化成本: " 
              << (static_cast<double>(total_quantization_bits) / total_points) << " bits/点" << std::endl;
    
    // 各策略的平均量化成本
    if (v_only_count > 0 || theta_only_count > 0 || both_count > 0) {
        std::cout << "\n各策略平均量化成本:" << std::endl;
        
        if (v_only_count > 0) {
            double avg_v_cost = static_cast<double>(total_quantization_bits) * v_only_count / 
                               (v_only_count + theta_only_count + both_count) / v_only_count;
            std::cout << "  速度校正平均量化成本: " << std::setprecision(1) << avg_v_cost << " bits" << std::endl;
        }
        
        if (theta_only_count > 0) {
            double avg_theta_cost = static_cast<double>(total_quantization_bits) * theta_only_count / 
                                   (v_only_count + theta_only_count + both_count) / theta_only_count;
            std::cout << "  角度校正平均量化成本: " << avg_theta_cost << " bits" << std::endl;
        }
        
        if (both_count > 0) {
            double avg_both_cost = static_cast<double>(total_quantization_bits) * both_count / 
                                  (v_only_count + theta_only_count + both_count) / both_count;
            std::cout << "  完全校正平均量化成本: " << avg_both_cost << " bits" << std::endl;
        }
    }
    
    // 压缩效率分析
    double zero_ratio = static_cast<double>(zero_corr_count) / total_points;
    double correction_ratio = 1.0 - zero_ratio;
    
    std::cout << "\n=== 压缩效率分析 ===" << std::endl;
    std::cout << "零校正比例: " << std::setprecision(2) << (zero_ratio * 100) << "%" << std::endl;
    std::cout << "需要校正比例: " << (correction_ratio * 100) << "%" << std::endl;
    
    // 优化建议
    if (correction_ratio > 0.8) {
        std::cout << "\n🔧 压缩比优化建议 (校正比例过高 " << (correction_ratio * 100) << "%):" << std::endl;
        std::cout << "  1. 🎯 改进预测算法 - 当前预测准确性较低" << std::endl;
        std::cout << "  2. 📏 增大量化步长 - 减少校正需求" << std::endl;
        std::cout << "  3. 🔄 考虑自适应误差界限 - 动态调整精度要求" << std::endl;
        std::cout << "  4. 📊 分析数据特征 - 针对性优化预测模型" << std::endl;
    } else if (correction_ratio > 0.6) {
        std::cout << "\n💡 压缩比可进一步优化 (校正比例较高 " << (correction_ratio * 100) << "%):" << std::endl;
        std::cout << "  1. 🎯 微调预测参数" << std::endl;
        std::cout << "  2. 📏 优化量化策略" << std::endl;
        std::cout << "  3. 🔄 考虑更复杂的预测模型" << std::endl;
    } else if (correction_ratio > 0.4) {
        std::cout << "\n✅ 预测效果良好 (零校正比例 " << (zero_ratio * 100) << "%)，可微调:" << std::endl;
        std::cout << "  1. 📏 精细调整量化参数" << std::endl;
        std::cout << "  2. 🎯 优化边界情况处理" << std::endl;
    } else {
        std::cout << "\n🏆 预测效果优秀！零校正比例达到 " << (zero_ratio * 100) << "%" << std::endl;
    }
    
    // 理论压缩比分析
    std::cout << "\n=== 理论压缩比分析 ===" << std::endl;
    double theoretical_best = 128.0; // 如果全部零校正
    double current_avg_bits = static_cast<double>(total_bits) / total_points;
    double theoretical_current = 128.0 / current_avg_bits;
    
    std::cout << "当前平均编码: " << std::setprecision(2) << current_avg_bits << " bits/点" << std::endl;
    std::cout << "理论压缩比: " << theoretical_current << ":1" << std::endl;
    std::cout << "理论最优 (100%零校正): " << theoretical_best << ":1" << std::endl;
    std::cout << "效率百分比: " << (1.0 / current_avg_bits * 100) << "%" << std::endl;
}
