#include <iostream>
#include <vector>
#include <iomanip>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

// 全局变量用于追踪bit流操作
int g_bit_position = 0;
bool g_debug_mode = false;

int main() {
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.4, 40.0, 1001});
    
    std::cout << "=== COMPRESSION ===\n";
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    
    // 手动追踪第一个点
    std::cout << "\n--- Point 0 (First Point) ---\n";
    compressor.AddGpsPoint(data[0]);
    auto stats_0 = compressor.GetStats();
    std::cout << "After point 0: total_bits=" << stats_0.total_bits 
              << ", timestamp_bits=" << stats_0.timestamp_bits << "\n";
    
    // 手动追踪第二个点
    std::cout << "\n--- Point 1 ---\n";
    std::cout << "Before point 1: total_bits=" << stats_0.total_bits << "\n";
    
    // 记录压缩前的bit位置
    int bits_before = stats_0.total_bits;
    
    compressor.AddGpsPoint(data[1]);
    auto stats_1 = compressor.GetStats();
    
    std::cout << "After point 1: total_bits=" << stats_1.total_bits 
              << ", timestamp_bits=" << stats_1.timestamp_bits << "\n";
    std::cout << "Point 1 encoded " << (stats_1.total_bits - bits_before) << " bits\n";
    std::cout << "  Timestamp delta bits: " << (stats_1.timestamp_bits - stats_0.timestamp_bits) << "\n";
    std::cout << "  Spatial bits: " << (stats_1.total_bits - bits_before - (stats_1.timestamp_bits - stats_0.timestamp_bits)) << "\n";
    
    compressor.Close();
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "\nCompressed data: " << compressed.length() << " bytes\n";
    std::cout << "Hex dump:\n";
    for (int i = 0; i < std::min(60, (int)compressed.length()); i++) {
        if (i % 16 == 0) std::cout << "  " << std::setw(4) << i << ": ";
        printf("%02x ", compressed[i]);
        if ((i + 1) % 16 == 0) std::cout << "\n";
    }
    std::cout << "\n";
    
    std::cout << "\n=== DECOMPRESSION ===\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    // 解压第一个点
    std::cout << "\n--- Decompressing Point 0 ---\n";
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    bool success0 = decompressor.ReadNextPoint(point);
    std::cout << "Success: " << success0 << "\n";
    std::cout << "Point 0: lon=" << point.longitude << ", lat=" << point.latitude 
              << ", ts=" << point.timestamp << "\n";
    
    // 解压第二个点
    std::cout << "\n--- Decompressing Point 1 ---\n";
    std::cout << "Expected: lon=116.4, lat=40.0, ts=1001\n";
    
    bool success1 = decompressor.ReadNextPoint(point);
    std::cout << "Success: " << success1 << "\n";
    std::cout << "Point 1: lon=" << point.longitude << ", lat=" << point.latitude 
              << ", ts=" << point.timestamp << "\n";
    
    // 验证结果
    double lon_error = std::abs(point.longitude - 116.4);
    double lat_error = std::abs(point.latitude - 40.0);
    uint64_t ts_error = (point.timestamp > 1001) ? (point.timestamp - 1001) : (1001 - point.timestamp);
    
    std::cout << "\nErrors:\n";
    std::cout << "  Longitude: " << lon_error << " (expected ~0)\n";
    std::cout << "  Latitude: " << lat_error << " (expected ~0)\n";
    std::cout << "  Timestamp: " << ts_error << " (expected 0)\n";
    
    if (lon_error < 0.01 && lat_error < 0.01 && ts_error == 0) {
        std::cout << "\n✓ SUCCESS\n";
        return 0;
    } else {
        std::cout << "\n✗ FAILED - Data mismatch!\n";
        return 1;
    }
}
