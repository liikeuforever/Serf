#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor.h"
#include "src/decompressor/serf_xor_decompressor.h"
#include "src/compressor/serf_xor_compressor_zero_opt.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt.h"
#include "src/compressor/serf_xor_compressor_zero_opt_v2.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt_v2.h"

void test_v2_scenarios() {
    std::cout << "=== Testing V2 Zero Sequence Optimization (Leading Space) ===" << std::endl;
    
    double max_diff = 0.1;
    long adjust_digit = 10;
    
    // Test Case 1: No repeated values (should have minimal overhead)
    {
        std::cout << "\n--- Test 1: No Repeated Values ---" << std::endl;
        std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
        
        // Original
        SerfXORCompressor orig(1000, max_diff, adjust_digit);
        for (double v : data) orig.AddValue(v);
        orig.Close();
        long orig_size = orig.compressed_size_last_block();
        
        // V1
        SerfXORCompressorZeroOpt v1(1000, max_diff, adjust_digit);
        for (double v : data) v1.AddValue(v);
        v1.Close();
        long v1_size = v1.compressed_size_last_block();
        
        // V2
        SerfXORCompressorZeroOptV2 v2(1000, max_diff, adjust_digit);
        for (double v : data) v2.AddValue(v);
        v2.Close();
        long v2_size = v2.compressed_size_last_block();
        
        std::cout << "Original: " << orig_size << " bits" << std::endl;
        std::cout << "V1:       " << v1_size << " bits (overhead: " << (v1_size - orig_size) << ")" << std::endl;
        std::cout << "V2:       " << v2_size << " bits (overhead: " << (v2_size - orig_size) << ")" << std::endl;
        
        if (v2_size <= orig_size && v2_size <= v1_size) {
            std::cout << "✅ V2 optimal for no-repeat scenario" << std::endl;
        }
    }
    
    // Test Case 2: Many repeated values (should benefit from optimization)
    {
        std::cout << "\n--- Test 2: Many Repeated Values ---" << std::endl;
        std::vector<double> data = {42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 42.0, 43.0};
        
        // Original
        SerfXORCompressor orig(1000, max_diff, adjust_digit);
        for (double v : data) orig.AddValue(v);
        orig.Close();
        long orig_size = orig.compressed_size_last_block();
        
        // V1
        SerfXORCompressorZeroOpt v1(1000, max_diff, adjust_digit);
        for (double v : data) v1.AddValue(v);
        v1.Close();
        long v1_size = v1.compressed_size_last_block();
        
        // V2
        SerfXORCompressorZeroOptV2 v2(1000, max_diff, adjust_digit);
        for (double v : data) v2.AddValue(v);
        v2.Close();
        long v2_size = v2.compressed_size_last_block();
        
        std::cout << "Original: " << orig_size << " bits" << std::endl;
        std::cout << "V1:       " << v1_size << " bits (improvement: " << (orig_size - v1_size) << ")" << std::endl;
        std::cout << "V2:       " << v2_size << " bits (improvement: " << (orig_size - v2_size) << ")" << std::endl;
        
        if (v2_size < orig_size) {
            std::cout << "✅ V2 beneficial for repeat scenario" << std::endl;
        }
        
        if (v2_size <= v1_size) {
            std::cout << "✅ V2 better than or equal to V1" << std::endl;
        } else {
            std::cout << "⚠️  V2 slightly worse than V1 by " << (v2_size - v1_size) << " bits" << std::endl;
        }
    }
    
    // Test Case 3: Verify correctness
    {
        std::cout << "\n--- Test 3: Correctness Verification ---" << std::endl;
        std::vector<double> data = {10.0, 10.0, 20.0, 20.0, 20.0, 30.0};
        
        SerfXORCompressorZeroOptV2 v2_comp(1000, max_diff, adjust_digit);
        SerfXORDecompressorZeroOptV2 v2_decomp(adjust_digit);
        
        for (double v : data) v2_comp.AddValue(v);
        v2_comp.Close();
        
        Array<uint8_t> compressed = v2_comp.compressed_bytes_last_block();
        std::vector<double> decompressed = v2_decomp.Decompress(compressed);
        
        std::cout << "Input size:  " << data.size() << std::endl;
        std::cout << "Output size: " << decompressed.size() << std::endl;
        
        bool correct = (data.size() == decompressed.size());
        for (size_t i = 0; i < data.size() && correct; i++) {
            if (std::abs(data[i] - decompressed[i]) > max_diff + 1e-6) {
                correct = false;
                std::cout << "Mismatch at index " << i << ": " << data[i] << " vs " << decompressed[i] << std::endl;
            }
        }
        
        std::cout << "Correctness: " << (correct ? "✅ PASS" : "❌ FAIL") << std::endl;
    }
}

int main() {
    try {
        test_v2_scenarios();
        std::cout << "\n🎉 V2 Testing Complete!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
