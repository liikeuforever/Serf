/**
 * TrajCompress-SP 测试程序
 * 
 * 功能：
 * 1. 读取Geolife GPS数据集
 * 2. 使用TrajCompress-SP算法进行压缩
 * 3. 解压并验证精度
 * 4. 与Serf-QT进行对比
 * 5. 输出详细的统计信息
 */

#include "src/compressor/trajcompress_sp_compressor.h"
#include "src/compressor/serf_qt_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include "src/compressor/serf_qt_curve_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <chrono>
#include <iomanip>

using GpsPoint = TrajCompressSPCompressor::GpsPoint;

// 从CSV文件读取GPS数据
std::vector<GpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    std::string line;
    int count = 0;
    
    while (std::getline(file, line) && (max_points < 0 || count < max_points)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                double longitude = std::stod(lon_str);
                double latitude = std::stod(lat_str);
                points.emplace_back(longitude, latitude);
                count++;
            } catch (const std::exception& e) {
                std::cerr << "解析错误: " << line << std::endl;
            }
        }
    }
    
    file.close();
    std::cout << "成功加载 " << points.size() << " 个GPS点" << std::endl;
    return points;
}

// 计算两点间的距离（度）- 使用欧几里得距离，与标准测试一致
double CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

// 计算最大误差和平均误差
void CalculateErrors(const std::vector<GpsPoint>& original,
                    const std::vector<GpsPoint>& decompressed,
                    double& max_error,
                    double& avg_error,
                    int& points_exceeding_threshold,
                    double threshold) {
    max_error = 0;
    avg_error = 0;
    points_exceeding_threshold = 0;
    
    if (original.size() != decompressed.size()) {
        std::cerr << "警告: 原始数据和解压数据点数不一致 (" 
                  << original.size() << " vs " << decompressed.size() << ")" << std::endl;
        return;
    }
    
    for (size_t i = 0; i < original.size(); ++i) {
        double error = CalculateDistance(original[i], decompressed[i]);
        max_error = std::max(max_error, error);
        avg_error += error;
        
        if (error > threshold) {
            points_exceeding_threshold++;
        }
    }
    
    avg_error /= original.size();
}

