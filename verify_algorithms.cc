#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "src/compressor/trajcompress_sp_compressor.h"
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include "src/compressor/serf_qt_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include "src/compressor/serf_qt_curve_compressor.h"
#include "src/decompressor/serf_qt_decompressor.h"
#include "src/decompressor/serf_qt_linear_decompressor.h"
#include "src/decompressor/serf_qt_curve_decompressor.h"

// 使用TrajSP的GpsPoint作为主要类型
using TrajSPGpsPoint = TrajCompressSPCompressor::GpsPoint;
using SimpleGpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;

uint64_t ParseTimestamp(const std::string& time_str) {
    struct tm tm = {};
    std::istringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return static_cast<uint64_t>(mktime(&tm));
}

double CalculateDistance(const SimpleGpsPoint& p1, const SimpleGpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

double CalculateDistance(const TrajSPGpsPoint& p1, const SimpleGpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

std::vector<SimpleGpsPoint> LoadData(const std::string& csv_file, int max_points = 100) {
    std::vector<SimpleGpsPoint> data;
    std::ifstream file(csv_file);
    std::string line;
    std::getline(file, line); // header
    
    int count = 0;
    while (std::getline(file, line) && count < max_points) {
        std::istringstream ss(line);
        std::string lon_str, lat_str, time_str;
        
        if (std::getline(ss, lon_str, ',') && 
            std::getline(ss, lat_str, ',') && 
            std::getline(ss, time_str)) {
            data.push_back(SimpleGpsPoint(std::stod(lon_str), std::stod(lat_str), ParseTimestamp(time_str)));
            count++;
        }
    }
    
    return data;
}

void TestTrajSP(const std::vector<SimpleGpsPoint>& data, double epsilon) {
    std::cout << "\n=== 测试 TrajCompress-SP ===" << std::endl;
    
    TrajCompressSPCompressor compressor(data.size(), epsilon);
    for (const auto& p : data) {
        compressor.AddGpsPoint(TrajSPGpsPoint(p.longitude, p.latitude, p.timestamp));
    }
    compressor.Close();
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "压缩大小: " << compressed.length() << " bytes" << std::endl;
    
    TrajCompressSPDecompressor decompressor(compressed.begin(), compressed.length());
    std::vector<TrajSPGpsPoint> decompressed;
    TrajSPGpsPoint point;
    for (size_t i = 0; i < data.size(); ++i) {
        if (decompressor.ReadNextPoint(point)) {
            decompressed.push_back(point);
        } else {
            break;
        }
    }
    
    std::cout << "解压点数: " << decompressed.size() << " / " << data.size() << std::endl;
    
    if (decompressed.size() != data.size()) {
        std::cout << "✗ 点数不匹配！" << std::endl;
        return;
    }
    
    double max_error = 0, sum_error = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        double error = CalculateDistance(decompressed[i], data[i]);
        max_error = std::max(max_error, error);
        sum_error += error;
    }
    
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "平均误差: " << std::scientific << sum_error / data.size() << " 度" << std::endl;
    std::cout << "阈值: " << std::scientific << epsilon << " 度" << std::endl;
    std::cout << (max_error <= epsilon * 1.415 ? "✓ 通过" : "✗ 超标") << std::endl;
}

void TestSimple(const std::vector<SimpleGpsPoint>& data, double epsilon) {
    std::cout << "\n=== 测试 TrajCompress-SP-Simple ===" << std::endl;
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), epsilon);
    for (const auto& p : data) {
        compressor.AddGpsPoint(p);  // p已经是SimpleGpsPoint类型
    }
    compressor.Close();
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "压缩大小: " << compressed.length() << " bytes" << std::endl;
    
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    std::vector<SimpleGpsPoint> decompressed;
    SimpleGpsPoint point;
    for (size_t i = 0; i < data.size(); ++i) {
        if (decompressor.ReadNextPoint(point)) {
            decompressed.push_back(point);
        } else {
            break;
        }
    }
    
    std::cout << "解压点数: " << decompressed.size() << " / " << data.size() << std::endl;
    
    if (decompressed.size() != data.size()) {
        std::cout << "✗ 点数不匹配！" << std::endl;
        return;
    }
    
    double max_error = 0, sum_error = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        double error = CalculateDistance(decompressed[i], data[i]);
        max_error = std::max(max_error, error);
        sum_error += error;
    }
    
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "平均误差: " << std::scientific << sum_error / data.size() << " 度" << std::endl;
    std::cout << "阈值: " << std::scientific << epsilon << " 度" << std::endl;
    std::cout << (max_error <= epsilon * 1.415 ? "✓ 通过" : "✗ 超标") << std::endl;
}

