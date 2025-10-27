#include <iostream>
#include <cstdint>
#include "src/utils/output_bit_stream.h"
#include "src/utils/input_bit_stream.h"

int main() {
    // 测试timestamp delta编码（包括负数）
    std::vector<int64_t> test_deltas = {1, -21910, 22095, 0, -1, 100000};
    
    OutputBitStream out_stream(1000);
    
    std::cout << "=== 编码测试 ===\n";
    for (int64_t delta_signed : test_deltas) {
        uint64_t delta_unsigned = static_cast<uint64_t>(delta_signed);
        out_stream.WriteLong(delta_unsigned, 64);
        std::cout << "编码: " << delta_signed << " -> 0x" << std::hex << delta_unsigned << std::dec << "\n";
    }
    
    out_stream.Flush();
    auto buffer = out_stream.GetBuffer(1000);
    
    std::cout << "\n=== 解码测试 ===\n";
    InputBitStream in_stream(buffer.begin(), buffer.length());
    
    for (size_t i = 0; i < test_deltas.size(); i++) {
        uint64_t delta_unsigned = in_stream.ReadLong(64);
        int64_t delta_signed = static_cast<int64_t>(delta_unsigned);
        std::cout << "解码: 0x" << std::hex << delta_unsigned << std::dec << " -> " << delta_signed;
        
        if (delta_signed == test_deltas[i]) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ (期望 " << test_deltas[i] << ")\n";
        }
    }
    
    return 0;
}