// 测试TrajCompress-SP
void TestTrajCompressSP(const std::vector<GpsPoint>& gps_data, double epsilon) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试 TrajCompress-SP 算法" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "数据点数: " << gps_data.size() << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // 压缩
    auto start_time = std::chrono::high_resolution_clock::now();
    
    TrajCompressSPCompressor compressor(gps_data.size(), epsilon);
    
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(point);
    }
    
    compressor.Close();
    
    auto end_compress_time = std::chrono::high_resolution_clock::now();
    auto compress_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_compress_time - start_time).count();
    
    // 获取压缩数据
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    int compressed_size_bits = compressor.GetCompressedSizeInBits();
    
    std::cout << "\n--- 压缩完成 ---" << std::endl;
    std::cout << "压缩时间: " << compress_duration << " ms" << std::endl;
    std::cout << "压缩大小: " << compressed_size_bits << " bits (" 
              << (compressed_size_bits / 8) << " bytes)" << std::endl;
    
    // 解压缩
    auto start_decompress_time = std::chrono::high_resolution_clock::now();
    
    TrajCompressSPDecompressor decompressor(compressed_data.begin(), compressed_data.length());
    std::vector<GpsPoint> decompressed_data;
    GpsPoint point;
    for (size_t i = 0; i < gps_data.size(); ++i) {
        if (decompressor.ReadNextPoint(point)) {
            decompressed_data.push_back(point);
        } else {
            break;
        }
    }
    
    auto end_decompress_time = std::chrono::high_resolution_clock::now();
    auto decompress_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_decompress_time - start_decompress_time).count();
    
    std::cout << "\n--- 解压完成 ---" << std::endl;
    std::cout << "解压时间: " << decompress_duration << " ms" << std::endl;
    std::cout << "解压点数: " << decompressed_data.size() << std::endl;
    
    // 精度验证
    double max_error, avg_error;
    int points_exceeding;
    CalculateErrors(gps_data, decompressed_data, max_error, avg_error, 
                   points_exceeding, epsilon);
    
    std::cout << "\n--- 精度验证 ---" << std::endl;
    std::cout << "最大误差: " << std::scientific << std::setprecision(6) << max_error 
              << " 度 (" << std::fixed << std::setprecision(2) << (max_error * 111000) << " 米)" << std::endl;
    std::cout << "平均误差: " << std::scientific << avg_error 
              << " 度 (" << std::fixed << (avg_error * 111000) << " 米)" << std::endl;
    std::cout << "超过阈值点数: " << points_exceeding << " / " << gps_data.size() 
              << " (" << std::setprecision(2) << (100.0 * points_exceeding / gps_data.size()) << "%)" << std::endl;
    
    // 精度满足性检查 - 使用标准测试的误差判断逻辑
    std::cout << "\n--- 精度满足性检查 ---" << std::endl;
    bool precision_satisfied = (max_error <= epsilon);
    std::cout << "设置误差阈值: " << std::scientific << epsilon << " 度 (" 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "实际最大误差: " << std::scientific << max_error << " 度 (" 
              << std::fixed << (max_error * 111000) << " 米)" << std::endl;
    
    // 使用标准测试的断言逻辑
    if (precision_satisfied) {
        std::cout << "✅ 精度要求满足！最大误差 " << std::scientific << max_error 
                  << " ≤ 阈值 " << epsilon << std::endl;
    } else {
        std::cout << "❌ 精度要求不满足！最大误差 " << std::scientific << max_error 
                  << " > 阈值 " << epsilon << std::endl;
        std::cout << "⚠️  注意：1e-5度是非常严格的精度要求，可能需要调整算法参数" << std::endl;
    }
    
    // 详细误差分布分析
    std::cout << "\n--- 详细误差分布分析 ---" << std::endl;
    std::vector<double> errors;
    for (size_t i = 0; i < gps_data.size() && i < decompressed_data.size(); ++i) {
        double error = CalculateDistance(gps_data[i], decompressed_data[i]);
        errors.push_back(error);
    }
    
    if (!errors.empty()) {
        std::sort(errors.begin(), errors.end());
        
        size_t p50_idx = errors.size() * 0.50;
        size_t p90_idx = errors.size() * 0.90;
        size_t p95_idx = errors.size() * 0.95;
        size_t p99_idx = errors.size() * 0.99;
        
        std::cout << "P50 (中位数): " << std::scientific << errors[p50_idx] 
                  << " 度 (" << std::fixed << std::setprecision(2) 
                  << (errors[p50_idx] * 111000) << " 米)" << std::endl;
        std::cout << "P90: " << std::scientific << errors[p90_idx] 
                  << " 度 (" << std::fixed << (errors[p90_idx] * 111000) << " 米)" << std::endl;
        std::cout << "P95: " << std::scientific << errors[p95_idx] 
                  << " 度 (" << std::fixed << (errors[p95_idx] * 111000) << " 米)" << std::endl;
        std::cout << "P99: " << std::scientific << errors[p99_idx] 
                  << " 度 (" << std::fixed << (errors[p99_idx] * 111000) << " 米)" << std::endl;
        
        // 误差分布统计
        int within_epsilon = 0;
        int within_2epsilon = 0;
        int within_5epsilon = 0;
        
        for (double error : errors) {
            if (error <= epsilon) within_epsilon++;
            if (error <= 2 * epsilon) within_2epsilon++;
            if (error <= 5 * epsilon) within_5epsilon++;
        }
        
        std::cout << "\n误差分布统计:" << std::endl;
        std::cout << "≤ 1×阈值: " << within_epsilon << " / " << errors.size() 
                  << " (" << std::setprecision(1) << (100.0 * within_epsilon / errors.size()) << "%)" << std::endl;
        std::cout << "≤ 2×阈值: " << within_2epsilon << " / " << errors.size() 
                  << " (" << (100.0 * within_2epsilon / errors.size()) << "%)" << std::endl;
        std::cout << "≤ 5×阈值: " << within_5epsilon << " / " << errors.size() 
                  << " (" << (100.0 * within_5epsilon / errors.size()) << "%)" << std::endl;
    }
    
    // 输出详细统计
    compressor.GetStats().PrintDetailedStats();
}

