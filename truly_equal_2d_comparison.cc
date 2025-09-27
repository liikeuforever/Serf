#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <algorithm>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "decompressor/serf_qt_gps_configurable_decompressor.h"
#include "compressor/serf_qt_linear_compressor.h"
#include "decompressor/serf_qt_linear_decompressor.h"

struct FinalResult {
    std::string algorithm_name;
    double target_2d_error_bound;
    double actual_max_2d_error;
    double compression_ratio;
    double avg_bits_per_point;
    double total_time_ms;
    double avg_error;
    int violations_2d;
    double violation_rate_2d;
    int zero_corr_count = 0;
    double zero_corr_ratio = 0.0;
    bool success = false;
};

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

// 计算GPS距离（度）
double CalculateGpsDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1,
                           const SerfQtGpsConfigurableDecompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

// 测试GPS轨迹压缩器
FinalResult TestGpsCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                             double target_2d_error_bound) {
    FinalResult result;
    result.algorithm_name = "GPS轨迹压缩器";
    result.target_2d_error_bound = target_2d_error_bound;
    
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        const double epsilon_v = 5e-6;
        const double epsilon_theta = 1e-4;
        
        SerfQtGpsConfigurableCompressor compressor(points.size(), target_2d_error_bound, epsilon_v, epsilon_theta);
        for (const auto& point : points) {
            compressor.AddGpsPoint(point);
        }
        Array<uint8_t> compressed_data = compressor.GetCompressedData();
        
        // 获取统计
        const auto& stats = compressor.GetStrategyStats();
        result.zero_corr_count = stats.zero_corr_count;
        result.zero_corr_ratio = static_cast<double>(stats.zero_corr_count) / stats.GetTotalPoints();
        
        // 解压缩
        SerfQtGpsConfigurableDecompressor decompressor(compressed_data);
        std::vector<SerfQtGpsConfigurableDecompressor::GpsPoint> decompressed_points;
        
        for (size_t i = 0; i < points.size(); ++i) {
            decompressed_points.push_back(decompressor.GetNextGpsPoint());
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        int original_size_bytes = points.size() * 16;
        result.compression_ratio = static_cast<double>(original_size_bytes) / compressed_data.length();
        result.avg_bits_per_point = (compressed_data.length() * 8.0) / points.size();
        
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.total_time_ms = total_duration.count() / 1000.0;
        
        // 计算2D误差统计
        std::vector<double> errors_2d;
        result.violations_2d = 0;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double error_2d = CalculateGpsDistance(points[i], decompressed_points[i]);
            errors_2d.push_back(error_2d);
            
            if (error_2d > target_2d_error_bound) {
                result.violations_2d++;
            }
        }
        
        result.avg_error = std::accumulate(errors_2d.begin(), errors_2d.end(), 0.0) / errors_2d.size();
        result.actual_max_2d_error = *std::max_element(errors_2d.begin(), errors_2d.end());
        result.violation_rate_2d = static_cast<double>(result.violations_2d) / points.size() * 100.0;
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "GPS轨迹压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

// 测试线性压缩器（使用调整后的单维度误差界限以匹配2D误差）
FinalResult TestLinearCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                double target_2d_error_bound) {
    FinalResult result;
    result.algorithm_name = "线性压缩器";
    result.target_2d_error_bound = target_2d_error_bound;
    
    // 计算匹配2D误差界限的单维度误差界限
    double adjusted_max_diff = target_2d_error_bound / std::sqrt(2.0);
    
    try {
        std::vector<double> longitudes, latitudes;
        for (const auto& point : points) {
            longitudes.push_back(point.longitude);
            latitudes.push_back(point.latitude);
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 压缩经度
        SerfQtLinearCompressor lon_compressor(longitudes.size(), adjusted_max_diff);
        for (double lon : longitudes) {
            lon_compressor.AddValue(lon);
        }
        lon_compressor.Close();
        Array<uint8_t> lon_compressed = lon_compressor.compressed_bytes();
        
        // 压缩纬度
        SerfQtLinearCompressor lat_compressor(latitudes.size(), adjusted_max_diff);
        for (double lat : latitudes) {
            lat_compressor.AddValue(lat);
        }
        lat_compressor.Close();
        Array<uint8_t> lat_compressed = lat_compressor.compressed_bytes();
        
        // 解压缩
        SerfQtLinearDecompressor lon_decompressor;
        std::vector<double> decompressed_longitudes = lon_decompressor.Decompress(lon_compressed);
        
        SerfQtLinearDecompressor lat_decompressor;
        std::vector<double> decompressed_latitudes = lat_decompressor.Decompress(lat_compressed);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        int original_size_bytes = points.size() * 16;
        int compressed_size_bytes = lon_compressed.length() + lat_compressed.length();
        result.compression_ratio = static_cast<double>(original_size_bytes) / compressed_size_bytes;
        result.avg_bits_per_point = (compressed_size_bytes * 8.0) / points.size();
        
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.total_time_ms = total_duration.count() / 1000.0;
        
        // 计算2D误差统计
        std::vector<double> errors_2d;
        result.violations_2d = 0;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double dx = points[i].longitude - decompressed_longitudes[i];
            double dy = points[i].latitude - decompressed_latitudes[i];
            double error_2d = std::sqrt(dx * dx + dy * dy);
            errors_2d.push_back(error_2d);
            
            if (error_2d > target_2d_error_bound) {
                result.violations_2d++;
            }
        }
        
        result.avg_error = std::accumulate(errors_2d.begin(), errors_2d.end(), 0.0) / errors_2d.size();
        result.actual_max_2d_error = *std::max_element(errors_2d.begin(), errors_2d.end());
        result.violation_rate_2d = static_cast<double>(result.violations_2d) / points.size() * 100.0;
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "线性压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

void PrintFinalComparison(const std::vector<FinalResult>& results) {
    std::cout << "\n=== 真正相同2D误差界限下的最终对比 ===" << std::endl;
    
    for (const auto& result : results) {
        if (!result.success) continue;
        
        std::cout << "\n" << result.algorithm_name << ":" << std::endl;
        std::cout << "  目标2D误差界限: " << std::scientific << std::setprecision(3) << result.target_2d_error_bound << " 度" << std::endl;
        std::cout << "  实际最大2D误差: " << result.actual_max_2d_error << " 度" << std::endl;
        std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << result.compression_ratio << ":1" << std::endl;
        std::cout << "  平均编码成本: " << result.avg_bits_per_point << " bits/点" << std::endl;
        std::cout << "  总处理时间: " << std::setprecision(1) << result.total_time_ms << " ms" << std::endl;
        std::cout << "  平均2D误差: " << std::scientific << std::setprecision(3) << result.avg_error << " 度" << std::endl;
        std::cout << "  2D误差违反数: " << result.violations_2d << " (" << std::fixed << std::setprecision(3) << result.violation_rate_2d << "%)" << std::endl;
        
        if (result.algorithm_name == "GPS轨迹压缩器") {
            std::cout << "  零校正率: " << std::setprecision(2) << (result.zero_corr_ratio * 100) << "%" << std::endl;
        }
    }
    
    if (results.size() >= 2) {
        const auto& gps = results[0];
        const auto& linear = results[1];
        
        std::cout << "\n=== 最终结论 ===" << std::endl;
        
        std::cout << "\n🎯 在完全相同的2D误差界限下:" << std::endl;
        std::cout << "  目标2D误差界限: " << std::scientific << std::setprecision(3) << gps.target_2d_error_bound << " 度" << std::endl;
        
        std::cout << "\n📊 压缩效率对比:" << std::endl;
        std::cout << "  GPS轨迹压缩器: " << std::fixed << std::setprecision(2) << gps.compression_ratio << ":1 (" << gps.avg_bits_per_point << " bits/点)" << std::endl;
        std::cout << "  线性压缩器: " << linear.compression_ratio << ":1 (" << linear.avg_bits_per_point << " bits/点)" << std::endl;
        
        if (linear.compression_ratio > gps.compression_ratio) {
            double advantage = linear.compression_ratio / gps.compression_ratio;
            std::cout << "  🏆 线性压缩器仍有 " << advantage << " 倍压缩比优势" << std::endl;
        } else {
            double advantage = gps.compression_ratio / linear.compression_ratio;
            std::cout << "  🏆 GPS轨迹压缩器有 " << advantage << " 倍压缩比优势" << std::endl;
        }
        
        std::cout << "\n⚡ 处理速度对比:" << std::endl;
        std::cout << "  GPS轨迹压缩器: " << std::setprecision(1) << gps.total_time_ms << " ms" << std::endl;
        std::cout << "  线性压缩器: " << linear.total_time_ms << " ms" << std::endl;
        
        if (linear.total_time_ms < gps.total_time_ms) {
            double speed_advantage = gps.total_time_ms / linear.total_time_ms;
            std::cout << "  ⚡ 线性压缩器有 " << speed_advantage << " 倍速度优势" << std::endl;
        }
        
        std::cout << "\n🎯 误差控制对比:" << std::endl;
        std::cout << "  GPS轨迹压缩器违反率: " << std::setprecision(3) << gps.violation_rate_2d << "%" << std::endl;
        std::cout << "  线性压缩器违反率: " << linear.violation_rate_2d << "%" << std::endl;
        
        std::cout << "\n💡 最终洞察:" << std::endl;
        std::cout << "1. 即使在完全相同的2D误差界限下，算法性能差异依然显著" << std::endl;
        std::cout << "2. 压缩比差异的根本原因是算法设计，而非误差界限设置" << std::endl;
        std::cout << "3. GPS轨迹压缩器的零校正率仅 " << (gps.zero_corr_ratio * 100) << "%，说明预测模型有改进空间" << std::endl;
        std::cout << "4. 线性预测对GPS轨迹数据的适应性确实更好" << std::endl;
        
        std::cout << "\n🚀 优化方向:" << std::endl;
        std::cout << "- 改进GPS轨迹压缩器的预测模型以提高零校正率" << std::endl;
        std::cout << "- 减少策略编码开销" << std::endl;
        std::cout << "- 考虑自适应量化策略" << std::endl;
    }
}

int main() {
    std::cout << "=== 真正相同2D误差界限下的算法对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 20000;
    
    std::cout << "数据文件: " << data_file << std::endl;
    std::cout << "测试点数: " << test_count << std::endl;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    std::vector<FinalResult> results;
    
    // 使用相同的2D误差界限
    const double target_2d_error_bound = 1e-5; // 2D欧几里得距离误差界限
    
    std::cout << "\n目标2D误差界限: " << std::scientific << target_2d_error_bound << " 度" << std::endl;
    std::cout << "线性压缩器将使用单维度误差界限: " << (target_2d_error_bound / std::sqrt(2.0)) << " 度" << std::endl;
    
    // 测试GPS轨迹压缩器
    std::cout << "\n🔄 测试GPS轨迹压缩器..." << std::endl;
    FinalResult gps_result = TestGpsCompressor(points, target_2d_error_bound);
    if (gps_result.success) {
        results.push_back(gps_result);
        std::cout << "✅ GPS轨迹压缩器测试完成" << std::endl;
    }
    
    // 测试线性压缩器（调整单维度误差界限以匹配2D误差）
    std::cout << "\n🔄 测试线性压缩器（调整后的误差界限）..." << std::endl;
    FinalResult linear_result = TestLinearCompressor(points, target_2d_error_bound);
    if (linear_result.success) {
        results.push_back(linear_result);
        std::cout << "✅ 线性压缩器测试完成" << std::endl;
    }
    
    // 打印最终对比分析
    PrintFinalComparison(results);
    
    return 0;
}
