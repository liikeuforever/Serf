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

struct DetailedResult {
    std::string algorithm_name;
    double error_bound;
    double compression_ratio;
    double avg_bits_per_point;
    double compression_time_ms;
    double decompression_time_ms;
    double avg_error;
    double max_error;
    int error_violations;
    int total_points;
    double violation_rate;
    
    // GPS轨迹压缩器特有统计
    int zero_corr_count = 0;
    int v_only_count = 0;
    int theta_only_count = 0;
    int both_count = 0;
    double zero_corr_ratio = 0.0;
    double strategy_overhead_ratio = 0.0;
    
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
DetailedResult TestGpsCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                double error_bound, double epsilon_v, double epsilon_theta) {
    DetailedResult result;
    result.algorithm_name = "GPS轨迹压缩器";
    result.error_bound = error_bound;
    result.total_points = points.size();
    
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
        for (const auto& point : points) {
            compressor.AddGpsPoint(point);
        }
        Array<uint8_t> compressed_data = compressor.GetCompressedData();
        
        auto compress_end = std::chrono::high_resolution_clock::now();
        
        // 获取详细统计
        const auto& stats = compressor.GetStrategyStats();
        result.zero_corr_count = stats.zero_corr_count;
        result.v_only_count = stats.v_only_count;
        result.theta_only_count = stats.theta_only_count;
        result.both_count = stats.both_count;
        result.zero_corr_ratio = static_cast<double>(stats.zero_corr_count) / stats.GetTotalPoints();
        result.strategy_overhead_ratio = static_cast<double>(stats.total_strategy_bits) / 
                                        (stats.total_strategy_bits + stats.total_quantization_bits);
        
        // 解压缩
        SerfQtGpsConfigurableDecompressor decompressor(compressed_data);
        std::vector<SerfQtGpsConfigurableDecompressor::GpsPoint> decompressed_points;
        
        for (size_t i = 0; i < points.size(); ++i) {
            decompressed_points.push_back(decompressor.GetNextGpsPoint());
        }
        
        auto decompress_end = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        int original_size_bytes = points.size() * 16;
        result.compression_ratio = static_cast<double>(original_size_bytes) / compressed_data.length();
        result.avg_bits_per_point = (compressed_data.length() * 8.0) / points.size();
        
        auto compress_duration = std::chrono::duration_cast<std::chrono::microseconds>(compress_end - start_time);
        auto decompress_duration = std::chrono::duration_cast<std::chrono::microseconds>(decompress_end - compress_end);
        result.compression_time_ms = compress_duration.count() / 1000.0;
        result.decompression_time_ms = decompress_duration.count() / 1000.0;
        
        // 计算误差统计
        std::vector<double> errors;
        result.error_violations = 0;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double error = CalculateGpsDistance(points[i], decompressed_points[i]);
            errors.push_back(error);
            
            if (error > error_bound) {
                result.error_violations++;
            }
        }
        
        result.avg_error = std::accumulate(errors.begin(), errors.end(), 0.0) / errors.size();
        result.max_error = *std::max_element(errors.begin(), errors.end());
        result.violation_rate = static_cast<double>(result.error_violations) / points.size() * 100.0;
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "GPS轨迹压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

