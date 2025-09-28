#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

#include "compressor/serf_qt_gps_configurable_compressor.h"

// 读取GPS点数据
std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> ReadGpsPoints(const std::string& filename, int max_points) {
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    std::string line;
    std::getline(file, line); // 跳过标题行
    
    int count = 0;
    while (std::getline(file, line) && count < max_points) {
        std::istringstream iss(line);
        std::string longitude_str, latitude_str;
        
        if (std::getline(iss, longitude_str, ',') && std::getline(iss, latitude_str)) {
            try {
                double longitude = std::stod(longitude_str);
                double latitude = std::stod(latitude_str);
                points.emplace_back(longitude, latitude);
                count++;
            } catch (const std::exception& e) {
                continue;
            }
        }
    }
    
    return points;
}

void AnalyzeStrategyDistribution(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                double error_bound, double epsilon_v, double epsilon_theta) {
    
    std::cout << "\n=== 策略分布详细分析 ===" << std::endl;
    std::cout << "测试点数: " << points.size() << std::endl;
    std::cout << "误差界限: " << std::scientific << error_bound << " 度" << std::endl;
    std::cout << "参数设置: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        compressor.AddGpsPoint(point);
    }
    
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    const auto& stats = compressor.GetStrategyStats();
    
    int total_points = stats.GetTotalPoints();
    int total_bits = stats.total_strategy_bits + stats.total_quantization_bits;
    
    std::cout << "\n📊 策略使用统计:" << std::endl;
    std::cout << "  总处理点数: " << total_points << std::endl;
    std::cout << "  FLAG_ZERO_CORR (零校正):   " << std::setw(8) << stats.zero_corr_count 
              << " (" << std::fixed << std::setprecision(2) 
              << (100.0 * stats.zero_corr_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_V_ONLY (速度校正):    " << std::setw(8) << stats.v_only_count 
              << " (" << (100.0 * stats.v_only_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_THETA_ONLY (角度校正): " << std::setw(8) << stats.theta_only_count 
              << " (" << (100.0 * stats.theta_only_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_BOTH (完全校正):      " << std::setw(8) << stats.both_count 
              << " (" << (100.0 * stats.both_count / total_points) << "%)" << std::endl;
    
    // 计算各策略的平均编码成本
    std::cout << "\n💰 各策略编码成本分析:" << std::endl;
    
    // 零校正成本
    int zero_corr_bits = stats.zero_corr_count * 1; // 每次1个比特
    std::cout << "  零校正总成本: " << zero_corr_bits << " bits (" 
              << std::setprecision(1) << (100.0 * zero_corr_bits / total_bits) << "%)" << std::endl;
    std::cout << "    平均成本: 1.0 bits/点" << std::endl;
    
    // 其他策略的量化成本分布
    int correction_count = stats.v_only_count + stats.theta_only_count + stats.both_count;
    double avg_quantization_per_correction = 0.0;
    if (correction_count > 0) {
        // 估算各策略的平均量化成本
        avg_quantization_per_correction = static_cast<double>(stats.total_quantization_bits) / correction_count;
        
        std::cout << "  速度校正平均成本: " << std::setprecision(1) 
                  << (2.0 + avg_quantization_per_correction * stats.v_only_count / correction_count) << " bits/点" << std::endl;
        std::cout << "  角度校正平均成本: " 
                  << (3.0 + avg_quantization_per_correction * stats.theta_only_count / correction_count) << " bits/点" << std::endl;
        std::cout << "  完全校正平均成本: " 
                  << (3.0 + avg_quantization_per_correction * stats.both_count / correction_count) << " bits/点" << std::endl;
    }
    
    // 分析压缩效率
    std::cout << "\n📈 压缩效率分析:" << std::endl;
    double zero_corr_ratio = static_cast<double>(stats.zero_corr_count) / total_points;
    double light_correction_ratio = static_cast<double>(stats.v_only_count + stats.theta_only_count) / total_points;
    double heavy_correction_ratio = static_cast<double>(stats.both_count) / total_points;
    
    std::cout << "  零校正比例: " << std::setprecision(2) << (zero_corr_ratio * 100) << "%" << std::endl;
    std::cout << "  轻量校正比例 (V_ONLY + THETA_ONLY): " << (light_correction_ratio * 100) << "%" << std::endl;
    std::cout << "  重量校正比例 (BOTH): " << (heavy_correction_ratio * 100) << "%" << std::endl;
    
    // 理论压缩比分析
    std::cout << "\n🎯 理论压缩比分析:" << std::endl;
    double current_avg_bits = static_cast<double>(total_bits) / total_points;
    double current_compression_ratio = 128.0 / current_avg_bits;
    
    std::cout << "  当前平均编码成本: " << current_avg_bits << " bits/点" << std::endl;
    std::cout << "  当前压缩比: " << current_compression_ratio << ":1" << std::endl;
    
    // 模拟不同零校正比例下的压缩比
    std::cout << "\n🚀 零校正比例提升的潜在收益:" << std::endl;
    
    std::vector<double> target_zero_ratios = {0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8};
    
    for (double target_ratio : target_zero_ratios) {
        // 假设提升零校正比例，其余按当前比例分配
        double remaining_ratio = 1.0 - target_ratio;
        double current_correction_ratio = 1.0 - zero_corr_ratio;
        
        double new_v_only_ratio = remaining_ratio * (stats.v_only_count / static_cast<double>(correction_count));
        double new_theta_only_ratio = remaining_ratio * (stats.theta_only_count / static_cast<double>(correction_count));
        double new_both_ratio = remaining_ratio * (stats.both_count / static_cast<double>(correction_count));
        
        // 估算新的平均编码成本
        double estimated_avg_bits = target_ratio * 1.0 + // 零校正
                                   new_v_only_ratio * (2.0 + avg_quantization_per_correction) + // 速度校正
                                   new_theta_only_ratio * (3.0 + avg_quantization_per_correction) + // 角度校正
                                   new_both_ratio * (3.0 + avg_quantization_per_correction * 2); // 完全校正
        
        double estimated_compression_ratio = 128.0 / estimated_avg_bits;
        double improvement = estimated_compression_ratio / current_compression_ratio;
        
        std::cout << "  零校正比例 " << std::setprecision(0) << (target_ratio * 100) << "%: "
                  << "压缩比 " << std::setprecision(2) << estimated_compression_ratio << ":1 "
                  << "(提升 " << std::setprecision(1) << ((improvement - 1) * 100) << "%)" << std::endl;
    }
    
    // 分析当前预测失败的原因
    std::cout << "\n🔍 预测失败原因分析:" << std::endl;
    std::cout << "  当前零校正率: " << (zero_corr_ratio * 100) << "%" << std::endl;
    std::cout << "  预测失败率: " << ((1.0 - zero_corr_ratio) * 100) << "%" << std::endl;
    
    if (zero_corr_ratio < 0.3) {
        std::cout << "  🚨 零校正率过低！主要问题:" << std::endl;
        std::cout << "    1. 预测模型过于简单 - 仅使用一阶运动预测" << std::endl;
        std::cout << "    2. GPS轨迹复杂性 - 包含转弯、停留、加速等非线性运动" << std::endl;
        std::cout << "    3. 误差界限可能过于严格" << std::endl;
    } else if (zero_corr_ratio < 0.5) {
        std::cout << "  ⚠️  零校正率偏低，有改进空间" << std::endl;
    } else {
        std::cout << "  ✅ 零校正率良好" << std::endl;
    }
    
    // 优化建议
    std::cout << "\n💡 优化建议:" << std::endl;
    std::cout << "1. 改进预测模型:" << std::endl;
    std::cout << "   - 使用二阶或三阶预测模型" << std::endl;
    std::cout << "   - 引入轨迹曲率分析" << std::endl;
    std::cout << "   - 基于历史模式的智能预测" << std::endl;
    
    std::cout << "2. 优化策略选择:" << std::endl;
    std::cout << "   - 减少策略标志开销" << std::endl;
    std::cout << "   - 使用概率编码或熵编码" << std::endl;
    std::cout << "   - 动态调整策略阈值" << std::endl;
    
    std::cout << "3. 自适应参数调整:" << std::endl;
    std::cout << "   - 根据轨迹特征调整量化步长" << std::endl;
    std::cout << "   - 在直线段使用更大的误差容忍度" << std::endl;
    std::cout << "   - 分段处理不同复杂度的轨迹" << std::endl;
    
    if (zero_corr_ratio < 0.2) {
        std::cout << "\n🎯 紧急优化目标: 将零校正率提升到30%以上" << std::endl;
        std::cout << "   预期收益: 压缩比可提升 " << std::setprecision(0) 
                  << ((128.0 / (0.3 * 1.0 + 0.7 * current_avg_bits) / current_compression_ratio - 1) * 100) 
                  << "%" << std::endl;
    }
}

int main() {
    std::cout << "=== GPS轨迹压缩策略分布详细分析 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 30000;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 使用最佳参数进行分析
    const double error_bound = 1e-5;
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    AnalyzeStrategyDistribution(points, error_bound, epsilon_v, epsilon_theta);
    
    return 0;
}
