#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <chrono>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "decompressor/serf_qt_gps_configurable_decompressor.h"

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
    double min_error;
    int violations;
    double compression_time_ms;
    double decompression_time_ms;
    int total_points;
    bool success;
    
    // 误差分布统计
    double percentile_50;
    double percentile_90;
    double percentile_95;
    double percentile_99;
    
    // 压缩统计
    int compressed_size_bytes;
    int original_size_bytes;
};

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

// 计算GPS距离（度）
double CalculateGpsDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1,
                           const SerfQtGpsConfigurableDecompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

// 计算百分位数
double CalculatePercentile(std::vector<double>& values, double percentile) {
    if (values.empty()) return 0.0;
    
    std::sort(values.begin(), values.end());
    size_t index = static_cast<size_t>(percentile * (values.size() - 1));
    return values[index];
}

// 测试特定参数组合
TestResult TestParameterCombination(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                   const ParameterConfig& config, double error_bound) {
    TestResult result;
    result.config = config;
    result.total_points = points.size();
    result.success = false;
    
    std::cout << "  测试配置: " << config.description << std::flush;
    
    try {
        const int block_size = points.size();
        
        // 压缩
        auto start_time = std::chrono::high_resolution_clock::now();
        SerfQtGpsConfigurableCompressor compressor(block_size, error_bound, 
                                                  config.epsilon_v, config.epsilon_theta);
        
        for (const auto& point : points) {
            compressor.AddGpsPoint(point);
        }
        
        Array<uint8_t> compressed_data = compressor.GetCompressedData();
        auto compress_end = std::chrono::high_resolution_clock::now();
        
        // 解压缩
        SerfQtGpsConfigurableDecompressor decompressor(compressed_data);
        std::vector<SerfQtGpsConfigurableDecompressor::GpsPoint> decompressed_points;
        
        for (int i = 0; i < points.size(); ++i) {
            decompressed_points.push_back(decompressor.GetNextGpsPoint());
        }
        auto decompress_end = std::chrono::high_resolution_clock::now();
        
        // 计算性能指标
        result.original_size_bytes = points.size() * 16; // 每个点16字节 (2个double)
        result.compressed_size_bytes = compressed_data.length();
        result.compression_ratio = static_cast<double>(result.original_size_bytes) / result.compressed_size_bytes;
        
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
        result.min_error = *std::min_element(errors.begin(), errors.end());
        
        // 计算百分位数
        std::vector<double> errors_copy = errors;
        result.percentile_50 = CalculatePercentile(errors_copy, 0.50);
        result.percentile_90 = CalculatePercentile(errors_copy, 0.90);
        result.percentile_95 = CalculatePercentile(errors_copy, 0.95);
        result.percentile_99 = CalculatePercentile(errors_copy, 0.99);
        
        result.success = true;
        std::cout << " ✅" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << " ❌ 错误: " << e.what() << std::endl;
    }
    
    return result;
}

