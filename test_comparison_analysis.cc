#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

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
    std::cout << "=== 对比两种测试方法的差异 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 5000;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double gps_error_bound = 1e-5;
    const double linear_error_bound = gps_error_bound / std::sqrt(2.0);
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n=== 测试1: 实际压缩器性能 ===" << std::endl;
    
    // 实际GPS轨迹压缩器
    SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    for (const auto& point : points) {
        gps_compressor.AddGpsPoint(point);
    }
    const auto& gps_stats = gps_compressor.GetStrategyStats();
    
    // 实际线性压缩器
    SerfQtLinearCompressor linear_compressor(points.size(), linear_error_bound);
    for (const auto& point : points) {
        linear_compressor.AddValue(point.longitude);
        linear_compressor.AddValue(point.latitude);
    }
    linear_compressor.Close();
    
    Array<uint8_t> gps_compressed = gps_compressor.GetCompressedData();
    Array<uint8_t> linear_compressed = linear_compressor.compressed_bytes();
    
    double gps_actual_zero_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_stats.GetTotalPoints() * 100.0;
    double gps_compression_ratio = static_cast<double>(points.size() * 16) / gps_compressed.length();
    double linear_compression_ratio = static_cast<double>(points.size() * 16) / linear_compressed.length();
    
    std::cout << "📊 实际压缩器结果:" << std::endl;
    std::cout << "  GPS轨迹压缩器零校正率: " << std::fixed << std::setprecision(2) << gps_actual_zero_rate << "%" << std::endl;
    std::cout << "  GPS轨迹压缩器压缩比: " << gps_compression_ratio << ":1" << std::endl;
    std::cout << "  线性压缩器压缩比: " << linear_compression_ratio << ":1" << std::endl;
    
    std::cout << "\n=== 测试2: 模拟预测准确率 ===" << std::endl;
    
    // 模拟GPS轨迹压缩器的预测（简化版本）
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> gps_reconstructed;
    std::vector<std::pair<double, double>> gps_motions; // velocity, theta
    int gps_sim_predictions = 0, gps_sim_accurate = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        if (i < 2) {
            // 前两个点直接添加
            gps_reconstructed.push_back(points[i]);
            if (i == 1) {
                double dx = points[1].longitude - points[0].longitude;
                double dy = points[1].latitude - points[0].latitude;
                double v = std::sqrt(dx * dx + dy * dy);
                double theta = std::atan2(dy, dx);
                gps_motions.emplace_back(v, theta);
            }
            continue;
        }
        
        // 预测（零阶预测）
        const auto& current_point = gps_reconstructed.back();
        const auto& current_motion = gps_motions.back();
        
        double predicted_lon = current_point.longitude + current_motion.first * std::cos(current_motion.second);
        double predicted_lat = current_point.latitude + current_motion.first * std::sin(current_motion.second);
        
        SerfQtGpsConfigurableCompressor::GpsPoint predicted(predicted_lon, predicted_lat);
        
        // 计算误差
        double dx = points[i].longitude - predicted.longitude;
        double dy = points[i].latitude - predicted.latitude;
        double error = std::sqrt(dx * dx + dy * dy);
        
        gps_sim_predictions++;
        if (error <= gps_error_bound) {
            gps_sim_accurate++;
        }
        
        // 更新状态（简化：假设完美重构）
        gps_reconstructed.push_back(points[i]);
        
        // 计算新运动矢量
        const auto& prev = gps_reconstructed[gps_reconstructed.size() - 2];
        const auto& curr = gps_reconstructed.back();
        double new_dx = curr.longitude - prev.longitude;
        double new_dy = curr.latitude - prev.latitude;
        double new_v = std::sqrt(new_dx * new_dx + new_dy * new_dy);
        double new_theta = std::atan2(new_dy, new_dx);
        gps_motions.emplace_back(new_v, new_theta);
    }
    
    // 模拟线性压缩器的预测
    std::vector<double> linear_lons, linear_lats;
    int linear_sim_predictions = 0, linear_sim_accurate = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        if (i < 2) {
            // 前两个点直接添加
            linear_lons.push_back(points[i].longitude);
            linear_lats.push_back(points[i].latitude);
            continue;
        }
        
        // 线性预测
        double predicted_lon = 2.0 * linear_lons.back() - linear_lons[linear_lons.size() - 2];
        double predicted_lat = 2.0 * linear_lats.back() - linear_lats[linear_lats.size() - 2];
        
        // 计算2D误差
        double dx = points[i].longitude - predicted_lon;
        double dy = points[i].latitude - predicted_lat;
        double error_2d = std::sqrt(dx * dx + dy * dy);
        
        linear_sim_predictions++;
        if (error_2d <= gps_error_bound) {  // 使用相同的2D误差界限
            linear_sim_accurate++;
        }
        
        // 更新状态（简化：假设完美重构）
        linear_lons.push_back(points[i].longitude);
        linear_lats.push_back(points[i].latitude);
    }
    
    double gps_sim_rate = static_cast<double>(gps_sim_accurate) / gps_sim_predictions * 100.0;
    double linear_sim_rate = static_cast<double>(linear_sim_accurate) / linear_sim_predictions * 100.0;
    
    std::cout << "📊 模拟预测结果:" << std::endl;
    std::cout << "  GPS轨迹模拟零校正率: " << std::setprecision(2) << gps_sim_rate << "%" << std::endl;
    std::cout << "  线性压缩模拟准确率: " << linear_sim_rate << "%" << std::endl;
    
    std::cout << "\n=== 差异分析 ===" << std::endl;
    
    double actual_vs_sim_gps = std::abs(gps_actual_zero_rate - gps_sim_rate);
    
    std::cout << "🔍 关键差异:" << std::endl;
    std::cout << "1. GPS轨迹压缩器:" << std::endl;
    std::cout << "   实际零校正率: " << gps_actual_zero_rate << "%" << std::endl;
    std::cout << "   模拟零校正率: " << gps_sim_rate << "%" << std::endl;
    std::cout << "   差异: " << actual_vs_sim_gps << "%" << std::endl;
    
    if (actual_vs_sim_gps > 2.0) {
        std::cout << "\n🚨 实际压缩器与模拟预测存在显著差异！" << std::endl;
        std::cout << "可能的原因:" << std::endl;
        std::cout << "1. 量化误差累积：实际压缩器使用量化后的重构点进行预测" << std::endl;
        std::cout << "2. 状态同步问题：压缩器和解压器的状态可能不完全一致" << std::endl;
        std::cout << "3. 几何剪枝算法：实际压缩器有复杂的策略选择逻辑" << std::endl;
        std::cout << "4. 误差界限检查：实际实现可能有细微的数值精度问题" << std::endl;
        std::cout << "5. 编码策略开销：策略标志的编码可能影响最终选择" << std::endl;
    } else {
        std::cout << "\n✅ 实际压缩器与模拟预测基本一致" << std::endl;
    }
    
    std::cout << "\n2. 线性压缩器:" << std::endl;
    std::cout << "   模拟使用2D误差界限: " << gps_error_bound << " 度" << std::endl;
    std::cout << "   实际使用1D误差界限: " << linear_error_bound << " 度" << std::endl;
    std::cout << "   这可能导致不同的预测命中率" << std::endl;
    
    std::cout << "\n💡 总结:" << std::endl;
    std::cout << "fair_prediction_comparison.cc 和实际压缩性能测试的差异主要来自:" << std::endl;
    std::cout << "1. 模拟预测 vs 实际压缩器的复杂性差异" << std::endl;
    std::cout << "2. 误差界限的不同解释方式" << std::endl;
    std::cout << "3. 量化误差和状态同步的影响" << std::endl;
    std::cout << "4. 编码策略选择的复杂性" << std::endl;
    
    return 0;
}
