#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

struct GpsPoint {
    double longitude;
    double latitude;
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
};

std::vector<GpsPoint> ReadGpsData(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "⚠️  无法打开文件: " << filename << std::endl;
        return points;
    }

    std::string line;
    std::getline(file, line); // Skip header

    int count = 0;
    while (std::getline(file, line) && (max_points == -1 || count < max_points)) {
        std::istringstream iss(line);
        std::string lon_str, lat_str;
        if (std::getline(iss, lon_str, ',') && std::getline(iss, lat_str, ',')) {
            GpsPoint point(std::stod(lon_str), std::stod(lat_str));
            points.push_back(point);
            count++;
        }
    }
    return points;
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "验证：固定参数 vs 动态参数 - 精确对比" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 测试Track原始数据
    std::cout << "\n正在加载 Track (原始) 数据集..." << std::endl;
    auto gps_data = ReadGpsData("test/data_set/Track_63530k_longitude_latitude.csv", -1);
    std::cout << "加载了 " << gps_data.size() << " 个点" << std::endl;
    
    // 测试Linear
    std::cout << "\n1️⃣ 测试 Linear (基准)..." << std::endl;
    SerfQtLinearCompressor lon_linear(gps_data.size(), epsilon);
    SerfQtLinearCompressor lat_linear(gps_data.size(), epsilon);
    for (const auto& point : gps_data) {
        lon_linear.AddValue(point.longitude);
        lat_linear.AddValue(point.latitude);
    }
    lon_linear.Close();
    lat_linear.Close();
    long linear_bits = lon_linear.get_compressed_size_in_bits() + 
                       lat_linear.get_compressed_size_in_bits();
    double linear_bpp = static_cast<double>(linear_bits) / gps_data.size();
    std::cout << "   Linear: " << std::fixed << std::setprecision(6) 
              << linear_bpp << " bits/点" << std::endl;
    
    // 测试固定参数（关闭动态调整）
    std::cout << "\n2️⃣ 测试 Adaptive (固定参数，enable_adaptive=false)..." << std::endl;
    TrajCompressSPAdaptiveCompressor fixed_compressor(
        gps_data.size(), epsilon, 
        false,  // enable_adaptive = false（固定参数）
        32, 128, 256
    );
    for (const auto& point : gps_data) {
        fixed_compressor.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(
            point.longitude, point.latitude));
    }
    fixed_compressor.Close();
    long fixed_bits = fixed_compressor.GetCompressedSizeInBits();
    double fixed_bpp = static_cast<double>(fixed_bits) / gps_data.size();
    std::cout << "   固定参数: " << std::fixed << std::setprecision(6) 
              << fixed_bpp << " bits/点" << std::endl;
    
    // 测试动态参数（启用动态调整）
    std::cout << "\n3️⃣ 测试 Adaptive (动态参数，enable_adaptive=true)..." << std::endl;
    TrajCompressSPAdaptiveCompressor dynamic_compressor(
        gps_data.size(), epsilon, 
        true,   // enable_adaptive = true（动态参数）
        32, 128, 256
    );
    for (const auto& point : gps_data) {
        dynamic_compressor.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(
            point.longitude, point.latitude));
    }
    dynamic_compressor.Close();
    long dynamic_bits = dynamic_compressor.GetCompressedSizeInBits();
    double dynamic_bpp = static_cast<double>(dynamic_bits) / gps_data.size();
    std::cout << "   动态参数: " << std::fixed << std::setprecision(6) 
              << dynamic_bpp << " bits/点" << std::endl;
    
    // 对比汇总
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "📊 精确对比结果" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << std::left << std::setw(20) << "算法" 
              << std::right << std::setw(20) << "bits/点 (6位精度)" 
              << std::setw(20) << "bits/点 (2位精度)" 
              << std::setw(25) << "vs Linear" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    std::cout << std::left << std::setw(20) << "Linear (基准)" 
              << std::right << std::setw(20) << std::fixed << std::setprecision(6) << linear_bpp
              << std::setw(20) << std::fixed << std::setprecision(2) << linear_bpp
              << std::setw(25) << "0.00%" << std::endl;
    
    double fixed_diff = ((fixed_bpp - linear_bpp) / linear_bpp) * 100;
    std::cout << std::left << std::setw(20) << "固定参数" 
              << std::right << std::setw(20) << std::fixed << std::setprecision(6) << fixed_bpp
              << std::setw(20) << std::fixed << std::setprecision(2) << fixed_bpp
              << std::setw(25) << (std::string("+") + std::to_string(fixed_diff).substr(0, 6) + "%") 
              << std::endl;
    
    double dynamic_diff = ((dynamic_bpp - linear_bpp) / linear_bpp) * 100;
    std::cout << std::left << std::setw(20) << "动态参数" 
              << std::right << std::setw(20) << std::fixed << std::setprecision(6) << dynamic_bpp
              << std::setw(20) << std::fixed << std::setprecision(2) << dynamic_bpp
              << std::setw(25);
    if (dynamic_diff < 0) {
        std::cout << "✅ " << std::to_string(dynamic_diff).substr(0, 6) << "%";
    } else {
        std::cout << "+" << std::to_string(dynamic_diff).substr(0, 6) << "%";
    }
    std::cout << std::endl;
    
    std::cout << std::string(100, '-') << std::endl;
    double improvement = ((fixed_bpp - dynamic_bpp) / fixed_bpp) * 100;
    std::cout << "动态参数 vs 固定参数 改进: ";
    if (improvement > 0) {
        std::cout << "🎉 -" << std::to_string(improvement).substr(0, 5) << "%" << std::endl;
    } else {
        std::cout << "+" << std::to_string(-improvement).substr(0, 5) << "%" << std::endl;
    }
    
    std::cout << "\n💡 结论：" << std::endl;
    std::cout << "• 6位精度可以看到微小差异" << std::endl;
    std::cout << "• 2位精度会因为四舍五入而看起来相同" << std::endl;
    std::cout << "• trajcompress_sp_test 使用2位精度显示，所以看起来差异不明显" << std::endl;
    
    return 0;
}