// 测试线性压缩器
DetailedResult TestLinearCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                   double max_diff) {
    DetailedResult result;
    result.algorithm_name = "线性压缩器";
    result.error_bound = max_diff;
    result.total_points = points.size();
    
    try {
        std::vector<double> longitudes, latitudes;
        for (const auto& point : points) {
            longitudes.push_back(point.longitude);
            latitudes.push_back(point.latitude);
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 压缩经度
        SerfQtLinearCompressor lon_compressor(longitudes.size(), max_diff);
        for (double lon : longitudes) {
            lon_compressor.AddValue(lon);
        }
        lon_compressor.Close();
        Array<uint8_t> lon_compressed = lon_compressor.compressed_bytes();
        
        // 压缩纬度
        SerfQtLinearCompressor lat_compressor(latitudes.size(), max_diff);
        for (double lat : latitudes) {
            lat_compressor.AddValue(lat);
        }
        lat_compressor.Close();
        Array<uint8_t> lat_compressed = lat_compressor.compressed_bytes();
        
        auto compress_end = std::chrono::high_resolution_clock::now();
        
        // 解压缩
        SerfQtLinearDecompressor lon_decompressor;
        std::vector<double> decompressed_longitudes = lon_decompressor.Decompress(lon_compressed);
        
        SerfQtLinearDecompressor lat_decompressor;
        std::vector<double> decompressed_latitudes = lat_decompressor.Decompress(lat_compressed);
        
        auto decompress_end = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        int original_size_bytes = points.size() * 16;
        int compressed_size_bytes = lon_compressed.length() + lat_compressed.length();
        result.compression_ratio = static_cast<double>(original_size_bytes) / compressed_size_bytes;
        result.avg_bits_per_point = (compressed_size_bytes * 8.0) / points.size();
        
        auto compress_duration = std::chrono::duration_cast<std::chrono::microseconds>(compress_end - start_time);
        auto decompress_duration = std::chrono::duration_cast<std::chrono::microseconds>(decompress_end - compress_end);
        result.compression_time_ms = compress_duration.count() / 1000.0;
        result.decompression_time_ms = decompress_duration.count() / 1000.0;
        
        // 计算误差统计
        std::vector<double> errors;
        result.error_violations = 0;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double dx = points[i].longitude - decompressed_longitudes[i];
            double dy = points[i].latitude - decompressed_latitudes[i];
            double error = std::sqrt(dx * dx + dy * dy);
            errors.push_back(error);
            
            if (error > max_diff) {
                result.error_violations++;
            }
        }
        
        result.avg_error = std::accumulate(errors.begin(), errors.end(), 0.0) / errors.size();
        result.max_error = *std::max_element(errors.begin(), errors.end());
        result.violation_rate = static_cast<double>(result.error_violations) / points.size() * 100.0;
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "线性压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

void PrintComprehensiveAnalysis(const std::vector<DetailedResult>& results) {
    std::cout << "\n=== 相同误差界限下的综合性能分析 ===" << std::endl;
    
    for (const auto& result : results) {
        if (!result.success) continue;
        
        std::cout << "\n" << result.algorithm_name << " (误差界限: " 
                  << std::scientific << std::setprecision(2) << result.error_bound << " 度)" << std::endl;
        std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << result.compression_ratio << ":1" << std::endl;
        std::cout << "  平均编码成本: " << std::setprecision(2) << result.avg_bits_per_point << " bits/点" << std::endl;
        std::cout << "  压缩时间: " << std::setprecision(1) << result.compression_time_ms << " ms" << std::endl;
        std::cout << "  解压时间: " << result.decompression_time_ms << " ms" << std::endl;
        std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << result.avg_error << " 度" << std::endl;
        std::cout << "  最大误差: " << result.max_error << " 度" << std::endl;
        std::cout << "  违反率: " << std::fixed << std::setprecision(3) << result.violation_rate << "%" << std::endl;
        
        if (result.algorithm_name == "GPS轨迹压缩器") {
            std::cout << "  零校正率: " << std::setprecision(2) << (result.zero_corr_ratio * 100) << "%" << std::endl;
            std::cout << "  策略开销占比: " << (result.strategy_overhead_ratio * 100) << "%" << std::endl;
            std::cout << "  策略分布: 零校正(" << result.zero_corr_count 
                      << ") 速度(" << result.v_only_count 
                      << ") 角度(" << result.theta_only_count 
                      << ") 完全(" << result.both_count << ")" << std::endl;
        }
    }
    
    if (results.size() >= 2) {
        const auto& gps = results[0];
        const auto& linear = results[1];
        
        std::cout << "\n=== 直接对比分析 ===" << std::endl;
        std::cout << "压缩比对比: " << std::fixed << std::setprecision(2) 
                  << linear.compression_ratio << ":1 vs " << gps.compression_ratio << ":1" << std::endl;
        std::cout << "线性压缩器压缩比优势: " << (linear.compression_ratio / gps.compression_ratio) << " 倍" << std::endl;
        
        std::cout << "\n编码效率对比:" << std::endl;
        std::cout << "  GPS轨迹压缩器: " << std::setprecision(2) << gps.avg_bits_per_point << " bits/点" << std::endl;
        std::cout << "  线性压缩器: " << linear.avg_bits_per_point << " bits/点" << std::endl;
        std::cout << "  效率差异: " << (gps.avg_bits_per_point / linear.avg_bits_per_point) << " 倍" << std::endl;
        
        std::cout << "\n速度对比:" << std::endl;
        double gps_total = gps.compression_time_ms + gps.decompression_time_ms;
        double linear_total = linear.compression_time_ms + linear.decompression_time_ms;
        std::cout << "  GPS轨迹压缩器总时间: " << std::setprecision(1) << gps_total << " ms" << std::endl;
        std::cout << "  线性压缩器总时间: " << linear_total << " ms" << std::endl;
        std::cout << "  线性压缩器速度优势: " << (gps_total / linear_total) << " 倍" << std::endl;
        
        std::cout << "\n误差控制对比:" << std::endl;
        std::cout << "  GPS轨迹压缩器违反率: " << std::setprecision(3) << gps.violation_rate << "%" << std::endl;
        std::cout << "  线性压缩器违反率: " << linear.violation_rate << "%" << std::endl;
        
        if (gps.violation_rate < linear.violation_rate) {
            std::cout << "  🎯 GPS轨迹压缩器误差控制更好" << std::endl;
        } else if (linear.violation_rate < gps.violation_rate) {
            std::cout << "  🎯 线性压缩器误差控制更好" << std::endl;
        }
        
        std::cout << "\n=== 算法特性深度分析 ===" << std::endl;
        std::cout << "GPS轨迹压缩器的问题:" << std::endl;
        std::cout << "  1. 预测准确性低: 只有" << (gps.zero_corr_ratio * 100) << "%的点可以零校正" << std::endl;
        std::cout << "  2. 策略开销大: " << (gps.strategy_overhead_ratio * 100) << "%的比特用于策略标志" << std::endl;
        std::cout << "  3. 算法复杂: 需要处理4种不同的校正策略" << std::endl;
        
        std::cout << "\n线性压缩器的优势:" << std::endl;
        std::cout << "  1. 预测效果好: 违反率仅" << linear.violation_rate << "%" << std::endl;
        std::cout << "  2. 编码简单: 无策略开销，直接编码量化值" << std::endl;
        std::cout << "  3. 处理速度快: 简单的线性预测和量化" << std::endl;
    }
}

int main() {
    std::cout << "=== 相同误差界限下的算法性能对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 30000;
    
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
    
    std::vector<DetailedResult> results;
    
    // 使用相同的误差界限进行测试
    const double common_error_bound = 1.2e-5; // 使用中间值
    
    std::cout << "\n🔄 测试GPS轨迹压缩器 (误差界限: " << std::scientific << common_error_bound << ")..." << std::endl;
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    DetailedResult gps_result = TestGpsCompressor(points, common_error_bound, epsilon_v, epsilon_theta);
    if (gps_result.success) {
        results.push_back(gps_result);
        std::cout << "✅ GPS轨迹压缩器测试完成" << std::endl;
    }
    
    std::cout << "\n🔄 测试线性压缩器 (误差界限: " << common_error_bound << ")..." << std::endl;
    DetailedResult linear_result = TestLinearCompressor(points, common_error_bound);
    if (linear_result.success) {
        results.push_back(linear_result);
        std::cout << "✅ 线性压缩器测试完成" << std::endl;
    }
    
    // 打印综合分析
    PrintComprehensiveAnalysis(results);
    
    return 0;
}
