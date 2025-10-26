#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "src/compressor/trajcompress_sp_compressor.h"
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

struct SimpleGpsPoint {
    double longitude;
    double latitude;
    uint64_t timestamp;
};

std::vector<SimpleGpsPoint> LoadData(const std::string& path, int limit) {
    std::vector<SimpleGpsPoint> data;
    std::ifstream file(path);
    std::string line;
    std::getline(file, line); // skip header
    
    while (std::getline(file, line) && data.size() < limit) {
        std::istringstream ss(line);
        std::string lon_str, lat_str, ts_str;
        if (std::getline(ss, lon_str, ',') && 
            std::getline(ss, lat_str, ',') &&
            std::getline(ss, ts_str, ',')) {
            SimpleGpsPoint p;
            p.longitude = std::stod(lon_str);
            p.latitude = std::stod(lat_str);
            p.timestamp = std::stoull(ts_str);
            data.push_back(p);
        }
    }
    return data;
}

int main() {
    auto data = LoadData("test/data_set/data_set_with_timestamp/Track_63530k_with_timestamp.csv", 10000);
    std::cout << "Loaded " << data.size() << " points\n";
    
    double epsilon = 1e-5;
    
    // TrajSP
    TrajCompressSPCompressor trajsp(data.size(), epsilon);
    for (const auto& p : data) {
        trajsp.AddGpsPoint(TrajCompressSPCompressor::GpsPoint(p.longitude, p.latitude, p.timestamp));
    }
    trajsp.Close();
    auto trajsp_stats = trajsp.GetStats();
    
    std::cout << "\n=== TrajSP Stats ===\n";
    std::cout << "Total bits: " << trajsp_stats.total_bits << "\n";
    std::cout << "Timestamp bits: " << trajsp_stats.timestamp_bits << "\n";
    std::cout << "Spatial bits: " << (trajsp_stats.total_bits - trajsp_stats.timestamp_bits) << "\n";
    std::cout << "BPP (spatial): " << (trajsp_stats.total_bits - trajsp_stats.timestamp_bits) / (double)data.size() << "\n";
    std::cout << "Predictor flag bits: " << trajsp_stats.predictor_flag_bits << "\n";
    std::cout << "Quantized data bits: " << trajsp_stats.quantized_data_bits << "\n";
    
    // Simple
    TrajCompressSPAdaptiveSimpleCompressor simple(data.size(), epsilon, 96);
    for (const auto& p : data) {
        simple.AddGpsPoint(TrajCompressSPAdaptiveSimpleCompressor::GpsPoint(p.longitude, p.latitude, p.timestamp));
    }
    simple.Close();
    auto simple_stats = simple.GetStats();
    
    std::cout << "\n=== Simple Stats ===\n";
    std::cout << "Total bits: " << simple_stats.total_bits << "\n";
    std::cout << "Timestamp bits: " << simple_stats.timestamp_bits << "\n";
    std::cout << "Spatial bits: " << (simple_stats.total_bits - simple_stats.timestamp_bits) << "\n";
    std::cout << "BPP (spatial): " << (simple_stats.total_bits - simple_stats.timestamp_bits) / (double)data.size() << "\n";
    std::cout << "Predictor flag bits: " << simple_stats.predictor_flag_bits << "\n";
    std::cout << "Mode switch bits: " << simple_stats.mode_switch_bits << "\n";
    std::cout << "Quantized data bits: " << simple_stats.quantized_data_bits << "\n";
    
    std::cout << "\n=== Difference ===\n";
    int spatial_diff = (simple_stats.total_bits - simple_stats.timestamp_bits) - 
                      (trajsp_stats.total_bits - trajsp_stats.timestamp_bits);
    std::cout << "Simple - TrajSP (spatial): " << spatial_diff << " bits\n";
    std::cout << "BPP difference: " << spatial_diff / (double)data.size() << "\n";
    
    return 0;
}
// Debug for Geolife
