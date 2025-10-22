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
    auto gps_data = LoadGpsDataFromCSV("/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv", 100000);
    std::cout << "加载 " << gps_data.size() << " 个点" << std::endl;
    
    double epsilon = 1e-5;
    TrajCompressSPAdaptiveSimpleCompressor compressor(gps_data.size(), epsilon, 96);
    
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    std::cout << "压缩完成" << std::endl;
    
    Array<uint8_t> compressed = compressor.GetCompressedData();
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    std::vector<SimpleGpsPoint> decompressed;
    SimpleGpsPoint point;
    
    double max_error = 0;
    int first_large_error = -1;
    
    for (size_t i = 0; i < gps_data.size(); ++i) {
        if (!decompressor.ReadNextPoint(point)) {
            std::cout << "解压失败在第 " << i << " 个点" << std::endl;
            break;
        }
        decompressed.push_back(point);
        
        double error = CalculateDistance(gps_data[i], point);
        if (error > max_error) {
            max_error = error;
        }
        
        // 找到第一个超过 2 倍阈值的点
        if (first_large_error < 0 && error > 2e-5) {
            first_large_error = i;
            std::cout << std::fixed << std::setprecision(10);
            std::cout << "\n第一个超标点: " << i << std::endl;
            std::cout << "  原始: (" << gps_data[i].longitude << ", " << gps_data[i].latitude << ")" << std::endl;
            std::cout << "  解压: (" << point.longitude << ", " << point.latitude << ")" << std::endl;
            std::cout << "  误差: " << std::scientific << error << " 度" << std::endl;
            
            // 输出前几个点
            for (int j = std::max(0, (int)i - 5); j < (int)i; ++j) {
                double e = CalculateDistance(gps_data[j], decompressed[j]);
                std::cout << "  点" << j << " 误差: " << e << std::endl;
            }
        }
        
        // 每10000个点输出一次统计
        if ((i + 1) % 10000 == 0) {
            std::cout << "\r进度: " << (i + 1) << " 个点, 当前最大误差: " << std::scientific << max_error << std::flush;
        }
    }
    
    std::cout << "\n\n解压完成: " << decompressed.size() << " 个点" << std::endl;
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "第一个超标点位置: " << first_large_error << std::endl;
    
    return 0;
}
