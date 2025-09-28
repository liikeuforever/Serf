#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "decompressor/serf_qt_gps_configurable_decompressor.h"

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
    std::cout << "=== 调试同步问题：逐点对比压缩器和解压器状态 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 10;  // 只测试前10个点，便于详细调试
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double gps_error_bound = 1.4e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 1e-5;
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    std::cout << "\n=== 压缩过程 ===" << std::endl;
    
    // 压缩
    SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    
    for (int i = 0; i < points.size(); ++i) {
        std::cout << "\n压缩点 " << i << ": (" << std::fixed << std::setprecision(6) 
                  << points[i].longitude << ", " << points[i].latitude << ")" << std::endl;
        gps_compressor.AddGpsPoint(points[i]);
    }
    
    Array<uint8_t> compressed_data = gps_compressor.GetCompressedData();
    std::cout << "\n压缩完成，数据大小: " << compressed_data.length() << " 字节" << std::endl;
    
    std::cout << "\n=== 解压过程 ===" << std::endl;
    
    // 解压
    SerfQtGpsConfigurableDecompressor gps_decompressor(compressed_data);
    
    std::cout << "\n逐点解压对比:" << std::endl;
    std::cout << std::setw(4) << "点" 
              << std::setw(15) << "原始经度" 
              << std::setw(15) << "原始纬度" 
              << std::setw(15) << "重构经度" 
              << std::setw(15) << "重构纬度" 
              << std::setw(12) << "误差(度)" 
              << std::setw(12) << "状态" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    double max_error = 0.0;
    int error_count = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        auto decompressed_point = gps_decompressor.GetNextGpsPoint();
        
        double error = Calculate2DDistance(
            SerfQtGpsConfigurableCompressor::GpsPoint(points[i].longitude, points[i].latitude),
            SerfQtGpsConfigurableCompressor::GpsPoint(decompressed_point.longitude, decompressed_point.latitude)
        );
        
        if (error > max_error) {
            max_error = error;
        }
        
        std::string status = "✅";
        if (error > gps_error_bound) {
            error_count++;
            status = "❌";
        }
        
        std::cout << std::setw(4) << i
                  << std::setw(15) << std::fixed << std::setprecision(6) << points[i].longitude
                  << std::setw(15) << points[i].latitude
                  << std::setw(15) << decompressed_point.longitude
                  << std::setw(15) << decompressed_point.latitude
                  << std::setw(12) << std::scientific << std::setprecision(2) << error
                  << std::setw(12) << status << std::endl;
        
        // 详细分析前几个点
        if (i < 5) {
            std::cout << "  详细分析点" << i << ":" << std::endl;
            std::cout << "    原始点: (" << std::fixed << std::setprecision(8) 
                      << points[i].longitude << ", " << points[i].latitude << ")" << std::endl;
            std::cout << "    重构点: (" << decompressed_point.longitude << ", " 
                      << decompressed_point.latitude << ")" << std::endl;
            std::cout << "    误差: " << std::scientific << error << " 度" << std::endl;
            
            if (error > gps_error_bound) {
                std::cout << "    ⚠️ 误差超出界限！" << std::endl;
                double error_ratio = error / gps_error_bound;
                std::cout << "    误差倍数: " << std::fixed << std::setprecision(1) << error_ratio << "x" << std::endl;
            }
        }
    }
    
    std::cout << "\n=== 同步问题诊断 ===" << std::endl;
    
    std::cout << "📊 误差统计:" << std::endl;
    std::cout << "  最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "  误差界限: " << gps_error_bound << " 度" << std::endl;
    std::cout << "  超出界限的点: " << error_count << " / " << points.size() << std::endl;
    std::cout << "  误差率: " << std::fixed << std::setprecision(1) << (100.0 * error_count / points.size()) << "%" << std::endl;
    
    if (error_count == 0) {
        std::cout << "✅ 同步完美！没有误差问题" << std::endl;
    } else if (error_count < points.size() / 2) {
        std::cout << "⚠️ 部分同步问题，需要进一步调试" << std::endl;
    } else {
        std::cout << "🚨 严重同步问题！压缩器和解压器状态不一致" << std::endl;
    }
    
    std::cout << "\n💡 可能的问题原因:" << std::endl;
    if (max_error > 1.0) {
        std::cout << "1. 🔧 状态更新逻辑错误 - 误差过大表明基本逻辑有问题" << std::endl;
        std::cout << "2. 📊 参数读写不一致 - 压缩器和解压器使用了不同的参数" << std::endl;
        std::cout << "3. 🔄 编码解码不匹配 - 编码和解码逻辑不对称" << std::endl;
    } else if (max_error > gps_error_bound * 10) {
        std::cout << "1. 📏 量化精度问题 - 量化步长可能过大" << std::endl;
        std::cout << "2. 🎯 预测逻辑差异 - 压缩器和解压器的预测不一致" << std::endl;
    } else {
        std::cout << "1. ✅ 基本逻辑正确，可能是精度调优问题" << std::endl;
    }
    
    return 0;
}
