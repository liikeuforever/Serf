#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include "src/compressor/serf_qt_linear_compressor.h"
#include "src/decompressor/serf_qt_linear_decompressor.h"

struct GpsPoint {
    double longitude;
    double latitude;
    uint64_t timestamp;
    GpsPoint(double lon = 0, double lat = 0, uint64_t ts = 0) 
        : longitude(lon), latitude(lat), timestamp(ts) {}
};

uint64_t ParseTimestamp(const std::string& time_str) {
    struct tm tm = {};
    std::istringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return static_cast<uint64_t>(mktime(&tm));
}

int main() {
    std::string csv_file = "test/data_set/data_set_with_timestamp/Geolife_100k_with_timestamp.csv";
    
    std::cout << "加载数据..." << std::endl;
    std::vector<GpsPoint> data;
    std::ifstream file(csv_file);
    std::string line;
    std::getline(file, line); // header
    
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string lon_str, lat_str, time_str;
        
        if (std::getline(ss, lon_str, ',') && 
            std::getline(ss, lat_str, ',') && 
            std::getline(ss, time_str)) {
            data.push_back(GpsPoint(std::stod(lon_str), std::stod(lat_str), ParseTimestamp(time_str)));
        }
    }
    
    std::cout << "成功加载 " << data.size() << " 个点" << std::endl;
    
    // 测试Linear压缩器（只压缩经度）
    std::cout << "\n开始压缩经度..." << std::endl;
    SerfQtLinearCompressor lon_compressor(data.size(), 1e-5);
    
    for (size_t i = 0; i < data.size(); i++) {
        if (i % 10000 == 0) {
            std::cout << "  压缩进度: " << i << "/" << data.size() << std::endl;
        }
        lon_compressor.AddValue(data[i].longitude, data[i].timestamp);
    }
    lon_compressor.Close();
    
    std::cout << "压缩完成，大小: " << lon_compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    
    // 测试解压
    std::cout << "\n开始解压..." << std::endl;
    auto compressed_data = lon_compressor.compressed_bytes();
    std::cout << "压缩数据大小: " << compressed_data.length() << " bytes" << std::endl;
    
    SerfQtLinearDecompressor decompressor;
    std::cout << "调用Decompress..." << std::endl;
    auto decompressed = decompressor.Decompress(compressed_data);
    
    std::cout << "解压完成，得到 " << decompressed.size() << " 个点" << std::endl;
    std::cout << "期望: " << data.size() << " 个点" << std::endl;
    
    if (decompressed.size() == data.size()) {
        std::cout << "✓ 点数匹配" << std::endl;
    } else {
        std::cout << "✗ 点数不匹配！" << std::endl;
    }
    
    return 0;
}
