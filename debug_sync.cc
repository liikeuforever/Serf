#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>

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

int main() {
    std::cout << "=== 压缩端与解压端同步性调试 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const double error_bound = 1e-4;  // 使用中等严格的误差界限
    const int test_count = 10;        // 只测试10个点，便于详细分析
    const int block_size = 100;
    
    // 读取测试数据
    auto test_points = ReadGpsPoints(data_file, test_count);
    if (test_points.size() < test_count) {
        std::cerr << "无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "测试点数: " << test_count << std::endl;
    std::cout << "误差界限: " << std::scientific << error_bound << " 度" << std::endl;
    
    // 显示原始数据
    std::cout << "\n原始GPS点:" << std::endl;
    for (size_t i = 0; i < test_points.size(); ++i) {
        std::cout << "  " << i << ": (" << std::fixed << std::setprecision(6) 
                  << test_points[i].longitude << ", " << test_points[i].latitude << ")" << std::endl;
    }
    
    // 压缩
    std::cout << "\n=== 开始压缩 ===" << std::endl;
    SerfQtGpsTrajectoryCompressor compressor(block_size, error_bound);
    
    for (size_t i = 0; i < test_points.size(); ++i) {
        std::cout << "\n压缩第 " << i << " 个点: (" 
                  << std::fixed << std::setprecision(6) 
                  << test_points[i].longitude << ", " << test_points[i].latitude << ")" << std::endl;
        compressor.AddGpsPoint(test_points[i]);
    }
    
    compressor.Close();
    
    // 解压缩
    std::cout << "\n=== 开始解压缩 ===" << std::endl;
    Array<uint8_t> compressed_data = compressor.compressed_bytes();
    SerfQtGpsTrajectoryDecompressor decompressor(compressed_data);
    
    std::vector<SerfQtGpsTrajectoryDecompressor::GpsPoint> reconstructed_points;
    
    for (int i = 0; i < test_count; ++i) {
        auto point = decompressor.DecompressNextPoint();
        reconstructed_points.push_back(point);
        
        std::cout << "解压第 " << i << " 个点: (" 
                  << std::fixed << std::setprecision(6) 
                  << point.longitude << ", " << point.latitude << ")" << std::endl;
    }
    
    // 详细误差分析
    std::cout << "\n=== 详细误差分析 ===" << std::endl;
    std::cout << "索引  原始经度      原始纬度      重构经度      重构纬度      误差          状态" << std::endl;
    std::cout << "------------------------------------------------------------------------------------" << std::endl;
    
    double max_error = 0.0;
    int violations = 0;
    
    for (size_t i = 0; i < test_points.size(); ++i) {
        double error = CalculateDistance(test_points[i], reconstructed_points[i]);
        max_error = std::max(max_error, error);
        
        std::string status = (error <= error_bound) ? "✓ 满足" : "✗ 超出";
        if (error > error_bound) violations++;
        
        std::cout << std::setw(4) << i 
                  << std::setw(14) << std::fixed << std::setprecision(6) << test_points[i].longitude
                  << std::setw(14) << std::fixed << std::setprecision(6) << test_points[i].latitude
                  << std::setw(14) << std::fixed << std::setprecision(6) << reconstructed_points[i].longitude
                  << std::setw(14) << std::fixed << std::setprecision(6) << reconstructed_points[i].latitude
                  << std::setw(12) << std::scientific << std::setprecision(3) << error
                  << "  " << status << std::endl;
    }
    
    std::cout << "\n总结:" << std::endl;
    std::cout << "  最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "  误差界限: " << std::scientific << error_bound << " 度" << std::endl;
    std::cout << "  违反数量: " << violations << "/" << test_points.size() << std::endl;
    std::cout << "  成功率: " << std::fixed << std::setprecision(1) 
              << ((test_points.size() - violations) * 100.0 / test_points.size()) << "%" << std::endl;
    
    // 压缩统计
    long original_bits = test_points.size() * 2 * 64;
    long compressed_bits = compressor.get_compressed_size_in_bits();
    double compression_ratio = static_cast<double>(original_bits) / static_cast<double>(compressed_bits);
    
    std::cout << "\n压缩统计:" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    
    return 0;
}
