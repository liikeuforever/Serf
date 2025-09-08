#include <iostream>
#include <vector>
#include <cmath>
#include "src/compressor/serf_xor_compressor.h"
#include "src/decompressor/serf_xor_decompressor.h"

int main() {
    std::cout << "Testing approximation logic..." << std::endl;
    
    double max_diff = 0.1;
    long adjust_digit = 1000000;
    
    double v1 = 10.0;
    double v2 = 10.1;
    
    std::cout << "v1 = " << v1 << ", v2 = " << v2 << std::endl;
    std::cout << "max_diff = " << max_diff << std::endl;
    std::cout << "adjust_digit = " << adjust_digit << std::endl;
    std::cout << "abs(v1 - adjust_digit - v2) = " << std::abs(v1 - adjust_digit - v2) << std::endl;
    std::cout << "Is within max_diff? " << (std::abs(v1 - adjust_digit - v2) <= max_diff) << std::endl;
    
    // Let's try with a larger difference
    std::cout << "\nTesting with larger difference..." << std::endl;
    
    int window_size = 1000;
    SerfXORCompressor compressor(window_size, max_diff, adjust_digit);
    SerfXORDecompressor decompressor(adjust_digit);
    
    std::vector<double> test_data = {10.0, 10.0, 10.0, 10.5}; // 0.5 difference
    
    std::cout << "Input data: ";
    for (double v : test_data) {
        std::cout << v << " ";
    }
    std::cout << std::endl;
    
    // Compress
    for (double value : test_data) {
        compressor.AddValue(value);
    }
    compressor.Close();
    
    Array<uint8_t> compressed_data = compressor.compressed_bytes_last_block();
    long compressed_size = compressor.compressed_size_last_block();
    
    std::cout << "Compressed size: " << compressed_size << " bits" << std::endl;
    
    // Decompress
    std::vector<double> decompressed_data = decompressor.Decompress(compressed_data);
    
    std::cout << "Output data: ";
    for (double v : decompressed_data) {
        std::cout << v << " ";
    }
    std::cout << std::endl;
    
    return 0;
}
