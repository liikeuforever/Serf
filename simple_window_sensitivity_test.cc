/**
 * TrajCompress-SP-Adaptive-Simple 窗口参数敏感性测试
 * 
 * 功能：
 * 1. 测试不同 kEvaluationWindow 参数对压缩性能的影响
 * 2. 在15个数据集上进行测试
 * 3. 输出压缩比、最大误差、平均误差到CSV文件
 */

#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <chrono>
#include <iomanip>

using GpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;

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
                // 忽略解析错误
            }
        }
    }
    
    file.close();
    return points;
}

// 计算两点间的距离（度）
double CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

// 计算最大误差和平均误差
void CalculateErrors(const std::vector<GpsPoint>& original,
                    const std::vector<GpsPoint>& decompressed,
                    double& max_error,
                    double& avg_error) {
    max_error = 0;
    avg_error = 0;
    
    if (original.size() != decompressed.size()) {
        std::cerr << "警告: 点数不一致 (" << original.size() << " vs " << decompressed.size() << ")" << std::endl;
        size_t min_size = std::min(original.size(), decompressed.size());
        for (size_t i = 0; i < min_size; ++i) {
            double error = CalculateDistance(original[i], decompressed[i]);
            max_error = std::max(max_error, error);
            avg_error += error;
        }
        avg_error /= min_size;
        return;
    }
    
    for (size_t i = 0; i < original.size(); ++i) {
        double error = CalculateDistance(original[i], decompressed[i]);
        max_error = std::max(max_error, error);
        avg_error += error;
    }
    
    avg_error /= original.size();
}

// 数据集配置结构
struct DatasetConfig {
    std::string name;
    std::string path;
    int total_points;
    
    DatasetConfig(const std::string& n, const std::string& p, int pts)
        : name(n), path(p), total_points(pts) {}
};

