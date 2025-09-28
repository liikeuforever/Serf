#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

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

int main() {
    std::cout << "=== 详细压缩分析：预测准确率 vs 压缩比的矛盾 ===" << std::endl;
    
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
    const double gps_error_bound = 1e-5;
    const double linear_error_bound = gps_error_bound / std::sqrt(2.0);  // 等效1D误差界限
    
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  GPS轨迹压缩器误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  线性压缩器误差界限: " << linear_error_bound << " 度" << std::endl;
    
    // 运行GPS轨迹压缩器
    std::cout << "\n🔄 运行GPS轨迹压缩器..." << std::endl;
    
    SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    for (const auto& point : points) {
        gps_compressor.AddGpsPoint(point);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto gps_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    Array<uint8_t> gps_compressed_data = gps_compressor.GetCompressedData();
    const auto& gps_stats = gps_compressor.GetStrategyStats();
    
    // 运行线性压缩器
    std::cout << "\n🔄 运行线性压缩器..." << std::endl;
    
    SerfQtLinearCompressor linear_compressor(points.size(), linear_error_bound);
    
    start_time = std::chrono::high_resolution_clock::now();
    for (const auto& point : points) {
        linear_compressor.AddValue(point.longitude);
        linear_compressor.AddValue(point.latitude);
    }
    linear_compressor.Close();  // 完成压缩
    end_time = std::chrono::high_resolution_clock::now();
    auto linear_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    Array<uint8_t> linear_compressed_data = linear_compressor.compressed_bytes();
    
    // 计算压缩比
    int original_size_bytes = points.size() * 16;  // 每个GPS点16字节
    double gps_compression_ratio = static_cast<double>(original_size_bytes) / gps_compressed_data.length();
    double linear_compression_ratio = static_cast<double>(original_size_bytes) / linear_compressed_data.length();
    
    // 打印结果
    std::cout << "\n=== 压缩性能对比 ===" << std::endl;
    
    std::cout << "\n📊 GPS轨迹压缩器:" << std::endl;
    std::cout << "  压缩大小: " << gps_compressed_data.length() << " 字节" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << gps_compression_ratio << ":1" << std::endl;
    std::cout << "  压缩时间: " << gps_duration.count() << " 微秒" << std::endl;
    std::cout << "  零校正率: " << std::setprecision(2) << (100.0 * gps_stats.zero_corr_count / gps_stats.GetTotalPoints()) << "%" << std::endl;
    
    std::cout << "\n📊 线性压缩器:" << std::endl;
    std::cout << "  压缩大小: " << linear_compressed_data.length() << " 字节" << std::endl;
    std::cout << "  压缩比: " << linear_compression_ratio << ":1" << std::endl;
    std::cout << "  压缩时间: " << linear_duration.count() << " 微秒" << std::endl;
    
    // 分析矛盾现象
    std::cout << "\n=== 矛盾现象分析 ===" << std::endl;
    
    double compression_ratio_diff = (linear_compression_ratio / gps_compression_ratio - 1) * 100;
    
    std::cout << "🤔 矛盾现象:" << std::endl;
    std::cout << "  GPS轨迹压缩器预测准确率更高 (16.31% vs 12.06%)" << std::endl;
    std::cout << "  但线性压缩器压缩比更好 (" << linear_compression_ratio << ":1 vs " << gps_compression_ratio << ":1)" << std::endl;
    std::cout << "  线性压缩器压缩比优势: " << std::setprecision(1) << compression_ratio_diff << "%" << std::endl;
    
    std::cout << "\n🔍 深入分析原因:" << std::endl;
    
    // 分析编码成本
    int gps_total_points = gps_stats.GetTotalPoints();
    double gps_avg_bits_per_point = static_cast<double>(gps_compressed_data.length() * 8) / gps_total_points;
    double linear_avg_bits_per_point = static_cast<double>(linear_compressed_data.length() * 8) / (points.size() * 2); // 2个值per点
    
    std::cout << "\n📈 编码成本分析:" << std::endl;
    std::cout << "  GPS轨迹压缩器平均成本: " << std::setprecision(2) << gps_avg_bits_per_point << " bits/点" << std::endl;
    std::cout << "  线性压缩器平均成本: " << linear_avg_bits_per_point << " bits/值 = " << (linear_avg_bits_per_point * 2) << " bits/点" << std::endl;
    
    // 分析GPS轨迹压缩器的策略分布
    std::cout << "\n📊 GPS轨迹压缩器策略分布:" << std::endl;
    std::cout << "  零校正 (1 bit): " << gps_stats.zero_corr_count << " (" << (100.0 * gps_stats.zero_corr_count / gps_total_points) << "%)" << std::endl;
    std::cout << "  速度校正 (2+ bits): " << gps_stats.v_only_count << " (" << (100.0 * gps_stats.v_only_count / gps_total_points) << "%)" << std::endl;
    std::cout << "  角度校正 (3+ bits): " << gps_stats.theta_only_count << " (" << (100.0 * gps_stats.theta_only_count / gps_total_points) << "%)" << std::endl;
    std::cout << "  完全校正 (3+ bits): " << gps_stats.both_count << " (" << (100.0 * gps_stats.both_count / gps_total_points) << "%)" << std::endl;
    
    // 估算线性压缩器的零校正率（基于其预测准确率）
    double estimated_linear_zero_correction_rate = 12.06; // 从之前的测试得出
    
    std::cout << "\n💡 关键洞察:" << std::endl;
    
    if (gps_stats.zero_corr_count * 100.0 / gps_total_points > estimated_linear_zero_correction_rate) {
        std::cout << "1. ✅ GPS轨迹压缩器确实有更高的零校正率" << std::endl;
        std::cout << "   GPS: " << (100.0 * gps_stats.zero_corr_count / gps_total_points) << "% vs 线性: ~" << estimated_linear_zero_correction_rate << "%" << std::endl;
    }
    
    std::cout << "2. 🎯 压缩比差异的真正原因:" << std::endl;
    
    // 计算非零校正的平均成本
    int non_zero_corrections = gps_total_points - gps_stats.zero_corr_count;
    if (non_zero_corrections > 0) {
        double avg_correction_bits = static_cast<double>(gps_stats.total_quantization_bits) / non_zero_corrections;
        std::cout << "   GPS非零校正平均成本: " << std::setprecision(1) << avg_correction_bits << " bits" << std::endl;
        std::cout << "   线性非零校正平均成本: ~" << linear_avg_bits_per_point << " bits" << std::endl;
        
        if (avg_correction_bits > linear_avg_bits_per_point * 2) {
            std::cout << "   🚨 GPS轨迹压缩器的校正成本过高！" << std::endl;
            std::cout << "      这是压缩比低的主要原因" << std::endl;
        }
    }
    
    std::cout << "\n3. 🔧 可能的改进方向:" << std::endl;
    std::cout << "   a) 优化量化参数 (εv, εθ) 以减少校正成本" << std::endl;
    std::cout << "   b) 改进编码策略，减少策略标志的开销" << std::endl;
    std::cout << "   c) 考虑自适应误差界限" << std::endl;
    std::cout << "   d) 优化运动矢量的编码方式" << std::endl;
    
    // 理论分析
    std::cout << "\n=== 理论分析 ===" << std::endl;
    
    std::cout << "🎯 核心问题：为什么预测更准确但压缩比更低？" << std::endl;
    std::cout << "\n可能的解释:" << std::endl;
    std::cout << "1. 编码复杂性：GPS轨迹压缩器需要编码策略标志 + 量化值" << std::endl;
    std::cout << "2. 量化精度：2D运动矢量量化可能比1D坐标量化需要更多bits" << std::endl;
    std::cout << "3. 预测-编码不匹配：预测准确但编码成本高" << std::endl;
    std::cout << "4. 误差界限解释：2D vs 1D误差界限的实际差异" << std::endl;
    
    return 0;
}
