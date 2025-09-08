#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor_zero_opt.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt.h"
#include "src/compressor/serf_xor_compressor.h"
#include "src/decompressor/serf_xor_decompressor.h"

void test_with_params(double max_diff, long adjust_digit, const std::vector<double>& data) {
    std::cout << "\n=== Testing with max_diff=" << max_diff << ", adjust_digit=" << adjust_digit << " ===" << std::endl;
    
    int window_size = 1000;
    
    // Test original compressor
    SerfXORCompressor orig_compressor(window_size, max_diff, adjust_digit);
    SerfXORDecompressor orig_decompressor(adjust_digit);
    
    for (double value : data) {
        orig_compressor.AddValue(value);
    }
    orig_compressor.Close();
    
    Array<uint8_t> orig_compressed = orig_compressor.compressed_bytes_last_block();
    long orig_size = orig_compressor.compressed_size_last_block();
    std::vector<double> orig_decompressed = orig_decompressor.Decompress(orig_compressed);
    
    // Test optimized compressor
    SerfXORCompressorZeroOpt opt_compressor(window_size, max_diff, adjust_digit);
    SerfXORDecompressorZeroOpt opt_decompressor(adjust_digit);
    
    for (double value : data) {
        opt_compressor.AddValue(value);
    }
    opt_compressor.Close();
    
    Array<uint8_t> opt_compressed = opt_compressor.compressed_bytes_last_block();
    long opt_size = opt_compressor.compressed_size_last_block();
    std::vector<double> opt_decompressed = opt_decompressor.Decompress(opt_compressed);
    
    // Print results
    std::cout << "Input:      ";
    for (double v : data) std::cout << v << " ";
    std::cout << std::endl;
    
    std::cout << "Original:   ";
    for (double v : orig_decompressed) std::cout << v << " ";
    std::cout << " (" << orig_size << " bits)" << std::endl;
    
    std::cout << "Optimized:  ";
    for (double v : opt_decompressed) std::cout << v << " ";
    std::cout << " (" << opt_size << " bits)" << std::endl;
    
    if (opt_size < orig_size) {
        std::cout << "✓ Optimization successful! Saved " << (orig_size - opt_size) << " bits" << std::endl;
    } else if (opt_size == orig_size) {
        std::cout << "= Same size (no optimization needed)" << std::endl;
    } else {
        std::cout << "✗ Optimization failed (increased size)" << std::endl;
    }
}

int main() {
    std::cout << "Testing zero sequence optimization with proper parameters..." << std::endl;
    
    // Test 1: Small adjust_digit, many identical values
    std::vector<double> test1 = {1.0, 1.0, 1.0, 1.0, 1.0, 2.0};
    test_with_params(0.1, 10, test1);
    
    // Test 2: Long sequence of identical values
    std::vector<double> test2(100, 42.0);
    test2.push_back(43.0);
    test_with_params(0.1, 10, test2);
    
    // Test 3: No identical values (should not benefit from optimization)
    std::vector<double> test3 = {1.0, 2.0, 3.0, 4.0, 5.0};
    test_with_params(0.1, 10, test3);
    
    return 0;
}
