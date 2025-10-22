#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

using SimpleGpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;

std::vector<SimpleGpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points) {
    std::vector<SimpleGpsPoint> points;
    std::ifstream file(filename);
    if (!file.is_open()) return points;
    
    std::string line;
    std::getline(file, line);
    
    int count = 0;
    while (std::getline(file, line) && (max_points < 0 || count < max_points)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            points.emplace_back(std::stod(lon_str), std::stod(lat_str));
            count++;
        }
    }
    return points;
}

double CalculateDistance(const SimpleGpsPoint& p1, const SimpleGpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    // 只加载到38600个点，重点调试38559附近
    auto gps_data = LoadGpsDataFromCSV("/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv", 38600);
    std::cout << "加载 " << gps_data.size() << " 个点" << std::endl;
    
    double epsilon = 1e-5;
    TrajCompressSPAdaptiveSimpleCompressor compressor(gps_data.size(), epsilon, 96);
    
    // 压缩
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    auto stats = compressor.GetStats();
    std::cout << "压缩完成，模式切换次数: " << stats.mode_switch_count << std::endl;
    
    // 获取压缩数据
    Array<uint8_t> compressed = compressor.GetCompressedData();
    std::cout << "压缩数据大小: " << compressed.length() << " bytes" << std::endl;
    
    // 解压
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    std::vector<SimpleGpsPoint> decompressed;
    SimpleGpsPoint point;
    
    int window_idx_38559 = 38559 / 96;  // = 401
    int window_start = window_idx_38559 * 96;  // = 38496
    
    std::cout << "\n38559所在评估窗口: 第" << window_idx_38559 << "个窗口 (点" << window_start << "-" << (window_start+95) << ")" << std::endl;
    std::cout << "38559在窗口内位置: " << (38559 % 96) << std::endl;
    
    for (size_t i = 0; i < gps_data.size(); ++i) {
        if (!decompressor.ReadNextPoint(point)) {
            std::cout << "解压失败在第 " << i << " 个点" << std::endl;
            break;
        }
        decompressed.push_back(point);
        
        double error = CalculateDistance(gps_data[i], point);
        
        // 输出38496-38592（完整的第401和402窗口）
        if (i >= 38496 && i <= 38592) {
            std::cout << std::fixed << std::setprecision(10);
            std::cout << "点" << std::setw(5) << i << ": ";
            std::cout << "原(" << std::setw(15) << gps_data[i].longitude << "," << std::setw(15) << gps_data[i].latitude << ") ";
            std::cout << "解(" << std::setw(15) << point.longitude << "," << std::setw(15) << point.latitude << ") ";
            std::cout << "误差=" << std::scientific << std::setprecision(3) << error;
            if (i % 96 == 0) std::cout << " <-- 窗口" << (i/96) << "开始";
            if (i == 38559) std::cout << " <-- *** 第一个超标点 ***";
            std::cout << std::endl;
        }
    }
    
    std::cout << "\n解压完成: " << decompressed.size() << " 个点" << std::endl;
    
    // 统计误差
    double max_error = 0;
    for (size_t i = 0; i < gps_data.size(); ++i) {
        double error = CalculateDistance(gps_data[i], decompressed[i]);
        max_error = std::max(max_error, error);
    }
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    
    return 0;
}
