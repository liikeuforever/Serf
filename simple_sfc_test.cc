#include "compressor/space_filling_curve.h"
#include "compressor/sfc_compressor.h"
#include "compressor/serf_qt_gps_configurable_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>

using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

std::vector<GeoPoint> LoadGeolifeData(const std::string& filepath) {
    std::vector<GeoPoint> points;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开文件: " << filepath << std::endl;
        return points;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(iss, lon_str, ',') && std::getline(iss, lat_str, ',')) {
            try {
                double lon = std::stod(lon_str);
                double lat = std::stod(lat_str);
                points.emplace_back(lon, lat);
            } catch (...) {
                continue;
            }
        }
    }
    
    file.close();
    return points;
}

void TestCompressionAndAccuracy(const std::vector<GeoPoint>& points,
                                SFCCompressor::CompressionMethod method,
                                double max_error,
                                const std::string& method_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试: " << method_name << std::endl;
    std::cout << "========================================" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    SFCCompressor compressor(method, max_error);
    for (const auto& point : points) {
        compressor.AddPoint(point);
    }
    
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    auto stats = compressor.GetStatistics();
    const_cast<SFCCompressor::Statistics&>(stats).Calculate();
    
    BoundingBox bbox = compressor.GetBoundingBox();
    int encoding_bits = compressor.GetEncodingBits();
    
    std::cout << "\n📦 压缩结果:" << std::endl;
    std::cout << "  - 原始点数: " << points.size() << std::endl;
    std::cout << "  - 压缩大小: " << compressed_data.length() << " bytes" << std::endl;
    std::cout << "  - 压缩比: " << std::fixed << std::setprecision(2) 
              << stats.compression_ratio << ":1" << std::endl;
    std::cout << "  - 平均编码: " << stats.avg_bits_per_point << " bits/点" << std::endl;
    std::cout << "  - 压缩耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "  - 编码比特数: " << encoding_bits << " bits" << std::endl;
    
    std::cout << "\n🗺️  边界框:" << std::endl;
    std::cout << "  - 经度: [" << std::setprecision(6) << bbox.min_lon << ", " << bbox.max_lon << "]" << std::endl;
    std::cout << "  - 纬度: [" << bbox.min_lat << ", " << bbox.max_lat << "]" << std::endl;
    std::cout << "  - 宽度: " << bbox.GetWidth() << " 度" << std::endl;
    std::cout << "  - 高度: " << bbox.GetHeight() << " 度" << std::endl;
    std::cout << "  - 宽高比: " << std::setprecision(2) << (bbox.GetWidth() / bbox.GetHeight()) << std::endl;
    
    // 验证精度：重新编码解码前10个点
    std::cout << "\n🔍 精度验证 (前10个点):" << std::endl;
    double max_error_found = 0.0;
    
    for (size_t i = 0; i < std::min(size_t(10), points.size()); ++i) {
        const auto& orig = points[i];
        
        // 编码
        uint64_t code;
        if (method == SFCCompressor::STANDARD_GEOHASH) {
            code = SpaceFillingCurve::EncodeStandardGeoHash(orig, encoding_bits);
        } else if (method == SFCCompressor::MBR_GEOHASH) {
            code = SpaceFillingCurve::EncodeMBRGeoHash(orig, encoding_bits, bbox);
        } else {
            int order = encoding_bits / 2;
            code = SpaceFillingCurve::EncodeHilbert(orig, order, bbox);
        }
        
        // 解码
        GeoPoint decoded;
        if (method == SFCCompressor::STANDARD_GEOHASH) {
            decoded = SpaceFillingCurve::DecodeStandardGeoHash(code, encoding_bits);
        } else if (method == SFCCompressor::MBR_GEOHASH) {
            decoded = SpaceFillingCurve::DecodeMBRGeoHash(code, encoding_bits, bbox);
        } else {
            int order = encoding_bits / 2;
            decoded = SpaceFillingCurve::DecodeHilbert(code, order, bbox);
        }
        
        double error = SpaceFillingCurve::CalculateDistance(orig, decoded);
        max_error_found = std::max(max_error_found, error);
        
        std::cout << "  点" << i << ": (" << std::fixed << std::setprecision(6)
                  << orig.longitude << ", " << orig.latitude << ") -> code=" << code
                  << " -> (" << decoded.longitude << ", " << decoded.latitude 
                  << "), 误差=" << std::scientific << std::setprecision(2) << error << " 度";
        if (error > max_error) std::cout << " ⚠️";
        std::cout << std::endl;
    }
    
    std::cout << "\n  ✓ 前10个点最大误差: " << std::scientific << max_error_found << " 度" << std::endl;
    
    // 统计信息
    std::cout << "\n📊 编码统计:" << std::endl;
    std::cout << "  - 头部开销: " << stats.header_bits << " bits" << std::endl;
    std::cout << "  - 第一个点: " << stats.encoding_bits_used << " bits" << std::endl;
    std::cout << "  - 差值总开销: " << stats.diff_encoding_bits << " bits" << std::endl;
    std::cout << "  - 差值范围: [" << stats.min_diff << ", " << stats.max_diff << "]" << std::endl;
    std::cout << "  - 平均差值绝对值: " << std::fixed << std::setprecision(0) 
              << stats.avg_diff_magnitude << std::endl;
}

