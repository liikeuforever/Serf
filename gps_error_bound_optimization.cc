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

struct TestResult {
    double error_bound;
    double zero_correction_rate;
    double compression_ratio;
    double avg_bits_per_point;
    int compression_time_ms;
    int zero_corr_count;
    int total_points;
};

int main() {
    std::cout << "=== GPS轨迹压缩器误差界限优化测试 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 50000;  // 使用5万条数据进行测试
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置量化参数
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 2e-4;
    
    // 测试不同的误差界限
    std::vector<double> error_bounds = {
        1.0e-5,   // 很严格
        1.4e-5,   // 原始设置
        2.0e-5,   // 适中
        2.5e-5,   // 较宽松
        3.0e-5,   // 宽松
        4.0e-5,   // 很宽松
        5.0e-5    // 非常宽松
    };
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  量化参数: εv=" << std::scientific << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    std::cout << "  数据点数: " << points.size() << std::endl;
    
    // 首先运行线性预测作为基准
    std::cout << "\n=== 基准测试: 线性预测 (Serf-QT) ===" << std::endl;
    
    const double linear_max_diff = 1e-5;
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
    
    std::cout << "📊 线性预测基准结果:" << std::endl;
    std::cout << "  压缩大小: " << linear_total_size << " 字节" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << linear_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << linear_avg_bits << " bits/点" << std::endl;
    std::cout << "  压缩时间: " << linear_duration.count() << " 毫秒" << std::endl;
    
    // 测试不同误差界限的GPS轨迹压缩器
    std::cout << "\n=== GPS轨迹压缩器误差界限优化测试 ===" << std::endl;
    
    std::vector<TestResult> results;
    
    std::cout << std::setw(12) << "误差界限" 
              << std::setw(12) << "零校正率" 
              << std::setw(12) << "压缩比" 
              << std::setw(15) << "平均bits/点" 
              << std::setw(12) << "时间(ms)" 
              << std::setw(15) << "vs线性优势" 
              << std::setw(12) << "零校正数" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    for (double error_bound : error_bounds) {
        start_time = std::chrono::high_resolution_clock::now();
        
        SerfQtGpsConfigurableCompressor gps_compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
        
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
        
        // 保存结果
        TestResult result;
        result.error_bound = error_bound;
        result.zero_correction_rate = gps_zero_correction_rate;
        result.compression_ratio = gps_compression_ratio;
        result.avg_bits_per_point = gps_avg_bits;
        result.compression_time_ms = gps_duration.count();
        result.zero_corr_count = gps_stats.zero_corr_count;
        result.total_points = gps_total_points;
        results.push_back(result);
        
        // 打印结果
        std::cout << std::setw(12) << std::scientific << std::setprecision(1) << error_bound
                  << std::setw(12) << std::fixed << std::setprecision(2) << gps_zero_correction_rate << "%"
                  << std::setw(12) << gps_compression_ratio
                  << std::setw(15) << gps_avg_bits
                  << std::setw(12) << gps_duration.count()
                  << std::setw(15) << std::showpos << compression_advantage << "%" << std::noshowpos
                  << std::setw(12) << gps_stats.zero_corr_count << std::endl;
    }
    
    // 找到最佳结果
    std::cout << "\n=== 最佳结果分析 ===" << std::endl;
    
    // 按压缩比排序找最佳
    auto best_compression = *std::max_element(results.begin(), results.end(), 
        [](const TestResult& a, const TestResult& b) {
            return a.compression_ratio < b.compression_ratio;
        });
    
    // 按零校正率排序找最佳
    auto best_zero_rate = *std::max_element(results.begin(), results.end(), 
        [](const TestResult& a, const TestResult& b) {
            return a.zero_correction_rate < b.zero_correction_rate;
        });
    
    std::cout << "🏆 最佳压缩比配置:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << best_compression.error_bound << " 度" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << best_compression.compression_ratio << ":1" << std::endl;
    std::cout << "  零校正率: " << best_compression.zero_correction_rate << "%" << std::endl;
    std::cout << "  vs线性预测优势: " << std::showpos << (best_compression.compression_ratio / linear_compression_ratio - 1) * 100 << "%" << std::noshowpos << std::endl;
    
    std::cout << "\n🎯 最佳零校正率配置:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << best_zero_rate.error_bound << " 度" << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << best_zero_rate.zero_correction_rate << "%" << std::endl;
    std::cout << "  压缩比: " << best_zero_rate.compression_ratio << ":1" << std::endl;
    std::cout << "  vs线性预测优势: " << std::showpos << (best_zero_rate.compression_ratio / linear_compression_ratio - 1) * 100 << "%" << std::noshowpos << std::endl;
    
    // 分析趋势
    std::cout << "\n=== 趋势分析 ===" << std::endl;
    
    std::cout << "📈 误差界限 vs 零校正率趋势:" << std::endl;
    for (const auto& result : results) {
        std::cout << "  " << std::scientific << std::setprecision(1) << result.error_bound 
                  << " → " << std::fixed << std::setprecision(1) << result.zero_correction_rate << "%" << std::endl;
    }
    
    std::cout << "\n📊 误差界限 vs 压缩比趋势:" << std::endl;
    for (const auto& result : results) {
        std::cout << "  " << std::scientific << std::setprecision(1) << result.error_bound 
                  << " → " << std::fixed << std::setprecision(2) << result.compression_ratio << ":1" << std::endl;
    }
    
    // 找到超越线性预测的配置
    std::cout << "\n=== 超越线性预测的配置 ===" << std::endl;
    
    bool found_better = false;
    for (const auto& result : results) {
        if (result.compression_ratio > linear_compression_ratio) {
            found_better = true;
            double advantage = (result.compression_ratio / linear_compression_ratio - 1) * 100;
            std::cout << "🚀 误差界限 " << std::scientific << result.error_bound 
                      << ": 压缩比 " << std::fixed << std::setprecision(2) << result.compression_ratio 
                      << ":1 (优势 +" << advantage << "%)" << std::endl;
        }
    }
    
    if (!found_better) {
        std::cout << "❌ 没有找到超越线性预测的配置" << std::endl;
        std::cout << "建议进一步调整量化参数或尝试更大的误差界限" << std::endl;
    }
    
    // 推荐配置
    std::cout << "\n=== 推荐配置 ===" << std::endl;
    
    // 找到平衡点：零校正率>30%且压缩比较好的配置
    TestResult recommended = results[0];
    for (const auto& result : results) {
        if (result.zero_correction_rate > 30.0 && 
            result.compression_ratio > recommended.compression_ratio) {
            recommended = result;
        }
    }
    
    std::cout << "💡 推荐配置 (平衡零校正率和压缩比):" << std::endl;
    std::cout << "  误差界限: " << std::scientific << recommended.error_bound << " 度" << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << recommended.zero_correction_rate << "%" << std::endl;
    std::cout << "  压缩比: " << recommended.compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << recommended.avg_bits_per_point << " bits/点" << std::endl;
    
    double recommended_advantage = (recommended.compression_ratio / linear_compression_ratio - 1) * 100;
    if (recommended_advantage > 0) {
        std::cout << "  vs线性预测: +" << recommended_advantage << "% 优势" << std::endl;
    } else {
        std::cout << "  vs线性预测: " << recommended_advantage << "% 劣势" << std::endl;
    }
    
    return 0;
}
