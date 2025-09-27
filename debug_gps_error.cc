#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

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
    std::cout << "=== GPS轨迹压缩误差调试 ===" << std::endl;
    
    const std::string data_file = "../test/data_set/Geolife_100k_longitude_latitude.csv";
    const double strict_error_bound = 1e-5;  // 1e-5度的严格误差界限
    const int test_count = 5;  // 只测试前5个点
    
    // 读取测试数据
    auto test_points = ReadGpsPoints(data_file, test_count);
    if (test_points.size() < test_count) {
        std::cerr << "无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "读取了 " << test_points.size() << " 个GPS点" << std::endl;
    std::cout << "误差界限: " << strict_error_bound << " 度" << std::endl << std::endl;
    
    // 显示原始数据
    std::cout << "原始GPS点:" << std::endl;
    for (size_t i = 0; i < test_points.size(); ++i) {
        std::cout << "  " << i << ": (" << test_points[i].longitude 
                  << ", " << test_points[i].latitude << ")" << std::endl;
    }
    std::cout << std::endl;
    
    // 压缩
    SerfQtGpsTrajectoryCompressor compressor(test_count, strict_error_bound);
    
    for (size_t i = 0; i < test_points.size(); ++i) {
        std::cout << "处理第 " << i << " 个点..." << std::endl;
        compressor.AddGpsPoint(test_points[i]);
    }
    
    compressor.Close();
    
    // 解压缩
    Array<uint8_t> compressed_data = compressor.compressed_bytes();
    SerfQtGpsTrajectoryDecompressor decompressor(compressed_data);
    
    std::vector<SerfQtGpsTrajectoryDecompressor::GpsPoint> reconstructed_points;
    for (size_t i = 0; i < test_points.size(); ++i) {
        auto point = decompressor.DecompressNextPoint();
        reconstructed_points.push_back(point);
    }
    
    // 分析误差
    std::cout << "\n=== 误差分析 ===" << std::endl;
    std::cout << "索引  原始经度      原始纬度      重构经度      重构纬度      误差" << std::endl;
    std::cout << "------------------------------------------------------------------------" << std::endl;
    
    double max_error = 0.0;
    int violations = 0;
    
    for (size_t i = 0; i < test_points.size(); ++i) {
        double error = CalculateDistance(test_points[i], reconstructed_points[i]);
        max_error = std::max(max_error, error);
        
        if (error > strict_error_bound) {
            violations++;
        }
        
        std::cout << std::setw(4) << i 
                  << std::setw(14) << std::fixed << std::setprecision(6) << test_points[i].longitude
                  << std::setw(14) << std::fixed << std::setprecision(6) << test_points[i].latitude
                  << std::setw(14) << std::fixed << std::setprecision(6) << reconstructed_points[i].longitude
                  << std::setw(14) << std::fixed << std::setprecision(6) << reconstructed_points[i].latitude
                  << std::setw(12) << std::scientific << std::setprecision(3) << error;
        
        if (error > strict_error_bound) {
            std::cout << " ✗ 超出界限";
        } else {
            std::cout << " ✓ 满足要求";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n总结:" << std::endl;
    std::cout << "  最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "  误差界限: " << std::scientific << strict_error_bound << " 度" << std::endl;
    std::cout << "  违反数量: " << violations << "/" << test_points.size() << std::endl;
    
    // 压缩统计
    long original_bits = test_points.size() * 2 * 64;
    long compressed_bits = compressor.get_compressed_size_in_bits();
    double compression_ratio = static_cast<double>(original_bits) / static_cast<double>(compressed_bits);
    
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    
    return 0;
}