void TestSerfQT(const std::vector<GeoPoint>& points, double max_error) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试: Serf-QT GPS 压缩器" << std::endl;
    std::cout << "========================================" << std::endl;
    
    int block_size = 1000;
    double e_max = max_error;
    double epsilon_v = 1e-5;
    double epsilon_theta = 0.01;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    SerfQtGpsConfigurableCompressor compressor(block_size, e_max, epsilon_v, epsilon_theta);
    for (const auto& point : points) {
        SerfQtGpsConfigurableCompressor::GpsPoint gps_point(point.longitude, point.latitude);
        compressor.AddGpsPoint(gps_point);
    }
    
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int serf_bits = compressor.GetCompressedSizeInBits();
    double serf_avg_bits = static_cast<double>(serf_bits) / points.size();
    double serf_ratio = (points.size() * 128.0) / serf_bits;
    
    std::cout << "\n📦 压缩结果:" << std::endl;
    std::cout << "  - 原始点数: " << points.size() << std::endl;
    std::cout << "  - 压缩大小: " << compressed_data.length() << " bytes" << std::endl;
    std::cout << "  - 压缩比: " << std::fixed << std::setprecision(2) << serf_ratio << ":1" << std::endl;
    std::cout << "  - 平均编码: " << serf_avg_bits << " bits/点" << std::endl;
    std::cout << "  - 压缩耗时: " << duration.count() << " ms" << std::endl;
    
    std::cout << "\n📊 策略统计:" << std::endl;
    const auto& stats = compressor.GetStrategyStats();
    int total = stats.GetTotalPoints();
    std::cout << "  - 零校正: " << stats.zero_corr_count << " (" 
              << std::setprecision(1) << (100.0 * stats.zero_corr_count / total) << "%)" << std::endl;
    std::cout << "  - 速度校正: " << stats.v_only_count << " (" 
              << (100.0 * stats.v_only_count / total) << "%)" << std::endl;
    std::cout << "  - 角度校正: " << stats.theta_only_count << " (" 
              << (100.0 * stats.theta_only_count / total) << "%)" << std::endl;
    std::cout << "  - 完全校正: " << stats.both_count << " (" 
              << (100.0 * stats.both_count / total) << "%)" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "空间填充曲线压缩 - 修正后测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::string data_file = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    std::cout << "\n📁 加载数据集: " << data_file << std::endl;
    
    auto points = LoadGeolifeData(data_file);
    
    if (points.empty()) {
        std::cerr << "❌ 错误: 无法加载数据!" << std::endl;
        return 1;
    }
    
    std::cout << "✓ 成功加载 " << points.size() << " 个点" << std::endl;
    
    double max_error = 1e-5;
    std::cout << "\n🎯 目标精度: 最大误差 <= " << std::scientific << max_error << " 度" << std::endl;
    
    TestCompressionAndAccuracy(points, SFCCompressor::STANDARD_GEOHASH, max_error, "标准 GeoHash");
    TestCompressionAndAccuracy(points, SFCCompressor::MBR_GEOHASH, max_error, "MBR优化 GeoHash (修正后)");
    TestCompressionAndAccuracy(points, SFCCompressor::HILBERT_CURVE, max_error, "Hilbert 曲线 (修正后)");
    TestSerfQT(points, max_error);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "📊 总结对比" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n关键修正：" << std::endl;
    std::cout << "1. ✅ 修正量化逻辑：从 *(n-1) 改为 *n" << std::endl;
    std::cout << "2. ✅ 完善 Hilbert 曲线 Rot 函数" << std::endl;
    std::cout << "3. ✅ MBR-GeoHash 根据宽高比动态分配比特" << std::endl;
    std::cout << "\n数据集特点：" << std::endl;
    std::cout << "- GeoLife 轨迹数据具有空间局部性" << std::endl;
    std::cout << "- 相邻点之间经纬度变化很小" << std::endl;
    std::cout << "- 适合用差值编码压缩" << std::endl;
    
    return 0;
}

