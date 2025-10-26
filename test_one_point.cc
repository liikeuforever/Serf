#include <iostream>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    TrajCompressSPAdaptiveSimpleCompressor::GpsPoint p0{116.3, 39.9, 1000};
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(1, 1e-5, 96);
    compressor.AddGpsPoint(p0);
    compressor.Close();
    
    auto stats = compressor.GetStats();
    std::cout << "After 1 point:\n";
    std::cout << "  total_bits: " << stats.total_bits << "\n";
    std::cout << "  Is byte-aligned: " << ((stats.total_bits % 8 == 0) ? "YES" : "NO") << "\n";
    std::cout << "  Total bytes: " << (stats.total_bits / 8) << " + " << (stats.total_bits % 8) << " bits\n";
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "  Compressed data length: " << compressed.length() << " bytes\n";
    
    return 0;
}
