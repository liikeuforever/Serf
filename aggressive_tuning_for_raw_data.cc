/**
 * 针对原始高采样率数据的激进参数调优
 * 目标：让Adaptive在Geolife和Track原始数据上超越Linear
 */

#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
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
    double ldr_only_ratio;
};

std::vector<GpsPoint> ReadGpsData(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> data;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return data;
    }
    
    std::string line;
    std::getline(file, line);
    
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

TestResult TestConfiguration(const std::vector<GpsPoint>& gps_data, 
                             const ParameterConfig& config,
                             double epsilon) {
    TestResult result;
    result.config = config;
    
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
    result.ldr_only_ratio = static_cast<double>(stats.ldr_only_mode_points) / stats.total_points;
    
    return result;
}

double TestLinear(const std::vector<GpsPoint>& gps_data, double epsilon) {
    SerfQtLinearCompressor lon_compressor(gps_data.size(), epsilon);
    SerfQtLinearCompressor lat_compressor(gps_data.size(), epsilon);
    
    for (const auto& point : gps_data) {
        lon_compressor.AddValue(point.longitude);
        lat_compressor.AddValue(point.latitude);
    }
    
    lon_compressor.Close();
    lat_compressor.Close();
    
    long total_bits = lon_compressor.get_compressed_size_in_bits() + 
                      lat_compressor.get_compressed_size_in_bits();
    
    return static_cast<double>(total_bits) / gps_data.size();
}

