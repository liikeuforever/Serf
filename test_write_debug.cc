#include <iostream>
#include <vector>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"
#include "src/utils/output_bit_stream.h"

int main() {
    // 测试WriteLong的行为
    OutputBitStream stream(1000);
    
    std::cout << "Testing WriteLong(1000, 64):\n";
    stream.WriteLong(1000, 64);
    
    std::cout << "Testing WriteLong(1, 64):\n";
    stream.WriteLong(1, 64);
    
    stream.Flush();
    
    auto buffer = stream.GetBuffer(16);
    std::cout << "\nBuffer contents (first 16 bytes):\n";
    for (int i = 0; i < 16; i++) {
        printf("%02x ", buffer[i]);
    }
    std::cout << "\n";
    
    std::cout << "\nBytes 0-7 should be 1000:\n";
    std::cout << "Bytes 8-15 should be 1:\n";
    
    return 0;
}