// 获取所有测试数据集
std::vector<DatasetConfig> GetAllDatasets() {
    std::string base_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/";
    std::vector<DatasetConfig> datasets;
    
    // Geolife 数据集
    datasets.emplace_back("Geolife (原始)", 
                         base_path + "Geolife_100k_longitude_latitude.csv", 100000);
    datasets.emplace_back("Geolife (5x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_5x.csv", 19999);
    datasets.emplace_back("Geolife (10x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_10x.csv", 9999);
    datasets.emplace_back("Geolife (20x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_20x.csv", 4999);
    datasets.emplace_back("Geolife (40x降采样)", 
                         base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_40x.csv", 2499);
    
    // Track 数据集
    datasets.emplace_back("Track (原始)", 
                         base_path + "Track_63530k_longitude_latitude.csv", 63530);
    datasets.emplace_back("Track (5x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_5x.csv", 12705);
    datasets.emplace_back("Track (10x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_10x.csv", 6352);
    datasets.emplace_back("Track (20x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_20x.csv", 3176);
    datasets.emplace_back("Track (40x降采样)", 
                         base_path + "downsampled/Track_63530k_longitude_latitude_downsample_40x.csv", 1588);
    
    // Trajectory 数据集
    datasets.emplace_back("Trajectory (原始)", 
                         base_path + "Trajtory_97k_longitude_latitude.csv", 97009);
    datasets.emplace_back("Trajectory (5x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_5x.csv", 19402);
    datasets.emplace_back("Trajectory (10x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_10x.csv", 9701);
    datasets.emplace_back("Trajectory (20x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_20x.csv", 4851);
    datasets.emplace_back("Trajectory (40x降采样)", 
                         base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_40x.csv", 2426);
    
    return datasets;
}

// 测试结果结构
struct TestResult {
    std::string dataset_name;
    int window_size;
    int points;
    double bpp;  // bits per point
    double max_error;
    double avg_error;
    bool decompress_success;
};

// 测试单个配置
TestResult TestSingleConfig(const DatasetConfig& dataset, int window_size, double epsilon) {
    TestResult result;
    result.dataset_name = dataset.name;
    result.window_size = window_size;
    result.decompress_success = false;
    
    // 加载数据
    auto gps_data = LoadGpsDataFromCSV(dataset.path, -1);
    result.points = gps_data.size();
    
    if (gps_data.empty()) {
        result.bpp = 0;
        result.max_error = 0;
        result.avg_error = 0;
        return result;
    }
    
    // 压缩
    TrajCompressSPAdaptiveSimpleCompressor compressor(gps_data.size(), epsilon, window_size);
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    // 获取压缩大小
    int compressed_bits = compressor.GetCompressedSizeInBits();
    result.bpp = static_cast<double>(compressed_bits) / gps_data.size();
    
    // 解压缩
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed_data.begin(), compressed_data.length());
    
    std::vector<GpsPoint> decompressed;
    GpsPoint point;
    for (size_t i = 0; i < gps_data.size(); ++i) {
        if (decompressor.ReadNextPoint(point)) {
            decompressed.push_back(point);
        } else {
            break;
        }
    }
    
    result.decompress_success = (decompressed.size() == gps_data.size());
    
    // 计算误差
    CalculateErrors(gps_data, decompressed, result.max_error, result.avg_error);
    
    return result;
}

int main() {
    double epsilon = 1e-5;  // 1e-5度 约等于1.1米
    
    // 测试的窗口大小范围
    std::vector<int> window_sizes = {16, 24, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192};
    
    auto datasets = GetAllDatasets();
    
    std::cout << "TrajCompress-SP-Adaptive-Simple 窗口参数敏感性测试" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "测试数据集数量: " << datasets.size() << std::endl;
    std::cout << "测试窗口大小: ";
    for (int ws : window_sizes) {
        std::cout << ws << " ";
    }
    std::cout << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约 " 
              << std::fixed << std::setprecision(2) << (epsilon * 111000) << " 米)" << std::endl;
    std::cout << std::string(100, '=') << std::endl << std::endl;
    
    // 存储所有测试结果
    std::vector<TestResult> all_results;
    
    // 对每个数据集和每个窗口大小进行测试
    int total_tests = datasets.size() * window_sizes.size();
    int current_test = 0;
    
    for (const auto& dataset : datasets) {
        std::cout << "\n正在测试数据集: " << dataset.name << std::endl;
        
        for (int window_size : window_sizes) {
            current_test++;
            std::cout << "  [" << current_test << "/" << total_tests << "] "
                      << "Window=" << window_size << " ... " << std::flush;
            
            auto result = TestSingleConfig(dataset, window_size, epsilon);
            all_results.push_back(result);
            
            std::cout << "BPP=" << std::fixed << std::setprecision(3) << result.bpp
                      << ", MaxErr=" << std::scientific << std::setprecision(2) << result.max_error
                      << ", AvgErr=" << result.avg_error;
            
            if (!result.decompress_success) {
                std::cout << " ⚠️ 解压失败";
            }
            std::cout << std::endl;
        }
    }
    
    // 生成CSV文件
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream csv_filename;
    csv_filename << "simple_window_sensitivity_" 
                 << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S") 
                 << ".csv";
    
    std::ofstream csv_file(csv_filename.str());
    if (csv_file.is_open()) {
        // 写入CSV头
        csv_file << "Dataset,WindowSize,Points,BPP,MaxError,AvgError,DecompressSuccess\n";
        
        // 写入所有结果（保留6位有效数字）
        for (const auto& result : all_results) {
            csv_file << result.dataset_name << ","
                     << result.window_size << ","
                     << result.points << ","
                     << std::fixed << std::setprecision(6) << result.bpp << ","
                     << std::scientific << std::setprecision(6) << result.max_error << ","
                     << result.avg_error << ","
                     << (result.decompress_success ? "TRUE" : "FALSE") << "\n";
        }
        
        csv_file.close();
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "✅ 测试完成！结果已导出到: " << csv_filename.str() << std::endl;
        std::cout << std::string(100, '=') << std::endl;
    } else {
        std::cerr << "无法创建CSV文件" << std::endl;
    }
    
    // 输出每个数据集的最优窗口大小
    std::cout << "\n每个数据集的最优窗口大小（最小BPP）:" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& dataset : datasets) {
        double min_bpp = 1e9;
        int best_window = 0;
        
        for (const auto& result : all_results) {
            if (result.dataset_name == dataset.name && result.bpp < min_bpp) {
                min_bpp = result.bpp;
                best_window = result.window_size;
            }
        }
        
        std::cout << std::setw(30) << dataset.name 
                  << " -> Window=" << std::setw(4) << best_window
                  << ", BPP=" << std::fixed << std::setprecision(3) << min_bpp << std::endl;
    }
    
    return 0;
}


