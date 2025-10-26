#include <iostream>
#include <vector>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.4, 40.0, 1001});
    
    std::cout << "Compressing " << data.size() << " points\n";
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    for (size_t i = 0; i < data.size(); i++) {
        std::cout << "Adding point " << i << ": (" << data[i].longitude << ", " << data[i].latitude << ", ts=" << data[i].timestamp << ")\n";
        compressor.AddGpsPoint(data[i]);
        std::cout << "  Added successfully\n";
    }
    compressor.Close();
    
    std::cout << "Compression complete\n";
    return 0;
}
