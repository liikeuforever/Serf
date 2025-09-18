#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor.h"
#include "src/decompressor/serf_xor_decompressor.h"
#include "src/compressor/serf_xor_compressor_zero_opt.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt.h"
#include "src/compressor/serf_xor_compressor_zero_opt_v2.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt_v2.h"

void test_scenario(const std::string& name, const std::vector<double>& data, 
                   double max_diff, long adjust_digit) {
    std::cout << "\n=== Testing " << name << " ===" << std::endl;
    std::cout << "Data size: " << data.size() << " values" << std::endl;
    
    // Test Original
    SerfXORCompressor original_compressor(1000, max_diff, adjust_digit);
    SerfXORDecompressor original_decompressor(adjust_digit);
    
    for (double v : data) original_compressor.AddValue(v);
    original_compressor.Close();
    
    Array<uint8_t> original_compressed = original_compressor.compressed_bytes_last_block();
    long original_size = original_compressor.compressed_size_last_block();
    std::vector<double> original_decompressed = original_decompressor.Decompress(original_compressed);
    
    // Test V1 (2-bit flag)
    SerfXORCompressorZeroOpt v1_compressor(1000, max_diff, adjust_digit);
    SerfXORDecompressorZeroOpt v1_decompressor(adjust_digit);
    
    for (double v : data) v1_compressor.AddValue(v);
    v1_compressor.Close();
    
    Array<uint8_t> v1_compressed = v1_compressor.compressed_bytes_last_block();
    long v1_size = v1_compressor.compressed_size_last_block();
    std::vector<double> v1_decompressed = v1_decompressor.Decompress(v1_compressed);
    
    // Test V2 (leading space)
    SerfXORCompressorZeroOptV2 v2_compressor(1000, max_diff, adjust_digit);
    SerfXORDecompressorZeroOptV2 v2_decompressor(adjust_digit);
    
    for (double v : data) v2_compressor.AddValue(v);
    v2_compressor.Close();
    
    Array<uint8_t> v2_compressed = v2_compressor.compressed_bytes_last_block();
    long v2_size = v2_compressor.compressed_size_last_block();
    std::vector<double> v2_decompressed = v2_decompressor.Decompress(v2_compressed);
    
    // Print results
    std::cout << "Original:   " << original_size << " bits" << std::endl;
    std::cout << "V1 (flag):  " << v1_size << " bits (" 
              << (v1_size < original_size ? "+" : "") 
              << (double)(v1_size - original_size) / original_size * 100 << "%)" << std::endl;
    std::cout << "V2 (space): " << v2_size << " bits (" 
              << (v2_size < original_size ? "+" : "") 
              << (double)(v2_size - original_size) / original_size * 100 << "%)" << std::endl;
    
    if (v2_size < v1_size) {
        std::cout << "✅ V2 better than V1: " << (v1_size - v2_size) << " bits saved" << std::endl;
    } else if (v2_size == v1_size) {
        std::cout << "= V2 equal to V1" << std::endl;
    } else {
        std::cout << "❌ V2 worse than V1: " << (v2_size - v1_size) << " bits overhead" << std::endl;
    }
    
    // Verify correctness
    bool v1_correct = (v1_decompressed.size() == data.size());
    bool v2_correct = (v2_decompressed.size() == data.size());
    
    for (size_t i = 0; i < data.size() && v1_correct && v2_correct; i++) {
        if (std::abs(v1_decompressed[i] - data[i]) > max_diff + 1e-6) v1_correct = false;
        if (std::abs(v2_decompressed[i] - data[i]) > max_diff + 1e-6) v2_correct = false;
    }
    
    std::cout << "V1 correctness: " << (v1_correct ? "✅" : "❌") << std::endl;
    std::cout << "V2 correctness: " << (v2_correct ? "✅" : "❌") << std::endl;
}

int main() {
    double max_diff = 0.1;
    long adjust_digit = 10;
    
    std::cout << "Testing Zero Sequence Optimization V2 (Leading Space Encoding)" << std::endl;
    std::cout << "Parameters: max_diff=" << max_diff << ", adjust_digit=" << adjust_digit << std::endl;
    
    // Test Case 1: Many repeated values (should benefit from optimization)
    std::vector<double> many_repeats = {
        10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0,  // 10 identical
        20.0, 20.0, 20.0, 20.0, 20.0,  // 5 more identical
        30.0  // Different
    };
    test_scenario("Many Repeated Values", many_repeats, max_diff, adjust_digit);
    
    // Test Case 2: Few repeated values (should have minimal overhead)
    std::vector<double> few_repeats = {
        1.0, 2.0, 3.0, 4.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0  // Only one repeat
    };
    test_scenario("Few Repeated Values", few_repeats, max_diff, adjust_digit);
    
    // Test Case 3: No repeated values (should have no overhead)
    std::vector<double> no_repeats = {
        1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0  // No repeats
    };
    test_scenario("No Repeated Values", no_repeats, max_diff, adjust_digit);
    
    // Test Case 4: Single long sequence (best case for optimization)
    std::vector<double> long_sequence(50, 42.0);  // 50 identical values
    long_sequence.push_back(43.0);  // One different
    test_scenario("Long Sequence", long_sequence, max_diff, adjust_digit);
    
    return 0;
}
