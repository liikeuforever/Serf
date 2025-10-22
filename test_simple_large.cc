#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using SimpleGpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;

std::vector<SimpleGpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points) {
    std::vector<SimpleGpsPoint> points;
    std::ifstream file(filename);
    if (!file.is_open()) return points;
    
    std::string line;
    std::getline(file, line); // skip header
    
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

int main() {
    auto gps_data = LoadGpsDataFromCSV("/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv", 99999);
    std::cout << "加载 " << gps_data.size() << " 个点" << std::endl;
    
    double epsilon = 1e-5;
    TrajCompressSPAdaptiveSimpleCompressor compressor(gps_data.size(), epsilon, 96);
    
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    std::cout << "压缩完成: " << compressor.GetCompressedSizeInBits() << " bits" << std::endl;
    
    Array<uint8_t> compressed = compressor.GetCompressedData();
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    std::vector<SimpleGpsPoint> decompressed;
    SimpleGpsPoint point;
    int fail_at = -1;
    
    for (size_t i = 0; i < gps_data.size(); ++i) {
        if (!decompressor.ReadNextPoint(point)) {
            fail_at = i;
            std::cout << "解压失败在第 " << i << " 个点" << std::endl;
            break;
        }
        decompressed.push_back(point);
    }
    
    std::cout << "解压完成: " << decompressed.size() << " 个点" << std::endl;
    if (fail_at >= 0) {
        std::cout << "失败位置: " << fail_at << std::endl;
    }
    
    return 0;
}
