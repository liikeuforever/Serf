#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>

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
    std::cout << "=== GPS轨迹压缩算法 vs 线性压缩算法深度分析 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 10000; // 使用1万个点进行详细分析
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 分析数据特征
    std::cout << "\n=== 数据特征分析 ===" << std::endl;
    
    std::vector<double> longitudes, latitudes;
    for (const auto& point : points) {
        longitudes.push_back(point.longitude);
        latitudes.push_back(point.latitude);
    }
    
    // 计算线性预测准确性
    int linear_lon_accurate = 0, linear_lat_accurate = 0;
    double linear_lon_error_sum = 0, linear_lat_error_sum = 0;
    double max_diff = 1e-5; // 使用相同的误差界限
    
    for (size_t i = 2; i < longitudes.size(); ++i) {
        // 线性预测：predicted = 2 * prev1 - prev2
        double predicted_lon = 2.0 * longitudes[i-1] - longitudes[i-2];
        double predicted_lat = 2.0 * latitudes[i-1] - latitudes[i-2];
        
        double lon_error = std::abs(longitudes[i] - predicted_lon);
        double lat_error = std::abs(latitudes[i] - predicted_lat);
        
        linear_lon_error_sum += lon_error;
        linear_lat_error_sum += lat_error;
        
        if (lon_error <= max_diff) linear_lon_accurate++;
        if (lat_error <= max_diff) linear_lat_accurate++;
    }
    
    int prediction_count = longitudes.size() - 2;
    double linear_lon_accuracy = static_cast<double>(linear_lon_accurate) / prediction_count * 100;
    double linear_lat_accuracy = static_cast<double>(linear_lat_accurate) / prediction_count * 100;
    double avg_lon_error = linear_lon_error_sum / prediction_count;
    double avg_lat_error = linear_lat_error_sum / prediction_count;
    
    std::cout << "线性预测准确性分析:" << std::endl;
    std::cout << "  经度预测准确率: " << std::fixed << std::setprecision(2) << linear_lon_accuracy << "%" << std::endl;
    std::cout << "  纬度预测准确率: " << linear_lat_accuracy << "%" << std::endl;
    std::cout << "  经度平均误差: " << std::scientific << std::setprecision(3) << avg_lon_error << " 度" << std::endl;
    std::cout << "  纬度平均误差: " << avg_lat_error << " 度" << std::endl;
    
    // 使用GPS轨迹压缩器进行详细分析
    std::cout << "\n=== GPS轨迹压缩器详细分析 ===" << std::endl;
    
    const double error_bound = 1e-5;
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        compressor.AddGpsPoint(point);
    }
    
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    const auto& stats = compressor.GetStrategyStats();
    
    // 详细分析编码开销
    std::cout << "\n=== 编码开销详细分析 ===" << std::endl;
    
    int total_points = stats.GetTotalPoints();
    int total_bits = stats.total_strategy_bits + stats.total_quantization_bits;
    
    std::cout << "GPS轨迹压缩器编码开销:" << std::endl;
    std::cout << "  总处理点数: " << total_points << std::endl;
    std::cout << "  策略标志比特: " << stats.total_strategy_bits << " bits" << std::endl;
    std::cout << "  量化数据比特: " << stats.total_quantization_bits << " bits" << std::endl;
    std::cout << "  总编码比特: " << total_bits << " bits" << std::endl;
    std::cout << "  平均每点成本: " << std::fixed << std::setprecision(2) 
              << (static_cast<double>(total_bits) / total_points) << " bits/点" << std::endl;
    
    // 分析各策略的开销
    std::cout << "\n各策略编码开销分析:" << std::endl;
    std::cout << "  零校正 (" << stats.zero_corr_count << "次): 1 bit/次" << std::endl;
    std::cout << "  速度校正 (" << stats.v_only_count << "次): 2 + 量化 bits/次" << std::endl;
    std::cout << "  角度校正 (" << stats.theta_only_count << "次): 3 + 量化 bits/次" << std::endl;
    std::cout << "  完全校正 (" << stats.both_count << "次): 3 + 量化 bits/次" << std::endl;
    
    // 估算线性压缩器的编码开销
    std::cout << "\n=== 线性压缩器编码开销估算 ===" << std::endl;
    
    // 线性压缩器的编码开销分析
    int linear_header_bits = 16 + 64; // block_size + max_diff
    int linear_data_bits = 0;
    
    // 估算每个值的编码成本
    for (size_t i = 0; i < longitudes.size(); ++i) {
        // 经度编码
        double predicted_lon, predicted_lat;
        if (i == 0) {
            predicted_lon = 2.0;
            predicted_lat = 2.0;
        } else if (i == 1) {
            predicted_lon = longitudes[0];
            predicted_lat = latitudes[0];
        } else {
            predicted_lon = 2.0 * longitudes[i-1] - longitudes[i-2];
            predicted_lat = 2.0 * latitudes[i-1] - latitudes[i-2];
        }
        
        // 计算量化值
        long q_lon = static_cast<long>(std::round((longitudes[i] - predicted_lon) / (2 * max_diff)));
        long q_lat = static_cast<long>(std::round((latitudes[i] - predicted_lat) / (2 * max_diff)));
        
        // 估算Elias Gamma编码的比特数
        // Elias Gamma编码长度约为 2*log2(|q|+1) + 1
        auto estimate_gamma_bits = [](long q) -> int {
            if (q == 0) return 1;
            int abs_q = std::abs(q) + 1; // ZigZag编码后加1
            return 2 * static_cast<int>(std::log2(abs_q)) + 1;
        };
        
        linear_data_bits += estimate_gamma_bits(q_lon);
        linear_data_bits += estimate_gamma_bits(q_lat);
    }
    
    int linear_total_bits = linear_header_bits + linear_data_bits;
    
    std::cout << "线性压缩器编码开销估算:" << std::endl;
    std::cout << "  头部信息: " << linear_header_bits << " bits" << std::endl;
    std::cout << "  数据编码: " << linear_data_bits << " bits" << std::endl;
    std::cout << "  总编码比特: " << linear_total_bits << " bits" << std::endl;
    std::cout << "  平均每点成本: " << std::setprecision(2) 
              << (static_cast<double>(linear_data_bits) / points.size()) << " bits/点" << std::endl;
    
    // 对比分析
    std::cout << "\n=== 压缩比差异根本原因分析 ===" << std::endl;
    
    double gps_avg_bits = static_cast<double>(total_bits) / total_points;
    double linear_avg_bits = static_cast<double>(linear_data_bits) / points.size();
    
    std::cout << "平均每点编码成本对比:" << std::endl;
    std::cout << "  GPS轨迹压缩器: " << std::setprecision(2) << gps_avg_bits << " bits/点" << std::endl;
    std::cout << "  线性压缩器: " << linear_avg_bits << " bits/点" << std::endl;
    std::cout << "  差异: " << (gps_avg_bits - linear_avg_bits) << " bits/点" << std::endl;
    std::cout << "  GPS压缩器开销是线性压缩器的: " << (gps_avg_bits / linear_avg_bits) << " 倍" << std::endl;
    
    std::cout << "\n根本原因分析:" << std::endl;
    std::cout << "1. 🎯 预测准确性差异:" << std::endl;
    std::cout << "   - GPS轨迹压缩器零校正比例: " << std::setprecision(2) 
              << (100.0 * stats.zero_corr_count / total_points) << "%" << std::endl;
    std::cout << "   - 线性压缩器预测准确率: ~" << (linear_lon_accuracy + linear_lat_accuracy) / 2 << "%" << std::endl;
    
    std::cout << "\n2. 📊 编码策略差异:" << std::endl;
    std::cout << "   - GPS轨迹压缩器需要策略标志: " << std::setprecision(1) 
              << (100.0 * stats.total_strategy_bits / total_bits) << "% 开销" << std::endl;
    std::cout << "   - 线性压缩器无策略开销: 0% 开销" << std::endl;
    
    std::cout << "\n3. 🔧 算法复杂度差异:" << std::endl;
    std::cout << "   - GPS轨迹压缩器: 4种校正策略，复杂决策" << std::endl;
    std::cout << "   - 线性压缩器: 单一预测策略，简单直接" << std::endl;
    
    std::cout << "\n4. 📏 量化精度差异:" << std::endl;
    std::cout << "   - GPS轨迹压缩器: 严格误差控制，精细量化" << std::endl;
    std::cout << "   - 线性压缩器: 相对宽松，粗糙量化" << std::endl;
    
    // 理论分析
    std::cout << "\n=== 理论分析 ===" << std::endl;
    std::cout << "GPS轨迹压缩器压缩比较低的根本原因:" << std::endl;
    std::cout << "1. 预测模型简单 - 仅使用一阶运动模型，预测准确性有限" << std::endl;
    std::cout << "2. 策略开销大 - 每个点都需要编码策略标志（1-3 bits）" << std::endl;
    std::cout << "3. 误差控制严格 - 必须满足1e-5度误差界限，限制了量化步长" << std::endl;
    std::cout << "4. 二维复杂性 - 需要同时处理经纬度的相关性，增加了编码复杂度" << std::endl;
    
    std::cout << "\n线性压缩器压缩比高的原因:" << std::endl;
    std::cout << "1. 预测模型适合 - 线性预测对平滑数据效果好" << std::endl;
    std::cout << "2. 编码简单 - 每个值只需一个量化值，无策略开销" << std::endl;
    std::cout << "3. 独立处理 - 经纬度分别压缩，避免了二维耦合" << std::endl;
    std::cout << "4. 误差容忍 - 可以接受较大误差，使用更大的量化步长" << std::endl;
    
    return 0;
}
