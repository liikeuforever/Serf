#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <chrono>

#include "compressor/serf_qt_gps_trajectory_compressor.h"
#include "decompressor/serf_qt_gps_trajectory_decompressor.h"

std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> ReadGpsPoints(const std::string& filename, int count) {
    std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> points;
    std::ifstream file(filename);
    std::string line;
    
    int read_count = 0;
    while (std::getline(file, line) && read_count < count) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str)) {
            try {
                double longitude = std::stod(lon_str);
                double latitude = std::stod(lat_str);
                points.emplace_back(longitude, latitude);
                read_count++;
            } catch (const std::exception& e) {
                continue;
            }
        }
    }
    
    return points;
}

double CalculateDistance(const SerfQtGpsTrajectoryCompressor::GpsPoint& p1, 
                        const SerfQtGpsTrajectoryDecompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

void AnalyzeErrors(const std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint>& original,
                   const std::vector<SerfQtGpsTrajectoryDecompressor::GpsPoint>& reconstructed,
                   double error_bound) {
    
    std::vector<double> errors;
    errors.reserve(original.size());
    
    for (size_t i = 0; i < original.size(); ++i) {
        double error = CalculateDistance(original[i], reconstructed[i]);
        errors.push_back(error);
    }
    
    // 统计分析
    double max_error = *std::max_element(errors.begin(), errors.end());
    double min_error = *std::min_element(errors.begin(), errors.end());
    double sum_error = std::accumulate(errors.begin(), errors.end(), 0.0);
    double avg_error = sum_error / errors.size();
    
    // 计算违反数量
    int violations = 0;
    for (double error : errors) {
        if (error > error_bound) {
            violations++;
        }
    }
    
    // 计算百分位数
    std::vector<double> sorted_errors = errors;
    std::sort(sorted_errors.begin(), sorted_errors.end());
    
    double p50 = sorted_errors[sorted_errors.size() * 0.5];
    double p90 = sorted_errors[sorted_errors.size() * 0.9];
    double p95 = sorted_errors[sorted_errors.size() * 0.95];
    double p99 = sorted_errors[sorted_errors.size() * 0.99];
    
    std::cout << "\n=== 误差统计分析 (" << original.size() << "个GPS点) ===" << std::endl;
    std::cout << "误差界限: " << std::scientific << std::setprecision(3) << error_bound << " 度" << std::endl;
    std::cout << "总点数: " << original.size() << std::endl;
    std::cout << "\n误差分布:" << std::endl;
    std::cout << "  最小误差: " << std::scientific << std::setprecision(3) << min_error << " 度" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << avg_error << " 度" << std::endl;
    std::cout << "  最大误差: " << std::scientific << std::setprecision(3) << max_error << " 度" << std::endl;
    
    std::cout << "\n百分位数:" << std::endl;
    std::cout << "  50%: " << std::scientific << std::setprecision(3) << p50 << " 度" << std::endl;
    std::cout << "  90%: " << std::scientific << std::setprecision(3) << p90 << " 度" << std::endl;
    std::cout << "  95%: " << std::scientific << std::setprecision(3) << p95 << " 度" << std::endl;
    std::cout << "  99%: " << std::scientific << std::setprecision(3) << p99 << " 度" << std::endl;
    
    std::cout << "\n误差控制:" << std::endl;
    std::cout << "  违反界限: " << violations << "/" << original.size() 
              << " (" << std::fixed << std::setprecision(1) << (violations * 100.0 / original.size()) << "%)" << std::endl;
    std::cout << "  满足要求: " << (original.size() - violations) << "/" << original.size()
              << " (" << std::fixed << std::setprecision(1) << ((original.size() - violations) * 100.0 / original.size()) << "%)" << std::endl;
    
    if (violations == 0) {
        std::cout << "  ✅ 所有点都满足误差要求！" << std::endl;
    } else if (violations <= original.size() * 0.05) {
        std::cout << "  ✅ 95%以上的点满足误差要求，表现优秀！" << std::endl;
    } else if (violations <= original.size() * 0.1) {
        std::cout << "  ⚠️  90%以上的点满足误差要求，表现良好" << std::endl;
    } else {
        std::cout << "  ❌ 误差控制需要改进" << std::endl;
    }
}

int main() {
    std::cout << "=== GPS轨迹压缩算法验证 (1000个点) ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const double error_bound = 1e-4;  // 1e-4度的误差界限（约11米）
    const int test_count = 1000;      // 测试1000个点
    const int block_size = 1000;      // 块大小设为1000
    
    std::cout << "数据文件: " << data_file << std::endl;
    std::cout << "测试点数: " << test_count << std::endl;
    std::cout << "误差界限: " << std::scientific << error_bound << " 度 (约" 
              << std::fixed << std::setprecision(1) << (error_bound * 111000) << "米)" << std::endl;
    
    // 读取测试数据
    auto test_points = ReadGpsPoints(data_file, test_count);
    if (test_points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，只读取了 " << test_points.size() << " 个点" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << test_points.size() << " 个GPS点" << std::endl;
    
    // 显示数据范围
    double min_lon = test_points[0].longitude, max_lon = test_points[0].longitude;
    double min_lat = test_points[0].latitude, max_lat = test_points[0].latitude;
    
    for (const auto& point : test_points) {
        min_lon = std::min(min_lon, point.longitude);
        max_lon = std::max(max_lon, point.longitude);
        min_lat = std::min(min_lat, point.latitude);
        max_lat = std::max(max_lat, point.latitude);
    }
    
    std::cout << "数据范围:" << std::endl;
    std::cout << "  经度: " << std::fixed << std::setprecision(6) << min_lon 
              << " ~ " << max_lon << " (跨度: " << (max_lon - min_lon) << "度)" << std::endl;
    std::cout << "  纬度: " << std::fixed << std::setprecision(6) << min_lat 
              << " ~ " << max_lat << " (跨度: " << (max_lat - min_lat) << "度)" << std::endl;
    
    // 压缩
    std::cout << "\n🔄 开始压缩..." << std::endl;
    SerfQtGpsTrajectoryCompressor compressor(block_size, error_bound);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& point : test_points) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    auto compress_time = std::chrono::high_resolution_clock::now();
    
    // 解压缩
    std::cout << "🔄 开始解压缩..." << std::endl;
    Array<uint8_t> compressed_data = compressor.compressed_bytes();
    SerfQtGpsTrajectoryDecompressor decompressor(compressed_data);
    
    std::vector<SerfQtGpsTrajectoryDecompressor::GpsPoint> reconstructed_points;
    reconstructed_points.reserve(test_count);
    
    for (int i = 0; i < test_count; ++i) {
        auto point = decompressor.DecompressNextPoint();
        reconstructed_points.push_back(point);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // 性能统计
    auto compress_duration = std::chrono::duration_cast<std::chrono::microseconds>(compress_time - start_time);
    auto decompress_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - compress_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "✅ 压缩解压缩完成！" << std::endl;
    
    // 压缩比分析
    long original_bits = test_count * 2 * 64;  // 每个GPS点2个double，每个double 64位
    long compressed_bits = compressor.get_compressed_size_in_bits();
    double compression_ratio = static_cast<double>(original_bits) / static_cast<double>(compressed_bits);
    
    std::cout << "\n=== 压缩性能分析 ===" << std::endl;
    std::cout << "原始大小: " << original_bits << " 比特 (" << (original_bits / 8) << " 字节)" << std::endl;
    std::cout << "压缩大小: " << compressed_bits << " 比特 (" << (compressed_bits / 8) << " 字节)" << std::endl;
    std::cout << "压缩比: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    std::cout << "空间节省: " << std::fixed << std::setprecision(1) 
              << (100.0 * (1.0 - static_cast<double>(compressed_bits) / original_bits)) << "%" << std::endl;
    
    std::cout << "\n=== 时间性能分析 ===" << std::endl;
    std::cout << "压缩时间: " << compress_duration.count() << " 微秒" << std::endl;
    std::cout << "解压时间: " << decompress_duration.count() << " 微秒" << std::endl;
    std::cout << "总时间: " << total_duration.count() << " 微秒" << std::endl;
    std::cout << "压缩速度: " << std::fixed << std::setprecision(0) 
              << (test_count * 1000000.0 / compress_duration.count()) << " 点/秒" << std::endl;
    
    // 误差分析
    AnalyzeErrors(test_points, reconstructed_points, error_bound);
    
    return 0;
}
