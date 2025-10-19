/**
 * 自适应压缩器参数调优测试
 * 系统化测试不同参数组合，寻找最优配置
 */

#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <map>
#include <algorithm>

struct GpsPoint {
    double longitude;
    double latitude;
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
};

struct ParameterConfig {
    int cost_window_size;
    int stability_margin;
    bool clear_after_switch;
    int evaluation_interval;
    
    std::string GetName() const {
        std::stringstream ss;
        ss << "W" << cost_window_size 
           << "_M" << stability_margin 
           << "_" << (clear_after_switch ? "C" : "NC")
           << "_E" << evaluation_interval;
        return ss.str();
    }
};

struct TestResult {
    ParameterConfig config;
    double bits_per_point;
    int total_bits;
    int flag_bits;
    int mode_switch_count;
    int ldr_only_points;
    int multi_predictor_points;
    double ldr_only_ratio;
};

// 读取GPS数据
std::vector<GpsPoint> ReadGpsData(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> data;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return data;
    }
    
    std::string line;
    std::getline(file, line); // 跳过表头
    
    while (std::getline(file, line) && (max_points < 0 || data.size() < (size_t)max_points)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                double lon = std::stod(lon_str);
                double lat = std::stod(lat_str);
                data.push_back(GpsPoint(lon, lat));
            } catch (...) {
                continue;
            }
        }
    }
    
    return data;
}

// 测试单个参数配置
TestResult TestConfiguration(const std::vector<GpsPoint>& gps_data, 
                             const ParameterConfig& config,
                             double epsilon) {
    TestResult result;
    result.config = config;
    
    // 使用参数化构造函数
    TrajCompressSPAdaptiveCompressor compressor(
        gps_data.size(), 
        epsilon,
        config.cost_window_size,
        config.stability_margin,
        config.clear_after_switch,
        config.evaluation_interval
    );
    
    using AdaptiveGpsPoint = TrajCompressSPAdaptiveCompressor::GpsPoint;
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(AdaptiveGpsPoint(point.longitude, point.latitude));
    }
    
    compressor.Close();
    auto stats = compressor.GetStats();
    
    result.bits_per_point = static_cast<double>(stats.total_bits) / stats.total_points;
    result.total_bits = stats.total_bits;
    result.flag_bits = stats.predictor_flag_bits;
    result.mode_switch_count = stats.mode_switch_count;
    result.ldr_only_points = stats.ldr_only_mode_points;
    result.multi_predictor_points = stats.multi_predictor_mode_points;
    result.ldr_only_ratio = static_cast<double>(stats.ldr_only_mode_points) / stats.total_points;
    
    return result;
}

void PrintResultsTable(const std::vector<TestResult>& results) {
    std::cout << "\n" << std::string(140, '=') << std::endl;
    std::cout << "参数调优测试结果" << std::endl;
    std::cout << std::string(140, '=') << std::endl;
    
    std::cout << std::left 
              << std::setw(25) << "配置"
              << std::setw(12) << "Bits/点"
              << std::setw(12) << "总Bits"
              << std::setw(12) << "标志Bits"
              << std::setw(10) << "切换次数"
              << std::setw(12) << "LDR-Only%"
              << std::setw(15) << "LDR-Only点"
              << std::setw(15) << "Multi点"
              << std::endl;
    std::cout << std::string(140, '-') << std::endl;
    
    // 找出最优配置
    const TestResult* best = &results[0];
    for (const auto& r : results) {
        if (r.bits_per_point < best->bits_per_point) {
            best = &r;
        }
    }
    
    for (const auto& r : results) {
        bool is_best = (&r == best);
        if (is_best) std::cout << "👑 ";
        else std::cout << "   ";
        
        std::cout << std::left
                  << std::setw(25) << r.config.GetName()
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.bits_per_point
                  << std::setw(12) << r.total_bits
                  << std::setw(12) << r.flag_bits
                  << std::setw(10) << r.mode_switch_count
                  << std::setw(12) << std::fixed << std::setprecision(1) << (r.ldr_only_ratio * 100)
                  << std::setw(15) << r.ldr_only_points
                  << std::setw(15) << r.multi_predictor_points;
        
        if (is_best) {
            std::cout << " <- 最优";
        }
        std::cout << std::endl;
    }
    
    std::cout << std::string(140, '=') << std::endl;
    
    // 打印最优配置详情
    std::cout << "\n最优配置分析:" << std::endl;
    std::cout << "  配置名称: " << best->config.GetName() << std::endl;
    std::cout << "  成本窗口大小: " << best->config.cost_window_size << std::endl;
    std::cout << "  稳定边际: " << best->config.stability_margin << std::endl;
    std::cout << "  切换后清空: " << (best->config.clear_after_switch ? "是" : "否") << std::endl;
    std::cout << "  评估间隔: " << best->config.evaluation_interval << std::endl;
    std::cout << "  性能: " << std::fixed << std::setprecision(2) << best->bits_per_point << " bits/点" << std::endl;
    std::cout << "  LDR-Only使用率: " << std::fixed << std::setprecision(1) 
              << (best->ldr_only_ratio * 100) << "%" << std::endl;
    
    // 与当前默认配置(W64_M2_C_E16)对比
    const TestResult* baseline = nullptr;
    for (const auto& r : results) {
        if (r.config.cost_window_size == 64 && 
            r.config.stability_margin == 2 && 
            r.config.clear_after_switch == true &&
            r.config.evaluation_interval == 16) {
            baseline = &r;
            break;
        }
    }
    
    if (baseline && baseline != best) {
        double improvement = ((baseline->bits_per_point - best->bits_per_point) / baseline->bits_per_point) * 100;
        std::cout << "\n相比默认配置(W64_M2_C_E16)提升: " 
                  << std::fixed << std::setprecision(2) << improvement << "%" << std::endl;
    }
}

