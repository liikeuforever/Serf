/**
 * 评估窗口大小梯度搜索
 * 
 * 目标：在 16-200 范围内找到最优的 kEvaluationWindow
 * 策略：
 *   阶段1：粗略搜索（步长16）
 *   阶段2：精细搜索（在最优值附近，步长4）
 */

#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <map>

// 定义GPS点结构
struct GpsPoint {
    double longitude;
    double latitude;
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
};

using SimpleGpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;

// 从CSV文件读取GPS数据
std::vector<GpsPoint> LoadGpsDataFromCSV(const std::string& filename) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    std::string line;
    std::getline(file, line); // 跳过header
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                points.emplace_back(std::stod(lon_str), std::stod(lat_str));
            } catch (...) {}
        }
    }
    
    file.close();
    return points;
}

// 数据集配置
struct DatasetConfig {
    std::string name;
    std::string path;
    DatasetConfig(const std::string& n, const std::string& p) : name(n), path(p) {}
};

// 测试结果
struct TestResult {
    int window_size;
    std::map<std::string, double> dataset_bpp;  // 每个数据集的BPP
    double avg_bpp;  // 平均BPP
};

// 测试单个窗口大小
TestResult TestWindowSize(const std::vector<DatasetConfig>& datasets, double epsilon, int window_size) {
    TestResult result;
    result.window_size = window_size;
    result.avg_bpp = 0.0;
    
    for (const auto& dataset : datasets) {
        auto gps_data = LoadGpsDataFromCSV(dataset.path);
        if (gps_data.empty()) continue;
        
        TrajCompressSPAdaptiveSimpleCompressor compressor(
            gps_data.size(), 
            epsilon,
            window_size  // 使用指定的窗口大小
        );
        
        for (const auto& point : gps_data) {
            compressor.AddGpsPoint(SimpleGpsPoint(point.longitude, point.latitude));
        }
        
        compressor.Close();
        
        int bits = compressor.GetCompressedSizeInBits();
        double bpp = static_cast<double>(bits) / gps_data.size();
        
        result.dataset_bpp[dataset.name] = bpp;
        result.avg_bpp += bpp;
    }
    
    result.avg_bpp /= datasets.size();
    return result;
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n";
    std::cout << "====================================================================================================\n";
    std::cout << "评估窗口大小梯度搜索测试\n";
    std::cout << "====================================================================================================\n";
    std::cout << "搜索范围: 16 - 200\n";
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约1.1米)\n";
    std::cout << "====================================================================================================\n\n";
    
    // 定义测试数据集（选择代表性数据集）
    std::string base_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/";
    std::vector<DatasetConfig> test_datasets = {
        DatasetConfig("Geolife(原始)", base_path + "Geolife_100k_longitude_latitude.csv"),
        DatasetConfig("Track(原始)", base_path + "Track_63530k_longitude_latitude.csv"),
        DatasetConfig("Trajectory(原始)", base_path + "Trajtory_97k_longitude_latitude.csv"),
        DatasetConfig("Geolife(10x)", base_path + "downsampled/Geolife_100k_longitude_latitude_downsample_10x.csv"),
        DatasetConfig("Track(10x)", base_path + "downsampled/Track_63530k_longitude_latitude_downsample_10x.csv"),
        DatasetConfig("Trajectory(5x)", base_path + "downsampled/Trajtory_97k_longitude_latitude_downsample_5x.csv")
    };
    
    std::vector<TestResult> results;
    
    // ===== 阶段1：粗略搜索 =====
    std::cout << "【阶段1】粗略搜索 (步长16)\n";
    std::cout << std::string(100, '-') << std::endl;
    
    std::vector<int> coarse_windows = {16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192};
    
    for (int window : coarse_windows) {
        std::cout << "测试 window_size = " << std::setw(3) << window << " ... " << std::flush;
        TestResult result = TestWindowSize(test_datasets, epsilon, window);
        results.push_back(result);
        std::cout << "平均BPP: " << std::fixed << std::setprecision(6) << result.avg_bpp << std::endl;
    }
    
    // 找出最优窗口
    auto best_it = std::min_element(results.begin(), results.end(), 
        [](const TestResult& a, const TestResult& b) { return a.avg_bpp < b.avg_bpp; });
    
    int best_coarse_window = best_it->window_size;
    double best_coarse_bpp = best_it->avg_bpp;
    
    std::cout << "\n阶段1最优窗口: " << best_coarse_window << " (平均BPP: " 
              << std::fixed << std::setprecision(6) << best_coarse_bpp << ")\n\n";
    
    // ===== 阶段2：精细搜索 =====
    std::cout << "【阶段2】精细搜索 (在最优值附近 ±16，步长4)\n";
    std::cout << std::string(100, '-') << std::endl;
    
    int fine_start = std::max(16, best_coarse_window - 16);
    int fine_end = std::min(200, best_coarse_window + 16);
    
    for (int window = fine_start; window <= fine_end; window += 4) {
        // 跳过已测试的
        bool already_tested = false;
        for (const auto& r : results) {
            if (r.window_size == window) {
                already_tested = true;
                break;
            }
        }
        if (already_tested) continue;
        
        std::cout << "测试 window_size = " << std::setw(3) << window << " ... " << std::flush;
        TestResult result = TestWindowSize(test_datasets, epsilon, window);
        results.push_back(result);
        std::cout << "平均BPP: " << std::fixed << std::setprecision(6) << result.avg_bpp << std::endl;
    }
    
    // 找出全局最优
    best_it = std::min_element(results.begin(), results.end(), 
        [](const TestResult& a, const TestResult& b) { return a.avg_bpp < b.avg_bpp; });
    
    int best_window = best_it->window_size;
    double best_bpp = best_it->avg_bpp;
    
    std::cout << "\n";
    std::cout << "====================================================================================================\n";
    std::cout << "搜索完成\n";
    std::cout << "====================================================================================================\n";
    std::cout << "最优评估窗口: " << best_window << "\n";
    std::cout << "平均BPP: " << std::fixed << std::setprecision(6) << best_bpp << "\n";
    std::cout << "====================================================================================================\n\n";
    
    // 输出详细对比表
    std::cout << "详细结果对比:\n";
    std::cout << std::string(120, '-') << std::endl;
    std::cout << std::setw(10) << "窗口大小";
    for (const auto& dataset : test_datasets) {
        std::cout << std::setw(16) << dataset.name.substr(0, 15);
    }
    std::cout << std::setw(16) << "平均BPP" << std::setw(10) << "排名" << std::endl;
    std::cout << std::string(120, '-') << std::endl;
    
    // 排序
    std::sort(results.begin(), results.end(), 
        [](const TestResult& a, const TestResult& b) { return a.window_size < b.window_size; });
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::cout << std::setw(10) << r.window_size;
        
        for (const auto& dataset : test_datasets) {
            auto it = r.dataset_bpp.find(dataset.name);
            if (it != r.dataset_bpp.end()) {
                std::cout << std::setw(16) << std::fixed << std::setprecision(4) << it->second;
            } else {
                std::cout << std::setw(16) << "N/A";
            }
        }
        
        std::cout << std::setw(16) << std::fixed << std::setprecision(6) << r.avg_bpp;
        
        if (r.window_size == best_window) {
            std::cout << std::setw(10) << "★最优";
        } else {
            // 计算排名
            int rank = 1;
            for (const auto& other : results) {
                if (other.avg_bpp < r.avg_bpp) rank++;
            }
            std::cout << std::setw(10) << rank;
        }
        std::cout << std::endl;
    }
    std::cout << std::string(120, '-') << std::endl;
    
    // 对比96（当前默认值）
    auto window_96 = std::find_if(results.begin(), results.end(),
        [](const TestResult& r) { return r.window_size == 96; });
    
    if (window_96 != results.end()) {
        double diff = best_bpp - window_96->avg_bpp;
        double pct = (diff / window_96->avg_bpp) * 100;
        
        std::cout << "\n对比当前默认值 (window_size=96):\n";
        std::cout << "  当前 (96):  " << std::fixed << std::setprecision(6) << window_96->avg_bpp << " BPP\n";
        std::cout << "  最优 (" << best_window << "): " << best_bpp << " BPP\n";
        std::cout << "  改进:       " << std::showpos << diff << " BPP (" << pct << "%)\n";
        
        if (std::abs(pct) < 0.1) {
            std::cout << "\n✅ 结论: 当前默认值96已经很接近最优，改进幅度 < 0.1%\n";
        } else if (pct < 0) {
            std::cout << "\n🎯 结论: 最优窗口 " << best_window << " 可以带来 " << std::abs(pct) << "% 的改进！\n";
        }
    }
    
    std::cout << "\n推荐配置: kEvaluationWindow = " << best_window << "\n\n";
    
    return 0;
}

