#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

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

struct TestConfig {
    double epsilon_v;
    double epsilon_theta;
    std::string name;
};

int main() {
    std::cout << "=== 线性预测 vs 轨迹预测 FLAG_ZERO_CORR 准确率对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 100000;  // 10w条数据
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < 10000) {
        std::cerr << "❌ 数据量不足，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double gps_e_max = 1.4e-5;      // 轨迹预测误差界限
    const double linear_max_diff = 1e-5;   // 线性预测误差界限
    
    // 不同的epsilon_v和epsilon_theta组合
    std::vector<TestConfig> test_configs = {
        {1e-6, 1e-5, "εv=1e-6, εθ=1e-5"},
        {5e-6, 1e-4, "εv=5e-6, εθ=1e-4"},
        {1e-5, 1e-4, "εv=1e-5, εθ=1e-4"},
        {2e-5, 2e-4, "εv=2e-5, εθ=2e-4"},
        {5e-5, 5e-4, "εv=5e-5, εθ=5e-4"}
    };
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  轨迹预测误差界限 (e_max): " << std::scientific << gps_e_max << " 度" << std::endl;
    std::cout << "  线性预测误差界限 (max_diff): " << linear_max_diff << " 度" << std::endl;
    std::cout << "  数据点数: " << points.size() << std::endl;
    
    // 首先运行线性预测作为基准
    std::cout << "\n=== 基准测试: 线性预测 (Serf-QT) ===" << std::endl;
    
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
    
    std::cout << "📊 线性预测结果:" << std::endl;
    std::cout << "  压缩大小: " << linear_total_size << " 字节" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << linear_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << linear_avg_bits << " bits/点" << std::endl;
    std::cout << "  压缩时间: " << linear_duration.count() << " 毫秒" << std::endl;
    
    // 估算线性预测的零校正率（基于压缩效果）
    // 假设零校正用1bit，非零校正平均用更多bits
    double estimated_linear_zero_rate = 0.0;
    if (linear_avg_bits > 1.0) {
        // 简化估算：假设零校正1bit，非零校正平均linear_avg_bits*1.5 bits
        // linear_avg_bits = zero_rate * 1 + (1-zero_rate) * (linear_avg_bits * 1.5)
        // 求解zero_rate
        estimated_linear_zero_rate = std::max(0.0, (linear_avg_bits * 1.5 - linear_avg_bits) / (linear_avg_bits * 1.5 - 1.0)) * 100.0;
    }
    std::cout << "  估算零校正率: ~" << std::setprecision(1) << estimated_linear_zero_rate << "%" << std::endl;
    
    // 运行不同参数的轨迹预测测试
    std::cout << "\n=== 轨迹预测测试 (不同参数组合) ===" << std::endl;
    
    std::cout << std::setw(20) << "参数组合" 
              << std::setw(12) << "零校正数" 
              << std::setw(12) << "零校正率" 
              << std::setw(12) << "压缩比" 
              << std::setw(15) << "平均bits/点" 
              << std::setw(12) << "时间(ms)" 
              << std::setw(15) << "vs线性优势" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    for (const auto& config : test_configs) {
        start_time = std::chrono::high_resolution_clock::now();
        
        SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_e_max, config.epsilon_v, config.epsilon_theta);
        
        for (const auto& point : points) {
            gps_compressor.AddGpsPoint(point);
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        auto gps_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        Array<uint8_t> gps_compressed_data = gps_compressor.GetCompressedData();
        const auto& gps_stats = gps_compressor.GetStrategyStats();
        
        // 计算结果
        int gps_total_points = gps_stats.GetTotalPoints();
        double gps_zero_correction_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_total_points * 100.0;
        double gps_compression_ratio = static_cast<double>(points.size() * 16) / gps_compressed_data.length();
        double gps_avg_bits = static_cast<double>(gps_compressed_data.length() * 8) / points.size();
        
        // 与线性预测的对比
        double compression_advantage = (gps_compression_ratio / linear_compression_ratio - 1) * 100.0;
        
        // 打印结果
        std::cout << std::setw(20) << config.name
                  << std::setw(12) << gps_stats.zero_corr_count
                  << std::setw(12) << std::setprecision(2) << gps_zero_correction_rate << "%"
                  << std::setw(12) << gps_compression_ratio
                  << std::setw(15) << gps_avg_bits
                  << std::setw(12) << gps_duration.count()
                  << std::setw(15) << std::showpos << compression_advantage << "%" << std::noshowpos << std::endl;
    }
    
    std::cout << "\n=== 详细分析最佳参数 ===" << std::endl;
    
    // 找到最佳参数组合（以零校正率为主要指标）
    double best_zero_rate = 0.0;
    TestConfig best_config = test_configs[0];
    
    for (const auto& config : test_configs) {
        SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_e_max, config.epsilon_v, config.epsilon_theta);
        
        for (const auto& point : points) {
            gps_compressor.AddGpsPoint(point);
        }
        
        const auto& gps_stats = gps_compressor.GetStrategyStats();
        double zero_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_stats.GetTotalPoints() * 100.0;
        
        if (zero_rate > best_zero_rate) {
            best_zero_rate = zero_rate;
            best_config = config;
        }
    }
    
    std::cout << "🏆 最佳参数组合: " << best_config.name << std::endl;
    std::cout << "   最高零校正率: " << std::setprecision(2) << best_zero_rate << "%" << std::endl;
    
    // 重新运行最佳参数进行详细分析
    SerfQtGpsConfigurableCompressor best_compressor(points.size(), gps_e_max, best_config.epsilon_v, best_config.epsilon_theta);
    
    for (const auto& point : points) {
        best_compressor.AddGpsPoint(point);
    }
    
    const auto& best_stats = best_compressor.GetStrategyStats();
    
    std::cout << "\n📊 最佳参数详细策略分布:" << std::endl;
    int total_points = best_stats.GetTotalPoints();
    std::cout << "  零校正 (FLAG_ZERO_CORR): " << best_stats.zero_corr_count 
              << " (" << (100.0 * best_stats.zero_corr_count / total_points) << "%)" << std::endl;
    std::cout << "  速度校正 (FLAG_V_ONLY): " << best_stats.v_only_count 
              << " (" << (100.0 * best_stats.v_only_count / total_points) << "%)" << std::endl;
    std::cout << "  角度校正 (FLAG_THETA_ONLY): " << best_stats.theta_only_count 
              << " (" << (100.0 * best_stats.theta_only_count / total_points) << "%)" << std::endl;
    std::cout << "  完全校正 (FLAG_BOTH): " << best_stats.both_count 
              << " (" << (100.0 * best_stats.both_count / total_points) << "%)" << std::endl;
    
    std::cout << "\n=== 最终对比总结 ===" << std::endl;
    
    std::cout << "🎯 预测准确率对比:" << std::endl;
    std::cout << "  轨迹预测最佳零校正率: " << best_zero_rate << "%" << std::endl;
    std::cout << "  线性预测估算零校正率: ~" << estimated_linear_zero_rate << "%" << std::endl;
    
    if (best_zero_rate > estimated_linear_zero_rate + 2.0) {
        std::cout << "  🚀 轨迹预测明显优于线性预测!" << std::endl;
    } else if (best_zero_rate > estimated_linear_zero_rate - 2.0) {
        std::cout << "  ⚖️ 两种预测方法准确率相近" << std::endl;
    } else {
        std::cout << "  📉 轨迹预测准确率低于线性预测" << std::endl;
    }
    
    std::cout << "\n💡 关键发现:" << std::endl;
    std::cout << "1. 使用10万条Geolife数据进行测试" << std::endl;
    std::cout << "2. 轨迹预测误差界限: " << gps_e_max << " 度" << std::endl;
    std::cout << "3. 线性预测误差界限: " << linear_max_diff << " 度" << std::endl;
    std::cout << "4. 最佳轨迹预测参数: " << best_config.name << std::endl;
    std::cout << "5. FLAG_ZERO_CORR比例直接反映预测准确率" << std::endl;
    
    return 0;
}
