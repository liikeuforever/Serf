#include <iostream>
#include <iomanip>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    // 测试1: 小数据集（100个点）
    std::cout << "=== 测试1: 100个点 ===\n";
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    for (int i = 0; i < 100; i++) {
        data.push_back({116.3 + i * 0.001, 39.9 + i * 0.001, static_cast<uint64_t>(1000 + i)});
    }
    
    std::cout << "压缩中...\n";
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    
    for (const auto& p : data) {
        compressor.AddGpsPoint(p);
    }
    compressor.Close();
    
    auto stats = compressor.GetStats();
    std::cout << "压缩完成:\n";
    std::cout << "  total_bits: " << stats.total_bits << "\n";
    std::cout << "  timestamp_bits: " << stats.timestamp_bits << "\n";
    std::cout << "  spatial_bits: " << (stats.total_bits - stats.timestamp_bits) << "\n";
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "  压缩数据: " << compressed.length() << " bytes\n";
    
    std::cout << "\n解压中...\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    int count = 0;
    double max_lon_error = 0, max_lat_error = 0;
    uint64_t max_ts_error = 0;
    
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    while (decompressor.ReadNextPoint(point)) {
        if (count < data.size()) {
            double lon_err = std::abs(point.longitude - data[count].longitude);
            double lat_err = std::abs(point.latitude - data[count].latitude);
            uint64_t ts_err = (point.timestamp > data[count].timestamp) ? 
                             (point.timestamp - data[count].timestamp) : 
                             (data[count].timestamp - point.timestamp);
            
            max_lon_error = std::max(max_lon_error, lon_err);
            max_lat_error = std::max(max_lat_error, lat_err);
            max_ts_error = std::max(max_ts_error, ts_err);
            
            if (count < 3 || count >= 97) {
                std::cout << "  Point " << count << ": ";
                std::cout << "lon=" << std::fixed << std::setprecision(6) << point.longitude;
                std::cout << " (err=" << std::scientific << lon_err << "), ";
                std::cout << "lat=" << std::fixed << std::setprecision(6) << point.latitude;
                std::cout << " (err=" << std::scientific << lat_err << "), ";
                std::cout << "ts=" << point.timestamp;
                std::cout << " (err=" << ts_err << ")\n";
            }
        }
        count++;
        
        if (count > data.size() + 10) {
            std::cout << "  ⚠️  解压出太多点，终止！\n";
            break;
        }
    }
    
    std::cout << "\n解压完成: " << count << " 个点\n";
    std::cout << "最大误差:\n";
    std::cout << "  经度: " << std::scientific << max_lon_error << " 度\n";
    std::cout << "  纬度: " << max_lat_error << " 度\n";
    std::cout << "  时间戳: " << max_ts_error << "\n";
    
    if (count == data.size() && max_ts_error == 0) {
        std::cout << "\n✓ 测试1通过\n\n";
    } else {
        std::cout << "\n✗ 测试1失败: 期望 " << data.size() << " 个点，得到 " << count << " 个点\n\n";
        return 1;
    }
    
    // 测试2: 跨越mode switch边界（第96个点会触发mode switch）
    std::cout << "=== 测试2: 跨越mode switch边界（200个点，评估窗口=96） ===\n";
    data.clear();
    for (int i = 0; i < 200; i++) {
        data.push_back({116.3 + i * 0.001, 39.9 + i * 0.001, static_cast<uint64_t>(1000 + i)});
    }
    
    std::cout << "压缩中...\n";
    TrajCompressSPAdaptiveSimpleCompressor compressor2(data.size(), 1e-5, 96);
    for (const auto& p : data) {
        compressor2.AddGpsPoint(p);
    }
    compressor2.Close();
    
    auto compressed2 = compressor2.GetCompressedData();
    std::cout << "  压缩数据: " << compressed2.length() << " bytes\n";
    
    std::cout << "\n解压中...\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor2(compressed2.begin(), compressed2.length());
    
    count = 0;
    max_ts_error = 0;
    while (decompressor2.ReadNextPoint(point)) {
        if (count < data.size()) {
            uint64_t ts_err = (point.timestamp > data[count].timestamp) ? 
                             (point.timestamp - data[count].timestamp) : 
                             (data[count].timestamp - point.timestamp);
            max_ts_error = std::max(max_ts_error, ts_err);
            
            // 打印mode switch附近的点
            if (count >= 94 && count <= 98) {
                std::cout << "  Point " << count << " (near mode switch): ts=" << point.timestamp 
                          << " (expected=" << data[count].timestamp << ", err=" << ts_err << ")\n";
            }
        }
        count++;
        
        if (count > data.size() + 10) {
            std::cout << "  ⚠️  解压出太多点，终止！\n";
            break;
        }
    }
    
    std::cout << "\n解压完成: " << count << " 个点\n";
    std::cout << "最大timestamp误差: " << max_ts_error << "\n";
    
    if (count == data.size() && max_ts_error == 0) {
        std::cout << "\n✓ 测试2通过\n\n";
        std::cout << "=== 所有测试通过！ ===\n";
        return 0;
    } else {
        std::cout << "\n✗ 测试2失败\n";
        return 1;
    }
}