// 测试Serf-QT（用于对比）
void TestSerfQT(const std::vector<GpsPoint>& gps_data, double epsilon) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试 Serf-QT 算法（前值预测）" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "数据点数: " << gps_data.size() << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // 压缩经度和纬度（分别压缩两个浮点数，公平对比）
    auto start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtCompressor lon_compressor(gps_data.size(), epsilon);
    SerfQtCompressor lat_compressor(gps_data.size(), epsilon);
    
    for (const auto& point : gps_data) {
        lon_compressor.AddValue(point.longitude);
        lat_compressor.AddValue(point.latitude);
    }
    
    lon_compressor.Close();
    lat_compressor.Close();
    
    auto end_compress_time = std::chrono::high_resolution_clock::now();
    auto compress_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_compress_time - start_time).count();
    
    int total_bits = lon_compressor.get_compressed_size_in_bits() + 
                     lat_compressor.get_compressed_size_in_bits();
    
    std::cout << "\n--- 压缩完成 ---" << std::endl;
    std::cout << "压缩时间: " << compress_duration << " ms" << std::endl;
    std::cout << "压缩大小: " << total_bits << " bits (" << (total_bits / 8) << " bytes)" << std::endl;
    std::cout << "  经度: " << lon_compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    std::cout << "  纬度: " << lat_compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    
    // 计算压缩比
    int original_bits = gps_data.size() * 128;  // 每点2个double = 128 bits
    double compression_ratio = static_cast<double>(original_bits) / total_bits;
    double avg_bits_per_point = static_cast<double>(total_bits) / gps_data.size();
    
    std::cout << "\n--- 压缩效率 ---" << std::endl;
    std::cout << "原始数据: " << original_bits << " bits" << std::endl;
    std::cout << "压缩比: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    std::cout << "平均每点: " << avg_bits_per_point << " bits/点" << std::endl;
}

// 测试Serf-QT-Linear（纯线性预测）
void TestSerfQTLinear(const std::vector<GpsPoint>& gps_data, double epsilon) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试 Serf-QT-Linear 算法（纯线性预测）" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "数据点数: " << gps_data.size() << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // 压缩经度和纬度
    auto start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtLinearCompressor lon_compressor(gps_data.size(), epsilon);
    SerfQtLinearCompressor lat_compressor(gps_data.size(), epsilon);
    
    for (const auto& point : gps_data) {
        lon_compressor.AddValue(point.longitude);
        lat_compressor.AddValue(point.latitude);
    }
    
    lon_compressor.Close();
    lat_compressor.Close();
    
    auto end_compress_time = std::chrono::high_resolution_clock::now();
    auto compress_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_compress_time - start_time).count();
    
    int total_bits = lon_compressor.get_compressed_size_in_bits() + 
                     lat_compressor.get_compressed_size_in_bits();
    
    std::cout << "\n--- 压缩完成 ---" << std::endl;
    std::cout << "压缩时间: " << compress_duration << " ms" << std::endl;
    std::cout << "压缩大小: " << total_bits << " bits (" << (total_bits / 8) << " bytes)" << std::endl;
    std::cout << "  经度: " << lon_compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    std::cout << "  纬度: " << lat_compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    
    // 计算压缩比
    int original_bits = gps_data.size() * 128;
    double compression_ratio = static_cast<double>(original_bits) / total_bits;
    double avg_bits_per_point = static_cast<double>(total_bits) / gps_data.size();
    
    std::cout << "\n--- 压缩效率 ---" << std::endl;
    std::cout << "原始数据: " << original_bits << " bits" << std::endl;
    std::cout << "压缩比: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    std::cout << "平均每点: " << avg_bits_per_point << " bits/点" << std::endl;
}

// 测试Serf-QT-Curve（纯曲线预测）
void TestSerfQTCurve(const std::vector<GpsPoint>& gps_data, double epsilon) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试 Serf-QT-Curve 算法（纯曲线预测）" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "数据点数: " << gps_data.size() << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // 压缩经度和纬度
    auto start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtCurveCompressor lon_compressor(gps_data.size(), epsilon);
    SerfQtCurveCompressor lat_compressor(gps_data.size(), epsilon);
    
    for (const auto& point : gps_data) {
        lon_compressor.AddValue(point.longitude);
        lat_compressor.AddValue(point.latitude);
    }
    
    lon_compressor.Close();
    lat_compressor.Close();
    
    auto end_compress_time = std::chrono::high_resolution_clock::now();
    auto compress_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_compress_time - start_time).count();
    
    int total_bits = lon_compressor.get_compressed_size_in_bits() + 
                     lat_compressor.get_compressed_size_in_bits();
    
    std::cout << "\n--- 压缩完成 ---" << std::endl;
    std::cout << "压缩时间: " << compress_duration << " ms" << std::endl;
    std::cout << "压缩大小: " << total_bits << " bits (" << (total_bits / 8) << " bytes)" << std::endl;
    std::cout << "  经度: " << lon_compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    std::cout << "  纬度: " << lat_compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    
    // 计算压缩比
    int original_bits = gps_data.size() * 128;
    double compression_ratio = static_cast<double>(original_bits) / total_bits;
    double avg_bits_per_point = static_cast<double>(total_bits) / gps_data.size();
    
    std::cout << "\n--- 压缩效率 ---" << std::endl;
    std::cout << "原始数据: " << original_bits << " bits" << std::endl;
    std::cout << "压缩比: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    std::cout << "平均每点: " << avg_bits_per_point << " bits/点" << std::endl;
}

