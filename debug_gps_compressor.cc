#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>

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

int main() {
    std::cout << "=== 调试GPS轨迹压缩器实际行为 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 5000;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double error_bound = 1e-5;
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << error_bound << " 度" << std::endl;
    std::cout << "  εv = " << epsilon_v << ", εθ = " << epsilon_theta << std::endl;
    
    // 运行实际的GPS轨迹压缩器
    std::cout << "\n🔄 运行GPS轨迹压缩器..." << std::endl;
    
    SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        compressor.AddGpsPoint(point);
    }
    
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    const auto& stats = compressor.GetStrategyStats();
    
    // 打印详细统计
    std::cout << "\n=== 实际GPS轨迹压缩器统计 ===" << std::endl;
    
    int total_points = stats.GetTotalPoints();
    std::cout << "总处理点数: " << total_points << std::endl;
    std::cout << "零校正次数: " << stats.zero_corr_count << " (" 
              << std::fixed << std::setprecision(2) 
              << (100.0 * stats.zero_corr_count / total_points) << "%)" << std::endl;
    std::cout << "速度校正次数: " << stats.v_only_count << " (" 
              << (100.0 * stats.v_only_count / total_points) << "%)" << std::endl;
    std::cout << "角度校正次数: " << stats.theta_only_count << " (" 
              << (100.0 * stats.theta_only_count / total_points) << "%)" << std::endl;
    std::cout << "完全校正次数: " << stats.both_count << " (" 
              << (100.0 * stats.both_count / total_points) << "%)" << std::endl;
    
    // 分析编码成本
    int total_bits = stats.total_strategy_bits + stats.total_quantization_bits;
    std::cout << "\n编码成本分析:" << std::endl;
    std::cout << "策略标志比特: " << stats.total_strategy_bits << " bits" << std::endl;
    std::cout << "量化数据比特: " << stats.total_quantization_bits << " bits" << std::endl;
    std::cout << "总比特数: " << total_bits << " bits" << std::endl;
    std::cout << "平均每点成本: " << std::setprecision(2) 
              << (static_cast<double>(total_bits) / total_points) << " bits/点" << std::endl;
    
    // 压缩比
    int original_size_bytes = points.size() * 16;
    double compression_ratio = static_cast<double>(original_size_bytes) / compressed_data.length();
    std::cout << "\n压缩结果:" << std::endl;
    std::cout << "原始大小: " << original_size_bytes << " 字节" << std::endl;
    std::cout << "压缩大小: " << compressed_data.length() << " 字节" << std::endl;
    std::cout << "压缩比: " << compression_ratio << ":1" << std::endl;
    
    // 对比我的模拟结果
    std::cout << "\n=== 与模拟结果对比 ===" << std::endl;
    std::cout << "实际零校正率: " << (100.0 * stats.zero_corr_count / total_points) << "%" << std::endl;
    std::cout << "模拟零校正率: 13.71%" << std::endl;
    
    double diff = std::abs((100.0 * stats.zero_corr_count / total_points) - 13.71);
    if (diff < 2.0) {
        std::cout << "✅ 模拟结果与实际结果基本一致 (差异: " << diff << "%)" << std::endl;
    } else {
        std::cout << "❌ 模拟结果与实际结果存在差异 (差异: " << diff << "%)" << std::endl;
        std::cout << "可能的原因:" << std::endl;
        std::cout << "1. 模拟逻辑不完全准确" << std::endl;
        std::cout << "2. 几何剪枝逻辑的影响" << std::endl;
        std::cout << "3. 策略选择算法的复杂性" << std::endl;
    }
    
    // 分析为什么压缩比低
    std::cout << "\n=== 压缩比分析 ===" << std::endl;
    std::cout << "当前零校正率: " << (100.0 * stats.zero_corr_count / total_points) << "%" << std::endl;
    std::cout << "需要校正的点: " << (100.0 * (total_points - stats.zero_corr_count) / total_points) << "%" << std::endl;
    
    if (stats.zero_corr_count < total_points * 0.3) {
        std::cout << "\n🚨 零校正率过低是压缩比低的主要原因!" << std::endl;
        std::cout << "问题分析:" << std::endl;
        std::cout << "1. 预测模型可能不适合这个数据集的特征" << std::endl;
        std::cout << "2. 误差界限可能设置过于严格" << std::endl;
        std::cout << "3. 量化参数可能需要调整" << std::endl;
        
        // 估算如果提高零校正率的收益
        std::vector<double> target_rates = {0.3, 0.5, 0.7};
        std::cout << "\n💡 提高零校正率的潜在收益:" << std::endl;
        
        double current_avg_bits = static_cast<double>(total_bits) / total_points;
        double current_compression_ratio = 128.0 / current_avg_bits;
        
        for (double target_rate : target_rates) {
            // 简化估算：假设其余点使用当前的平均校正成本
            double correction_avg_bits = static_cast<double>(stats.total_quantization_bits + stats.total_strategy_bits - stats.zero_corr_count) 
                                        / (total_points - stats.zero_corr_count);
            
            double estimated_avg_bits = target_rate * 1.0 + (1.0 - target_rate) * correction_avg_bits;
            double estimated_compression_ratio = 128.0 / estimated_avg_bits;
            double improvement = (estimated_compression_ratio / current_compression_ratio - 1) * 100;
            
            std::cout << "  零校正率 " << std::setprecision(0) << (target_rate * 100) << "%: "
                      << "压缩比 " << std::setprecision(2) << estimated_compression_ratio << ":1 "
                      << "(提升 " << std::setprecision(1) << improvement << "%)" << std::endl;
        }
    }
    
    return 0;
}