int main() {
    double epsilon = 1e-5;
    
    // 测试数据集
    struct Dataset {
        std::string name;
        std::string path;
        int max_points;
    };
    
    std::vector<Dataset> datasets = {
        {"Geolife原始", "test/data_set/Geolife_100k_longitude_latitude.csv", 100000},
        {"Track原始", "test/data_set/Track_63530k_longitude_latitude.csv", 63530}
    };
    
    std::cout << "\n" << std::string(120, '=') << std::endl;
    std::cout << "针对原始高采样率数据的激进参数调优" << std::endl;
    std::cout << "目标: 让Adaptive超越Linear预测" << std::endl;
    std::cout << std::string(120, '=') << std::endl;
    
    for (const auto& ds : datasets) {
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "数据集: " << ds.name << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        
        // 读取数据
        auto gps_data = ReadGpsData(ds.path, ds.max_points);
        if (gps_data.empty()) {
            std::cerr << "⚠️  读取数据失败，跳过" << std::endl;
            continue;
        }
        
        std::cout << "实际读取点数: " << gps_data.size() << std::endl;
        
        // 测试Linear基准
        double linear_perf = TestLinear(gps_data, epsilon);
        std::cout << "Linear基准性能: " << std::fixed << std::setprecision(4) 
                  << linear_perf << " bits/点" << std::endl;
        
        // 生成大量参数组合
        std::vector<ParameterConfig> configs;
        
        // 窗口大小: 从32到512，更密集的采样
        std::vector<int> window_sizes = {32, 48, 64, 80, 96, 112, 128, 144, 160, 192, 224, 256, 320, 384, 448, 512};
        
        // 边际值: 0（最激进）, 1（低）, 2（标准）
        std::vector<int> margins = {0, 1, 2};
        
        // 清空策略
        std::vector<bool> clear_flags = {true, false};
        
        // 评估间隔
        std::vector<int> eval_intervals = {8, 12, 16, 20, 24};
        
        // 生成所有组合（但只测试最有希望的）
        // 策略1: 大窗口 + 不清空 + 零边际（最激进）
        for (int w : {128, 160, 192, 224, 256, 320}) {
            configs.push_back({w, 0, false, 16});
            configs.push_back({w, 0, false, 12});
        }
        
        // 策略2: 中窗口 + 不清空 + 零边际
        for (int w : {80, 96, 112}) {
            configs.push_back({w, 0, false, 16});
            configs.push_back({w, 0, false, 12});
            configs.push_back({w, 0, false, 8});
        }
        
        // 策略3: 各种窗口 + 不清空 + 低边际
        for (int w : {64, 80, 96, 112, 128, 160, 192}) {
            configs.push_back({w, 1, false, 16});
        }
        
        // 策略4: 超大窗口测试
        for (int w : {384, 448, 512}) {
            configs.push_back({w, 0, false, 16});
            configs.push_back({w, 1, false, 16});
        }
        
        // 策略5: 频繁评估
        for (int w : {96, 128, 160}) {
            configs.push_back({w, 0, false, 8});
            configs.push_back({w, 1, false, 8});
        }
        
        // 添加当前最优配置作为基准
        configs.push_back({96, 1, false, 16});  // 当前最优
        configs.push_back({64, 2, true, 16});   // 原始默认
        
        std::cout << "\n开始测试 " << configs.size() << " 种参数配置..." << std::endl;
        std::cout << "目标: 找到能超越Linear (" << std::fixed << std::setprecision(4) 
                  << linear_perf << ") 的配置\n" << std::endl;
        
        std::vector<TestResult> results;
        
        int count = 0;
        int beat_linear_count = 0;
        TestResult* best = nullptr;
        
        for (const auto& config : configs) {
            count++;
            std::cout << "  [" << std::setw(2) << count << "/" << configs.size() << "] " 
                      << std::setw(20) << config.GetName() << "..." << std::flush;
            
            TestResult result = TestConfiguration(gps_data, config, epsilon);
            results.push_back(result);
            
            std::cout << " " << std::fixed << std::setprecision(4) 
                      << result.bits_per_point << " bits/点";
            
            if (result.bits_per_point < linear_perf) {
                std::cout << " 🎉 超越Linear!";
                beat_linear_count++;
            } else {
                double gap = ((result.bits_per_point - linear_perf) / linear_perf) * 100;
                std::cout << " (+" << std::fixed << std::setprecision(3) << gap << "%)";
            }
            
            if (best == nullptr || result.bits_per_point < best->bits_per_point) {
                best = &results.back();
                std::cout << " <- 当前最优";
            }
            
            std::cout << std::endl;
        }
        
        // 打印总结
        std::cout << "\n" << std::string(120, '-') << std::endl;
        std::cout << "测试完成总结:" << std::endl;
        std::cout << "  测试配置数: " << configs.size() << std::endl;
        std::cout << "  超越Linear配置数: " << beat_linear_count << std::endl;
        std::cout << "  最优性能: " << std::fixed << std::setprecision(4) 
                  << best->bits_per_point << " bits/点";
        
        if (best->bits_per_point < linear_perf) {
            double improvement = ((linear_perf - best->bits_per_point) / linear_perf) * 100;
            std::cout << " (超越Linear " << std::fixed << std::setprecision(3) 
                      << improvement << "%) 🎉🎉🎉" << std::endl;
        } else {
            double gap = ((best->bits_per_point - linear_perf) / linear_perf) * 100;
            std::cout << " (仍落后Linear " << std::fixed << std::setprecision(3) 
                      << gap << "%)" << std::endl;
        }
        
        std::cout << "  最优配置: " << best->config.GetName() << std::endl;
        std::cout << "    窗口大小: " << best->config.cost_window_size << std::endl;
        std::cout << "    稳定边际: " << best->config.stability_margin << std::endl;
        std::cout << "    切换后清空: " << (best->config.clear_after_switch ? "是" : "否") << std::endl;
        std::cout << "    评估间隔: " << best->config.evaluation_interval << std::endl;
        std::cout << "    LDR-Only使用率: " << std::fixed << std::setprecision(1) 
                  << (best->ldr_only_ratio * 100) << "%" << std::endl;
        
        // 列出所有超越Linear的配置
        if (beat_linear_count > 0) {
            std::cout << "\n所有超越Linear的配置:" << std::endl;
            for (const auto& r : results) {
                if (r.bits_per_point < linear_perf) {
                    double improvement = ((linear_perf - r.bits_per_point) / linear_perf) * 100;
                    std::cout << "  " << std::setw(20) << r.config.GetName() 
                              << ": " << std::fixed << std::setprecision(4) << r.bits_per_point 
                              << " (超越" << std::fixed << std::setprecision(3) << improvement << "%)" 
                              << " LDR-Only=" << std::fixed << std::setprecision(1) 
                              << (r.ldr_only_ratio * 100) << "%" << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n" << std::string(120, '=') << std::endl;
    std::cout << "✅ 激进参数调优完成！" << std::endl;
    std::cout << std::string(120, '=') << std::endl;
    
    return 0;
}

