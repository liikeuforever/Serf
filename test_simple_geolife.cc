#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

uint64_t ParseTimestamp(const std::string& time_str) {
    struct tm tm = {};
    std::istringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return static_cast<uint64_t>(mktime(&tm));
}

int main() {
    std::string csv_file = "test/data_set/data_set_with_timestamp/Geolife_100k_with_timestamp.csv";
    
    std::cout << "加载数据...\n";
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    
    std::ifstream file(csv_file);
    std::string line;
    std::getline(file, line); // header
    
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string lon_str, lat_str, time_str;
        
        if (std::getline(ss, lon_str, ',') && 
            std::getline(ss, lat_str, ',') && 
            std::getline(ss, time_str)) {
            data.push_back({std::stod(lon_str), std::stod(lat_str), ParseTimestamp(time_str)});
        }
    }
    
    std::cout << "成功加载 " << data.size() << " 个点\n";
    
    std::cout << "压缩中...\n";
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    
    for (size_t i = 0; i < data.size(); i++) {
        if (i % 10000 == 0) {
            std::cout << "  " << i << "/" << data.size() << "\r";
            std::cout.flush();
        }
        compressor.AddGpsPoint(data[i]);
    }
    
    compressor.Close();
    std::cout << "\n压缩完成\n";
    
    auto stats = compressor.GetStats();
    std::cout << "Spatial BPP: " << (stats.total_bits - stats.timestamp_bits) / static_cast<double>(data.size()) << "\n";
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "压缩数据: " << compressed.length() << " bytes\n";
    
    std::cout << "\n解压中...\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    int count = 0;
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    while (decompressor.ReadNextPoint(point)) {
        count++;
        if (count % 10000 == 0) {
            std::cout << "  " << count << "/" << data.size() << "\r";
            std::cout.flush();
        }
    }
    
    std::cout << "\n解压完成: " << count << " 个点\n";
    
    if (count == static_cast<int>(data.size())) {
        std::cout << "✓ 成功\n";
        return 0;
    } else {
        std::cout << "✗ 失败\n";
        return 1;
    }
}
