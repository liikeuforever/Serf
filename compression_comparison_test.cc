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

struct CompressionResult {
    std::string algorithm_name;
    int original_size_bytes;
    int compressed_size_bytes;
    double compression_ratio;
    double compression_time_ms;
    double decompression_time_ms;
    double max_error;
    double avg_error;
    int error_violations;
    bool success;
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
                continue; // 跳过无效数据
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
CompressionResult TestGpsTrajectoryCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                             double error_bound, double epsilon_v, double epsilon_theta) {
    CompressionResult result;
    result.algorithm_name = "GPS轨迹压缩器 (Serf-QT-GPS)";
    result.success = false;
    
    try {
        // 压缩
        auto start_time = std::chrono::high_resolution_clock::now();
        
        SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
        for (const auto& point : points) {
            compressor.AddGpsPoint(point);
        }
        Array<uint8_t> compressed_data = compressor.GetCompressedData();
        
        auto compress_end = std::chrono::high_resolution_clock::now();
        
        // 解压缩
        SerfQtGpsConfigurableDecompressor decompressor(compressed_data);
        std::vector<SerfQtGpsConfigurableDecompressor::GpsPoint> decompressed_points;
        
        for (size_t i = 0; i < points.size(); ++i) {
            decompressed_points.push_back(decompressor.GetNextGpsPoint());
        }
        
        auto decompress_end = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        result.original_size_bytes = points.size() * 16; // 每个点16字节
        result.compressed_size_bytes = compressed_data.length();
        result.compression_ratio = static_cast<double>(result.original_size_bytes) / result.compressed_size_bytes;
        
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
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "GPS轨迹压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

// 测试线性压缩器（分别压缩经度和纬度）
CompressionResult TestLinearCompressor(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                      double max_diff) {
    CompressionResult result;
    result.algorithm_name = "线性压缩器 (Serf-QT-Linear)";
    result.success = false;
    
    try {
        // 分离经度和纬度
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
        
        // 解压缩经度
        SerfQtLinearDecompressor lon_decompressor;
        std::vector<double> decompressed_longitudes = lon_decompressor.Decompress(lon_compressed);
        
        // 解压缩纬度
        SerfQtLinearDecompressor lat_decompressor;
        std::vector<double> decompressed_latitudes = lat_decompressor.Decompress(lat_compressed);
        
        auto decompress_end = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        result.original_size_bytes = points.size() * 16; // 每个点16字节
        result.compressed_size_bytes = lon_compressed.length() + lat_compressed.length();
        result.compression_ratio = static_cast<double>(result.original_size_bytes) / result.compressed_size_bytes;
        
        auto compress_duration = std::chrono::duration_cast<std::chrono::microseconds>(compress_end - start_time);
        auto decompress_duration = std::chrono::duration_cast<std::chrono::microseconds>(decompress_end - compress_end);
        result.compression_time_ms = compress_duration.count() / 1000.0;
        result.decompression_time_ms = decompress_duration.count() / 1000.0;
        
        // 计算误差统计
        std::vector<double> errors;
        result.error_violations = 0;
        double error_bound = max_diff; // 使用max_diff作为误差界限
        
        for (size_t i = 0; i < points.size(); ++i) {
            double dx = points[i].longitude - decompressed_longitudes[i];
            double dy = points[i].latitude - decompressed_latitudes[i];
            double error = std::sqrt(dx * dx + dy * dy);
            errors.push_back(error);
            
            if (error > error_bound) {
                result.error_violations++;
            }
        }
        
        result.avg_error = std::accumulate(errors.begin(), errors.end(), 0.0) / errors.size();
        result.max_error = *std::max_element(errors.begin(), errors.end());
        
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "线性压缩器测试失败: " << e.what() << std::endl;
    }
    
    return result;
}

// 打印结果对比
void PrintComparisonResults(const std::vector<CompressionResult>& results) {
    std::cout << "\n=== 压缩算法性能对比 ===" << std::endl;
    
    // 表头
    std::cout << std::left 
              << std::setw(25) << "算法"
              << std::setw(12) << "压缩比"
              << std::setw(12) << "压缩时间"
              << std::setw(12) << "解压时间"
              << std::setw(12) << "平均误差"
              << std::setw(12) << "最大误差"
              << std::setw(10) << "违反数"
              << std::setw(8) << "状态" << std::endl;
    
    std::cout << std::string(110, '-') << std::endl;
    
    for (const auto& result : results) {
        if (!result.success) continue;
        
        std::cout << std::left 
                  << std::setw(25) << result.algorithm_name.substr(0, 23)
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << result.compression_ratio << ":1"
                  << std::setprecision(1)
                  << std::setw(12) << result.compression_time_ms << "ms"
                  << std::setw(12) << result.decompression_time_ms << "ms"
                  << std::scientific << std::setprecision(2)
                  << std::setw(12) << result.avg_error
                  << std::setw(12) << result.max_error
                  << std::fixed << std::setprecision(0)
                  << std::setw(10) << result.error_violations
                  << std::setw(8) << (result.error_violations == 0 ? "✅" : "❌") << std::endl;
    }
    
    // 详细分析
    std::cout << "\n=== 详细性能分析 ===" << std::endl;
    
    for (const auto& result : results) {
        if (!result.success) continue;
        
        std::cout << "\n" << result.algorithm_name << ":" << std::endl;
        std::cout << "  原始大小: " << std::fixed << std::setprecision(2) 
                  << (result.original_size_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "  压缩大小: " << (result.compressed_size_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "  空间节省: " << std::setprecision(1) 
                  << (100.0 * (1.0 - static_cast<double>(result.compressed_size_bytes) / result.original_size_bytes)) << "%" << std::endl;
        std::cout << "  总处理时间: " << (result.compression_time_ms + result.decompression_time_ms) << " ms" << std::endl;
        std::cout << "  处理速度: " << std::setprecision(0) 
                  << (result.original_size_bytes / 16 / ((result.compression_time_ms + result.decompression_time_ms) / 1000.0)) << " 点/秒" << std::endl;
        std::cout << "  误差控制: " << (result.error_violations == 0 ? "完美" : "有违反") << std::endl;
    }
}

int main() {
    std::cout << "=== GPS轨迹压缩算法性能对比测试 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 50000; // 使用5万个点进行全面对比
    
    std::cout << "数据文件: " << data_file << std::endl;
    std::cout << "测试点数: " << test_count << std::endl;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << " 个点" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 数据范围分析
    double min_lon = points[0].longitude, max_lon = points[0].longitude;
    double min_lat = points[0].latitude, max_lat = points[0].latitude;
    
    for (const auto& point : points) {
        min_lon = std::min(min_lon, point.longitude);
        max_lon = std::max(max_lon, point.longitude);
        min_lat = std::min(min_lat, point.latitude);
        max_lat = std::max(max_lat, point.latitude);
    }
    
    std::cout << "\n数据范围分析:" << std::endl;
    std::cout << "  经度: " << std::fixed << std::setprecision(6) << min_lon << " ~ " << max_lon 
              << " (跨度: " << (max_lon - min_lon) << "度)" << std::endl;
    std::cout << "  纬度: " << min_lat << " ~ " << max_lat 
              << " (跨度: " << (max_lat - min_lat) << "度)" << std::endl;
    
    std::vector<CompressionResult> results;
    
    // 测试GPS轨迹压缩器（使用最佳参数）
    std::cout << "\n🔄 测试GPS轨迹压缩器..." << std::endl;
    const double error_bound = 1e-5;
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    CompressionResult gps_result = TestGpsTrajectoryCompressor(points, error_bound, epsilon_v, epsilon_theta);
    if (gps_result.success) {
        results.push_back(gps_result);
        std::cout << "✅ GPS轨迹压缩器测试完成" << std::endl;
    }
    
    // 测试线性压缩器（使用相同的误差界限）
    std::cout << "\n🔄 测试线性压缩器..." << std::endl;
    const double max_diff = error_bound; // 使用相同的误差界限
    
    CompressionResult linear_result = TestLinearCompressor(points, max_diff);
    if (linear_result.success) {
        results.push_back(linear_result);
        std::cout << "✅ 线性压缩器测试完成" << std::endl;
    }
    
    // 测试不同误差界限的线性压缩器
    std::cout << "\n🔄 测试不同误差界限的线性压缩器..." << std::endl;
    std::vector<double> max_diffs = {1e-4, 1e-3, 1e-2};
    
    for (double diff : max_diffs) {
        CompressionResult linear_result_alt = TestLinearCompressor(points, diff);
        if (linear_result_alt.success) {
            linear_result_alt.algorithm_name += " (误差=" + std::to_string(diff) + ")";
            results.push_back(linear_result_alt);
        }
    }
    
    // 打印对比结果
    PrintComparisonResults(results);
    
    // 算法优势分析
    std::cout << "\n=== 算法优势分析 ===" << std::endl;
    
    if (results.size() >= 2) {
        const auto& gps_res = results[0];
        const auto& linear_res = results[1];
        
        std::cout << "\n压缩比对比:" << std::endl;
        if (gps_res.compression_ratio > linear_res.compression_ratio) {
            double improvement = (gps_res.compression_ratio / linear_res.compression_ratio - 1) * 100;
            std::cout << "  🏆 GPS轨迹压缩器压缩比更优，提升 " << std::fixed << std::setprecision(1) 
                      << improvement << "%" << std::endl;
        } else {
            double improvement = (linear_res.compression_ratio / gps_res.compression_ratio - 1) * 100;
            std::cout << "  🏆 线性压缩器压缩比更优，提升 " << improvement << "%" << std::endl;
        }
        
        std::cout << "\n处理速度对比:" << std::endl;
        double gps_speed = test_count / ((gps_res.compression_time_ms + gps_res.decompression_time_ms) / 1000.0);
        double linear_speed = test_count / ((linear_res.compression_time_ms + linear_res.decompression_time_ms) / 1000.0);
        
        if (gps_speed > linear_speed) {
            double improvement = (gps_speed / linear_speed - 1) * 100;
            std::cout << "  ⚡ GPS轨迹压缩器处理速度更快，提升 " << improvement << "%" << std::endl;
        } else {
            double improvement = (linear_speed / gps_speed - 1) * 100;
            std::cout << "  ⚡ 线性压缩器处理速度更快，提升 " << improvement << "%" << std::endl;
        }
        
        std::cout << "\n误差控制对比:" << std::endl;
        std::cout << "  GPS轨迹压缩器: " << gps_res.error_violations << " 个违反" << std::endl;
        std::cout << "  线性压缩器: " << linear_res.error_violations << " 个违反" << std::endl;
        
        if (gps_res.error_violations < linear_res.error_violations) {
            std::cout << "  🎯 GPS轨迹压缩器误差控制更好" << std::endl;
        } else if (linear_res.error_violations < gps_res.error_violations) {
            std::cout << "  🎯 线性压缩器误差控制更好" << std::endl;
        } else {
            std::cout << "  🎯 两种算法误差控制相当" << std::endl;
        }
    }
    
    return 0;
}
