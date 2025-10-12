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
    std::cout << "测试 Serf-QT 算法（对比基准）" << std::endl;
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

    // 综合对比测试
void ComparativeTest(const std::vector<GpsPoint>& gps_data, double epsilon) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "TrajCompress-SP vs Serf-QT 综合对比" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "TrajCompress-SP误差阈值: " << std::scientific << (epsilon * std::sqrt(2)) << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * std::sqrt(2) * 111000) << " 米)" << std::endl;
    std::cout << "Serf-QT误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // TrajCompress-SP (使用√2×epsilon，因为使用欧几里得距离)
    TrajCompressSPCompressor sp_compressor(gps_data.size(), epsilon * std::sqrt(2));
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
    std::cout << "TrajCompress-SP使用√2×epsilon: " << std::scientific << (epsilon * std::sqrt(2)) 
              << " 度 (约 " << std::fixed << std::setprecision(2) << (epsilon * std::sqrt(2) * 111000) << " 米)" << std::endl;
    std::cout << "Serf-QT使用epsilon: " << std::scientific << epsilon 
              << " 度 (约 " << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "这确保了公平的压缩性能对比（考虑欧几里得距离vs单维度误差）" << std::endl;
}

int main(int argc, char* argv[]) {
    // 默认参数
    std::string dataset_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int max_points = 10000;  // 默认测试1万个点
    double epsilon = 1e-5;   // 1e-5度 约等于1.1米，标准GPS精度要求
    
    // 解析命令行参数
    if (argc > 1) {
        dataset_path = argv[1];
    }
    if (argc > 2) {
        max_points = std::stoi(argv[2]);
    }
    if (argc > 3) {
        epsilon = std::stod(argv[3]);
    }
    
    std::cout << "TrajCompress-SP 轨迹压缩算法测试程序" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "数据集: " << dataset_path << std::endl;
    std::cout << "测试点数: " << max_points << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    
    // 加载数据
    auto gps_data = LoadGpsDataFromCSV(dataset_path, max_points);
    
    if (gps_data.empty()) {
        std::cerr << "无法加载GPS数据，程序退出" << std::endl;
        return 1;
    }
    
    // 测试TrajCompress-SP (使用√2×epsilon，因为使用欧几里得距离)
    TestTrajCompressSP(gps_data, epsilon * std::sqrt(2));
    
    // 测试Serf-QT（对比，使用epsilon）
    TestSerfQT(gps_data, epsilon);
    
    // 综合对比
    ComparativeTest(gps_data, epsilon);
    
    std::cout << "\n测试完成！" << std::endl;
    
    return 0;
}

