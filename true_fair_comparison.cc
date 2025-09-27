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

struct ComparisonResult {
    std::string algorithm_name;
    double configured_error_bound;
    double actual_max_2d_error_bound;
    double compression_ratio;
    double avg_bits_per_point;
    double compression_time_ms;
    double decompression_time_ms;
    double avg_error;
    double max_error;
    int error_violations_configured;
    int error_violations_2d;
    int total_points;
    double violation_rate_configured;
    double violation_rate_2d;
    
    // GPS轨迹压缩器特有统计
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
ComparisonResult TestGpsCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                  double error_bound, double epsilon_v, double epsilon_theta) {
    ComparisonResult result;
    result.algorithm_name = "GPS轨迹压缩器";
    result.configured_error_bound = error_bound;
    result.actual_max_2d_error_bound = error_bound; // 直接控制2D误差
    result.total_points = points.size();
    
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
        for (const auto& point : points) {
            compressor.AddGpsPoint(point);
        }
        Array<uint8_t> compressed_data = compressor.GetCompressedData();
        
        auto compress_end = std::chrono::high_resolution_clock::now();
        
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
        result.error_violations_configured = 0;
        result.error_violations_2d = 0;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double error = CalculateGpsDistance(points[i], decompressed_points[i]);
            errors.push_back(error);
            
            if (error > result.configured_error_bound) {
                result.error_violations_configured++;
            }
            if (error > result.actual_max_2d_error_bound) {
                result.error_violations_2d++;
            }
        }
        
        result.avg_error = std::accumulate(errors.begin(), errors.end(), 0.0) / errors.size();
        result.max_error = *std::max_element(errors.begin(), errors.end());
        result.violation_rate_configured = static_cast<double>(result.error_violations_configured) / points.size() * 100.0;
        result.violation_rate_2d = static_cast<double>(result.error_violations_2d) / points.size() * 100.0;
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "GPS轨迹压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

// 测试线性压缩器
ComparisonResult TestLinearCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                     double max_diff) {
    ComparisonResult result;
    result.algorithm_name = "线性压缩器";
    result.configured_error_bound = max_diff; // 单维度误差界限
    result.actual_max_2d_error_bound = max_diff * std::sqrt(2.0); // 理论最大2D误差
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
        std::vector<double> errors_2d;
        result.error_violations_configured = 0;
        result.error_violations_2d = 0;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double dx = points[i].longitude - decompressed_longitudes[i];
            double dy = points[i].latitude - decompressed_latitudes[i];
            double error_2d = std::sqrt(dx * dx + dy * dy);
            errors_2d.push_back(error_2d);
            
            // 检查单维度误差违反
            if (std::abs(dx) > max_diff || std::abs(dy) > max_diff) {
                result.error_violations_configured++;
            }
            
            // 检查2D误差违反
            if (error_2d > result.actual_max_2d_error_bound) {
                result.error_violations_2d++;
            }
        }
        
        result.avg_error = std::accumulate(errors_2d.begin(), errors_2d.end(), 0.0) / errors_2d.size();
        result.max_error = *std::max_element(errors_2d.begin(), errors_2d.end());
        result.violation_rate_configured = static_cast<double>(result.error_violations_configured) / points.size() * 100.0;
        result.violation_rate_2d = static_cast<double>(result.error_violations_2d) / points.size() * 100.0;
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "线性压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

