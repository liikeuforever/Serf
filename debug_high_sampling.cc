#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor.h"
#include "src/decompressor/serf_xor_decompressor.h"
#include "src/compressor/serf_xor_compressor_optimized.h"
#include "src/decompressor/serf_xor_decompressor_optimized.h"

int main() {
    std::cout << "Debug: Testing high sampling rate data..." << std::endl;
    
    // High sampling rate data with many consecutive identical values
    std::vector<double> test_data;
    
    // Add 100 identical values (should trigger zero-run optimization)
    for (int i = 0; i < 100; i++) {
        test_data.push_back(1.0);
    }
    
    // Add 50 more identical values of different value
    for (int i = 0; i < 50; i++) {
        test_data.push_back(2.0);
    }
    
    // Add some variation
    test_data.push_back(3.0);
    test_data.push_back(4.0);
    
    double max_diff = 0.1;
    long adjust_digit = 0;
    
    std::cout << "Test data: 100x(1.0) + 50x(2.0) + 3.0 + 4.0 = " << test_data.size() << " values" << std::endl;
    
    try {
        // Test Original
        std::cout << "\n--- Testing Original Serf-XOR ---" << std::endl;
        SerfXORCompressor orig_comp(1000, max_diff, adjust_digit);
        for (double v : test_data) {
            orig_comp.AddValue(v);
        }
        orig_comp.Close();
        
        auto orig_compressed = orig_comp.compressed_bytes_last_block();
        long orig_size = orig_comp.compressed_size_last_block();
        
        std::cout << "Original compressed size: " << orig_size << " bits" << std::endl;
        std::cout << "Original compression ratio: " << (double)(test_data.size() * 64) / orig_size << ":1" << std::endl;
        
        SerfXORDecompressor orig_decomp(adjust_digit);
        auto orig_decompressed = orig_decomp.Decompress(orig_compressed);
        
        std::cout << "Original decompressed count: " << orig_decompressed.size() << std::endl;
        
        // Test Optimized
        std::cout << "\n--- Testing Optimized Serf-XOR ---" << std::endl;
        SerfXORCompressorOptimized opt_comp(1000, max_diff, adjust_digit, 0.1);
        for (double v : test_data) {
            opt_comp.AddValue(v);
        }
        opt_comp.Close();
        
        auto opt_compressed = opt_comp.compressed_bytes_last_block();
        long opt_size = opt_comp.compressed_size_last_block();
        
        std::cout << "Optimized compressed size: " << opt_size << " bits" << std::endl;
        std::cout << "Optimized compression ratio: " << (double)(test_data.size() * 64) / opt_size << ":1" << std::endl;
        
        SerfXORDecompressorOptimized opt_decomp(adjust_digit, 0.1);
        auto opt_decompressed = opt_decomp.Decompress(opt_compressed);
        
        std::cout << "Optimized decompressed count: " << opt_decompressed.size() << std::endl;
        
        // Compare results
        std::cout << "\n--- Comparison ---" << std::endl;
        std::cout << "Size improvement: " << (double)orig_size / opt_size << "x" << std::endl;
        
        if (orig_decompressed.size() == opt_decompressed.size() && 
            orig_decompressed.size() == test_data.size()) {
            
            std::cout << "All sizes match!" << std::endl;
            
            bool match = true;
            for (size_t i = 0; i < test_data.size(); ++i) {
                double orig_error = std::abs(test_data[i] - orig_decompressed[i]);
                double opt_error = std::abs(test_data[i] - opt_decompressed[i]);
                
                if (orig_error > max_diff || opt_error > max_diff) {
                    std::cout << "Error at index " << i << ": orig=" << orig_error 
                              << ", opt=" << opt_error << std::endl;
                    match = false;
                }
            }
            
            if (match) {
                std::cout << "All values within error bounds!" << std::endl;
            }
        } else {
            std::cout << "Size mismatch: original=" << orig_decompressed.size() 
                      << ", optimized=" << opt_decompressed.size() 
                      << ", expected=" << test_data.size() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
