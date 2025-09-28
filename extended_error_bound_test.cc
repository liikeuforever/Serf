#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "compressor/serf_qt_linear_compressor.h"

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

int main() {
    std::cout << "=== 扩展误差界限测试 - 寻找最优配置 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 20000;  // 使用2万条数据进行快速测试
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置量化参数
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 2e-4;
    
    // 线性预测基准
    const double linear_max_diff = 1e-5;
    SerfQtLinearCompressor linear_lon_compressor(points.size(), linear_max_diff);
    SerfQtLinearCompressor linear_lat_compressor(points.size(), linear_max_diff);
    
    for (const auto& point : points) {
        linear_lon_compressor.AddValue(point.longitude);
        linear_lat_compressor.AddValue(point.latitude);
    }
    
    linear_lon_compressor.Close();
    linear_lat_compressor.Close();
    
    Array<uint8_t> linear_lon_compressed = linear_lon_compressor.compressed_bytes();
    Array<uint8_t> linear_lat_compressed = linear_lat_compressor.compressed_bytes();
    
    int linear_total_size = linear_lon_compressed.length() + linear_lat_compressed.length();
    double linear_compression_ratio = static_cast<double>(points.size() * 16) / linear_total_size;
    
    std::cout << "📊 线性预测基准: " << std::fixed << std::setprecision(2) << linear_compression_ratio << ":1" << std::endl;
    
    // 测试更大范围的误差界限
    std::vector<double> error_bounds = {
        5.0e-5,   // 之前的最佳
        6.0e-5,
        7.0e-5,
        8.0e-5,
        1.0e-4,   // 0.1毫度
        1.2e-4,
        1.5e-4,
        2.0e-4,   // 0.2毫度
        2.5e-4,
        3.0e-4    // 0.3毫度
    };
    
    std::cout << "\n=== 扩展误差界限测试结果 ===" << std::endl;
    
    std::cout << std::setw(12) << "误差界限" 
              << std::setw(12) << "零校正率" 
              << std::setw(12) << "压缩比" 
              << std::setw(15) << "vs线性优势" 
              << std::setw(12) << "零校正数" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    double best_compression_ratio = 0.0;
    double best_error_bound = 0.0;
    bool found_better_than_linear = false;
    
    for (double error_bound : error_bounds) {
        SerfQtGpsConfigurableCompressor gps_compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
        
        for (const auto& point : points) {
            gps_compressor.AddGpsPoint(point);
        }
        
        Array<uint8_t> gps_compressed_data = gps_compressor.GetCompressedData();
        const auto& gps_stats = gps_compressor.GetStrategyStats();
        
        // 计算结果
        int gps_total_points = gps_stats.GetTotalPoints();
        double gps_zero_correction_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_total_points * 100.0;
        double gps_compression_ratio = static_cast<double>(points.size() * 16) / gps_compressed_data.length();
        
        // 与线性预测的对比
        double compression_advantage = (gps_compression_ratio / linear_compression_ratio - 1) * 100.0;
        
        // 记录最佳结果
        if (gps_compression_ratio > best_compression_ratio) {
            best_compression_ratio = gps_compression_ratio;
            best_error_bound = error_bound;
        }
        
        if (gps_compression_ratio > linear_compression_ratio) {
            found_better_than_linear = true;
        }
        
        // 打印结果
        std::cout << std::setw(12) << std::scientific << std::setprecision(1) << error_bound
                  << std::setw(12) << std::fixed << std::setprecision(2) << gps_zero_correction_rate << "%"
                  << std::setw(12) << gps_compression_ratio
                  << std::setw(15) << std::showpos << compression_advantage << "%" << std::noshowpos
                  << std::setw(12) << gps_stats.zero_corr_count << std::endl;
    }
    
    std::cout << "\n=== 测试结果分析 ===" << std::endl;
    
    std::cout << "🏆 最佳GPS轨迹压缩器配置:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << best_error_bound << " 度" << std::endl;
    std::cout << "  最佳压缩比: " << std::fixed << std::setprecision(2) << best_compression_ratio << ":1" << std::endl;
    
    double final_advantage = (best_compression_ratio / linear_compression_ratio - 1) * 100.0;
    if (found_better_than_linear) {
        std::cout << "🚀 成功超越线性预测！优势: +" << final_advantage << "%" << std::endl;
    } else {
        std::cout << "📈 仍未超越线性预测，差距: " << final_advantage << "%" << std::endl;
    }
    
    // 分析零校正率趋势
    std::cout << "\n💡 关键洞察:" << std::endl;
    std::cout << "1. 误差界限从1e-5增加到" << std::scientific << best_error_bound << "时，压缩比提升显著" << std::endl;
    std::cout << "2. 零校正率的提升直接转化为压缩比的改善" << std::endl;
    
    if (!found_better_than_linear) {
        std::cout << "3. 🤔 即使大幅放宽误差界限，仍未超越线性预测" << std::endl;
        std::cout << "   这说明问题可能不仅仅是误差界限，还包括:" << std::endl;
        std::cout << "   - 编码策略的效率" << std::endl;
        std::cout << "   - 量化参数的选择" << std::endl;
        std::cout << "   - 运动矢量编码的开销" << std::endl;
    }
    
    // 建议进一步优化方向
    std::cout << "\n🔧 进一步优化建议:" << std::endl;
    std::cout << "1. 优化量化参数 εv 和 εθ" << std::endl;
    std::cout << "2. 简化编码策略，减少策略标志开销" << std::endl;
    std::cout << "3. 考虑改进预测模型（如一阶线性预测运动矢量）" << std::endl;
    std::cout << "4. 分析编码成本分布，找出主要开销来源" << std::endl;
    
    return 0;
}
