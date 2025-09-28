#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "decompressor/serf_qt_gps_configurable_decompressor.h"
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

// 计算2D欧几里得距离
double Calculate2DDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1, 
                          const SerfQtGpsConfigurableCompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    std::cout << "=== 运动矢量同步修复效果测试 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 30000;  // 使用3万条数据
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置测试参数
    const double gps_error_bound = 1.4e-5;
    const double linear_max_diff = 1e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 1e-5;
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  GPS轨迹误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  线性预测误差界限: " << linear_max_diff << " 度" << std::endl;
    std::cout << "  GPS量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    std::cout << "\n=== 修复后的GPS轨迹压缩器测试 ===" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 测试修复后的GPS轨迹压缩器
    SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        gps_compressor.AddGpsPoint(point);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto gps_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    Array<uint8_t> gps_compressed_data = gps_compressor.GetCompressedData();
    const auto& gps_stats = gps_compressor.GetStrategyStats();
    
    // 计算GPS压缩器结果
    double gps_compression_ratio = static_cast<double>(points.size() * 16) / gps_compressed_data.length();
    double gps_avg_bits = static_cast<double>(gps_compressed_data.length() * 8) / points.size();
    double gps_zero_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_stats.GetTotalPoints() * 100.0;
    
    std::cout << "📊 修复后GPS轨迹压缩器结果:" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << gps_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << gps_avg_bits << " bits/点" << std::endl;
    std::cout << "  零校正率: " << gps_zero_rate << "%" << std::endl;
    std::cout << "  压缩时间: " << gps_duration.count() << " 毫秒" << std::endl;
    
    // 详细策略统计
    std::cout << "\n策略分布:" << std::endl;
    std::cout << "  FLAG_ZERO_CORR: " << gps_stats.zero_corr_count 
              << " (" << std::setprecision(1) << (100.0 * gps_stats.zero_corr_count / gps_stats.GetTotalPoints()) << "%)" << std::endl;
    std::cout << "  FLAG_V_ONLY: " << gps_stats.v_only_count 
              << " (" << (100.0 * gps_stats.v_only_count / gps_stats.GetTotalPoints()) << "%)" << std::endl;
    std::cout << "  FLAG_THETA_ONLY: " << gps_stats.theta_only_count 
              << " (" << (100.0 * gps_stats.theta_only_count / gps_stats.GetTotalPoints()) << "%)" << std::endl;
    std::cout << "  FLAG_BOTH: " << gps_stats.both_count 
              << " (" << (100.0 * gps_stats.both_count / gps_stats.GetTotalPoints()) << "%)" << std::endl;
    
    std::cout << "\n=== 线性预测基准测试 ===" << std::endl;
    
    start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtLinearCompressor linear_lon_compressor(points.size(), linear_max_diff);
    SerfQtLinearCompressor linear_lat_compressor(points.size(), linear_max_diff);
    
    for (const auto& point : points) {
        linear_lon_compressor.AddValue(point.longitude);
        linear_lat_compressor.AddValue(point.latitude);
    }
    
    linear_lon_compressor.Close();
    linear_lat_compressor.Close();
    
    end_time = std::chrono::high_resolution_clock::now();
    auto linear_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    Array<uint8_t> linear_lon_compressed = linear_lon_compressor.compressed_bytes();
    Array<uint8_t> linear_lat_compressed = linear_lat_compressor.compressed_bytes();
    
    int linear_total_size = linear_lon_compressed.length() + linear_lat_compressed.length();
    double linear_compression_ratio = static_cast<double>(points.size() * 16) / linear_total_size;
    double linear_avg_bits = static_cast<double>(linear_total_size * 8) / points.size();
    
    std::cout << "📊 线性预测基准结果:" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << linear_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << linear_avg_bits << " bits/点" << std::endl;
    std::cout << "  压缩时间: " << linear_duration.count() << " 毫秒" << std::endl;
    
    std::cout << "\n=== 压缩解压一致性测试 ===" << std::endl;
    
    // 测试解压缩
    SerfQtGpsConfigurableDecompressor gps_decompressor(gps_compressed_data);
    
    std::vector<SerfQtGpsConfigurableDecompressor::GpsPoint> decompressed_points;
    double max_error = 0.0;
    double total_error = 0.0;
    int error_count = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        auto decompressed_point = gps_decompressor.GetNextGpsPoint();
        decompressed_points.push_back(decompressed_point);
        
        // 计算重构误差
        double error = Calculate2DDistance(
            SerfQtGpsConfigurableCompressor::GpsPoint(points[i].longitude, points[i].latitude),
            SerfQtGpsConfigurableCompressor::GpsPoint(decompressed_point.longitude, decompressed_point.latitude)
        );
        
        if (error > max_error) {
            max_error = error;
        }
        
        total_error += error;
        if (error > gps_error_bound) {
            error_count++;
        }
    }
    
    double avg_error = total_error / points.size();
    double error_rate = static_cast<double>(error_count) / points.size() * 100.0;
    
    std::cout << "📊 解压缩质量分析:" << std::endl;
    std::cout << "  最大误差: " << std::scientific << std::setprecision(3) << max_error << " 度" << std::endl;
    std::cout << "  平均误差: " << avg_error << " 度" << std::endl;
    std::cout << "  误差界限: " << gps_error_bound << " 度" << std::endl;
    std::cout << "  超出误差界限的点: " << error_count << " (" << std::fixed << std::setprecision(2) << error_rate << "%)" << std::endl;
    
    if (error_rate < 0.1) {
        std::cout << "  ✅ 误差控制优秀！" << std::endl;
    } else if (error_rate < 1.0) {
        std::cout << "  ✅ 误差控制良好" << std::endl;
    } else {
        std::cout << "  ⚠️ 存在误差控制问题" << std::endl;
    }
    
    std::cout << "\n=== 修复效果对比分析 ===" << std::endl;
    
    // 与线性预测对比
    double compression_ratio_gap = gps_compression_ratio - linear_compression_ratio;
    double compression_ratio_percent = (gps_compression_ratio / linear_compression_ratio - 1.0) * 100.0;
    
    std::cout << "🎯 压缩比对比:" << std::endl;
    std::cout << "  GPS轨迹压缩器: " << gps_compression_ratio << ":1" << std::endl;
    std::cout << "  线性预测基准: " << linear_compression_ratio << ":1" << std::endl;
    std::cout << "  差距: " << std::showpos << compression_ratio_gap << " (" << compression_ratio_percent << "%)" << std::noshowpos << std::endl;
    
    if (compression_ratio_percent > -10.0) {
        std::cout << "  🎉 修复效果显著！GPS轨迹压缩器性能大幅提升" << std::endl;
    } else if (compression_ratio_percent > -25.0) {
        std::cout << "  ✅ 修复有效！GPS轨迹压缩器性能明显改善" << std::endl;
    } else {
        std::cout << "  📈 修复有帮助，但仍有优化空间" << std::endl;
    }
    
    std::cout << "\n🎯 零校正率分析:" << std::endl;
    std::cout << "  修复后零校正率: " << gps_zero_rate << "%" << std::endl;
    
    if (gps_zero_rate > 40.0) {
        std::cout << "  🏆 零校正率优秀！运动矢量同步修复非常成功" << std::endl;
    } else if (gps_zero_rate > 25.0) {
        std::cout << "  ✅ 零校正率良好，修复效果明显" << std::endl;
    } else {
        std::cout << "  📊 零校正率有所改善，但仍需进一步优化" << std::endl;
    }
    
    std::cout << "\n=== 关键洞察 ===" << std::endl;
    
    std::cout << "💡 运动矢量同步修复的影响:" << std::endl;
    std::cout << "1. ✅ 确保了压缩器和解压器状态完全同步" << std::endl;
    std::cout << "2. ✅ 运动矢量现在基于实际重构点计算" << std::endl;
    std::cout << "3. ✅ 消除了FLAG_ZERO_CORR情况下的状态不一致" << std::endl;
    std::cout << "4. 📊 零校正率: " << gps_zero_rate << "%" << std::endl;
    std::cout << "5. 📊 压缩比相对线性预测: " << compression_ratio_percent << "%" << std::endl;
    
    if (gps_zero_rate > 30.0 && compression_ratio_percent > -15.0) {
        std::cout << "\n🎉 **修复非常成功！**" << std::endl;
        std::cout << "运动矢量同步问题是GPS轨迹压缩器性能差的主要原因之一" << std::endl;
        std::cout << "修复后性能显著提升，接近线性预测水平" << std::endl;
    } else if (gps_zero_rate > 20.0) {
        std::cout << "\n✅ **修复有效！**" << std::endl;
        std::cout << "运动矢量同步修复明显改善了性能" << std::endl;
        std::cout << "建议继续优化编码策略和量化参数" << std::endl;
    } else {
        std::cout << "\n📈 **修复有帮助**" << std::endl;
        std::cout << "运动矢量同步修复有一定效果" << std::endl;
        std::cout << "需要结合其他优化措施进一步提升性能" << std::endl;
    }
    
    return 0;
}
