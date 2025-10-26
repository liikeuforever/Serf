#include <iostream>
#include <vector>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.4, 40.0, 1001});
    data.push_back({116.5, 40.1, 1002});
    
    std::cout << "Compressing " << data.size() << " points\n";
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    for (const auto& p : data) {
        compressor.AddGpsPoint(p);
    }
    compressor.Close();
    
    auto compressed = compressor.GetCompressedData();
    auto stats = compressor.GetStats();
    std::cout << "Compressed: " << stats.total_bits << " bits\n";
    std::cout << "  Spatial: " << (stats.total_bits - stats.timestamp_bits) << " bits\n";
    std::cout << "  Timestamp: " << stats.timestamp_bits << " bits\n";
    
    std::cout << "Decompressing...\n";
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    
    int count = 0;
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    while (decompressor.ReadNextPoint(point) && count < data.size()) {
        std::cout << "Point " << count << ": (" << point.longitude << ", " << point.latitude << ", ts=" << point.timestamp << ")\n";
        count++;
    }
    
    std::cout << "Decompressed " << count << " points\n";
    return 0;
}
