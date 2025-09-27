#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <chrono>

#include "compressor/serf_qt_gps_configurable_compressor.h"

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

int main() {
    std::cout << "=== GPS轨迹压缩策略分布详细分析 (10万个点) ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const double error_bound = 1e-5;
    const int test_count = 99999;
    
    // 使用最佳参数配置
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "数据文件: " << data_file << std::endl;
    std::cout << "测试点数: " << test_count << std::endl;
    std::cout << "误差界限: " << std::scientific << error_bound << " 度" << std::endl;
    std::cout << "参数配置: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << " 个点" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 使用带统计功能的压缩器进行压缩
    std::cout << "\n🔄 开始详细策略分析..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
    
    for (size_t i = 0; i < points.size(); ++i) {
        if (i % 10000 == 0) {
            std::cout << "\r进度: " << i << "/" << points.size() 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (100.0 * i / points.size()) << "%)" << std::flush;
        }
        compressor.AddGpsPoint(points[i]);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    
    std::cout << "\r✅ 压缩完成！处理时间: " << duration.count() << " ms" << std::endl;
    
    // 获取并打印详细统计信息
    const auto& stats = compressor.GetStrategyStats();
    stats.PrintStats();
    
    // 验证压缩结果
    std::cout << "\n=== 压缩结果验证 ===" << std::endl;
    
    int original_size = points.size() * 16; // 每个点16字节
    int compressed_size = compressed_data.length();
    double compression_ratio = static_cast<double>(original_size) / compressed_size;
    
    std::cout << "原始大小: " << original_size << " 字节 (" << std::setprecision(2) << (original_size / 1024.0 / 1024.0) << " MB)" << std::endl;
    std::cout << "压缩大小: " << compressed_size << " 字节 (" << (compressed_size / 1024.0 / 1024.0) << " MB)" << std::endl;
    std::cout << "实际压缩比: " << compression_ratio << ":1" << std::endl;
    std::cout << "空间节省: " << std::setprecision(1) << (100.0 * (1.0 - static_cast<double>(compressed_size) / original_size)) << "%" << std::endl;
    
    return 0;
}
