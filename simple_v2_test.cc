#include <iostream>
#include <vector>
#include "src/compressor/serf_xor_compressor_zero_opt_v2.h"
#include "src/decompressor/serf_xor_decompressor_zero_opt_v2.h"

int main() {
    std::cout << "Simple V2 Test..." << std::endl;
    
    try {
        SerfXORCompressorZeroOptV2 compressor(1000, 0.1, 10);
        SerfXORDecompressorZeroOptV2 decompressor(10);
        
        std::vector<double> data = {1.0, 1.0, 1.0, 2.0};
        
        for (double v : data) {
            compressor.AddValue(v);
        }
        compressor.Close();
        
        Array<uint8_t> compressed = compressor.compressed_bytes_last_block();
        long size = compressor.compressed_size_last_block();
        
        std::cout << "Compressed size: " << size << " bits" << std::endl;
        
        std::vector<double> decompressed = decompressor.Decompress(compressed);
        
        std::cout << "Input:  ";
        for (double v : data) std::cout << v << " ";
        std::cout << std::endl;
        
        std::cout << "Output: ";
        for (double v : decompressed) std::cout << v << " ";
        std::cout << std::endl;
        
        std::cout << "Sizes: " << data.size() << " -> " << decompressed.size() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