// 四种算法综合对比测试
void FourWayComparativeTest(const std::vector<GpsPoint>& gps_data, double epsilon) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "四种算法综合对比" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "统一误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "统一量化步长: 2 * epsilon * 0.999" << std::endl;
    std::cout << "数据点数: " << gps_data.size() << std::endl;
    
    // TrajCompress-SP (多预测器切换)
    TrajCompressSPCompressor sp_compressor(gps_data.size(), epsilon);
    for (const auto& point : gps_data) {
        sp_compressor.AddGpsPoint(point);
    }
    sp_compressor.Close();
    int sp_bits = sp_compressor.GetCompressedSizeInBits();
    double sp_avg_bits = static_cast<double>(sp_bits) / gps_data.size();
    
    // Serf-QT (前值预测/零预测)
    SerfQtCompressor qt_lon(gps_data.size(), epsilon);
    SerfQtCompressor qt_lat(gps_data.size(), epsilon);
    for (const auto& point : gps_data) {
        qt_lon.AddValue(point.longitude);
        qt_lat.AddValue(point.latitude);
    }
    qt_lon.Close();
    qt_lat.Close();
    int qt_bits = qt_lon.get_compressed_size_in_bits() + qt_lat.get_compressed_size_in_bits();
    double qt_avg_bits = static_cast<double>(qt_bits) / gps_data.size();
    
    // Serf-QT-Linear (纯线性预测)
    SerfQtLinearCompressor linear_lon(gps_data.size(), epsilon);
    SerfQtLinearCompressor linear_lat(gps_data.size(), epsilon);
    for (const auto& point : gps_data) {
        linear_lon.AddValue(point.longitude);
        linear_lat.AddValue(point.latitude);
    }
    linear_lon.Close();
    linear_lat.Close();
    int linear_bits = linear_lon.get_compressed_size_in_bits() + linear_lat.get_compressed_size_in_bits();
    double linear_avg_bits = static_cast<double>(linear_bits) / gps_data.size();
    
    // Serf-QT-Curve (纯曲线预测)
    SerfQtCurveCompressor curve_lon(gps_data.size(), epsilon);
    SerfQtCurveCompressor curve_lat(gps_data.size(), epsilon);
    for (const auto& point : gps_data) {
        curve_lon.AddValue(point.longitude);
        curve_lat.AddValue(point.latitude);
    }
    curve_lon.Close();
    curve_lat.Close();
    int curve_bits = curve_lon.get_compressed_size_in_bits() + curve_lat.get_compressed_size_in_bits();
    double curve_avg_bits = static_cast<double>(curve_bits) / gps_data.size();
    
    // 对比结果表格
    std::cout << "\n四种算法性能对比:" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    std::cout << std::setw(25) << "算法" 
              << std::setw(20) << "总大小(bits)" 
              << std::setw(20) << "平均每点(bits)"
              << std::setw(20) << "压缩比"
              << std::setw(15) << "相对最优" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    int original_bits = gps_data.size() * 128;
    int min_bits = std::min({sp_bits, qt_bits, linear_bits, curve_bits});
    
    // TrajCompress-SP
    std::cout << std::setw(25) << "TrajCompress-SP" 
              << std::setw(20) << sp_bits
              << std::setw(20) << std::fixed << std::setprecision(2) << sp_avg_bits
              << std::setw(20) << (static_cast<double>(original_bits) / sp_bits) << ":1"
              << std::setw(15) << std::setprecision(1) 
              << ((1.0 - static_cast<double>(sp_bits) / min_bits) * 100) << "%" << std::endl;
    
    // Serf-QT (前值预测)
    std::cout << std::setw(25) << "Serf-QT (前值)" 
              << std::setw(20) << qt_bits
              << std::setw(20) << std::setprecision(2) << qt_avg_bits
              << std::setw(20) << (static_cast<double>(original_bits) / qt_bits) << ":1"
              << std::setw(15) << std::setprecision(1) 
              << ((1.0 - static_cast<double>(qt_bits) / min_bits) * 100) << "%" << std::endl;
    
    // Serf-QT-Linear
    std::cout << std::setw(25) << "Serf-QT-Linear" 
              << std::setw(20) << linear_bits
              << std::setw(20) << std::setprecision(2) << linear_avg_bits
              << std::setw(20) << (static_cast<double>(original_bits) / linear_bits) << ":1"
              << std::setw(15) << std::setprecision(1) 
              << ((1.0 - static_cast<double>(linear_bits) / min_bits) * 100) << "%" << std::endl;
    
    // Serf-QT-Curve
    std::cout << std::setw(25) << "Serf-QT-Curve" 
              << std::setw(20) << curve_bits
              << std::setw(20) << std::setprecision(2) << curve_avg_bits
              << std::setw(20) << (static_cast<double>(original_bits) / curve_bits) << ":1"
              << std::setw(15) << std::setprecision(1) 
              << ((1.0 - static_cast<double>(curve_bits) / min_bits) * 100) << "%" << std::endl;
    
    std::cout << std::string(100, '-') << std::endl;
    
    // 找出最优算法
    std::string best_algorithm;
    if (sp_bits == min_bits) best_algorithm = "TrajCompress-SP";
    else if (qt_bits == min_bits) best_algorithm = "Serf-QT (前值)";
    else if (linear_bits == min_bits) best_algorithm = "Serf-QT-Linear";
    else best_algorithm = "Serf-QT-Curve";
    
    std::cout << "\n✅ 最优算法: " << best_algorithm << std::endl;
    std::cout << "   压缩大小: " << min_bits << " bits (" << std::setprecision(2) 
              << (static_cast<double>(min_bits) / gps_data.size()) << " bits/点)" << std::endl;
    
    // 与Serf-QT基准对比
    std::cout << "\n相对Serf-QT (前值预测) 基准的改进:" << std::endl;
    std::cout << "  TrajCompress-SP:    " << std::fixed << std::setprecision(1)
              << ((1.0 - static_cast<double>(sp_bits) / qt_bits) * 100) << "%" << std::endl;
    std::cout << "  Serf-QT-Linear:     "
              << ((1.0 - static_cast<double>(linear_bits) / qt_bits) * 100) << "%" << std::endl;
    std::cout << "  Serf-QT-Curve:      "
              << ((1.0 - static_cast<double>(curve_bits) / qt_bits) * 100) << "%" << std::endl;
    
    // 算法特点说明
    std::cout << "\n算法特点说明:" << std::endl;
    std::cout << "  - TrajCompress-SP: 动态切换多个预测器(LDR/CP/ZP)，适应不同轨迹模式" << std::endl;
    std::cout << "  - Serf-QT (前值): 零预测，适合静止或慢速移动场景" << std::endl;
    std::cout << "  - Serf-QT-Linear: 纯线性预测，适合匀速直线运动" << std::endl;
    std::cout << "  - Serf-QT-Curve: 纯曲线预测，适合加速/减速/转弯场景" << std::endl;
}

    // 综合对比测试
