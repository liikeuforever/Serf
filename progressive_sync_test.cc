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

void TestDatasetSize(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points, 
                     int test_size, const std::string& size_name) {
    
    std::cout << "\n=== 测试 " << size_name << " (" << test_size << " 个点) ===" << std::endl;
    
    const double gps_error_bound = 1.4e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 1e-5;
    
    // 压缩
    SerfQtGpsConfigurableCompressor gps_compressor(test_size, gps_error_bound, epsilon_v, epsilon_theta);
    
    for (int i = 0; i < test_size; ++i) {
        gps_compressor.AddGpsPoint(points[i]);
    }
    
    Array<uint8_t> compressed_data = gps_compressor.GetCompressedData();
    const auto& gps_stats = gps_compressor.GetStrategyStats();
    
    // 解压
    SerfQtGpsConfigurableDecompressor gps_decompressor(compressed_data);
    
    double max_error = 0.0;
    double total_error = 0.0;
    int error_count = 0;
    
    // 检查前10个点和最后10个点的详细情况
    std::vector<double> first_errors, last_errors;
    
    for (int i = 0; i < test_size; ++i) {
        auto decompressed_point = gps_decompressor.GetNextGpsPoint();
        
        double error = Calculate2DDistance(
            SerfQtGpsConfigurableCompressor::GpsPoint(points[i].longitude, points[i].latitude),
            SerfQtGpsConfigurableCompressor::GpsPoint(decompressed_point.longitude, decompressed_point.latitude)
        );
        
        if (error > max_error) {
            max_error = error;
        }
        
        total_error += error;
        if (error > gps_error_bound) {
            error_count++;
        }
        
        // 收集前10个和后10个点的误差
        if (i < 10) {
            first_errors.push_back(error);
        } else if (i >= test_size - 10) {
            last_errors.push_back(error);
        }
    }
    
    double avg_error = total_error / test_size;
    double error_rate = static_cast<double>(error_count) / test_size * 100.0;
    double zero_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_stats.GetTotalPoints() * 100.0;
    
    std::cout << "📊 " << size_name << " 结果:" << std::endl;
    std::cout << "  最大误差: " << std::scientific << std::setprecision(3) << max_error << " 度" << std::endl;
    std::cout << "  平均误差: " << avg_error << " 度" << std::endl;
    std::cout << "  误差界限: " << gps_error_bound << " 度" << std::endl;
    std::cout << "  超出界限的点: " << error_count << " (" << std::fixed << std::setprecision(2) << error_rate << "%)" << std::endl;
    std::cout << "  零校正率: " << zero_rate << "%" << std::endl;
    
    // 分析前10个点的误差
    std::cout << "\n前10个点误差分析:" << std::endl;
    for (int i = 0; i < std::min(10, static_cast<int>(first_errors.size())); ++i) {
        std::string status = (first_errors[i] <= gps_error_bound) ? "✅" : "❌";
        std::cout << "  点" << i << ": " << std::scientific << std::setprecision(2) 
                  << first_errors[i] << " 度 " << status << std::endl;
    }
    
    // 分析后10个点的误差
    if (!last_errors.empty()) {
        std::cout << "\n后10个点误差分析:" << std::endl;
        for (int i = 0; i < last_errors.size(); ++i) {
            int point_index = test_size - 10 + i;
            std::string status = (last_errors[i] <= gps_error_bound) ? "✅" : "❌";
            std::cout << "  点" << point_index << ": " << std::scientific << std::setprecision(2) 
                      << last_errors[i] << " 度 " << status << std::endl;
        }
    }
    
    // 状态评估
    if (error_rate < 0.1) {
        std::cout << "  ✅ 状态同步优秀" << std::endl;
    } else if (error_rate < 1.0) {
        std::cout << "  ⚠️ 状态同步良好，有轻微问题" << std::endl;
    } else if (error_rate < 50.0) {
        std::cout << "  ❌ 状态同步有问题" << std::endl;
    } else {
        std::cout << "  🚨 状态同步严重失败" << std::endl;
    }
}

int main() {
    std::cout << "=== 渐进式同步测试：定位问题出现的数据规模 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int max_test_size = 5000;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, max_test_size);
    
    if (points.size() < max_test_size) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 测试不同规模的数据集
    std::vector<std::pair<int, std::string>> test_sizes = {
        {10, "极小数据集"},
        {50, "小数据集"},
        {100, "中小数据集"},
        {500, "中等数据集"},
        {1000, "较大数据集"},
        {2000, "大数据集"},
        {5000, "超大数据集"}
    };
    
    for (const auto& [size, name] : test_sizes) {
        if (size <= points.size()) {
            TestDatasetSize(points, size, name);
        }
    }
    
    std::cout << "\n=== 问题定位分析 ===" << std::endl;
    
    std::cout << "💡 通过渐进式测试，我们可以确定:" << std::endl;
    std::cout << "1. 🔍 问题出现的临界数据规模" << std::endl;
    std::cout << "2. 📊 误差是否随数据量增长而累积" << std::endl;
    std::cout << "3. 🎯 前期点和后期点的误差分布差异" << std::endl;
    std::cout << "4. ⚠️ 状态同步失败的具体模式" << std::endl;
    
    return 0;
}