struct DatasetInfo {
    std::string name;
    std::string path;
    int max_points;
};

int main(int argc, char* argv[]) {
    double epsilon = 1e-5;
    
    // 定义所有15个数据集
    std::vector<DatasetInfo> datasets = {
        // Geolife数据集
        {"Geolife (原始)", "test/data_set/Geolife_100k_longitude_latitude.csv", 100000},
        {"Geolife (5x降采样)", "test/data_set/downsampled/Geolife_100k_longitude_latitude_downsample_5x.csv", 20000},
        {"Geolife (10x降采样)", "test/data_set/downsampled/Geolife_100k_longitude_latitude_downsample_10x.csv", 10000},
        {"Geolife (20x降采样)", "test/data_set/downsampled/Geolife_100k_longitude_latitude_downsample_20x.csv", 5000},
        {"Geolife (40x降采样)", "test/data_set/downsampled/Geolife_100k_longitude_latitude_downsample_40x.csv", 2500},
        
        // Track数据集
        {"Track (原始)", "test/data_set/Track_63530k_longitude_latitude.csv", 63530},
        {"Track (5x降采样)", "test/data_set/downsampled/Track_63530k_longitude_latitude_downsample_5x.csv", 12706},
        {"Track (10x降采样)", "test/data_set/downsampled/Track_63530k_longitude_latitude_downsample_10x.csv", 6353},
        {"Track (20x降采样)", "test/data_set/downsampled/Track_63530k_longitude_latitude_downsample_20x.csv", 3177},
        {"Track (40x降采样)", "test/data_set/downsampled/Track_63530k_longitude_latitude_downsample_40x.csv", 1589},
        
        // Trajectory数据集
        {"Trajectory (原始)", "test/data_set/Trajtory_97k_longitude_latitude.csv", 97009},
        {"Trajectory (5x降采样)", "test/data_set/downsampled/Trajtory_97k_longitude_latitude_downsample_5x.csv", 19402},
        {"Trajectory (10x降采样)", "test/data_set/downsampled/Trajtory_97k_longitude_latitude_downsample_10x.csv", 9701},
        {"Trajectory (20x降采样)", "test/data_set/downsampled/Trajtory_97k_longitude_latitude_downsample_20x.csv", 4851},
        {"Trajectory (40x降采样)", "test/data_set/downsampled/Trajtory_97k_longitude_latitude_downsample_40x.csv", 2426}
    };
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "自适应压缩器参数调优测试 - 多数据集批量测试" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "测试数据集数量: " << datasets.size() << std::endl;
    std::cout << "误差阈值: " << epsilon << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 定义参数配置集合 - 精选最有希望的配置
    std::vector<ParameterConfig> configs;
    
    // 基准配置（当前默认值）
    configs.push_back({64, 2, true, 16});   // W64_M2_C_E16 (默认)
    
    // 基于初步测试结果，重点测试这些有希望的配置：
    
    // 更大窗口 + 不清空 (Geolife原始数据最优)
    configs.push_back({128, 1, false, 16}); // W128_M1_NC_E16 (Geolife原始最优)
    configs.push_back({128, 0, false, 16}); // W128_M0_NC_E16 (零边际变体)
    
    // 中等窗口 + 不清空 (Track原始数据最优)
    configs.push_back({96, 0, false, 12});  // W96_M0_NC_E12 (Track原始最优)
    configs.push_back({96, 1, false, 16});  // W96_M1_NC_E16 (类似变体)
    configs.push_back({96, 2, false, 16});  // W96_M2_NC_E16 (标准边际变体)
    
    // 大窗口 + 清空（对比测试）
    configs.push_back({128, 2, true, 16});  // W128_M2_C_E16
    configs.push_back({256, 2, true, 16});  // W256_M2_C_E16 (超大窗口)
    
    // 记录所有数据集的最优配置
    std::map<std::string, std::string> best_configs_per_dataset;
    std::map<std::string, double> best_performance_per_dataset;
    std::map<std::string, double> baseline_performance_per_dataset;
    
    // 遍历所有数据集
    for (size_t ds_idx = 0; ds_idx < datasets.size(); ds_idx++) {
        const auto& ds = datasets[ds_idx];
        
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "数据集 " << (ds_idx + 1) << "/" << datasets.size() << ": " << ds.name << std::endl;
        std::cout << "文件: " << ds.path << std::endl;
        std::cout << "最大点数: " << ds.max_points << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        
        // 读取数据
        auto gps_data = ReadGpsData(ds.path, ds.max_points);
        if (gps_data.empty()) {
            std::cerr << "⚠️  读取数据失败，跳过该数据集" << std::endl;
            continue;
        }
        
        std::cout << "实际读取点数: " << gps_data.size() << std::endl;
        std::cout << "\n开始测试 " << configs.size() << " 种参数配置...\n" << std::endl;
        
        // 运行所有配置的测试
        std::vector<TestResult> results;
        
        int count = 0;
        for (const auto& config : configs) {
            count++;
            std::cout << "  [" << count << "/" << configs.size() << "] " 
                      << std::setw(20) << config.GetName() << "..." << std::flush;
            
            TestResult result = TestConfiguration(gps_data, config, epsilon);
            results.push_back(result);
            
            std::cout << " ✓ " << std::fixed << std::setprecision(2) 
                      << result.bits_per_point << " bits/点" << std::endl;
        }
        
        // 打印结果表格
        PrintResultsTable(results);
        
        // 记录最优配置
        const TestResult* best = &results[0];
        const TestResult* baseline = nullptr;
        for (const auto& r : results) {
            if (r.bits_per_point < best->bits_per_point) {
                best = &r;
            }
            if (r.config.cost_window_size == 64 && 
                r.config.stability_margin == 2 && 
                r.config.clear_after_switch == true &&
                r.config.evaluation_interval == 16) {
                baseline = &r;
            }
        }
        
        best_configs_per_dataset[ds.name] = best->config.GetName();
        best_performance_per_dataset[ds.name] = best->bits_per_point;
        if (baseline) {
            baseline_performance_per_dataset[ds.name] = baseline->bits_per_point;
        }
    }
    
    // 打印总结
    std::cout << "\n\n" << std::string(100, '=') << std::endl;
    std::cout << "所有数据集最优配置总结" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    std::cout << std::left 
              << std::setw(30) << "数据集"
              << std::setw(20) << "最优配置"
              << std::setw(15) << "最优性能"
              << std::setw(15) << "基准性能"
              << std::setw(15) << "提升(%)"
              << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    for (const auto& pair : best_configs_per_dataset) {
        const std::string& ds_name = pair.first;
        const std::string& best_config = pair.second;
        double best_perf = best_performance_per_dataset[ds_name];
        double baseline_perf = baseline_performance_per_dataset[ds_name];
        double improvement = ((baseline_perf - best_perf) / baseline_perf) * 100;
        
        std::cout << std::left
                  << std::setw(30) << ds_name
                  << std::setw(20) << best_config
                  << std::setw(15) << std::fixed << std::setprecision(2) << best_perf
                  << std::setw(15) << std::fixed << std::setprecision(2) << baseline_perf
                  << std::setw(15) << std::fixed << std::setprecision(2) << improvement
                  << std::endl;
    }
    
    std::cout << std::string(100, '=') << std::endl;
    
    // 统计最常见的最优配置
    std::map<std::string, int> config_frequency;
    for (const auto& pair : best_configs_per_dataset) {
        config_frequency[pair.second]++;
    }
    
    std::cout << "\n最优配置频率统计:" << std::endl;
    std::vector<std::pair<std::string, int>> sorted_configs(config_frequency.begin(), config_frequency.end());
    std::sort(sorted_configs.begin(), sorted_configs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (const auto& pair : sorted_configs) {
        std::cout << "  " << std::setw(20) << pair.first 
                  << ": " << pair.second << " 个数据集 ("
                  << std::fixed << std::setprecision(1) 
                  << (pair.second * 100.0 / best_configs_per_dataset.size()) << "%)"
                  << std::endl;
    }
    
    std::cout << "\n✅ 测试完成！" << std::endl;
    
    return 0;
}

