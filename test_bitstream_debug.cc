#include <iostream>
#include <vector>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.4, 40.0, 1001});
    
    std::cout << "=== COMPRESSION ===\n";
    std::cout << "Input points:\n";
    for (size_t i = 0; i < data.size(); i++) {
        std::cout << "  Point " << i << ": lon=" << data[i].longitude 
                  << ", lat=" << data[i].latitude 
                  << ", ts=" << data[i].timestamp << "\n";
    }
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    for (const auto& p : data) {
        compressor.AddGpsPoint(p);
    }
    compressor.Close();
    
    auto compressed = compressor.GetCompressedData();
    auto stats = compressor.GetStats();
    
    std::cout << "\nCompression stats:\n";
    std::cout << "  Total bits: " << stats.total_bits << "\n";
    std::cout << "  Timestamp bits: " << stats.timestamp_bits << "\n";
    std::cout << "  Spatial bits: " << (stats.total_bits - stats.timestamp_bits) << "\n";
    std::cout << "  Compressed data size: " << compressed.length() << " bytes\n";
    
    std::cout << "\nFirst 20 bytes of compressed data (hex):\n  ";
    for (int i = 0; i < std::min(20, (int)compressed.length()); i++) {
        printf("%02x ", compressed[i]);
    }
    std::cout << "\n";
    
    std::cout << "\n=== DECOMPRESSION ===\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    std::cout << "Starting decompression...\n";
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    int count = 0;
    int max_iterations = 10;  // 防止无限循环
    
    while (count < max_iterations) {
        std::cout << "  Iteration " << count << ": Calling ReadNextPoint...\n";
        
        bool success = decompressor.ReadNextPoint(point);
        
        if (!success) {
            std::cout << "    ReadNextPoint returned false (EOF or error)\n";
            break;
        }
        
        std::cout << "    Success! Point " << count << ": lon=" << point.longitude 
                  << ", lat=" << point.latitude 
                  << ", ts=" << point.timestamp << "\n";
        
        count++;
        
        if (count >= (int)data.size()) {
            std::cout << "  Decompressed expected number of points, stopping.\n";
            break;
        }
    }
    
    std::cout << "\nDecompressed " << count << " points (expected " << data.size() << ")\n";
    
    if (count == (int)data.size()) {
        std::cout << "✓ SUCCESS\n";
        return 0;
    } else {
        std::cout << "✗ FAILED\n";
        return 1;
    }
}
