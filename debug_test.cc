#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor_zero_opt.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt.h"

int main() {
    std::cout << "Debug test for zero sequence optimization..." << std::endl;
    
    // Simple test with just a few values
    int window_size = 1000;
    double max_diff = 0.1;
    long adjust_digit = 1000000;
    
    SerfXORCompressorZeroOpt compressor(window_size, max_diff, adjust_digit);
    SerfXORDecompressorZeroOpt decompressor(adjust_digit);
    
    std::vector<double> test_data = {10.0, 10.0, 10.0, 10.1};
    
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
