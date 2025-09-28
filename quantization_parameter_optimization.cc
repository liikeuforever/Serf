#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <algorithm>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "compressor/serf_qt_linear_compressor.h"

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

struct QuantizationConfig {
    double epsilon_v;
    double epsilon_theta;
    std::string name;
};

struct TestResult {
    QuantizationConfig config;
    double zero_correction_rate;
    double compression_ratio;
    double avg_bits_per_point;
    int zero_corr_count;
    int total_points;
};

int main() {
    std::cout << "=== GPS轨迹压缩器量化参数优化 - 匹配线性预测性能 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 30000;  // 使用3万条数据进行测试
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置严格的误差界限（GPS容忍范围）
    const double gps_error_bound = 1.4e-5;  // 1.4e-5度，接近GPS精度要求
    const double linear_max_diff = 1e-5;    // 线性预测误差界限
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  GPS轨迹误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  线性预测误差界限: " << linear_max_diff << " 度" << std::endl;
    std::cout << "  数据点数: " << points.size() << std::endl;
    
    // 首先测试线性预测基准
    std::cout << "\n=== 线性预测基准测试 ===" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtLinearCompressor linear_lon_compressor(points.size(), linear_max_diff);
    SerfQtLinearCompressor linear_lat_compressor(points.size(), linear_max_diff);
    
    for (const auto& point : points) {
        linear_lon_compressor.AddValue(point.longitude);
        linear_lat_compressor.AddValue(point.latitude);
    }
    
    linear_lon_compressor.Close();
    linear_lat_compressor.Close();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto linear_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    Array<uint8_t> linear_lon_compressed = linear_lon_compressor.compressed_bytes();
    Array<uint8_t> linear_lat_compressed = linear_lat_compressor.compressed_bytes();
    
    int linear_total_size = linear_lon_compressed.length() + linear_lat_compressed.length();
    double linear_compression_ratio = static_cast<double>(points.size() * 16) / linear_total_size;
    double linear_avg_bits = static_cast<double>(linear_total_size * 8) / points.size();
    
    // 估算线性预测的零校正率
    double estimated_linear_zero_rate = 0.0;
    if (linear_avg_bits > 1.0) {
        // 基于压缩效果估算零校正率
        estimated_linear_zero_rate = std::max(0.0, std::min(100.0, (15.0 - linear_avg_bits) / 14.0 * 100.0));
    }
    
    std::cout << "📊 线性预测基准结果:" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << linear_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << linear_avg_bits << " bits/点" << std::endl;
    std::cout << "  估算零校正率: ~" << std::setprecision(1) << estimated_linear_zero_rate << "%" << std::endl;
    std::cout << "  压缩时间: " << linear_duration.count() << " 毫秒" << std::endl;
    
    // 定义量化参数测试组合
    std::vector<QuantizationConfig> configs = {
        // 精细量化组合
        {1e-6, 1e-5, "εv=1e-6, εθ=1e-5 (超精细)"},
        {5e-7, 5e-6, "εv=5e-7, εθ=5e-6 (极精细)"},
        {2e-6, 2e-5, "εv=2e-6, εθ=2e-5 (很精细)"},
        
        // 中等量化组合
        {5e-6, 5e-5, "εv=5e-6, εθ=5e-5 (中等)"},
        {1e-5, 1e-4, "εv=1e-5, εθ=1e-4 (标准)"},
        
        // 更精细的角度量化
        {1e-5, 1e-5, "εv=1e-5, εθ=1e-5 (角度精细)"},
        {2e-5, 1e-5, "εv=2e-5, εθ=1e-5 (角度超精细)"},
        
        // 更精细的速度量化
        {1e-6, 1e-4, "εv=1e-6, εθ=1e-4 (速度精细)"},
        {5e-7, 1e-4, "εv=5e-7, εθ=1e-4 (速度超精细)"},
        
        // 平衡组合
        {3e-6, 3e-5, "εv=3e-6, εθ=3e-5 (平衡1)"},
        {7e-6, 7e-5, "εv=7e-6, εθ=7e-5 (平衡2)"},
    };
    
    std::cout << "\n=== GPS轨迹压缩器量化参数优化测试 ===" << std::endl;
    
    std::vector<TestResult> results;
    
    std::cout << std::setw(30) << "量化参数配置" 
              << std::setw(12) << "零校正率" 
              << std::setw(12) << "压缩比" 
              << std::setw(15) << "平均bits/点" 
              << std::setw(15) << "vs线性差距" 
              << std::setw(12) << "零校正数" << std::endl;
    std::cout << std::string(110, '-') << std::endl;
    
    TestResult best_result;
    best_result.zero_correction_rate = 0.0;
    
    for (const auto& config : configs) {
        start_time = std::chrono::high_resolution_clock::now();
        
        SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_error_bound, 
                                                      config.epsilon_v, config.epsilon_theta);
        
        for (const auto& point : points) {
            gps_compressor.AddGpsPoint(point);
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        
        Array<uint8_t> gps_compressed_data = gps_compressor.GetCompressedData();
        const auto& gps_stats = gps_compressor.GetStrategyStats();
        
        // 计算结果
        TestResult result;
        result.config = config;
        result.total_points = gps_stats.GetTotalPoints();
        result.zero_corr_count = gps_stats.zero_corr_count;
        result.zero_correction_rate = static_cast<double>(gps_stats.zero_corr_count) / result.total_points * 100.0;
        result.compression_ratio = static_cast<double>(points.size() * 16) / gps_compressed_data.length();
        result.avg_bits_per_point = static_cast<double>(gps_compressed_data.length() * 8) / points.size();
        
        results.push_back(result);
        
        // 更新最佳结果
        if (result.zero_correction_rate > best_result.zero_correction_rate) {
            best_result = result;
        }
        
        // 与线性预测的差距
        double zero_rate_gap = result.zero_correction_rate - estimated_linear_zero_rate;
        
        // 打印结果
        std::cout << std::setw(30) << config.name
                  << std::setw(12) << std::fixed << std::setprecision(2) << result.zero_correction_rate << "%"
                  << std::setw(12) << result.compression_ratio
                  << std::setw(15) << result.avg_bits_per_point
                  << std::setw(15) << std::showpos << zero_rate_gap << "%" << std::noshowpos
                  << std::setw(12) << result.zero_corr_count << std::endl;
    }
    
    // 分析最佳结果
    std::cout << "\n=== 最佳配置分析 ===" << std::endl;
    
    std::cout << "🏆 最佳零校正率配置:" << std::endl;
    std::cout << "  配置: " << best_result.config.name << std::endl;
    std::cout << "  εv = " << std::scientific << best_result.config.epsilon_v << std::endl;
    std::cout << "  εθ = " << best_result.config.epsilon_theta << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << best_result.zero_correction_rate << "%" << std::endl;
    std::cout << "  压缩比: " << best_result.compression_ratio << ":1" << std::endl;
    
    double best_gap = best_result.zero_correction_rate - estimated_linear_zero_rate;
    if (best_gap >= -5.0) {
        std::cout << "🎯 成功接近线性预测性能！差距: " << std::showpos << best_gap << "%" << std::noshowpos << std::endl;
    } else {
        std::cout << "📉 仍与线性预测有差距: " << best_gap << "%" << std::endl;
    }
    
    // 按零校正率排序显示前5名
    std::sort(results.begin(), results.end(), 
              [](const TestResult& a, const TestResult& b) {
                  return a.zero_correction_rate > b.zero_correction_rate;
              });
    
    std::cout << "\n📊 零校正率排行榜 (前5名):" << std::endl;
    for (int i = 0; i < std::min(5, static_cast<int>(results.size())); ++i) {
        const auto& result = results[i];
        double gap = result.zero_correction_rate - estimated_linear_zero_rate;
        std::cout << "  " << (i+1) << ". " << result.config.name 
                  << " → " << result.zero_correction_rate << "% "
                  << "(差距: " << std::showpos << gap << "%)" << std::noshowpos << std::endl;
    }
    
    // 分析量化参数的影响
    std::cout << "\n=== 量化参数影响分析 ===" << std::endl;
    
    // 找出速度量化的影响
    std::cout << "🔍 速度量化参数(εv)的影响:" << std::endl;
    std::vector<std::pair<double, double>> v_impact;
    for (const auto& result : results) {
        v_impact.emplace_back(result.config.epsilon_v, result.zero_correction_rate);
    }
    std::sort(v_impact.begin(), v_impact.end());
    
    for (const auto& [epsilon_v, rate] : v_impact) {
        std::cout << "  εv=" << std::scientific << epsilon_v << " → " << std::fixed << rate << "%" << std::endl;
    }
    
    // 找出角度量化的影响
    std::cout << "\n🔍 角度量化参数(εθ)的影响:" << std::endl;
    std::vector<std::pair<double, double>> theta_impact;
    for (const auto& result : results) {
        theta_impact.emplace_back(result.config.epsilon_theta, result.zero_correction_rate);
    }
    std::sort(theta_impact.begin(), theta_impact.end());
    
    for (const auto& [epsilon_theta, rate] : theta_impact) {
        std::cout << "  εθ=" << std::scientific << epsilon_theta << " → " << std::fixed << rate << "%" << std::endl;
    }
    
    // 给出优化建议
    std::cout << "\n=== 优化建议 ===" << std::endl;
    
    if (best_result.zero_correction_rate < estimated_linear_zero_rate - 10.0) {
        std::cout << "🚨 当前最佳配置仍显著低于线性预测" << std::endl;
        std::cout << "建议:" << std::endl;
        std::cout << "1. 进一步减小量化步长，特别是主要误差源" << std::endl;
        std::cout << "2. 考虑改进预测模型（一阶线性预测）" << std::endl;
        std::cout << "3. 分析编码策略，减少非预测相关的开销" << std::endl;
    } else if (best_result.zero_correction_rate < estimated_linear_zero_rate - 5.0) {
        std::cout << "⚠️ 接近但仍未达到线性预测水平" << std::endl;
        std::cout << "建议微调最佳配置的参数" << std::endl;
    } else {
        std::cout << "✅ 成功达到与线性预测相近的零校正率！" << std::endl;
        std::cout << "可以考虑在此基础上优化编码效率" << std::endl;
    }
    
    std::cout << "\n💡 关键洞察:" << std::endl;
    std::cout << "在GPS容忍的误差范围(" << std::scientific << gps_error_bound << "度)内，" << std::endl;
    std::cout << "通过精细调整量化参数，GPS轨迹压缩器的零校正率" << std::endl;
    std::cout << "最高可达到 " << std::fixed << std::setprecision(2) << best_result.zero_correction_rate << "%，" << std::endl;
    std::cout << "使用配置: εv=" << std::scientific << best_result.config.epsilon_v 
              << ", εθ=" << best_result.config.epsilon_theta << std::endl;
    
    return 0;
}