void TestQT(const std::vector<SimpleGpsPoint>& data, double epsilon) {
    std::cout << "\n=== 测试 Serf-QT ===" << std::endl;
    
    SerfQtCompressor lon_compressor(data.size(), epsilon);
    SerfQtCompressor lat_compressor(data.size(), epsilon);
    
    for (const auto& p : data) {
        lon_compressor.AddValue(p.longitude);
        lat_compressor.AddValue(p.latitude);
    }
    lon_compressor.Close();
    lat_compressor.Close();
    
    auto lon_compressed = lon_compressor.compressed_bytes();
    auto lat_compressed = lat_compressor.compressed_bytes();
    
    std::cout << "压缩大小: lon=" << lon_compressed.length() << " + lat=" << lat_compressed.length() << " bytes" << std::endl;
    
    SerfQtDecompressor lon_decompressor;
    SerfQtDecompressor lat_decompressor;
    auto lon_values = lon_decompressor.Decompress(lon_compressed);
    auto lat_values = lat_decompressor.Decompress(lat_compressed);
    
    std::cout << "解压点数: lon=" << lon_values.size() << ", lat=" << lat_values.size() << " / " << data.size() << std::endl;
    
    if (lon_values.size() != data.size() || lat_values.size() != data.size()) {
        std::cout << "✗ 点数不匹配！" << std::endl;
        return;
    }
    
    double max_error = 0, sum_error = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        SimpleGpsPoint decompressed(lon_values[i], lat_values[i]);
        double error = CalculateDistance(decompressed, data[i]);
        max_error = std::max(max_error, error);
        sum_error += error;
    }
    
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "平均误差: " << std::scientific << sum_error / data.size() << " 度" << std::endl;
    std::cout << "阈值: " << std::scientific << epsilon << " 度" << std::endl;
    std::cout << (max_error <= epsilon * 1.415 ? "✓ 通过" : "✗ 超标") << std::endl;
}

void TestLinear(const std::vector<SimpleGpsPoint>& data, double epsilon) {
    std::cout << "\n=== 测试 Serf-QT-Linear ===" << std::endl;
    
    SerfQtLinearCompressor lon_compressor(data.size(), epsilon);
    SerfQtLinearCompressor lat_compressor(data.size(), epsilon);
    
    for (const auto& p : data) {
        lon_compressor.AddValue(p.longitude, p.timestamp);
        lat_compressor.AddValue(p.latitude, p.timestamp);
    }
    lon_compressor.Close();
    lat_compressor.Close();
    
    auto lon_compressed = lon_compressor.compressed_bytes();
    auto lat_compressed = lat_compressor.compressed_bytes();
    
    // 排除timestamp bits
    int timestamp_bits_per_point = 128; // 64位 × 2
    int total_timestamp_bits = timestamp_bits_per_point * data.size();
    int spatial_bits = lon_compressor.get_compressed_size_in_bits() + 
                       lat_compressor.get_compressed_size_in_bits() - 
                       total_timestamp_bits;
    
    std::cout << "压缩大小: lon=" << lon_compressed.length() << " + lat=" << lat_compressed.length() << " bytes" << std::endl;
    std::cout << "空间压缩bits: " << spatial_bits << " (排除timestamp)" << std::endl;
    
    SerfQtLinearDecompressor lon_decompressor;
    SerfQtLinearDecompressor lat_decompressor;
    auto lon_values = lon_decompressor.Decompress(lon_compressed);
    auto lat_values = lat_decompressor.Decompress(lat_compressed);
    
    std::cout << "解压点数: lon=" << lon_values.size() << ", lat=" << lat_values.size() << " / " << data.size() << std::endl;
    
    if (lon_values.size() != data.size() || lat_values.size() != data.size()) {
        std::cout << "✗ 点数不匹配！" << std::endl;
        return;
    }
    
    double max_error = 0, sum_error = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        SimpleGpsPoint decompressed(lon_values[i], lat_values[i]);
        double error = CalculateDistance(decompressed, data[i]);
        max_error = std::max(max_error, error);
        sum_error += error;
    }
    
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "平均误差: " << std::scientific << sum_error / data.size() << " 度" << std::endl;
    std::cout << "阈值: " << std::scientific << epsilon << " 度" << std::endl;
    std::cout << (max_error <= epsilon * 1.415 ? "✓ 通过" : "✗ 超标") << std::endl;
}

