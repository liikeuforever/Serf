#include <iostream>
#include <vector>
#include <chrono>
#include "src/compressor/serf_xor_compressor.h"
#include "src/decompressor/serf_xor_decompressor.h"
#include "src/compressor/serf_xor_compressor_optimized.h"
#include "src/decompressor/serf_xor_decompressor_optimized.h"

int main() {
    std::cout << "Debug: Testing deterministic high sampling rate data..." << std::endl;
    
    // Create deterministic high sampling rate data similar to the test
    std::vector<double> sensor_data;
    
    double current_value = 100.0;  // Start value
    sensor_data.push_back(current_value);
    
    // Generate 5000 values with 80% probability of staying the same
    for (int i = 1; i < 5000; ++i) {
        if (i % 5 < 4) {  // 80% chance of staying the same (deterministic)
            sensor_data.push_back(current_value);
        } else {
            current_value += 0.5;  // Small deterministic change
            sensor_data.push_back(current_value);
        }
    }
    
    double max_diff = 0.1;
    long adjust_digit = 100;  // Same as test
    
    std::cout << "Generated " << sensor_data.size() << " sensor readings" << std::endl;
    std::cout << "First 10 values: ";
    for (int i = 0; i < 10; i++) std::cout << sensor_data[i] << " ";
    std::cout << std::endl;
    
    try {
        // Test Original
        auto start_time = std::chrono::high_resolution_clock::now();
        SerfXORCompressor orig_comp(1000, max_diff, adjust_digit);
        for (double v : sensor_data) {
            orig_comp.AddValue(v);
        }
        orig_comp.Close();
        auto end_time = std::chrono::high_resolution_clock::now();
        auto orig_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        auto orig_compressed = orig_comp.compressed_bytes_last_block();
        long orig_size = orig_comp.compressed_size_last_block();
        
        // Test Optimized
        start_time = std::chrono::high_resolution_clock::now();
        SerfXORCompressorOptimized opt_comp(1000, max_diff, adjust_digit, 0.1);
        for (double v : sensor_data) {
            opt_comp.AddValue(v);
        }
        opt_comp.Close();
        end_time = std::chrono::high_resolution_clock::now();
        auto opt_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        auto opt_compressed = opt_comp.compressed_bytes_last_block();
        long opt_size = opt_comp.compressed_size_last_block();
        
        // Results
        std::cout << "\nResults:" << std::endl;
        std::cout << "Original - Time: " << orig_time.count() << " μs, Size: " << orig_size << " bits" << std::endl;
        std::cout << "Optimized - Time: " << opt_time.count() << " μs, Size: " << opt_size << " bits" << std::endl;
        
        if (orig_time.count() > 0) {
            double speedup = (double)orig_time.count() / opt_time.count();
            std::cout << "Compression speedup: " << speedup << "x" << std::endl;
        }
        
        if (orig_size > 0) {
            double compression_improvement = (double)orig_size / opt_size;
            std::cout << "Compression improvement: " << compression_improvement << "x" << std::endl;
        }
        
        // Test decompression
        SerfXORDecompressor orig_decomp(adjust_digit);
        auto orig_decompressed = orig_decomp.Decompress(orig_compressed);
        
        SerfXORDecompressorOptimized opt_decomp(adjust_digit, 0.1);
        auto opt_decompressed = opt_decomp.Decompress(opt_compressed);
        
        std::cout << "Decompressed sizes: " << orig_decompressed.size() << " vs " << opt_decompressed.size() << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
