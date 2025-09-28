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

void TestWithBlockSize(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points, 
                       int test_size, int block_size) {
    
    std::cout << "\n=== 测试 " << test_size << " 个点，block_size=" << block_size << " ===" << std::endl;
    
    const double gps_error_bound = 1.4e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 1e-5;
    
    try {
        // 压缩
        SerfQtGpsConfigurableCompressor gps_compressor(block_size, gps_error_bound, epsilon_v, epsilon_theta);
        
        for (int i = 0; i < test_size; ++i) {
            gps_compressor.AddGpsPoint(points[i]);
        }
        
        Array<uint8_t> compressed_data = gps_compressor.GetCompressedData();
        
        std::cout << "压缩成功，数据大小: " << compressed_data.length() << " 字节" << std::endl;
        
        // 解压
        SerfQtGpsConfigurableDecompressor gps_decompressor(compressed_data);
        
        double max_error = 0.0;
        int error_count = 0;
        
        for (int i = 0; i < test_size; ++i) {
            auto decompressed_point = gps_decompressor.GetNextGpsPoint();
            
            double error = Calculate2DDistance(
                SerfQtGpsConfigurableCompressor::GpsPoint(points[i].longitude, points[i].latitude),
                SerfQtGpsConfigurableCompressor::GpsPoint(decompressed_point.longitude, decompressed_point.latitude)
            );
            
            if (error > max_error) {
                max_error = error;
            }
            
            if (error > gps_error_bound) {
                error_count++;
            }
            
            // 只打印前5个点的详细信息
            if (i < 5) {
                std::cout << "  点" << i << ": 误差=" << std::scientific << std::setprecision(2) 
                          << error << " 度 " << ((error <= gps_error_bound) ? "✅" : "❌") << std::endl;
            }
        }
        
        double error_rate = static_cast<double>(error_count) / test_size * 100.0;
        
        std::cout << "📊 结果:" << std::endl;
        std::cout << "  最大误差: " << std::scientific << max_error << " 度" << std::endl;
        std::cout << "  超出界限的点: " << error_count << " (" << std::fixed << std::setprecision(1) << error_rate << "%)" << std::endl;
        
        if (error_rate < 0.1) {
            std::cout << "  ✅ 成功！" << std::endl;
        } else {
            std::cout << "  ❌ 失败！" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ 异常: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== 调试block_size对同步的影响 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, 100);
    
    if (points.size() < 50) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    std::cout << "\n=== 测试不同的block_size配置 ===" << std::endl;
    
    // 测试10个点（已知可以工作）
    std::cout << "\n🔍 测试10个点（已知可以工作的情况）:" << std::endl;
    TestWithBlockSize(points, 10, 10);   // block_size = 数据点数
    TestWithBlockSize(points, 10, 50);   // block_size > 数据点数
    TestWithBlockSize(points, 10, 100);  // block_size >> 数据点数
    
    // 测试50个点（已知会失败）
    std::cout << "\n🔍 测试50个点（已知会失败的情况）:" << std::endl;
    TestWithBlockSize(points, 50, 10);   // block_size < 数据点数
    TestWithBlockSize(points, 50, 50);   // block_size = 数据点数
    TestWithBlockSize(points, 50, 100);  // block_size > 数据点数
    
    std::cout << "\n=== 分析 ===" << std::endl;
    
    std::cout << "💡 关键观察:" << std::endl;
    std::cout << "1. 如果block_size影响同步，那么相同数据点数但不同block_size会有不同结果" << std::endl;
    std::cout << "2. 如果block_size不影响同步，那么问题在其他地方" << std::endl;
    std::cout << "3. 特别关注block_size < 数据点数的情况" << std::endl;
    
    return 0;
}
