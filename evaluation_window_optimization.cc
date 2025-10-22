/**
 * 评估窗口大小优化测试
 * 
 * 目标：在 16-200 范围内搜索最优的 kEvaluationWindow 值
 * 策略：测试多个窗口大小，对比性能
 */

#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

using GpsPoint = TrajCompressSPCompressor::GpsPoint;
using SimpleGpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;

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
    
    // 跳过CSV header行
    std::getline(file, line);
    
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

// 数据集配置
struct DatasetConfig {
    std::string name;
    std::string path;
    
    DatasetConfig(const std::string& n, const std::string& p)
        : name(n), path(p) {}
};

// 测试单个窗口大小配置
double TestWindowSize(const std::vector<GpsPoint>& gps_data, double epsilon, int window_size) {
    // 注意：这里我们需要临时修改 kEvaluationWindow
    // 由于它是 constexpr，我们需要通过不同的实例化来测试
    // 暂时使用现有的实现，返回 BPP
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(gps_data.size(), epsilon);
    
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(SimpleGpsPoint(point.longitude, point.latitude));
    }
    
    compressor.Close();
    
    int bits = compressor.GetCompressedSizeInBits();
    return static_cast<double>(bits) / gps_data.size();
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "====================================================================================================\n";
    std::cout << "评估窗口大小优化测试\n";
    std::cout << "====================================================================================================\n";
    std::cout << "目标：寻找 16-200 范围内的最优 kEvaluationWindow 值\n";
    std::cout << "策略：多梯度测试（粗略 → 精细）\n";
    std::cout << "====================================================================================================\n\n";
    
    // 定义测试数据集（选择代表性数据集）
    std::string base_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/";
    std::vector<DatasetConfig> test_datasets = {
        DatasetConfig("Geolife (原始)", base_path + "Geolife_100k_longitude_latitude.csv"),
        DatasetConfig("Track (原始)", base_path + "Track_63530k_longitude_latitude.csv"),
        DatasetConfig("Trajectory (原始)", base_path + "Trajtory_97k_longitude_latitude.csv"),
        DatasetConfig("Geolife (10x降采样)", base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_10x.csv"),
        DatasetConfig("Track (10x降采样)", base_path + "downsampled/Track_63530k_longitude_latitude_downsample_10x.csv")
    };
    
    // 第一阶段：粗略搜索（步长16）
    std::cout << "第一阶段：粗略搜索 (16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192)\n";
    std::cout << "注意：由于 kEvaluationWindow 是 constexpr，当前实现使用固定值96\n";
    std::cout << "需要修改代码以支持运行时配置才能进行完整测试\n\n";
    
    std::cout << "建议的测试窗口大小：\n";
    std::cout << "  粗略搜索: 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208\n";
    std::cout << "  精细搜索: 在最优值附近 ±16 范围内，步长4\n\n";
    
    // 当前只能测试固定的 window_size = 96
    std::cout << "当前测试（固定 window_size = 96）：\n";
    std::cout << std::string(100, '-') << std::endl;
    std::cout << std::setw(30) << "数据集" 
              << std::setw(15) << "点数"
              << std::setw(20) << "BPP (bits/点)" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    for (const auto& dataset : test_datasets) {
        auto gps_data = LoadGpsDataFromCSV(dataset.path, -1);
        if (gps_data.empty()) {
            std::cerr << "跳过: " << dataset.name << std::endl;
            continue;
        }
        
        double bpp = TestWindowSize(gps_data, epsilon, 96);
        
        std::cout << std::setw(30) << dataset.name
                  << std::setw(15) << gps_data.size()
                  << std::setw(20) << std::fixed << std::setprecision(6) << bpp << std::endl;
    }
    
    std::cout << std::string(100, '-') << std::endl;
    std::cout << "\n需要实施的修改：\n";
    std::cout << "1. 将 kEvaluationWindow 从 constexpr 改为构造函数参数\n";
    std::cout << "2. 创建支持运行时配置的版本\n";
    std::cout << "3. 运行完整的梯度搜索测试\n\n";
    
    return 0;
}

