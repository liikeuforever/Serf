#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

struct SimpleGpsPoint {
    double longitude;
    double latitude;
    uint64_t timestamp;
};

int main() {
    std::vector<SimpleGpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.4, 40.0, 1001});
    data.push_back({116.5, 40.1, 1002});
    
    std::cout << "Testing with " << data.size() << " points\n";
    std::cout << "Timestamps: " << data[0].timestamp << ", " << data[1].timestamp << ", " << data[2].timestamp << "\n";
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    for (size_t i = 0; i < data.size(); i++) {
        const auto& p = data[i];
        std::cout << "Adding point " << i << ": (" << p.longitude << ", " << p.latitude << ", ts=" << p.timestamp << ")\n";
        compressor.AddGpsPoint(TrajCompressSPAdaptiveSimpleCompressor::GpsPoint(p.longitude, p.latitude, p.timestamp));
    }
    compressor.Close();
    
    std::cout << "Compression complete\n";
    
    auto compressed = compressor.GetCompressedData();
    auto stats = compressor.GetStats();
    std::cout << "Total bits: " << stats.total_bits << "\n";
    std::cout << "Timestamp bits: " << stats.timestamp_bits << "\n";
    std::cout << "Spatial bits: " << (stats.total_bits - stats.timestamp_bits) << "\n";
    
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(compressed.begin(), compressed.length());
    std::cout << "Starting decompression...\n";
    
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint point;
    int count_decompressed = 0;
    while (decompressor.ReadNextPoint(point) && count_decompressed < data.size()) {
        std::cout << "Decompressed point " << count_decompressed << ": (" << point.longitude << ", " << point.latitude << ", ts=" << point.timestamp << ")\n";
        count_decompressed++;
    }
    
    std::cout << "Decompressed " << count_decompressed << " points (expected " << data.size() << ")\n";
    
    return 0;
}
