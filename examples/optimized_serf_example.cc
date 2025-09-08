#include <iostream>
#include <vector>
#include <chrono>
#include <random>

#include "src/compressor/serf_xor_compressor_optimized.h"
#include "src/decompressor/serf_xor_decompressor_optimized.h"

int main() {
    // Parameters for the optimized Serf-XOR algorithm
    int window_size = 1000;
    double max_diff = 0.1;
    long initial_adjust_digit = 100;
    double alpha = 0.1; // Exponential smoothing factor

    // Create optimized compressor and decompressor
    SerfXORCompressorOptimized compressor(window_size, max_diff, initial_adjust_digit, alpha);
    SerfXORDecompressorOptimized decompressor(initial_adjust_digit, alpha);

    // Generate test data - high sampling rate sensor data with similar consecutive values
    std::vector<double> test_data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dis(100.0, 5.0);
    
    // Generate 10000 values with high correlation (simulating high sampling rate sensor)
    double current_value = dis(gen);
    test_data.push_back(current_value);
    
    for (int i = 1; i < 10000; ++i) {
        // 80% chance of staying the same (high sampling rate characteristic)
        if (gen() % 100 < 80) {
            test_data.push_back(current_value);
        } else {
            // Small change
            current_value += (dis(gen) - 100.0) * 0.1;
            test_data.push_back(current_value);
        }
    }

    std::cout << "=== Optimized Serf-XOR Compression Example ===" << std::endl;
    std::cout << "Original data size: " << test_data.size() << " values" << std::endl;
    std::cout << "Data characteristics: High sampling rate sensor with many consecutive identical values" << std::endl;
    std::cout << "Optimizations applied:" << std::endl;
    std::cout << "  1. Exponential weighted moving average for dynamic adjust_digit" << std::endl;
    std::cout << "  2. Candidate-based approximator search (boundary + suffix matching)" << std::endl;
    std::cout << "  3. Run-length encoding for zero XOR results with Elias Gamma encoding" << std::endl;
    std::cout << std::endl;

    // Measure compression time
    auto start = std::chrono::high_resolution_clock::now();
    
    // Compress the data
    for (double value : test_data) {
        compressor.AddValue(value);
    }
    compressor.Close();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto compression_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Get compressed data
    Array<uint8_t> compressed_data = compressor.compressed_bytes_last_block();
    long compressed_size = compressor.compressed_size_last_block();
    
    std::cout << "Compression Results:" << std::endl;
    std::cout << "  Compressed size: " << compressed_size << " bits" << std::endl;
    std::cout << "  Compressed size: " << compressed_data.length() << " bytes" << std::endl;
    std::cout << "  Compression ratio: " << (double)(test_data.size() * 64) / compressed_size << ":1" << std::endl;
    std::cout << "  Compression time: " << compression_time.count() << " microseconds" << std::endl;
    std::cout << std::endl;

    // Measure decompression time
    start = std::chrono::high_resolution_clock::now();
    
    // Decompress the data
    std::vector<double> decompressed_data = decompressor.Decompress(compressed_data);
    
    end = std::chrono::high_resolution_clock::now();
    auto decompression_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Decompression Results:" << std::endl;
    std::cout << "  Decompressed size: " << decompressed_data.size() << " values" << std::endl;
    std::cout << "  Decompression time: " << decompression_time.count() << " microseconds" << std::endl;
    std::cout << std::endl;

    // Verify data integrity
    double max_error = 0.0;
    int error_count = 0;
    for (size_t i = 0; i < std::min(test_data.size(), decompressed_data.size()); ++i) {
        double error = std::abs(test_data[i] - decompressed_data[i]);
        if (error > max_diff) {
            error_count++;
        }
        max_error = std::max(max_error, error);
    }
    
    std::cout << "Data Integrity Check:" << std::endl;
    std::cout << "  Maximum error: " << max_error << std::endl;
    std::cout << "  Values exceeding error bound: " << error_count << std::endl;
    std::cout << "  Data integrity: " << (error_count == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << std::endl;

    // Show optimization benefits
    std::cout << "=== Optimization Benefits ===" << std::endl;
    std::cout << "1. Dynamic Adjust Digit:" << std::endl;
    std::cout << "   - Adapts to changing data characteristics over time" << std::endl;
    std::cout << "   - Reduces computation overhead compared to window-based min/max" << std::endl;
    std::cout << std::endl;
    
    std::cout << "2. Fast Approximator Search:" << std::endl;
    std::cout << "   - Tests only 4 candidate values instead of full range search" << std::endl;
    std::cout << "   - Significantly reduces compression time for high-frequency data" << std::endl;
    std::cout << std::endl;
    
    std::cout << "3. Zero-Run Compression:" << std::endl;
    std::cout << "   - Highly effective for high sampling rate sensors" << std::endl;
    std::cout << "   - Compresses sequences of identical values very efficiently" << std::endl;
    std::cout << "   - Uses Elias Gamma encoding for run lengths" << std::endl;

    return 0;
}
