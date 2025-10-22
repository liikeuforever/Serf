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

int main() {
    auto gps_data = LoadGpsDataFromCSV("/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv", 39000);
    
    double epsilon = 1e-5;
    TrajCompressSPAdaptiveSimpleCompressor compressor(gps_data.size(), epsilon, 96);
    
    for (size_t i = 0; i < gps_data.size(); ++i) {
        compressor.AddGpsPoint(gps_data[i]);
    }
    compressor.Close();
    
    auto stats = compressor.GetStats();
    std::cout << "压缩统计:" << std::endl;
    std::cout << "  LDR: " << stats.ldr_count << std::endl;
    std::cout << "  CP:  " << stats.cp_count << std::endl;
    std::cout << "  ZP:  " << stats.zp_count << std::endl;
    std::cout << "  总计: " << (stats.ldr_count + stats.cp_count + stats.zp_count) << std::endl;
    
    return 0;
}