void ComparativeTest(const std::vector<GpsPoint>& gps_data, double epsilon) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "TrajCompress-SP vs Serf-QT 综合对比" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "TrajCompress-SP误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "Serf-QT误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "注意：两者现在使用相同的一维量化步长 (2 * epsilon * 0.999)" << std::endl;
    
    // TrajCompress-SP（与Serf-QT使用相同的量化步长）
    TrajCompressSPCompressor sp_compressor(gps_data.size(), epsilon);
    for (const auto& point : gps_data) {
        sp_compressor.AddGpsPoint(point);
    }
    sp_compressor.Close();
    
    int sp_bits = sp_compressor.GetCompressedSizeInBits();
    double sp_avg_bits = static_cast<double>(sp_bits) / gps_data.size();
    
    // Serf-QT (分别压缩经度和纬度，使用epsilon)
    SerfQtCompressor qt_lon(gps_data.size(), epsilon);
    SerfQtCompressor qt_lat(gps_data.size(), epsilon);
    for (const auto& point : gps_data) {
        qt_lon.AddValue(point.longitude);
        qt_lat.AddValue(point.latitude);
    }
    qt_lon.Close();
    qt_lat.Close();
    
    int qt_bits = qt_lon.get_compressed_size_in_bits() + qt_lat.get_compressed_size_in_bits();
    double qt_avg_bits = static_cast<double>(qt_bits) / gps_data.size();
    
    // 对比结果
    std::cout << "\n算法性能对比:" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::setw(25) << "指标" << std::setw(20) << "TrajCompress-SP" 
              << std::setw(20) << "Serf-QT" << std::setw(15) << "改进" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    std::cout << std::setw(25) << "总压缩大小 (bits)" 
              << std::setw(20) << sp_bits 
              << std::setw(20) << qt_bits
              << std::setw(15) << std::fixed << std::setprecision(1) 
              << ((1.0 - static_cast<double>(sp_bits) / qt_bits) * 100) << "%" << std::endl;
    
    std::cout << std::setw(25) << "平均每点 (bits)" 
              << std::setw(20) << std::setprecision(2) << sp_avg_bits 
              << std::setw(20) << qt_avg_bits
              << std::setw(15) << std::setprecision(1) 
              << ((1.0 - sp_avg_bits / qt_avg_bits) * 100) << "%" << std::endl;
    
    int original_bits = gps_data.size() * 128;
    double sp_ratio = static_cast<double>(original_bits) / sp_bits;
    double qt_ratio = static_cast<double>(original_bits) / qt_bits;
    
    std::cout << std::setw(25) << "压缩比" 
              << std::setw(20) << std::setprecision(2) << sp_ratio << ":1"
              << std::setw(20) << qt_ratio << ":1"
              << std::setw(15) << std::setprecision(1) 
              << ((sp_ratio - qt_ratio) / qt_ratio * 100) << "%" << std::endl;
    
    std::cout << std::string(80, '-') << std::endl;
    
    if (sp_bits < qt_bits) {
        double improvement = (1.0 - static_cast<double>(sp_bits) / qt_bits) * 100;
        std::cout << "\n✅ TrajCompress-SP 相比 Serf-QT 压缩率提升: " 
                  << std::fixed << std::setprecision(1) << improvement << "%" << std::endl;
    } else {
        std::cout << "\n⚠️  TrajCompress-SP 压缩率低于 Serf-QT" << std::endl;
    }
    
    // 精度对比验证
    std::cout << "\n--- 精度对比验证 ---" << std::endl;
    std::cout << "TrajCompress-SP误差阈值: " << std::scientific << epsilon 
              << " 度 (约 " << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "Serf-QT误差阈值: " << std::scientific << epsilon 
              << " 度 (约 " << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "注意：" << std::endl;
    std::cout << "  - TrajCompress-SP在二维空间中使用预测器减少误差，单维量化步长与Serf-QT一致" << std::endl;
    std::cout << "  - Serf-QT独立压缩经纬度，每个维度的量化步长为 2*epsilon*0.999" << std::endl;
    std::cout << "  - 这是在相同一维精度保证下的公平对比" << std::endl;
}

