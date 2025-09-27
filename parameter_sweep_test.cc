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

struct ParameterConfig {
    double epsilon_v;
    double epsilon_theta;
    std::string description;
};

struct TestResult {
    ParameterConfig config;
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

// 测试当前编译的参数配置
TestResult TestCurrentConfiguration(const std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint>& points,
                                   const ParameterConfig& config, double error_bound) {
    TestResult result;
    result.config = config;
    result.total_points = points.size();
    result.success = false;
    
    try {
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
        std::cerr << "测试配置 " << config.description << " 时出错: " << e.what() << std::endl;
    }
    
    return result;
}

// 打印测试结果
void PrintResult(const TestResult& result, double error_bound) {
    std::cout << "\n=== " << result.config.description << " ===" << std::endl;
    std::cout << "参数配置:" << std::endl;
    std::cout << "  εv = " << std::scientific << result.config.epsilon_v << " 度/步" << std::endl;
    std::cout << "  εθ = " << result.config.epsilon_theta << " 弧度" << std::endl;
    
    if (!result.success) {
        std::cout << "❌ 测试失败" << std::endl;
        return;
    }
    
    std::cout << "\n性能指标:" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << result.compression_ratio << ":1" << std::endl;
    std::cout << "  压缩时间: " << std::setprecision(1) << result.compression_time_ms << " ms" << std::endl;
    std::cout << "  解压时间: " << result.decompression_time_ms << " ms" << std::endl;
    
    std::cout << "\n误差分析:" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << result.avg_error << " 度" << std::endl;
    std::cout << "  最大误差: " << result.max_error << " 度" << std::endl;
    std::cout << "  误差界限: " << error_bound << " 度" << std::endl;
    std::cout << "  违反数量: " << result.violations << "/" << result.total_points 
              << " (" << std::fixed << std::setprecision(1) 
              << (100.0 * result.violations / result.total_points) << "%)" << std::endl;
    
    if (result.violations == 0) {
        std::cout << "  ✅ 所有点都满足误差要求" << std::endl;
    } else {
        std::cout << "  ❌ 有 " << result.violations << " 个点超出误差界限" << std::endl;
    }
}

int main() {
    std::cout << "=== GPS轨迹压缩算法参数测试 (当前配置) ===" << std::endl;
    
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
    
    // 数据范围分析
    double min_lon = points[0].longitude, max_lon = points[0].longitude;
    double min_lat = points[0].latitude, max_lat = points[0].latitude;
    
    for (const auto& point : points) {
        min_lon = std::min(min_lon, point.longitude);
        max_lon = std::max(max_lon, point.longitude);
        min_lat = std::min(min_lat, point.latitude);
        max_lat = std::max(max_lat, point.latitude);
    }
    
    std::cout << "\n数据范围分析:" << std::endl;
    std::cout << "  经度: " << std::fixed << std::setprecision(6) << min_lon << " ~ " << max_lon 
              << " (跨度: " << (max_lon - min_lon) << "度)" << std::endl;
    std::cout << "  纬度: " << min_lat << " ~ " << max_lat 
              << " (跨度: " << (max_lat - min_lat) << "度)" << std::endl;
    
    // 当前编译的参数配置（从头文件中读取）
    ParameterConfig current_config = {
        1e-6,     // kEpsilonV (当前编译值)
        0.0001,   // kEpsilonTheta (当前编译值)
        "当前配置 (εv=1e-6, εθ=0.0001)"
    };
    
    std::cout << "\n🔄 测试当前参数配置..." << std::endl;
    TestResult result = TestCurrentConfiguration(points, current_config, error_bound);
    PrintResult(result, error_bound);
    
    // 提供参数调优建议
    std::cout << "\n=== 参数调优建议 ===" << std::endl;
    std::cout << "要测试不同的参数组合，请按以下步骤操作:" << std::endl;
    std::cout << "1. 修改 src/compressor/serf_qt_gps_trajectory_compressor.h 中的参数:" << std::endl;
    std::cout << "   - kEpsilonV: 速度量化步长 (建议范围: 5e-7 ~ 5e-5)" << std::endl;
    std::cout << "   - kEpsilonTheta: 角度量化步长 (建议范围: 0.00005 ~ 0.01)" << std::endl;
    std::cout << "2. 修改 src/decompressor/serf_qt_gps_trajectory_decompressor.h 中的对应参数" << std::endl;
    std::cout << "3. 重新编译: make -C build serf -j4" << std::endl;
    std::cout << "4. 重新运行此测试程序" << std::endl;
    
    if (result.success) {
        std::cout << "\n=== 当前配置评估 ===" << std::endl;
        if (result.violations == 0) {
            std::cout << "✅ 当前配置满足误差要求" << std::endl;
            std::cout << "📊 压缩性能: " << std::fixed << std::setprecision(2) << result.compression_ratio << ":1" << std::endl;
            
            if (result.compression_ratio < 5.0) {
                std::cout << "💡 建议: 可以尝试增大量化步长以提高压缩比" << std::endl;
            } else if (result.compression_ratio > 15.0) {
                std::cout << "💡 建议: 压缩比很好，可以尝试减小量化步长以提高精度" << std::endl;
            } else {
                std::cout << "💡 建议: 当前配置在压缩比和精度之间取得了良好平衡" << std::endl;
            }
        } else {
            std::cout << "❌ 当前配置不满足误差要求" << std::endl;
            std::cout << "💡 建议: 需要减小量化步长 (kEpsilonV 和/或 kEpsilonTheta)" << std::endl;
        }
    }
    
    return 0;
}
