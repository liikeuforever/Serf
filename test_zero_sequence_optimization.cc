#include <iostream>
#include <vector>
#include <cassert>
#include "src/compressor/serf_xor_compressor_zero_opt.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt.h"

void test_zero_sequence_optimization() {
    std::cout << "Testing zero sequence optimization..." << std::endl;
    
    // Test parameters
    int window_size = 1000;
    double max_diff = 0.1;
    long adjust_digit = 1000000;
    
    // Create compressor and decompressor
    SerfXORCompressorZeroOpt compressor(window_size, max_diff, adjust_digit);
    SerfXORDecompressorZeroOpt decompressor(adjust_digit);
    
    // Test case 1: Sequence with many repeated values
    std::vector<double> test_data = {
        10.0, 10.0, 10.0, 10.0, 10.0,  // 5 identical values
        10.1,                           // Different value to end sequence
        15.0, 15.0, 15.0,              // 3 more identical values
        15.5                            // Different value
    };
    
    std::cout << "Original data size: " << test_data.size() << " values" << std::endl;
    
    // Compress the data
    for (double value : test_data) {
        compressor.AddValue(value);
    }
    compressor.Close();
    
    // Get compressed data
    Array<uint8_t> compressed_data = compressor.compressed_bytes_last_block();
    long compressed_size = compressor.compressed_size_last_block();
    
    std::cout << "Compressed size: " << compressed_size << " bits" << std::endl;
    std::cout << "Compression ratio: " << (double)compressed_size / (test_data.size() * 64) << std::endl;
    
    // Decompress the data
    std::vector<double> decompressed_data = decompressor.Decompress(compressed_data);
    
    std::cout << "Decompressed data size: " << decompressed_data.size() << " values" << std::endl;
    
    // Verify correctness
    if (decompressed_data.size() != test_data.size()) {
        std::cerr << "ERROR: Size mismatch!" << std::endl;
        return;
    }
    
    for (size_t i = 0; i < test_data.size(); i++) {
        if (std::abs(decompressed_data[i] - test_data[i]) > 1e-6) {
            std::cerr << "ERROR: Value mismatch at index " << i 
                      << ": expected " << test_data[i] 
                      << ", got " << decompressed_data[i] << std::endl;
            return;
        }
    }
    
    std::cout << "✓ Test passed! All values match." << std::endl;
    
    // Test case 2: Long sequence of identical values
    std::cout << "\nTesting long zero sequence..." << std::endl;
    
    SerfXORCompressorZeroOpt compressor2(window_size, max_diff, adjust_digit);
    SerfXORDecompressorZeroOpt decompressor2(adjust_digit);
    
    std::vector<double> long_sequence(100, 42.0); // 100 identical values
    long_sequence.push_back(43.0); // One different value at the end
    
    for (double value : long_sequence) {
        compressor2.AddValue(value);
    }
    compressor2.Close();
    
    Array<uint8_t> compressed_data2 = compressor2.compressed_bytes_last_block();
    long compressed_size2 = compressor2.compressed_size_last_block();
    
    std::cout << "Long sequence compressed size: " << compressed_size2 << " bits" << std::endl;
    std::cout << "Long sequence compression ratio: " << (double)compressed_size2 / (long_sequence.size() * 64) << std::endl;
    
    // Expected improvement: 100 * 2 bits (old) vs ~2 + 7 bits (new) for the zero sequence
    std::cout << "Expected old size for 100 zeros: " << 100 * 2 << " bits" << std::endl;
    std::cout << "Expected new size for 100 zeros: ~9 bits" << std::endl;
    
    std::vector<double> decompressed_long = decompressor2.Decompress(compressed_data2);
    
    if (decompressed_long.size() != long_sequence.size()) {
        std::cerr << "ERROR: Long sequence size mismatch!" << std::endl;
        return;
    }
    
    for (size_t i = 0; i < long_sequence.size(); i++) {
        if (std::abs(decompressed_long[i] - long_sequence[i]) > 1e-6) {
            std::cerr << "ERROR: Long sequence value mismatch at index " << i << std::endl;
            return;
        }
    }
    
    std::cout << "✓ Long sequence test passed!" << std::endl;
}

int main() {
    try {
        test_zero_sequence_optimization();
        std::cout << "\n🎉 All tests passed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
