#include <iostream>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    for (int i = 0; i < 100; i++) {
        data.push_back({116.3 + i * 0.001, 39.9 + i * 0.001, static_cast<uint64_t>(1000 + i)});
    }
    
    std::cout << "Compressing " << data.size() << " points...\n";
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    
    for (const auto& p : data) {
        compressor.AddGpsPoint(p);
    }
    compressor.Close();
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "Compressed to " << compressed.length() << " bytes\n";
    
    std::cout << "Decompressing...\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    int count = 0;
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    while (decompressor.ReadNextPoint(point)) {
        count++;
        if (count <= 3 || count > 97) {
            std::cout << "Point " << count << ": lon=" << point.longitude 
                      << ", lat=" << point.latitude 
                      << ", ts=" << point.timestamp << "\n";
        }
    }
    std::cout << "Decompressed " << count << " points\n";
    
    if (count == data.size()) {
        std::cout << "✓ SUCCESS\n";
        return 0;
    } else {
        std::cout << "✗ FAILED: Expected " << data.size() << " points, got " << count << "\n";
        return 1;
    }
}