void TestCurve(const std::vector<SimpleGpsPoint>& data, double epsilon) {
    std::cout << "\n=== 测试 Serf-QT-Curve ===" << std::endl;
    
    SerfQtCurveCompressor lon_compressor(data.size(), epsilon);
    SerfQtCurveCompressor lat_compressor(data.size(), epsilon);
    
    for (const auto& p : data) {
        lon_compressor.AddValue(p.longitude, p.timestamp);
        lat_compressor.AddValue(p.latitude, p.timestamp);
    }
    lon_compressor.Close();
    lat_compressor.Close();
    
    auto lon_compressed = lon_compressor.compressed_bytes();
    auto lat_compressed = lat_compressor.compressed_bytes();
    
    // 排除timestamp bits
    int timestamp_bits_per_point = 128; // 64位 × 2
    int total_timestamp_bits = timestamp_bits_per_point * data.size();
    int spatial_bits = lon_compressor.get_compressed_size_in_bits() + 
                       lat_compressor.get_compressed_size_in_bits() - 
                       total_timestamp_bits;
    
    std::cout << "压缩大小: lon=" << lon_compressed.length() << " + lat=" << lat_compressed.length() << " bytes" << std::endl;
    std::cout << "空间压缩bits: " << spatial_bits << " (排除timestamp)" << std::endl;
    
    SerfQtCurveDecompressor lon_decompressor;
    SerfQtCurveDecompressor lat_decompressor;
    auto lon_values = lon_decompressor.Decompress(lon_compressed);
    auto lat_values = lat_decompressor.Decompress(lat_compressed);
    
    std::cout << "解压点数: lon=" << lon_values.size() << ", lat=" << lat_values.size() << " / " << data.size() << std::endl;
    
    if (lon_values.size() != data.size() || lat_values.size() != data.size()) {
        std::cout << "✗ 点数不匹配！" << std::endl;
        return;
    }
    
    double max_error = 0, sum_error = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        SimpleGpsPoint decompressed(lon_values[i], lat_values[i]);
        double error = CalculateDistance(decompressed, data[i]);
        max_error = std::max(max_error, error);
        sum_error += error;
    }
    
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "平均误差: " << std::scientific << sum_error / data.size() << " 度" << std::endl;
    std::cout << "阈值: " << std::scientific << epsilon << " 度" << std::endl;
    std::cout << (max_error <= epsilon * 1.415 ? "✓ 通过" : "✗ 超标") << std::endl;
}

int main() {
    std::string csv_file = "test/data_set/data_set_with_timestamp/Geolife_100k_with_timestamp.csv";
    double epsilon = 1e-5;
    
    std::cout << "加载测试数据（前100个点）..." << std::endl;
    auto data = LoadData(csv_file, 100);
    std::cout << "成功加载 " << data.size() << " 个点" << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度 (约1.11米)" << std::endl;
    std::cout << "2D误差阈值: " << std::scientific << epsilon * 1.415 << " 度 (约1.57米)" << std::endl;
    
    TestTrajSP(data, epsilon);
    TestSimple(data, epsilon);
    TestQT(data, epsilon);
    TestLinear(data, epsilon);
    TestCurve(data, epsilon);
    
    return 0;
}
