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
    
    return static_cast<double>(total_bits) / gps_data.size();
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "动态自适应参数测试" << std::endl;
    std::cout << "核心思想：参数随轨迹可预测性实时调整" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 测试数据集
    struct DatasetInfo {
        std::string name;
        std::string path;
        int max_points;
    };
    
    std::vector<DatasetInfo> datasets = {
        {"Geolife (原始)", "test/data_set/Geolife_100k_longitude_latitude.csv", 100000},
        {"Track (原始)", "test/data_set/Track_63530k_longitude_latitude.csv", 63530},
        {"Trajectory (原始)", "test/data_set/Trajectory_97k.csv", 97009},
        {"Geolife (5x降采样)", "test/data_set/downsampled/Geolife_100k_longitude_latitude_5x.csv", 20000},
        {"Track (5x降采样)", "test/data_set/downsampled/Track_63530k_longitude_latitude_5x.csv", 12706}
    };
    
    std::cout << "\n" << std::string(100, '-') << std::endl;
    std::cout << std::left << std::setw(25) << "数据集"
              << std::right << std::setw(12) << "点数"
              << std::setw(15) << "Linear"
              << std::setw(15) << "固定参数"
              << std::setw(15) << "动态参数"
              << std::setw(15) << "改进" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    int dynamic_wins = 0;
    double total_improvement = 0.0;
    
    for (const auto& dataset : datasets) {
        std::cout << "正在测试: " << dataset.name << "..." << std::flush;
        
        auto gps_data = ReadGpsData(dataset.path, dataset.max_points);
        if (gps_data.empty()) {
            std::cout << " ⚠️  跳过" << std::endl;
            continue;
        }
        
        double linear_bpp = TestLinear(gps_data, epsilon);
        double fixed_bpp = TestAdaptive(gps_data, epsilon, false);  // 固定参数
        double dynamic_bpp = TestAdaptive(gps_data, epsilon, true); // 动态参数
        
        double improvement = fixed_bpp - dynamic_bpp;
        total_improvement += improvement;
        
        if (dynamic_bpp < fixed_bpp) {
            dynamic_wins++;
        }
        
        std::cout << "\r" << std::left << std::setw(25) << dataset.name
                  << std::right << std::setw(12) << gps_data.size()
                  << std::setw(15) << std::fixed << std::setprecision(4) << linear_bpp
                  << std::setw(15) << fixed_bpp
                  << std::setw(15) << dynamic_bpp;
        
        if (dynamic_bpp < fixed_bpp) {
            std::cout << std::setw(15) << ("+" + std::to_string(improvement * 100).substr(0, 5) + "%") << " 🎉";
        } else {
            std::cout << std::setw(15) << ("-" + std::to_string(-improvement * 100).substr(0, 5) + "%");
        }
        std::cout << std::endl;
    }
    
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "\n✅ 测试完成！" << std::endl;
    std::cout << "动态参数胜出: " << dynamic_wins << "/" << datasets.size() << " 数据集" << std::endl;
    std::cout << "平均改进: " << std::fixed << std::setprecision(4) 
              << (total_improvement / datasets.size()) << " bits/点" << std::endl;
    
    std::cout << "\n📊 关键发现：" << std::endl;
    std::cout << "• 窗口大小: 32-128 (动态调整)" << std::endl;
    std::cout << "• 稳定边际: 基于成本差标准差" << std::endl;
    std::cout << "• 调整周期: 每256点评估一次" << std::endl;
    
    return 0;
}