// 数据集配置结构
struct DatasetConfig {
    std::string name;
    std::string path;
    int total_points;
    
    DatasetConfig(const std::string& n, const std::string& p, int pts)
        : name(n), path(p), total_points(pts) {}
};

// 获取所有测试数据集
std::vector<DatasetConfig> GetAllDatasets() {
    std::string base_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/";
    std::vector<DatasetConfig> datasets;
    
    // Geolife 数据集
    datasets.emplace_back("Geolife (原始)", 
                         base_path + "Geolife_100k_longitude_latitude.csv", 100000);
    datasets.emplace_back("Geolife (5x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_5x.csv", 19999);
    datasets.emplace_back("Geolife (10x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_10x.csv", 9999);
    datasets.emplace_back("Geolife (20x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_20x.csv", 4999);
    datasets.emplace_back("Geolife (40x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_40x.csv", 2499);
    
    // Track 数据集
    datasets.emplace_back("Track (原始)", 
                         base_path + "Track_63530k_longitude_latitude.csv", 63530);
    datasets.emplace_back("Track (5x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_5x.csv", 12705);
    datasets.emplace_back("Track (10x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_10x.csv", 6352);
    datasets.emplace_back("Track (20x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_20x.csv", 3176);
    datasets.emplace_back("Track (40x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_40x.csv", 1588);
    
    // Trajectory 数据集
    datasets.emplace_back("Trajectory (原始)", 
                         base_path + "Trajtory_97k_longitude_latitude.csv", 97009);
    datasets.emplace_back("Trajectory (5x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_5x.csv", 19402);
    datasets.emplace_back("Trajectory (10x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_10x.csv", 9701);
    datasets.emplace_back("Trajectory (20x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_20x.csv", 4851);
    datasets.emplace_back("Trajectory (40x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_40x.csv", 2426);
    
    return datasets;
}

