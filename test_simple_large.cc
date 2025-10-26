#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

// 解析时间戳
uint64_t ParseTimestamp(const std::string& time_str) {
    struct tm tm = {};
    std::istringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return static_cast<uint64_t>(mktime(&tm));
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file> [max_points]\n";
        return 1;
    }
    
    std::string csv_file = argv[1];
    int max_points = (argc > 2) ? std::atoi(argv[2]) : 10000;
    
    // 加载数据
    std::cout << "加载数据: " << csv_file << "\n";
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    
    std::ifstream file(csv_file);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << csv_file << "\n";
        return 1;
    }
    
    std::string line;
    std::getline(file, line); // 跳过header
    
    while (std::getline(file, line) && data.size() < static_cast<size_t>(max_points)) {
        std::istringstream ss(line);
        std::string lon_str, lat_str, time_str;
        
        if (std::getline(ss, lon_str, ',') && 
            std::getline(ss, lat_str, ',') && 
            std::getline(ss, time_str)) {
            double lon = std::stod(lon_str);
            double lat = std::stod(lat_str);
            uint64_t timestamp = ParseTimestamp(time_str);
            data.push_back({lon, lat, timestamp});
        }
    }
    
    std::cout << "成功加载 " << data.size() << " 个点\n";
    
    // 压缩
    std::cout << "\n=== 开始压缩 ===\n";
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    
    for (size_t i = 0; i < data.size(); i++) {
        if (i >= 1155 && i <= 1165) {
            std::cout << "Before AddGpsPoint(" << i << "): ts=" << data[i].timestamp << "\n";
            std::cout.flush();
        }
        
        compressor.AddGpsPoint(data[i]);
        
        if (i >= 1155 && i <= 1165) {
            std::cout << "After AddGpsPoint(" << i << ")\n";
            std::cout.flush();
        }
    }
    std::cout << "\n压缩完成\n";
    
    compressor.Close();
    std::cout << "Close完成\n";
    
    return 0;
}
