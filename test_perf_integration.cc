#include <iostream>
#include <chrono>
#include "Perf_expr_config.hpp"
#include "Perf_file_utils.hpp"
#include "Perf_expr_data_struct.hpp"

// Include our optimized compressors
#include "compressor/serf_xor_compressor_zero_opt.h"
#include "decompressor/serf_xor_decompressor_zero_opt.h"
#include "compressor/serf_xor_compressor_fast_search.h"
#include "compressor/serf_xor_compressor_combined_opt.h"
#include "compressor/serf_xor_compressor.h"
#include "decompressor/serf_xor_decompressor.h"

void TestPerfIntegration() {
    std::cout << "Testing Performance Integration..." << std::endl;
    
    // Use a small dataset for quick testing
    std::string test_dataset = "Air-pressure.csv";
    std::ifstream data_input_stream("test/data_set/" + test_dataset);
    if (!data_input_stream.is_open()) {
        std::cerr << "Failed to open test dataset" << std::endl;
        return;
    }
    
    int adjust_digit = kFileNameToAdjustDigit.find(test_dataset)->second;
    double max_diff = 0.1;
    int block_size = 50;
    
    // Read one block of data
    std::vector<double> test_data = ReadBlock(data_input_stream, block_size);
    if (test_data.size() != block_size) {
        std::cerr << "Failed to read test data" << std::endl;
        return;
    }
    
    std::cout << "Testing with " << test_data.size() << " values from " << test_dataset << std::endl;
    std::cout << "Parameters: max_diff=" << max_diff << ", adjust_digit=" << adjust_digit << std::endl;
    
    // Test all four versions
    struct TestResult {
        std::string name;
        long compressed_size;
        long compression_time_us;
        long decompression_time_us;
    };
    
    std::vector<TestResult> results;
    
    // 1. Original SerfXOR
    {
        SerfXORCompressor compressor(1000, max_diff, adjust_digit);
        SerfXORDecompressor decompressor(adjust_digit);
        
        auto start = std::chrono::steady_clock::now();
        for (double value : test_data) compressor.AddValue(value);
        compressor.Close();
        auto end = std::chrono::steady_clock::now();
        
        auto compress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        Array<uint8_t> compressed = compressor.compressed_bytes_last_block();
        
        start = std::chrono::steady_clock::now();
        std::vector<double> decompressed = decompressor.Decompress(compressed);
        end = std::chrono::steady_clock::now();
        
        auto decompress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.push_back({"SerfXOR", compressor.compressed_size_last_block(), 
                          compress_time.count(), decompress_time.count()});
    }
    
    // 2. Zero Optimization
    {
        SerfXORCompressorZeroOpt compressor(1000, max_diff, adjust_digit);
        SerfXORDecompressorZeroOpt decompressor(adjust_digit);
        
        auto start = std::chrono::steady_clock::now();
        for (double value : test_data) compressor.AddValue(value);
        compressor.Close();
        auto end = std::chrono::steady_clock::now();
        
        auto compress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        Array<uint8_t> compressed = compressor.compressed_bytes_last_block();
        
        start = std::chrono::steady_clock::now();
        std::vector<double> decompressed = decompressor.Decompress(compressed);
        end = std::chrono::steady_clock::now();
        
        auto decompress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.push_back({"SerfXOR_ZeroOpt", compressor.compressed_size_last_block(), 
                          compress_time.count(), decompress_time.count()});
    }
    
    // 3. Fast Search
    {
        SerfXORCompressorFastSearch compressor(1000, max_diff, adjust_digit);
        SerfXORDecompressor decompressor(adjust_digit);
        
        auto start = std::chrono::steady_clock::now();
        for (double value : test_data) compressor.AddValue(value);
        compressor.Close();
        auto end = std::chrono::steady_clock::now();
        
        auto compress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        Array<uint8_t> compressed = compressor.compressed_bytes_last_block();
        
        start = std::chrono::steady_clock::now();
        std::vector<double> decompressed = decompressor.Decompress(compressed);
        end = std::chrono::steady_clock::now();
        
        auto decompress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.push_back({"SerfXOR_FastSearch", compressor.compressed_size_last_block(), 
                          compress_time.count(), decompress_time.count()});
    }
    
    // 4. Combined Optimization
    {
        SerfXORCompressorCombinedOpt compressor(1000, max_diff, adjust_digit);
        SerfXORDecompressorZeroOpt decompressor(adjust_digit);
        
        auto start = std::chrono::steady_clock::now();
        for (double value : test_data) compressor.AddValue(value);
        compressor.Close();
        auto end = std::chrono::steady_clock::now();
        
        auto compress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        Array<uint8_t> compressed = compressor.compressed_bytes_last_block();
        
        start = std::chrono::steady_clock::now();
        std::vector<double> decompressed = decompressor.Decompress(compressed);
        end = std::chrono::steady_clock::now();
        
        auto decompress_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        results.push_back({"SerfXOR_CombinedOpt", compressor.compressed_size_last_block(), 
                          compress_time.count(), decompress_time.count()});
    }
    
    // Print results
    std::cout << "\n=== Performance Integration Test Results ===" << std::endl;
    std::cout << std::setw(20) << "Method" 
              << std::setw(15) << "Size(bits)"
              << std::setw(15) << "CompTime(μs)"
              << std::setw(15) << "DecompTime(μs)" << std::endl;
    std::cout << std::string(65, '-') << std::endl;
    
    for (const auto& result : results) {
        std::cout << std::setw(20) << result.name
                  << std::setw(15) << result.compressed_size
                  << std::setw(15) << result.compression_time_us
                  << std::setw(15) << result.decompression_time_us
                  << std::endl;
    }
    
    data_input_stream.close();
    std::cout << "\n✅ Performance integration test completed successfully!" << std::endl;
}

int main() {
    TestPerfIntegration();
    return 0;
}
