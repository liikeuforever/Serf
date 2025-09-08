#include <iostream>
#include <vector>
#include <cmath>

#include "src/compressor/serf_xor_compressor_optimized.h"
#include "src/decompressor/serf_xor_decompressor_optimized.h"

int main() {
    std::cout << "Testing optimized Serf-XOR with simple data..." << std::endl;
    
    // Simple test data
    std::vector<double> test_data = {1.0, 2.0, 3.0, 4.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0};
    
    // Parameters
    int window_size = 100;
    double max_diff = 0.1;
    long adjust_digit = 0;
    double alpha = 0.1;
    
    try {
        // Test compression
        SerfXORCompressorOptimized compressor(window_size, max_diff, adjust_digit, alpha);
        
        for (double value : test_data) {
            compressor.AddValue(value);
        }
        compressor.Close();
        
        Array<uint8_t> compressed = compressor.compressed_bytes_last_block();
        long compressed_size = compressor.compressed_size_last_block();
        
        std::cout << "Original data size: " << test_data.size() << " values" << std::endl;
        std::cout << "Compressed size: " << compressed_size << " bits" << std::endl;
        std::cout << "Compressed bytes: " << compressed.length() << " bytes" << std::endl;
        
        // Test decompression
        SerfXORDecompressorOptimized decompressor(adjust_digit, alpha);
        std::vector<double> decompressed = decompressor.Decompress(compressed);
        
        std::cout << "Decompressed size: " << decompressed.size() << " values" << std::endl;
        
        // Verify correctness
        bool success = true;
        if (test_data.size() != decompressed.size()) {
            std::cout << "ERROR: Size mismatch!" << std::endl;
            success = false;
        } else {
            for (size_t i = 0; i < test_data.size(); ++i) {
                double error = std::abs(test_data[i] - decompressed[i]);
                if (error > max_diff) {
                    std::cout << "ERROR at index " << i << ": " << test_data[i] 
                              << " vs " << decompressed[i] << " (error: " << error << ")" << std::endl;
                    success = false;
                }
            }
        }
        
        if (success) {
            std::cout << "SUCCESS: All tests passed!" << std::endl;
            return 0;
        } else {
            std::cout << "FAILED: Some tests failed!" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "UNKNOWN EXCEPTION occurred!" << std::endl;
        return 1;
    }
}
