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
    auto gps_data = LoadGpsDataFromCSV("/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv", 40000);
    std::cout << "加载 " << gps_data.size() << " 个点" << std::endl;
    
    double epsilon = 1e-5;
    TrajCompressSPAdaptiveSimpleCompressor compressor(gps_data.size(), epsilon, 96);
    
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    auto stats = compressor.GetStats();
    std::cout << "压缩统计:" << std::endl;
    std::cout << "  模式切换次数: " << stats.mode_switch_count << std::endl;
    std::cout << "  Multi模式点数: " << stats.multi_predictor_mode_points << std::endl;
    std::cout << "  LDR-Only模式点数: " << stats.ldr_only_mode_points << std::endl;
    
    Array<uint8_t> compressed = compressor.GetCompressedData();
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    std::vector<SimpleGpsPoint> decompressed;
    SimpleGpsPoint point;
    
    for (size_t i = 0; i < gps_data.size(); ++i) {
        if (!decompressor.ReadNextPoint(point)) {
            std::cout << "解压失败在第 " << i << " 个点" << std::endl;
            break;
        }
        decompressed.push_back(point);
        
        double error = CalculateDistance(gps_data[i], point);
        
        // 在38550-38570之间输出详细信息
        if (i >= 38550 && i <= 38570) {
            std::cout << std::fixed << std::setprecision(10);
            std::cout << "点" << i << ": ";
            std::cout << "原始(" << gps_data[i].longitude << ", " << gps_data[i].latitude << ") ";
            std::cout << "解压(" << point.longitude << ", " << point.latitude << ") ";
            std::cout << "误差=" << std::scientific << error;
            if (i % 96 == 0) std::cout << " [窗口边界]";
            std::cout << std::endl;
        }
    }
    
    std::cout << "\n解压完成: " << decompressed.size() << " 个点" << std::endl;
    
    return 0;
}
