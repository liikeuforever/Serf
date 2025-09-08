#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor.h"
#include "src/decompressor/serf_xor_decompressor.h"
#include "src/compressor/serf_xor_compressor_optimized.h"
#include "src/decompressor/serf_xor_decompressor_optimized.h"

int main() {
    std::cout << "Debug: Testing simple data with both algorithms..." << std::endl;
    
    // Very simple test data with many repeated values
    std::vector<double> test_data = {1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0, 3.0, 3.0};
    
    double max_diff = 0.1;
    long adjust_digit = 0;
    
    std::cout << "Test data: ";
    for (double v : test_data) std::cout << v << " ";
    std::cout << std::endl;
    
    try {
        // Test Original
        std::cout << "\n--- Testing Original Serf-XOR ---" << std::endl;
        SerfXORCompressor orig_comp(100, max_diff, adjust_digit);
        for (double v : test_data) {
            orig_comp.AddValue(v);
        }
        orig_comp.Close();
        
        auto orig_compressed = orig_comp.compressed_bytes_last_block();
        long orig_size = orig_comp.compressed_size_last_block();
        
        std::cout << "Original compressed size: " << orig_size << " bits" << std::endl;
        
        SerfXORDecompressor orig_decomp(adjust_digit);
        auto orig_decompressed = orig_decomp.Decompress(orig_compressed);
        
        std::cout << "Original decompressed: ";
        for (double v : orig_decompressed) std::cout << v << " ";
        std::cout << std::endl;
        
        // Test Optimized
        std::cout << "\n--- Testing Optimized Serf-XOR ---" << std::endl;
        SerfXORCompressorOptimized opt_comp(100, max_diff, adjust_digit, 0.1);
        for (double v : test_data) {
            opt_comp.AddValue(v);
        }
        opt_comp.Close();
        
        auto opt_compressed = opt_comp.compressed_bytes_last_block();
        long opt_size = opt_comp.compressed_size_last_block();
        
        std::cout << "Optimized compressed size: " << opt_size << " bits" << std::endl;
        
        SerfXORDecompressorOptimized opt_decomp(adjust_digit, 0.1);
        auto opt_decompressed = opt_decomp.Decompress(opt_compressed);
        
        std::cout << "Optimized decompressed: ";
        for (double v : opt_decompressed) std::cout << v << " ";
        std::cout << std::endl;
        
        // Compare results
        std::cout << "\n--- Comparison ---" << std::endl;
        std::cout << "Size ratio: " << (double)opt_size / orig_size << std::endl;
        
        if (orig_decompressed.size() == opt_decompressed.size()) {
            bool match = true;
            for (size_t i = 0; i < test_data.size(); ++i) {
                double error = std::abs(orig_decompressed[i] - opt_decompressed[i]);
                if (error > 1e-10) {
                    std::cout << "Difference at index " << i << ": " << error << std::endl;
                    match = false;
                }
            }
            if (match) {
                std::cout << "Results match perfectly!" << std::endl;
            }
        } else {
            std::cout << "Size mismatch: " << orig_decompressed.size() << " vs " << opt_decompressed.size() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
