/**
 * 测试 TrajCompress-SP-Adaptive-Simple 压缩器
 * 
 * 验证：
 * 1. 压缩解压缩逻辑是否正确
 * 2. 误差是否在精度范围内
 * 3. 数据是否完整
 */

#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>

using SimpleGpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;

// 从CSV文件读取GPS数据
std::vector<SimpleGpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points = -1) {
    std::vector<SimpleGpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    std::string line;
    std::getline(file, line); // 跳过header
    
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
    return points;
}

// 计算两点间的距离
double CalculateDistance(const SimpleGpsPoint& p1, const SimpleGpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

int main(int argc, char* argv[]) {
    std::string dataset_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int max_points = 1000;  // 先测试1000个点
    double epsilon = 1e-5;  // 一维精度（经度或纬度单独）
    int window_size = 96;
    
    // 解析命令行参数
    if (argc > 1) max_points = std::stoi(argv[1]);
    if (argc > 2) epsilon = std::stod(argv[2]);
    if (argc > 3) window_size = std::stoi(argv[3]);
    
    // 二维误差阈值 = sqrt(2) * epsilon（因为经度和纬度的误差可以独立达到epsilon）
    double epsilon_2d = epsilon * std::sqrt(2.0);
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "TrajCompress-SP-Adaptive-Simple 压缩解压测试" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "数据集: " << dataset_path << std::endl;
    std::cout << "测试点数: " << max_points << std::endl;
    std::cout << "一维精度: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << "二维误差阈值: " << std::scientific << epsilon_2d << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon_2d * 111000) << " 米)" << std::endl;
    std::cout << "评估窗口: " << window_size << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 加载数据
    std::cout << "\n[步骤1] 加载GPS数据..." << std::endl;
    auto original_data = LoadGpsDataFromCSV(dataset_path, max_points);
    
    if (original_data.empty()) {
        std::cerr << "❌ 无法加载GPS数据" << std::endl;
        return 1;
    }
    
    std::cout << "✓ 成功加载 " << original_data.size() << " 个GPS点" << std::endl;
    std::cout << "  首点: (" << std::fixed << std::setprecision(6) 
              << original_data[0].longitude << ", " << original_data[0].latitude << ")" << std::endl;
    std::cout << "  末点: (" << original_data.back().longitude << ", " 
              << original_data.back().latitude << ")" << std::endl;
    
    // 压缩
    std::cout << "\n[步骤2] 压缩数据..." << std::endl;
    TrajCompressSPAdaptiveSimpleCompressor compressor(original_data.size(), epsilon, window_size);
    
    for (size_t i = 0; i < original_data.size(); ++i) {
        compressor.AddGpsPoint(original_data[i]);
        
        // 每100个点输出一次进度
        if ((i + 1) % 100 == 0 || i == original_data.size() - 1) {
            std::cout << "\r  压缩进度: " << (i + 1) << "/" << original_data.size() 
                      << " (" << std::setprecision(1) << (100.0 * (i + 1) / original_data.size()) << "%)" 
                      << std::flush;
        }
    }
    std::cout << std::endl;
    
    compressor.Close();
    
    int compressed_bits = compressor.GetCompressedSizeInBits();
    double bits_per_point = static_cast<double>(compressed_bits) / original_data.size();
    
    std::cout << "✓ 压缩完成" << std::endl;
    std::cout << "  压缩大小: " << compressed_bits << " bits (" << (compressed_bits / 8) << " bytes)" << std::endl;
    std::cout << "  平均每点: " << std::setprecision(6) << bits_per_point << " bits/点" << std::endl;
    
    // 获取压缩数据
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    std::cout << "  压缩数据长度: " << compressed_data.length() << " bytes" << std::endl;
    
    // 输出统计信息
    const auto& stats = compressor.GetStats();
    std::cout << "\n  压缩统计:" << std::endl;
    std::cout << "    总点数: " << stats.total_points << std::endl;
    std::cout << "    LDR使用: " << stats.ldr_count << " (" 
              << std::setprecision(1) << (100.0 * stats.ldr_count / stats.total_points) << "%)" << std::endl;
    std::cout << "    CP使用:  " << stats.cp_count << " (" 
              << (100.0 * stats.cp_count / stats.total_points) << "%)" << std::endl;
    std::cout << "    ZP使用:  " << stats.zp_count << " (" 
              << (100.0 * stats.zp_count / stats.total_points) << "%)" << std::endl;
    std::cout << "    模式切换: " << stats.mode_switch_count << " 次" << std::endl;
    std::cout << "    Multi模式点数: " << stats.multi_predictor_mode_points << std::endl;
    std::cout << "    LDR-Only模式点数: " << stats.ldr_only_mode_points << std::endl;
    
    // 解压缩
    std::cout << "\n[步骤3] 解压缩数据..." << std::endl;
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed_data.begin(), compressed_data.length());
    
    std::vector<SimpleGpsPoint> decompressed_data;
    SimpleGpsPoint point;
    
    for (size_t i = 0; i < original_data.size(); ++i) {
        bool success = decompressor.ReadNextPoint(point);
        
        if (!success) {
            std::cerr << "\n❌ 解压失败！在第 " << (i + 1) << " 个点处" << std::endl;
            std::cerr << "  已解压: " << decompressed_data.size() << " 个点" << std::endl;
            std::cerr << "  预期: " << original_data.size() << " 个点" << std::endl;
            break;
        }
        
        decompressed_data.push_back(point);
        
        // 每100个点输出一次进度
        if ((i + 1) % 100 == 0 || i == original_data.size() - 1) {
            std::cout << "\r  解压进度: " << (i + 1) << "/" << original_data.size() 
                      << " (" << std::setprecision(1) << (100.0 * (i + 1) / original_data.size()) << "%)" 
                      << std::flush;
        }
    }
    std::cout << std::endl;
    
    if (decompressed_data.size() != original_data.size()) {
        std::cerr << "❌ 解压点数不匹配！" << std::endl;
        std::cerr << "  原始: " << original_data.size() << " 个点" << std::endl;
        std::cerr << "  解压: " << decompressed_data.size() << " 个点" << std::endl;
        return 1;
    }
    
    std::cout << "✓ 解压完成，共 " << decompressed_data.size() << " 个点" << std::endl;
    std::cout << "  首点: (" << std::fixed << std::setprecision(6) 
              << decompressed_data[0].longitude << ", " << decompressed_data[0].latitude << ")" << std::endl;
    std::cout << "  末点: (" << decompressed_data.back().longitude << ", " 
              << decompressed_data.back().latitude << ")" << std::endl;
    
    // 误差分析
    std::cout << "\n[步骤4] 误差分析..." << std::endl;
    
    double max_error = 0;
    double total_error = 0;
    int points_exceeding = 0;
    std::vector<double> errors;
    
    for (size_t i = 0; i < original_data.size(); ++i) {
        double error = CalculateDistance(original_data[i], decompressed_data[i]);
        errors.push_back(error);
        
        max_error = std::max(max_error, error);
        total_error += error;
        
        if (error > epsilon_2d) {
            points_exceeding++;
            
            // 输出前10个超过阈值的点
            if (points_exceeding <= 10) {
                std::cout << "  ⚠️  点 " << i << " 误差超限: " << std::scientific << error 
                          << " 度 (二维阈值: " << epsilon_2d << " 度)" << std::endl;
                std::cout << "      原始: (" << std::fixed << std::setprecision(8) 
                          << original_data[i].longitude << ", " << original_data[i].latitude << ")" << std::endl;
                std::cout << "      解压: (" << decompressed_data[i].longitude << ", " 
                          << decompressed_data[i].latitude << ")" << std::endl;
            }
        }
    }
    
    double avg_error = total_error / original_data.size();
    
    std::cout << "\n误差统计:" << std::endl;
    std::cout << "  最大误差: " << std::scientific << std::setprecision(6) << max_error 
              << " 度 (" << std::fixed << std::setprecision(2) << (max_error * 111000) << " 米)" << std::endl;
    std::cout << "  平均误差: " << std::scientific << avg_error 
              << " 度 (" << std::fixed << (avg_error * 111000) << " 米)" << std::endl;
    std::cout << "  超过阈值点数: " << points_exceeding << " / " << original_data.size() 
              << " (" << std::setprecision(2) << (100.0 * points_exceeding / original_data.size()) << "%)" << std::endl;
    
    // 误差分布
    std::sort(errors.begin(), errors.end());
    std::cout << "\n误差分布:" << std::endl;
    std::cout << "  P50 (中位数): " << std::scientific << errors[errors.size() / 2] 
              << " 度 (" << std::fixed << std::setprecision(2) << (errors[errors.size() / 2] * 111000) << " 米)" << std::endl;
    std::cout << "  P90: " << std::scientific << errors[errors.size() * 9 / 10] 
              << " 度 (" << std::fixed << (errors[errors.size() * 9 / 10] * 111000) << " 米)" << std::endl;
    std::cout << "  P95: " << std::scientific << errors[errors.size() * 95 / 100] 
              << " 度 (" << std::fixed << (errors[errors.size() * 95 / 100] * 111000) << " 米)" << std::endl;
    std::cout << "  P99: " << std::scientific << errors[errors.size() * 99 / 100] 
              << " 度 (" << std::fixed << (errors[errors.size() * 99 / 100] * 111000) << " 米)" << std::endl;
    
    // 精度验证
    std::cout << "\n[步骤5] 精度验证..." << std::endl;
    std::cout << "  一维精度: " << std::scientific << epsilon << " 度" << std::endl;
    std::cout << "  二维阈值: " << epsilon_2d << " 度" << std::endl;
    std::cout << "  实际最大误差: " << max_error << " 度" << std::endl;
    
    if (max_error <= epsilon_2d) {
        std::cout << "  ✅ 精度要求满足！最大误差 " << max_error << " ≤ 二维阈值 " << epsilon_2d << std::endl;
    } else {
        std::cout << "  ❌ 精度要求不满足！最大误差 " << max_error << " > 二维阈值 " << epsilon_2d << std::endl;
        std::cout << "  超出量: " << (max_error - epsilon_2d) << " 度 (" 
                  << std::fixed << std::setprecision(2) << ((max_error - epsilon_2d) * 111000) << " 米)" << std::endl;
        std::cout << "  超出比例: " << std::setprecision(2) << ((max_error / epsilon_2d - 1) * 100) << "%" << std::endl;
    }
    
    // 总结
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "测试总结" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "数据完整性: " << (decompressed_data.size() == original_data.size() ? "✅ 通过" : "❌ 失败") << std::endl;
    std::cout << "精度要求:   " << (max_error <= epsilon_2d ? "✅ 通过" : "❌ 失败") << std::endl;
    std::cout << "压缩比:     " << std::setprecision(2) << (original_data.size() * 128.0 / compressed_bits) << ":1" << std::endl;
    std::cout << "平均每点:   " << std::setprecision(6) << bits_per_point << " bits/点" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    if (decompressed_data.size() == original_data.size() && max_error <= epsilon_2d) {
        std::cout << "\n🎉 所有测试通过！" << std::endl;
        return 0;
    } else {
        std::cout << "\n⚠️  测试未完全通过，请检查实现。" << std::endl;
        return 1;
    }
}