void PrintTrueFairComparison(const std::vector<ComparisonResult>& results) {
    std::cout << "\n=== 真正公平的误差界限对比分析 ===" << std::endl;
    
    std::cout << "\n误差界限理解:" << std::endl;
    std::cout << "GPS轨迹压缩器: 控制2D欧几里得距离 sqrt((Δlon)² + (Δlat)²)" << std::endl;
    std::cout << "线性压缩器: 分别控制单维度 |Δlon| 和 |Δlat|" << std::endl;
    std::cout << "线性压缩器的理论最大2D误差: max_diff × √2 ≈ max_diff × 1.414" << std::endl;
    
    for (const auto& result : results) {
        if (!result.success) continue;
        
        std::cout << "\n" << result.algorithm_name << ":" << std::endl;
        std::cout << "  配置误差界限: " << std::scientific << std::setprecision(3) << result.configured_error_bound << " 度" << std::endl;
        std::cout << "  实际最大2D误差界限: " << result.actual_max_2d_error_bound << " 度" << std::endl;
        std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << result.compression_ratio << ":1" << std::endl;
        std::cout << "  平均编码成本: " << result.avg_bits_per_point << " bits/点" << std::endl;
        std::cout << "  压缩时间: " << std::setprecision(1) << result.compression_time_ms << " ms" << std::endl;
        std::cout << "  解压时间: " << result.decompression_time_ms << " ms" << std::endl;
        std::cout << "  平均2D误差: " << std::scientific << std::setprecision(3) << result.avg_error << " 度" << std::endl;
        std::cout << "  最大2D误差: " << result.max_error << " 度" << std::endl;
        std::cout << "  配置误差违反率: " << std::fixed << std::setprecision(3) << result.violation_rate_configured << "%" << std::endl;
        std::cout << "  2D误差违反率: " << result.violation_rate_2d << "%" << std::endl;
        
        if (result.algorithm_name == "GPS轨迹压缩器") {
            std::cout << "  零校正率: " << std::setprecision(2) << (result.zero_corr_ratio * 100) << "%" << std::endl;
        }
    }
    
    if (results.size() >= 2) {
        const auto& gps = results[0];
        const auto& linear = results[1];
        
        std::cout << "\n=== 真正公平的对比分析 ===" << std::endl;
        
        std::cout << "\n📏 误差界限对比:" << std::endl;
        std::cout << "  GPS轨迹压缩器2D误差界限: " << std::scientific << std::setprecision(3) << gps.actual_max_2d_error_bound << " 度" << std::endl;
        std::cout << "  线性压缩器理论最大2D误差: " << linear.actual_max_2d_error_bound << " 度" << std::endl;
        std::cout << "  线性压缩器2D误差界限是GPS的: " << std::fixed << std::setprecision(2) 
                  << (linear.actual_max_2d_error_bound / gps.actual_max_2d_error_bound) << " 倍" << std::endl;
        
        std::cout << "\n📊 在相似2D误差容忍度下的性能对比:" << std::endl;
        std::cout << "  压缩比: GPS " << gps.compression_ratio << ":1 vs 线性 " << linear.compression_ratio << ":1" << std::endl;
        std::cout << "  线性压缩器压缩比优势: " << (linear.compression_ratio / gps.compression_ratio) << " 倍" << std::endl;
        
        std::cout << "\n⚡ 处理速度对比:" << std::endl;
        double gps_total = gps.compression_time_ms + gps.decompression_time_ms;
        double linear_total = linear.compression_time_ms + linear.decompression_time_ms;
        std::cout << "  GPS轨迹压缩器: " << std::setprecision(1) << gps_total << " ms" << std::endl;
        std::cout << "  线性压缩器: " << linear_total << " ms" << std::endl;
        std::cout << "  线性压缩器速度优势: " << (gps_total / linear_total) << " 倍" << std::endl;
        
        std::cout << "\n🎯 实际误差控制效果:" << std::endl;
        std::cout << "  GPS轨迹压缩器2D违反率: " << std::setprecision(3) << gps.violation_rate_2d << "%" << std::endl;
        std::cout << "  线性压缩器2D违反率: " << linear.violation_rate_2d << "%" << std::endl;
        
        std::cout << "\n💡 关键洞察:" << std::endl;
        std::cout << "1. 线性压缩器的实际2D误差界限比GPS轨迹压缩器宽松" << std::endl;
        std::cout << "   " << (linear.actual_max_2d_error_bound / gps.actual_max_2d_error_bound) << " 倍" << std::endl;
        std::cout << "2. 即使在更宽松的误差界限下，线性压缩器仍有" << std::endl;
        std::cout << "   " << (linear.compression_ratio / gps.compression_ratio) << " 倍的压缩比优势" << std::endl;
        std::cout << "3. 这说明压缩比差异主要来自算法设计，而非误差界限设置" << std::endl;
        
        // 计算真正公平的对比
        std::cout << "\n🔍 如果使用相同的2D误差界限会如何？" << std::endl;
        double fair_linear_max_diff = gps.actual_max_2d_error_bound / std::sqrt(2.0);
        std::cout << "  为了匹配GPS的2D误差界限，线性压缩器应使用单维度误差界限: " 
                  << std::scientific << std::setprecision(3) << fair_linear_max_diff << " 度" << std::endl;
        std::cout << "  这比当前设置严格 " << (linear.configured_error_bound / fair_linear_max_diff) << " 倍" << std::endl;
        std::cout << "  在更严格的设置下，线性压缩器的压缩比会降低，但仍可能保持优势" << std::endl;
    }
}

int main() {
    std::cout << "=== 真正公平的GPS轨迹压缩算法对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 25000;
    
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
    
    std::vector<ComparisonResult> results;
    
    // 测试GPS轨迹压缩器
    const double gps_error_bound = 1e-5; // 2D欧几里得距离误差界限
    std::cout << "\n🔄 测试GPS轨迹压缩器 (2D误差界限: " << std::scientific << gps_error_bound << ")..." << std::endl;
    
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    ComparisonResult gps_result = TestGpsCompressor(points, gps_error_bound, epsilon_v, epsilon_theta);
    if (gps_result.success) {
        results.push_back(gps_result);
        std::cout << "✅ GPS轨迹压缩器测试完成" << std::endl;
    }
    
    // 测试线性压缩器
    const double linear_max_diff = 1e-5; // 单维度误差界限
    std::cout << "\n🔄 测试线性压缩器 (单维度误差界限: " << linear_max_diff 
              << ", 理论最大2D误差: " << (linear_max_diff * std::sqrt(2.0)) << ")..." << std::endl;
    
    ComparisonResult linear_result = TestLinearCompressor(points, linear_max_diff);
    if (linear_result.success) {
        results.push_back(linear_result);
        std::cout << "✅ 线性压缩器测试完成" << std::endl;
    }
    
    // 打印真正公平的对比分析
    PrintTrueFairComparison(results);
    
    return 0;
}