// 测试单个数据集
void TestSingleDataset(const DatasetConfig& dataset, double epsilon, int max_points = -1) {
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "测试数据集: " << dataset.name << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 加载数据
    auto gps_data = LoadGpsDataFromCSV(dataset.path, max_points);
    
    if (gps_data.empty()) {
        std::cerr << "无法加载GPS数据: " << dataset.path << std::endl;
        return;
    }
    
    std::cout << "实际加载点数: " << gps_data.size() << " / " << dataset.total_points << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // 四种算法综合对比
    FourWayComparativeTest(gps_data, epsilon);
}

// 测试所有数据集并生成汇总报告
void TestAllDatasetsAndGenerateSummary(double epsilon) {
    auto datasets = GetAllDatasets();
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "开始测试所有数据集" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "总数据集数量: " << datasets.size() << std::endl;
    std::cout << "统一误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // 用于汇总的结构
    struct SummaryResult {
        std::string dataset_name;
        int points;
        int sp_bits;
        int qt_bits;
        int linear_bits;
        int curve_bits;
        double sp_avg;
        double qt_avg;
        double linear_avg;
        double curve_avg;
    };
    
    std::vector<SummaryResult> summary_results;
    
    // 测试每个数据集
    for (const auto& dataset : datasets) {
        std::cout << "\n" << std::string(100, '-') << std::endl;
        std::cout << "正在测试: " << dataset.name << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        
        auto gps_data = LoadGpsDataFromCSV(dataset.path, -1);
        
        if (gps_data.empty()) {
            std::cerr << "跳过数据集: " << dataset.name << std::endl;
            continue;
        }
        
        // TrajCompress-SP
        TrajCompressSPCompressor sp_compressor(gps_data.size(), epsilon);
        for (const auto& point : gps_data) {
            sp_compressor.AddGpsPoint(point);
        }
        sp_compressor.Close();
        
        // Serf-QT
        SerfQtCompressor qt_lon(gps_data.size(), epsilon);
        SerfQtCompressor qt_lat(gps_data.size(), epsilon);
        for (const auto& point : gps_data) {
            qt_lon.AddValue(point.longitude);
            qt_lat.AddValue(point.latitude);
        }
        qt_lon.Close();
        qt_lat.Close();
        
        // Serf-QT-Linear
        SerfQtLinearCompressor linear_lon(gps_data.size(), epsilon);
        SerfQtLinearCompressor linear_lat(gps_data.size(), epsilon);
        for (const auto& point : gps_data) {
            linear_lon.AddValue(point.longitude);
            linear_lat.AddValue(point.latitude);
        }
        linear_lon.Close();
        linear_lat.Close();
        
        // Serf-QT-Curve
        SerfQtCurveCompressor curve_lon(gps_data.size(), epsilon);
        SerfQtCurveCompressor curve_lat(gps_data.size(), epsilon);
        for (const auto& point : gps_data) {
            curve_lon.AddValue(point.longitude);
            curve_lat.AddValue(point.latitude);
        }
        curve_lon.Close();
        curve_lat.Close();
        
        // 记录结果
        SummaryResult result;
        result.dataset_name = dataset.name;
        result.points = gps_data.size();
        result.sp_bits = sp_compressor.GetCompressedSizeInBits();
        result.qt_bits = qt_lon.get_compressed_size_in_bits() + qt_lat.get_compressed_size_in_bits();
        result.linear_bits = linear_lon.get_compressed_size_in_bits() + linear_lat.get_compressed_size_in_bits();
        result.curve_bits = curve_lon.get_compressed_size_in_bits() + curve_lat.get_compressed_size_in_bits();
        result.sp_avg = static_cast<double>(result.sp_bits) / result.points;
        result.qt_avg = static_cast<double>(result.qt_bits) / result.points;
        result.linear_avg = static_cast<double>(result.linear_bits) / result.points;
        result.curve_avg = static_cast<double>(result.curve_bits) / result.points;
        
        summary_results.push_back(result);
        
        std::cout << "✓ " << dataset.name << ": " << gps_data.size() << " 点完成" << std::endl;
    }
    
    // 输出汇总报告
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "所有数据集测试完成 - 汇总报告" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << std::endl;
    
    // 表头
    std::cout << std::setw(30) << "数据集" 
              << std::setw(10) << "点数"
              << std::setw(12) << "TrajSP"
              << std::setw(12) << "QT(前值)"
              << std::setw(12) << "QT-Linear"
              << std::setw(12) << "QT-Curve"
              << std::setw(12) << "最优" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    for (const auto& result : summary_results) {
        double min_avg = std::min({result.sp_avg, result.qt_avg, result.linear_avg, result.curve_avg});
        std::string best;
        if (result.sp_avg == min_avg) best = "TrajSP";
        else if (result.qt_avg == min_avg) best = "QT";
        else if (result.linear_avg == min_avg) best = "Linear";
        else best = "Curve";
        
        std::cout << std::setw(30) << result.dataset_name
                  << std::setw(10) << result.points
                  << std::setw(12) << std::fixed << std::setprecision(2) << result.sp_avg
                  << std::setw(12) << result.qt_avg
                  << std::setw(12) << result.linear_avg
                  << std::setw(12) << result.curve_avg
                  << std::setw(12) << best << std::endl;
    }
    
    std::cout << std::string(100, '-') << std::endl;
    std::cout << "注: 数值为平均每点比特数 (bits/点)" << std::endl;
    
    // 统计最优算法出现次数
    int sp_wins = 0, qt_wins = 0, linear_wins = 0, curve_wins = 0;
    for (const auto& result : summary_results) {
        double min_avg = std::min({result.sp_avg, result.qt_avg, result.linear_avg, result.curve_avg});
        if (result.sp_avg == min_avg) sp_wins++;
        else if (result.qt_avg == min_avg) qt_wins++;
        else if (result.linear_avg == min_avg) linear_wins++;
        else curve_wins++;
    }
    
    std::cout << "\n最优算法统计:" << std::endl;
    std::cout << "  TrajCompress-SP: " << sp_wins << " 个数据集" << std::endl;
    std::cout << "  Serf-QT (前值):  " << qt_wins << " 个数据集" << std::endl;
    std::cout << "  Serf-QT-Linear:  " << linear_wins << " 个数据集" << std::endl;
    std::cout << "  Serf-QT-Curve:   " << curve_wins << " 个数据集" << std::endl;
}

