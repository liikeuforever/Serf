#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

#include "compressor/serf_qt_gps_trajectory_compressor.h"
#include "decompressor/serf_qt_gps_trajectory_decompressor.h"

/**
 * 简单的GPS轨迹压缩测试程序
 * 使用Geolife数据集进行测试
 */

int main() {
    std::cout << "=== GPS轨迹压缩简单测试 ===" << std::endl;
    
    // 测试参数
    const std::string data_file = "../test/data_set/Geolife_100k_longitude_latitude.csv";
    const int block_size = 1000;
    const double max_error = 1e-4;
    const int test_points = 100;  // 测试前100个点
    
    std::cout << "数据文件: " << data_file << std::endl;
    std::cout << "块大小: " << block_size << std::endl;
    std::cout << "最大误差: " << max_error << " 度" << std::endl;
    std::cout << "测试点数: " << test_points << std::endl << std::endl;
    
    // 读取GPS数据
    std::ifstream file(data_file);
    if (!file.is_open()) {
        std::cerr << "无法打开数据文件: " << data_file << std::endl;
        return 1;
    }
    
    std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> gps_points;
    std::string line;
    int count = 0;
    
    while (std::getline(file, line) && count < test_points) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str)) {
            try {
                double longitude = std::stod(lon_str);
                double latitude = std::stod(lat_str);
                gps_points.emplace_back(longitude, latitude);
                count++;
            } catch (const std::exception& e) {
                std::cerr << "解析GPS坐标失败: " << line << std::endl;
                continue;
            }
        }
    }
    file.close();
    
    if (gps_points.empty()) {
        std::cerr << "没有读取到有效的GPS数据" << std::endl;
        return 1;
    }
    
    std::cout << "成功读取 " << gps_points.size() << " 个GPS点" << std::endl;
    
    // 显示前几个原始点
    std::cout << "\n前5个原始GPS点:" << std::endl;
    for (size_t i = 0; i < std::min(size_t(5), gps_points.size()); ++i) {
        std::cout << "  " << i << ": (" << std::fixed << std::setprecision(6) 
                  << gps_points[i].longitude << ", " << gps_points[i].latitude << ")" << std::endl;
    }
    
    // 压缩
    std::cout << "\n开始压缩..." << std::endl;
    SerfQtGpsTrajectoryCompressor compressor(block_size, max_error);
    
    for (const auto& point : gps_points) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    Array<uint8_t> compressed_data = compressor.compressed_bytes();
    long compressed_bits = compressor.get_compressed_size_in_bits();
    
    std::cout << "压缩完成!" << std::endl;
    std::cout << "压缩后大小: " << compressed_data.length() << " 字节 (" << compressed_bits << " 比特)" << std::endl;
    
    // 计算压缩比
    size_t original_size = gps_points.size() * 2 * 8;  // 每个点2个double，每个double 8字节
    double compression_ratio = static_cast<double>(original_size) / static_cast<double>(compressed_data.length());
    std::cout << "原始大小: " << original_size << " 字节" << std::endl;
    std::cout << "压缩比: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    
    // 解压缩
    std::cout << "\n开始解压缩..." << std::endl;
    SerfQtGpsTrajectoryDecompressor decompressor(compressed_data);
    
    std::vector<SerfQtGpsTrajectoryDecompressor::GpsPoint> reconstructed_points;
    for (size_t i = 0; i < gps_points.size(); ++i) {
        auto point = decompressor.DecompressNextPoint();
        reconstructed_points.push_back(point);
    }
    
    std::cout << "解压缩完成!" << std::endl;
    std::cout << "重构点数: " << reconstructed_points.size() << std::endl;
    
    // 计算误差统计
    double max_error_actual = 0.0;
    double total_error = 0.0;
    
    for (size_t i = 0; i < gps_points.size(); ++i) {
        double dx = gps_points[i].longitude - reconstructed_points[i].longitude;
        double dy = gps_points[i].latitude - reconstructed_points[i].latitude;
        double error = std::sqrt(dx * dx + dy * dy);
        
        max_error_actual = std::max(max_error_actual, error);
        total_error += error;
    }
    
    double avg_error = total_error / gps_points.size();
    
    std::cout << "\n=== 误差统计 ===" << std::endl;
    std::cout << "最大误差: " << std::scientific << std::setprecision(6) << max_error_actual << " 度" << std::endl;
    std::cout << "平均误差: " << std::scientific << std::setprecision(6) << avg_error << " 度" << std::endl;
    std::cout << "误差上界: " << std::scientific << std::setprecision(6) << max_error << " 度" << std::endl;
    
    if (max_error_actual <= max_error) {
        std::cout << "✓ 所有重构点都在误差上界内" << std::endl;
    } else {
        std::cout << "✗ 部分重构点超出误差上界" << std::endl;
    }
    
    // 显示前几个重构点的对比
    std::cout << "\n=== 前5个点的对比 ===" << std::endl;
    std::cout << std::setw(5) << "索引" << std::setw(15) << "原始经度" << std::setw(15) << "原始纬度" 
              << std::setw(15) << "重构经度" << std::setw(15) << "重构纬度" << std::setw(12) << "误差" << std::endl;
    std::cout << std::string(77, '-') << std::endl;
    
    for (size_t i = 0; i < std::min(size_t(5), gps_points.size()); ++i) {
        double dx = gps_points[i].longitude - reconstructed_points[i].longitude;
        double dy = gps_points[i].latitude - reconstructed_points[i].latitude;
        double error = std::sqrt(dx * dx + dy * dy);
        
        std::cout << std::setw(5) << i 
                  << std::setw(15) << std::fixed << std::setprecision(6) << gps_points[i].longitude
                  << std::setw(15) << std::fixed << std::setprecision(6) << gps_points[i].latitude
                  << std::setw(15) << std::fixed << std::setprecision(6) << reconstructed_points[i].longitude
                  << std::setw(15) << std::fixed << std::setprecision(6) << reconstructed_points[i].latitude
                  << std::setw(12) << std::scientific << std::setprecision(3) << error << std::endl;
    }
    
    std::cout << "\n测试完成!" << std::endl;
    return 0;
}
