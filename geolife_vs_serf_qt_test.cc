#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "compressor/serf_qt_compressor.h"

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
    std::cout << "=== Geolife GPS轨迹压缩器 vs Serf-QT 预测准确率对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 10000;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double gps_error_bound = 1e-5;
    const double serf_qt_error_bound = gps_error_bound;  // 使用相同的误差界限
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  GPS轨迹压缩器量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    std::cout << "\n=== 测试1: GPS轨迹压缩器 (运动矢量预测) ===" << std::endl;
    
    // 运行GPS轨迹压缩器
    auto start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        gps_compressor.AddGpsPoint(point);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto gps_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    Array<uint8_t> gps_compressed_data = gps_compressor.GetCompressedData();
    const auto& gps_stats = gps_compressor.GetStrategyStats();
    
    // 计算GPS轨迹压缩器结果
    int gps_total_points = gps_stats.GetTotalPoints();
    double gps_zero_correction_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_total_points * 100.0;
    double gps_compression_ratio = static_cast<double>(points.size() * 16) / gps_compressed_data.length();
    double gps_avg_bits_per_point = static_cast<double>(gps_compressed_data.length() * 8) / points.size();
    
    std::cout << "📊 GPS轨迹压缩器结果:" << std::endl;
    std::cout << "  总处理点数: " << gps_total_points << std::endl;
    std::cout << "  零校正次数 (FLAG_ZERO_CORR): " << gps_stats.zero_corr_count << std::endl;
    std::cout << "  预测准确率: " << std::fixed << std::setprecision(2) << gps_zero_correction_rate << "%" << std::endl;
    std::cout << "  压缩大小: " << gps_compressed_data.length() << " 字节" << std::endl;
    std::cout << "  压缩比: " << gps_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << std::setprecision(2) << gps_avg_bits_per_point << " bits/点" << std::endl;
    std::cout << "  压缩时间: " << gps_duration.count() << " 微秒" << std::endl;
    
    // 详细策略分布
    std::cout << "\n  策略分布:" << std::endl;
    std::cout << "    零校正 (预测准确): " << gps_stats.zero_corr_count << " (" << gps_zero_correction_rate << "%)" << std::endl;
    std::cout << "    速度校正: " << gps_stats.v_only_count << " (" << (100.0 * gps_stats.v_only_count / gps_total_points) << "%)" << std::endl;
    std::cout << "    角度校正: " << gps_stats.theta_only_count << " (" << (100.0 * gps_stats.theta_only_count / gps_total_points) << "%)" << std::endl;
    std::cout << "    完全校正: " << gps_stats.both_count << " (" << (100.0 * gps_stats.both_count / gps_total_points) << "%)" << std::endl;
    
    std::cout << "\n=== 测试2: Serf-QT (前值预测) ===" << std::endl;
    
    // 运行Serf-QT压缩器 - 经度
    start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtCompressor serf_qt_lon_compressor(points.size(), serf_qt_error_bound);
    for (const auto& point : points) {
        serf_qt_lon_compressor.AddValue(point.longitude);
    }
    serf_qt_lon_compressor.Close();
    
    SerfQtCompressor serf_qt_lat_compressor(points.size(), serf_qt_error_bound);
    for (const auto& point : points) {
        serf_qt_lat_compressor.AddValue(point.latitude);
    }
    serf_qt_lat_compressor.Close();
    
    end_time = std::chrono::high_resolution_clock::now();
    auto serf_qt_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    Array<uint8_t> serf_qt_lon_compressed = serf_qt_lon_compressor.compressed_bytes();
    Array<uint8_t> serf_qt_lat_compressed = serf_qt_lat_compressor.compressed_bytes();
    
    int serf_qt_total_size = serf_qt_lon_compressed.length() + serf_qt_lat_compressed.length();
    double serf_qt_compression_ratio = static_cast<double>(points.size() * 16) / serf_qt_total_size;
    double serf_qt_avg_bits_per_point = static_cast<double>(serf_qt_total_size * 8) / points.size();
    
    // 估算Serf-QT的预测准确率（基于压缩效果）
    // Serf-QT使用前值预测，零校正率通常较低
    double estimated_serf_qt_zero_rate = 0.0; // 需要通过其他方式估算
    
    std::cout << "📊 Serf-QT结果:" << std::endl;
    std::cout << "  经度压缩大小: " << serf_qt_lon_compressed.length() << " 字节" << std::endl;
    std::cout << "  纬度压缩大小: " << serf_qt_lat_compressed.length() << " 字节" << std::endl;
    std::cout << "  总压缩大小: " << serf_qt_total_size << " 字节" << std::endl;
    std::cout << "  压缩比: " << std::setprecision(2) << serf_qt_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << serf_qt_avg_bits_per_point << " bits/点" << std::endl;
    std::cout << "  压缩时间: " << serf_qt_duration.count() << " 微秒" << std::endl;
    
    std::cout << "\n=== 性能对比分析 ===" << std::endl;
    
    double compression_ratio_improvement = (gps_compression_ratio / serf_qt_compression_ratio - 1) * 100;
    double speed_improvement = (static_cast<double>(serf_qt_duration.count()) / gps_duration.count() - 1) * 100;
    
    std::cout << "\n📈 压缩性能对比:" << std::endl;
    std::cout << "  GPS轨迹压缩器: " << gps_compression_ratio << ":1" << std::endl;
    std::cout << "  Serf-QT: " << serf_qt_compression_ratio << ":1" << std::endl;
    
    if (compression_ratio_improvement > 0) {
        std::cout << "  🚀 GPS轨迹压缩器压缩比优势: +" << std::setprecision(1) << compression_ratio_improvement << "%" << std::endl;
    } else {
        std::cout << "  📉 GPS轨迹压缩器压缩比劣势: " << compression_ratio_improvement << "%" << std::endl;
    }
    
    std::cout << "\n⏱️ 速度对比:" << std::endl;
    std::cout << "  GPS轨迹压缩器: " << gps_duration.count() << " 微秒" << std::endl;
    std::cout << "  Serf-QT: " << serf_qt_duration.count() << " 微秒" << std::endl;
    
    if (speed_improvement < 0) {
        std::cout << "  🚀 GPS轨迹压缩器速度优势: " << std::setprecision(1) << -speed_improvement << "% 更快" << std::endl;
    } else {
        std::cout << "  📉 GPS轨迹压缩器速度劣势: " << speed_improvement << "% 更慢" << std::endl;
    }
    
    std::cout << "\n=== 预测准确率分析 ===" << std::endl;
    
    std::cout << "\n🎯 关键发现:" << std::endl;
    std::cout << "1. GPS轨迹压缩器预测准确率 (FLAG_ZERO_CORR): " << gps_zero_correction_rate << "%" << std::endl;
    std::cout << "2. 这表明运动矢量预测在 " << gps_zero_correction_rate << "% 的情况下能准确预测下一个GPS点" << std::endl;
    
    if (gps_zero_correction_rate > 15.0) {
        std::cout << "3. ✅ 预测准确率较高，说明运动矢量预测适合GPS轨迹数据" << std::endl;
    } else if (gps_zero_correction_rate > 10.0) {
        std::cout << "3. ⚠️ 预测准确率中等，可能需要优化预测算法或参数" << std::endl;
    } else {
        std::cout << "3. 🚨 预测准确率较低，需要改进预测模型" << std::endl;
    }
    
    std::cout << "\n💡 优化建议:" << std::endl;
    
    if (gps_zero_correction_rate < 20.0) {
        std::cout << "1. 考虑调整误差界限到更合理的值" << std::endl;
        std::cout << "2. 优化量化参数 εv 和 εθ" << std::endl;
        std::cout << "3. 考虑改进预测模型，如使用二阶预测" << std::endl;
    }
    
    if (compression_ratio_improvement < 0) {
        std::cout << "4. 分析编码策略的开销，优化策略标志编码" << std::endl;
        std::cout << "5. 考虑自适应参数选择" << std::endl;
    }
    
    std::cout << "\n=== 总结 ===" << std::endl;
    
    std::cout << "🎯 GPS轨迹压缩器的核心优势:" << std::endl;
    std::cout << "  - 2D运动矢量预测，考虑轨迹的方向性和连续性" << std::endl;
    std::cout << "  - 预测准确率: " << gps_zero_correction_rate << "%" << std::endl;
    
    if (compression_ratio_improvement > 0) {
        std::cout << "  - 压缩比优于传统Serf-QT" << std::endl;
    }
    
    std::cout << "\n🔧 改进空间:" << std::endl;
    std::cout << "  - 提高零校正率可显著改善压缩比" << std::endl;
    std::cout << "  - 优化编码策略可减少开销" << std::endl;
    
    return 0;
}
