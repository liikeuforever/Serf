#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

struct GpsPoint {
    double longitude;
    double latitude;
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
};

std::vector<GpsPoint> ReadGpsData(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "⚠️  无法打开文件: " << filename << std::endl;
        return points;
    }

    std::string line;
    std::getline(file, line); // Skip header

    int count = 0;
    while (std::getline(file, line) && (max_points == -1 || count < max_points)) {
        std::istringstream iss(line);
        std::string lon_str, lat_str;
        if (std::getline(iss, lon_str, ',') && std::getline(iss, lat_str, ',')) {
            GpsPoint point(std::stod(lon_str), std::stod(lat_str));
            points.push_back(point);
            count++;
        }
    }
    return points;
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

double TestAdaptive(const std::vector<GpsPoint>& gps_data, double epsilon, bool enable_adaptive) {
    TrajCompressSPAdaptiveCompressor compressor(
        gps_data.size(), epsilon, enable_adaptive);
    
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(
            point.longitude, point.latitude));
    }
    
    compressor.Close();
    long total_bits = compressor.GetCompressedSizeInBits();
    auto stats = compressor.GetStats();
    
    // 打印详细统计信息（可选）
    /*
    std::cout << "\n    - LDR-Only使用率: " << std::fixed << std::setprecision(2)
              << (100.0 * stats.ldr_only_mode_points / stats.total_points) << "%"
              << std::endl;
    */
    
    return static_cast<double>(total_bits) / gps_data.size();
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n" << std::string(110, '=') << std::endl;
    std::cout << "动态自适应参数系统 - 完整测试" << std::endl;
    std::cout << "核心思想：窗口大小和稳定边际随轨迹可预测性实时调整" << std::endl;
    std::cout << std::string(110, '=') << std::endl;
    
    // 测试数据集
    struct DatasetInfo {
        std::string name;
        std::string path;
        int max_points;
    };
    
    std::vector<DatasetInfo> datasets = {
        {"Geolife (原始)", "test/data_set/Geolife_100k_longitude_latitude.csv", 100000},
        {"Geolife (5x降采样)", "test/data_set/Geolife_100k_longitude_latitude_downsample_5x.csv", 20000},
        {"Geolife (10x降采样)", "test/data_set/Geolife_100k_longitude_latitude_downsample_10x.csv", 10000},
        {"Geolife (20x降采样)", "test/data_set/Geolife_100k_longitude_latitude_downsample_20x.csv", 5000},
        {"Geolife (40x降采样)", "test/data_set/Geolife_100k_longitude_latitude_downsample_40x.csv", 2500},
        {"Track (原始)", "test/data_set/Track_63530k_longitude_latitude.csv", 63530},
        {"Track (5x降采样)", "test/data_set/Track_63530k_longitude_latitude_downsample_5x.csv", 12706},
        {"Track (10x降采样)", "test/data_set/Track_63530k_longitude_latitude_downsample_10x.csv", 6353},
        {"Track (20x降采样)", "test/data_set/Track_63530k_longitude_latitude_downsample_20x.csv", 3177},
        {"Track (40x降采样)", "test/data_set/Track_63530k_longitude_latitude_downsample_40x.csv", 1589},
        {"Trajectory (原始)", "test/data_set/Trajectory_longitude_latitude.csv", 97009},
        {"Trajectory (5x降采样)", "test/data_set/Trajectory_longitude_latitude_downsample_5x.csv", 19402},
        {"Trajectory (10x降采样)", "test/data_set/Trajectory_longitude_latitude_downsample_10x.csv", 9701},
        {"Trajectory (20x降采样)", "test/data_set/Trajectory_longitude_latitude_downsample_20x.csv", 4851},
        {"Trajectory (40x降采样)", "test/data_set/Trajectory_longitude_latitude_downsample_40x.csv", 2426}
    };
    
    std::cout << "\n" << std::string(110, '-') << std::endl;
    std::cout << std::left << std::setw(28) << "数据集"
              << std::right << std::setw(10) << "点数"
              << std::setw(14) << "Linear"
              << std::setw(14) << "固定参数"
              << std::setw(14) << "动态参数"
              << std::setw(14) << "vs Linear"
              << std::setw(16) << "vs 固定" << std::endl;
    std::cout << std::string(110, '-') << std::endl;
    
    int dynamic_better_than_fixed = 0;
    int dynamic_better_than_linear = 0;
    double total_improvement_vs_fixed = 0.0;
    int tested_count = 0;
    
    for (const auto& dataset : datasets) {
        auto gps_data = ReadGpsData(dataset.path, dataset.max_points);
        if (gps_data.empty()) {
            continue;
        }
        
        double linear_bpp = TestLinear(gps_data, epsilon);
        double fixed_bpp = TestAdaptive(gps_data, epsilon, false);  // 固定参数（关闭自适应）
        double dynamic_bpp = TestAdaptive(gps_data, epsilon, true); // 动态参数（启用自适应）
        
        double improvement_vs_fixed = ((fixed_bpp - dynamic_bpp) / fixed_bpp) * 100;
        double improvement_vs_linear = ((linear_bpp - dynamic_bpp) / linear_bpp) * 100;
        
        total_improvement_vs_fixed += improvement_vs_fixed;
        tested_count++;
        
        if (dynamic_bpp < fixed_bpp) {
            dynamic_better_than_fixed++;
        }
        if (dynamic_bpp < linear_bpp) {
            dynamic_better_than_linear++;
        }
        
        std::cout << std::left << std::setw(28) << dataset.name
                  << std::right << std::setw(10) << gps_data.size()
                  << std::setw(14) << std::fixed << std::setprecision(4) << linear_bpp
                  << std::setw(14) << fixed_bpp
                  << std::setw(14) << dynamic_bpp;
        
        if (dynamic_bpp < linear_bpp) {
            std::cout << std::setw(14) << (std::string("✅ -") + std::to_string(std::abs(improvement_vs_linear)).substr(0, 4) + "%");
        } else {
            std::cout << std::setw(14) << (std::string("+") + std::to_string(improvement_vs_linear).substr(0, 4) + "%");
        }
        
        if (dynamic_bpp < fixed_bpp) {
            std::cout << std::setw(16) << (std::string("🎉 -") + std::to_string(improvement_vs_fixed).substr(0, 4) + "%");
        } else {
            std::cout << std::setw(16) << (std::string("+") + std::to_string(improvement_vs_fixed).substr(0, 4) + "%");
        }
        std::cout << std::endl;
    }
    
    std::cout << std::string(110, '=') << std::endl;
    std::cout << "\n📊 测试总结：" << std::endl;
    std::cout << "总测试数据集: " << tested_count << std::endl;
    std::cout << "动态 vs 固定参数: " << dynamic_better_than_fixed << "/" << tested_count 
              << " (" << std::fixed << std::setprecision(1) 
              << (100.0 * dynamic_better_than_fixed / tested_count) << "% 胜率)" << std::endl;
    std::cout << "动态 vs Linear: " << dynamic_better_than_linear << "/" << tested_count 
              << " (" << std::fixed << std::setprecision(1) 
              << (100.0 * dynamic_better_than_linear / tested_count) << "% 胜率)" << std::endl;
    std::cout << "平均改进 (vs 固定): " << std::fixed << std::setprecision(4) 
              << (total_improvement_vs_fixed / tested_count) << "%" << std::endl;
    
    std::cout << "\n💡 关键特性：" << std::endl;
    std::cout << "• 预测器流失率（Churn Rate）→ 动态窗口大小（32-128）" << std::endl;
    std::cout << "• 成本差标准差（StdDev）→ 动态稳定边际（0-5 bits）" << std::endl;
    std::cout << "• 每256点评估一次，实时自适应" << std::endl;
    std::cout << "• 完全基于数学原理，无魔法数字！✨" << std::endl;
    
    return 0;
}
