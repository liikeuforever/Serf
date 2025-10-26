#include <iostream>
#include <iomanip>
#include <vector>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

uint64_t ReadUint64At(const Array<uint8_t>& data, int byte_offset) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= (static_cast<uint64_t>(data[byte_offset + i]) << (i * 8));
    }
    return result;
}

int main() {
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.4, 40.0, 1001});
    
    TrajCompressSPAdaptiveSimpleCompressor compressor(data.size(), 1e-5, 96);
    for (const auto& p : data) {
        compressor.AddGpsPoint(p);
    }
    compressor.Close();
    
    auto compressed = compressor.GetCompressedData();
    
    std::cout << "Compressed data: " << compressed.length() << " bytes\n\n";
    
    std::cout << "Byte-by-byte analysis:\n";
    std::cout << "Bytes 0-1 (block_size): ";
    for (int i = 0; i < 2; i++) printf("%02x ", compressed[i]);
    std::cout << "\n";
    
    std::cout << "Bytes 2-9 (epsilon): ";
    for (int i = 2; i < 10; i++) printf("%02x ", compressed[i]);
    std::cout << "\n";
    
    std::cout << "Bytes 10-11 (eval_window): ";
    for (int i = 10; i < 12; i++) printf("%02x ", compressed[i]);
    std::cout << "\n";
    
    std::cout << "Bytes 12-19 (point0 lon): ";
    for (int i = 12; i < 20; i++) printf("%02x ", compressed[i]);
    std::cout << "\n";
    
    std::cout << "Bytes 20-27 (point0 lat): ";
    for (int i = 20; i < 28; i++) printf("%02x ", compressed[i]);
    std::cout << "\n";
    
    std::cout << "Bytes 28-35 (point0 ts): ";
    for (int i = 28; i < 36; i++) printf("%02x ", compressed[i]);
    uint64_t ts0 = ReadUint64At(compressed, 28);
    std::cout << " = " << ts0 << "\n";
    
    std::cout << "Bytes 36-43 (point1 ts_delta): ";
    for (int i = 36; i < 44; i++) printf("%02x ", compressed[i]);
    uint64_t ts_delta = ReadUint64At(compressed, 36);
    std::cout << " = " << ts_delta << "\n";
    
    std::cout << "\nExpected timestamp delta: 1\n";
    std::cout << "Actual timestamp delta: " << ts_delta << "\n";
    
    if (ts_delta == 1) {
        std::cout << "✓ Timestamp delta is correct in compressed data!\n";
    } else {
        std::cout << "✗ Timestamp delta is WRONG in compressed data!\n";
    }
    
    return 0;
}
