#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

struct GpsPoint {
    double longitude, latitude;
    GpsPoint(double lon = 0, double lat = 0) : longitude(lon), latitude(lat) {}
};

std::vector<GpsPoint> ReadData(const std::string& file) {
    std::vector<GpsPoint> points;
    std::ifstream f(file);
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string lon, lat;
        if (std::getline(iss, lon, ',') && std::getline(iss, lat, ',')) {
            points.push_back(GpsPoint(std::stod(lon), std::stod(lat)));
        }
    }
    return points;
}

int main() {
    double epsilon = 1e-5;
    auto data = ReadData("test/data_set/Track_63530k_longitude_latitude.csv");
    
    std::cout << "加载了 " << data.size() << " 个点\n\n";
    
    // Linear
    SerfQtLinearCompressor lon_l(data.size(), epsilon), lat_l(data.size(), epsilon);
    for (auto& p : data) { lon_l.AddValue(p.longitude); lat_l.AddValue(p.latitude); }
    lon_l.Close(); lat_l.Close();
    long linear_bits = lon_l.get_compressed_size_in_bits() + lat_l.get_compressed_size_in_bits();
    
    // Adaptive
    TrajCompressSPAdaptiveCompressor adaptive(data.size(), epsilon, true, 32, 128, 256);
    for (auto& p : data) {
        adaptive.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(p.longitude, p.latitude));
    }
    adaptive.Close();
    long adaptive_bits = adaptive.GetCompressedSizeInBits();
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Linear:   " << (double)linear_bits/data.size() << " bits/点\n";
    std::cout << "Adaptive: " << (double)adaptive_bits/data.size() << " bits/点\n";
    std::cout << "\n";
    std::cout << std::setprecision(2);
    std::cout << "Linear:   " << (double)linear_bits/data.size() << " bits/点 (2位精度)\n";
    std::cout << "Adaptive: " << (double)adaptive_bits/data.size() << " bits/点 (2位精度)\n";
    
    return 0;
}
