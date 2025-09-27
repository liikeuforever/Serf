#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <chrono>

#include "compressor/serf_qt_gps_trajectory_compressor.h"
#include "decompressor/serf_qt_gps_trajectory_decompressor.h"

struct TestResult {
    double epsilon_v;
    double epsilon_theta;
    double compression_ratio;
    double avg_error;
    double max_error;
    int violations;
    double compression_time_ms;
    double decompression_time_ms;
    int total_points;
    bool success;
};

// 读取GPS点数据
std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> ReadGpsPoints(const std::string& filename, int max_points) {
    std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> points;
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

// 计算GPS距离（度）
double CalculateGpsDistance(const SerfQtGpsTrajectoryCompressor::GpsPoint& p1,
                           const SerfQtGpsTrajectoryDecompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

// 测试特定参数组合
TestResult TestParameterCombination(const std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint>& points,
                                   double epsilon_v, double epsilon_theta, double error_bound) {
    TestResult result;
    result.epsilon_v = epsilon_v;
    result.epsilon_theta = epsilon_theta;
    result.total_points = points.size();
    result.success = false;
    
    try {
        // 创建临时的压缩器和解压器（需要修改参数）
        const int block_size = points.size();
        
        // 压缩
        auto start_time = std::chrono::high_resolution_clock::now();
        SerfQtGpsTrajectoryCompressor compressor(block_size, error_bound);
        
        for (const auto& point : points) {
            compressor.AddGpsPoint(point);
        }
        
        Array<uint8_t> compressed_data = compressor.GetCompressedData();
        auto compress_end = std::chrono::high_resolution_clock::now();
        
        // 解压缩
        SerfQtGpsTrajectoryDecompressor decompressor(compressed_data);
        std::vector<SerfQtGpsTrajectoryDecompressor::GpsPoint> decompressed_points;
        
        for (int i = 0; i < points.size(); ++i) {
            decompressed_points.push_back(decompressor.GetNextGpsPoint());
        }
        auto decompress_end = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        result.compression_ratio = static_cast<double>(points.size() * 128) / (compressed_data.GetLength() * 8);
        
        auto compress_duration = std::chrono::duration_cast<std::chrono::microseconds>(compress_end - start_time);
        auto decompress_duration = std::chrono::duration_cast<std::chrono::microseconds>(decompress_end - compress_end);
        result.compression_time_ms = compress_duration.count() / 1000.0;
        result.decompression_time_ms = decompress_duration.count() / 1000.0;
        
        // 计算误差统计
        std::vector<double> errors;
        result.violations = 0;
        
        for (size_t i = 0; i < points.size(); ++i) {
            double error = CalculateGpsDistance(points[i], decompressed_points[i]);
            errors.push_back(error);
            
            if (error > error_bound) {
                result.violations++;
            }
        }
        
        result.avg_error = std::accumulate(errors.begin(), errors.end(), 0.0) / errors.size();
        result.max_error = *std::max_element(errors.begin(), errors.end());
        result.success = true;
        
    } catch (const std::exception& e) {
        std::cerr << "测试参数 (εv=" << epsilon_v << ", εθ=" << epsilon_theta << ") 时出错: " << e.what() << std::endl;
    }
    
    return result;
}

// 打印测试结果表格
void PrintResultsTable(const std::vector<TestResult>& results, double error_bound) {
    std::cout << "\n=== 参数优化测试结果 (误差界限: " << std::scientific << error_bound << " 度) ===\n" << std::endl;
    
    std::cout << std::left << std::setw(12) << "εv (度/步)"
              << std::setw(15) << "εθ (弧度)" 
              << std::setw(12) << "压缩比"
              << std::setw(12) << "平均误差"
              << std::setw(12) << "最大误差"
              << std::setw(10) << "违反数"
              << std::setw(12) << "压缩时间"
              << std::setw(12) << "解压时间"
              << std::setw(8) << "状态" << std::endl;
    
    std::cout << std::string(110, '-') << std::endl;
    
    for (const auto& result : results) {
        if (!result.success) continue;
        
        std::cout << std::left << std::scientific << std::setprecision(2)
                  << std::setw(12) << result.epsilon_v
                  << std::setw(15) << result.epsilon_theta
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << result.compression_ratio
                  << std::scientific << std::setprecision(2)
                  << std::setw(12) << result.avg_error
                  << std::setw(12) << result.max_error
                  << std::fixed << std::setprecision(0)
                  << std::setw(10) << result.violations
                  << std::setprecision(1)
                  << std::setw(12) << result.compression_time_ms << "ms"
                  << std::setw(12) << result.decompression_time_ms << "ms"
                  << std::setw(8) << (result.violations == 0 ? "✅" : "❌") << std::endl;
    }
}

// 找到最优参数组合
void FindOptimalParameters(const std::vector<TestResult>& results) {
    std::cout << "\n=== 最优参数分析 ===" << std::endl;
    
    // 找到满足误差要求的结果
    std::vector<TestResult> valid_results;
    for (const auto& result : results) {
        if (result.success && result.violations == 0) {
            valid_results.push_back(result);
        }
    }
    
    if (valid_results.empty()) {
        std::cout << "❌ 没有找到满足误差要求的参数组合" << std::endl;
        return;
    }
    
    // 按压缩比排序
    std::sort(valid_results.begin(), valid_results.end(), 
              [](const TestResult& a, const TestResult& b) {
                  return a.compression_ratio > b.compression_ratio;
              });
    
    std::cout << "\n🏆 最佳压缩比参数:" << std::endl;
    const auto& best_compression = valid_results[0];
    std::cout << "  εv = " << std::scientific << best_compression.epsilon_v << " 度/步" << std::endl;
    std::cout << "  εθ = " << best_compression.epsilon_theta << " 弧度" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << best_compression.compression_ratio << ":1" << std::endl;
    std::cout << "  最大误差: " << std::scientific << best_compression.max_error << " 度" << std::endl;
    
    // 按压缩时间排序
    std::sort(valid_results.begin(), valid_results.end(), 
              [](const TestResult& a, const TestResult& b) {
                  return a.compression_time_ms < b.compression_time_ms;
              });
    
    std::cout << "\n⚡ 最快压缩速度参数:" << std::endl;
    const auto& fastest = valid_results[0];
    std::cout << "  εv = " << std::scientific << fastest.epsilon_v << " 度/步" << std::endl;
    std::cout << "  εθ = " << fastest.epsilon_theta << " 弧度" << std::endl;
    std::cout << "  压缩时间: " << std::fixed << std::setprecision(1) << fastest.compression_time_ms << " ms" << std::endl;
    std::cout << "  压缩比: " << std::setprecision(2) << fastest.compression_ratio << ":1" << std::endl;
}

int main() {
    std::cout << "=== GPS轨迹压缩算法参数优化测试 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const double error_bound = 1e-5;  // 1e-5度的严格误差界限
    const int test_count = 20000;     // 测试2万个点
    
    std::cout << "数据文件: " << data_file << std::endl;
    std::cout << "测试点数: " << test_count << std::endl;
    std::cout << "误差界限: " << std::scientific << error_bound << " 度 (约" 
              << std::fixed << std::setprecision(1) << (error_bound * 111000) << "米)" << std::endl;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << " 个点" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 定义测试参数范围
    std::vector<double> epsilon_v_values = {
        5e-7, 1e-6, 2e-6, 5e-6, 1e-5, 2e-5, 5e-5
    };
    
    std::vector<double> epsilon_theta_values = {
        0.00005, 0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01
    };
    
    std::cout << "\n🔄 开始参数优化测试..." << std::endl;
    std::cout << "速度量化步长范围: " << epsilon_v_values.size() << " 个值" << std::endl;
    std::cout << "角度量化步长范围: " << epsilon_theta_values.size() << " 个值" << std::endl;
    std::cout << "总测试组合数: " << epsilon_v_values.size() * epsilon_theta_values.size() << std::endl;
    
    std::vector<TestResult> results;
    int test_index = 0;
    int total_tests = epsilon_v_values.size() * epsilon_theta_values.size();
    
    for (double epsilon_v : epsilon_v_values) {
        for (double epsilon_theta : epsilon_theta_values) {
            test_index++;
            std::cout << "\r进度: " << test_index << "/" << total_tests 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (100.0 * test_index / total_tests) << "%)" << std::flush;
            
            TestResult result = TestParameterCombination(points, epsilon_v, epsilon_theta, error_bound);
            results.push_back(result);
        }
    }
    
    std::cout << "\n✅ 参数测试完成！" << std::endl;
    
    // 打印结果
    PrintResultsTable(results, error_bound);
    FindOptimalParameters(results);
    
    return 0;
}
