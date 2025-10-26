#include <iostream>
#include <vector>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.4, 40.0, 1001});
    
    std::cout << "=== COMPRESSION ===\n";
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    
    for (const auto& p : data) {
        compressor.AddGpsPoint(p);
    }
    
    compressor.Close();  // 重要：Close()才会设置stats_.total_bits
    
    auto stats = compressor.GetStats();
    std::cout << "After Close():\n";
    std::cout << "  total_bits: " << stats.total_bits << "\n";
    std::cout << "  timestamp_bits: " << stats.timestamp_bits << "\n";
    std::cout << "  spatial_bits: " << (stats.total_bits - stats.timestamp_bits) << "\n";
    std::cout << "  predictor_flag_bits: " << stats.predictor_flag_bits << "\n";
    std::cout << "  quantized_data_bits: " << stats.quantized_data_bits << "\n";
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "\nCompressed: " << compressed.length() << " bytes = " 
              << (compressed.length() * 8) << " bits\n";
    
    std::cout << "\n=== DECOMPRESSION ===\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    for (int i = 0; i < 2; i++) {
        if (decompressor.ReadNextPoint(point)) {
            std::cout << "Point " << i << ": lon=" << point.longitude 
                      << ", lat=" << point.latitude 
                      << ", ts=" << point.timestamp << "\n";
        } else {
            std::cout << "Point " << i << ": ReadNextPoint failed\n";
            break;
        }
    }
    
    return 0;
}