// 打印详细结果
void PrintDetailedResult(const TestResult& result, double error_bound) {
    std::cout << "\n=== " << result.config.description << " ===" << std::endl;
    std::cout << "参数配置:" << std::endl;
    std::cout << "  εv = " << std::scientific << std::setprecision(2) << result.config.epsilon_v << " 度/步" << std::endl;
    std::cout << "  εθ = " << result.config.epsilon_theta << " 弧度" << std::endl;
    
    if (!result.success) {
        std::cout << "❌ 测试失败" << std::endl;
        return;
    }
    
    std::cout << "\n性能指标:" << std::endl;
    std::cout << "  压缩比: " << std::fixed << std::setprecision(2) << result.compression_ratio << ":1" << std::endl;
    std::cout << "  原始大小: " << std::setprecision(1) << (result.original_size_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  压缩大小: " << (result.compressed_size_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  空间节省: " << (100.0 * (1.0 - static_cast<double>(result.compressed_size_bytes) / result.original_size_bytes)) << "%" << std::endl;
    std::cout << "  压缩时间: " << std::setprecision(1) << result.compression_time_ms << " ms" << std::endl;
    std::cout << "  解压时间: " << result.decompression_time_ms << " ms" << std::endl;
    std::cout << "  总时间: " << (result.compression_time_ms + result.decompression_time_ms) << " ms" << std::endl;
    std::cout << "  处理速度: " << std::setprecision(0) << (result.total_points / (result.compression_time_ms / 1000.0)) << " 点/秒" << std::endl;
    
    std::cout << "\n误差分析:" << std::endl;
    std::cout << "  最小误差: " << std::scientific << std::setprecision(3) << result.min_error << " 度" << std::endl;
    std::cout << "  平均误差: " << result.avg_error << " 度" << std::endl;
    std::cout << "  最大误差: " << result.max_error << " 度" << std::endl;
    std::cout << "  误差界限: " << error_bound << " 度" << std::endl;
    
    std::cout << "\n误差分布:" << std::endl;
    std::cout << "  50%: " << result.percentile_50 << " 度" << std::endl;
    std::cout << "  90%: " << result.percentile_90 << " 度" << std::endl;
    std::cout << "  95%: " << result.percentile_95 << " 度" << std::endl;
    std::cout << "  99%: " << result.percentile_99 << " 度" << std::endl;
    
    std::cout << "\n误差控制:" << std::endl;
    std::cout << "  违反数量: " << result.violations << "/" << result.total_points 
              << " (" << std::fixed << std::setprecision(3) 
              << (100.0 * result.violations / result.total_points) << "%)" << std::endl;
    
    if (result.violations == 0) {
        std::cout << "  ✅ 所有点都满足误差要求" << std::endl;
    } else {
        std::cout << "  ❌ 有 " << result.violations << " 个点超出误差界限" << std::endl;
    }
}

// 打印对比表格
void PrintComparisonTable(const std::vector<TestResult>& results, double error_bound) {
    std::cout << "\n=== 10万点参数对比表格 (误差界限: " << std::scientific << error_bound << " 度) ===\n" << std::endl;
    
    std::cout << std::left 
              << std::setw(20) << "配置"
              << std::setw(12) << "压缩比"
              << std::setw(12) << "平均误差"
              << std::setw(12) << "最大误差"
              << std::setw(10) << "违反数"
              << std::setw(12) << "压缩时间"
              << std::setw(12) << "处理速度"
              << std::setw(8) << "状态" << std::endl;
    
    std::cout << std::string(110, '-') << std::endl;
    
    for (const auto& result : results) {
        if (!result.success) continue;
        
        std::cout << std::left 
                  << std::setw(20) << result.config.description.substr(0, 18)
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << result.compression_ratio
                  << std::scientific << std::setprecision(2)
                  << std::setw(12) << result.avg_error
                  << std::setw(12) << result.max_error
                  << std::fixed << std::setprecision(0)
                  << std::setw(10) << result.violations
                  << std::setprecision(1)
                  << std::setw(12) << result.compression_time_ms << "ms"
                  << std::setprecision(0)
                  << std::setw(12) << (result.total_points / (result.compression_time_ms / 1000.0)) << "点/s"
                  << std::setw(8) << (result.violations == 0 ? "✅" : "❌") << std::endl;
    }
}

int main() {
    std::cout << "=== GPS轨迹压缩算法大规模测试 (约10万个点) ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const double error_bound = 1e-5;  // 1e-5度的严格误差界限
    const int test_count = 99999;     // 测试全部可用点数
    
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
    
    // 选择最有代表性的参数配置进行大规模测试
    std::vector<ParameterConfig> test_configs = {
        // 从2万点测试中选出的最优配置
        {5e-7, 0.00005, "超高精度"},
        {1e-6, 0.0001, "高精度"},
        {2e-6, 0.0002, "中高精度"},
        {5e-6, 0.0005, "最佳压缩比"},
        {1e-5, 0.0001, "最快速度"},
        
        // 额外测试边界配置
        {1e-6, 0.0005, "速度精确"},
        {5e-6, 0.0001, "角度精确"},
    };
    
    std::cout << "\n🔄 开始大规模参数测试..." << std::endl;
    std::cout << "测试配置数量: " << test_configs.size() << std::endl;
    
    std::vector<TestResult> results;
    
    for (size_t i = 0; i < test_configs.size(); ++i) {
        std::cout << "\n进度: " << (i + 1) << "/" << test_configs.size() 
                  << " (" << std::fixed << std::setprecision(1) 
                  << (100.0 * (i + 1) / test_configs.size()) << "%)" << std::endl;
        
        TestResult result = TestParameterCombination(points, test_configs[i], error_bound);
        results.push_back(result);
    }
    
    std::cout << "\n✅ 大规模测试完成！" << std::endl;
    
    // 打印详细结果
    for (const auto& result : results) {
        PrintDetailedResult(result, error_bound);
    }
    
    // 打印对比表格
    PrintComparisonTable(results, error_bound);
    
    // 分析大规模性能
    std::cout << "\n=== 大规模性能分析 ===" << std::endl;
    
    std::vector<TestResult> valid_results;
    for (const auto& result : results) {
        if (result.success && result.violations == 0) {
            valid_results.push_back(result);
        }
    }
    
    if (!valid_results.empty()) {
        std::cout << "✅ 在10万点规模下，有 " << valid_results.size() << " 个配置满足误差要求" << std::endl;
        
        // 找到最佳配置
        auto best_compression = *std::max_element(valid_results.begin(), valid_results.end(),
            [](const TestResult& a, const TestResult& b) {
                return a.compression_ratio < b.compression_ratio;
            });
        
        auto fastest = *std::min_element(valid_results.begin(), valid_results.end(),
            [](const TestResult& a, const TestResult& b) {
                return a.compression_time_ms < b.compression_time_ms;
            });
        
        std::cout << "\n🏆 10万点最佳压缩比: " << best_compression.config.description 
                  << " (" << std::fixed << std::setprecision(2) << best_compression.compression_ratio << ":1)" << std::endl;
        std::cout << "⚡ 10万点最快处理: " << fastest.config.description 
                  << " (" << std::setprecision(1) << fastest.compression_time_ms << "ms)" << std::endl;
        
        // 计算平均性能
        double avg_compression_ratio = 0, avg_time = 0, avg_speed = 0;
        for (const auto& result : valid_results) {
            avg_compression_ratio += result.compression_ratio;
            avg_time += result.compression_time_ms;
            avg_speed += (result.total_points / (result.compression_time_ms / 1000.0));
        }
        avg_compression_ratio /= valid_results.size();
        avg_time /= valid_results.size();
        avg_speed /= valid_results.size();
        
        std::cout << "\n📊 10万点平均性能:" << std::endl;
        std::cout << "  平均压缩比: " << std::setprecision(2) << avg_compression_ratio << ":1" << std::endl;
        std::cout << "  平均处理时间: " << std::setprecision(1) << avg_time << " ms" << std::endl;
        std::cout << "  平均处理速度: " << std::setprecision(0) << avg_speed << " 点/秒" << std::endl;
        
    } else {
        std::cout << "❌ 没有配置在10万点规模下满足误差要求" << std::endl;
    }
    
    return 0;
}
