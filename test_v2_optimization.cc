#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor_zero_opt.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt.h"
#include "src/compressor/serf_xor_compressor_zero_opt_v2.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt_v2.h"

void test_v2_optimization() {
    std::cout << "Testing V2 Zero Sequence Optimization (Leading Space Encoding)..." << std::endl;
    
    // Test parameters
    int window_size = 1000;
    double max_diff = 0.1;
    long adjust_digit = 10;
    
    // Test data with many repeated values
    std::vector<double> test_data = {
        42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, // 10 identical
        43.0, // Different value
        50.0, 50.0, 50.0, 50.0, 50.0, // 5 more identical
        51.0  // Different value
    };
    
    std::cout << "Test data: " << test_data.size() << " values with repeated sequences" << std::endl;
    
    // Test V1 (original zero optimization with 2-bit flag)
    SerfXORCompressorZeroOpt compressor_v1(window_size, max_diff, adjust_digit);
    SerfXORDecompressorZeroOpt decompressor_v1(adjust_digit);
    
    for (double value : test_data) {
        compressor_v1.AddValue(value);
    }
    compressor_v1.Close();
    
    Array<uint8_t> compressed_v1 = compressor_v1.compressed_bytes_last_block();
    long size_v1 = compressor_v1.compressed_size_last_block();
    std::vector<double> decompressed_v1 = decompressor_v1.Decompress(compressed_v1);
    
    // Test V2 (leading space encoding)
    SerfXORCompressorZeroOptV2 compressor_v2(window_size, max_diff, adjust_digit);
    SerfXORDecompressorZeroOptV2 decompressor_v2(adjust_digit);
    
    for (double value : test_data) {
        compressor_v2.AddValue(value);
    }
    compressor_v2.Close();
    
    Array<uint8_t> compressed_v2 = compressor_v2.compressed_bytes_last_block();
    long size_v2 = compressor_v2.compressed_size_last_block();
    std::vector<double> decompressed_v2 = decompressor_v2.Decompress(compressed_v2);
    
    // Print results
    std::cout << "\n=== V2 Optimization Comparison ===" << std::endl;
    std::cout << "V1 (2-bit flag):     " << size_v1 << " bits" << std::endl;
    std::cout << "V2 (leading space):  " << size_v2 << " bits" << std::endl;
    std::cout << "Space saving:        " << (size_v1 - size_v2) << " bits" << std::endl;
    std::cout << "Improvement:         " << (double)(size_v1 - size_v2) / size_v1 * 100 << "%" << std::endl;
    
    // Verify correctness
    std::cout << "\n=== Correctness Verification ===" << std::endl;
    std::cout << "Original data size:  " << test_data.size() << std::endl;
    std::cout << "V1 decompressed:     " << decompressed_v1.size() << std::endl;
    std::cout << "V2 decompressed:     " << decompressed_v2.size() << std::endl;
    
    bool v1_correct = true, v2_correct = true;
    
    if (decompressed_v1.size() != test_data.size()) {
        std::cout << "❌ V1 size mismatch!" << std::endl;
        v1_correct = false;
    }
    
    if (decompressed_v2.size() != test_data.size()) {
        std::cout << "❌ V2 size mismatch!" << std::endl;
        v2_correct = false;
    }
    
    for (size_t i = 0; i < test_data.size() && v1_correct && v2_correct; i++) {
        if (std::abs(decompressed_v1[i] - test_data[i]) > 1e-6) {
            std::cout << "❌ V1 value mismatch at index " << i << std::endl;
            v1_correct = false;
        }
        if (std::abs(decompressed_v2[i] - test_data[i]) > 1e-6) {
            std::cout << "❌ V2 value mismatch at index " << i << std::endl;
            v2_correct = false;
        }
    }
    
    if (v1_correct) std::cout << "✅ V1 correctness verified" << std::endl;
    if (v2_correct) std::cout << "✅ V2 correctness verified" << std::endl;
    
    if (v1_correct && v2_correct && size_v2 < size_v1) {
        std::cout << "\n🎉 V2 optimization successful!" << std::endl;
        std::cout << "Space-efficient encoding achieved better compression!" << std::endl;
    } else if (v1_correct && v2_correct && size_v2 == size_v1) {
        std::cout << "\n✅ V2 optimization equivalent" << std::endl;
        std::cout << "Same compression with potentially cleaner encoding" << std::endl;
    }
}

int main() {
    try {
        test_v2_optimization();
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