int main(int argc, char* argv[]) {
    double epsilon = 1e-5;   // 1e-5度 约等于1.1米，标准GPS精度要求
    
    std::cout << "TrajCompress-SP 轨迹压缩算法综合测试程序" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 解析命令行参数
    if (argc > 1) {
        std::string mode = argv[1];
        
        if (mode == "all") {
            // 测试所有数据集
            if (argc > 2) {
                epsilon = std::stod(argv[2]);
            }
            TestAllDatasetsAndGenerateSummary(epsilon);
        } else if (mode == "single") {
            // 测试单个数据集（旧模式兼容）
            std::string dataset_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
            int max_points = -1;
            
            if (argc > 2) {
                dataset_path = argv[2];
            }
            if (argc > 3) {
                max_points = std::stoi(argv[3]);
            }
            if (argc > 4) {
                epsilon = std::stod(argv[4]);
            }
            
            auto gps_data = LoadGpsDataFromCSV(dataset_path, max_points);
            if (gps_data.empty()) {
                std::cerr << "无法加载GPS数据，程序退出" << std::endl;
                return 1;
            }
            
            // 单独测试各个算法
            TestTrajCompressSP(gps_data, epsilon);
            TestSerfQT(gps_data, epsilon);
            TestSerfQTLinear(gps_data, epsilon);
            TestSerfQTCurve(gps_data, epsilon);
            
            // 四种算法综合对比
            FourWayComparativeTest(gps_data, epsilon);
            
            // 原有的两算法对比（保持兼容）
            ComparativeTest(gps_data, epsilon);
        } else {
            std::cout << "使用方法:" << std::endl;
            std::cout << "  测试所有数据集: " << argv[0] << " all [epsilon]" << std::endl;
            std::cout << "  测试单个数据集: " << argv[0] << " single <数据集路径> [最大点数] [epsilon]" << std::endl;
            std::cout << "\n示例:" << std::endl;
            std::cout << "  " << argv[0] << " all 1e-5" << std::endl;
            std::cout << "  " << argv[0] << " single test/data_set/Geolife_100k_longitude_latitude.csv 10000 1e-5" << std::endl;
            return 1;
        }
    } else {
        // 默认：快速测试模式（测试所有数据集）
        std::cout << "默认模式: 测试所有数据集" << std::endl;
        std::cout << "提示: 使用 './trajcompress_sp_test all' 或 './trajcompress_sp_test single <路径>' 指定模式" << std::endl;
        TestAllDatasetsAndGenerateSummary(epsilon);
    }
    
    std::cout << "\n测试完成！" << std::endl;
    
    return 0;
}

