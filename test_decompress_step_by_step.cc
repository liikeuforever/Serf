#include <iostream>
#include "src/utils/input_bit_stream.h"
#include "src/utils/output_bit_stream.h"
#include "src/utils/double.h"

int main() {
    // 模拟压缩器写入的数据
    OutputBitStream out_stream(1000);
    
    std::cout << "写入数据..." << std::endl;
    int bits = 0;
    bits += out_stream.WriteInt(3, 16);  // block_size
    std::cout << "  WriteInt(3, 16): " << bits << " bits" << std::endl;
    
    bits += out_stream.WriteLong(Double::DoubleToLongBits(1e-5 * 0.999), 64);  // max_diff
    std::cout << "  WriteLong(max_diff, 64): " << bits << " bits" << std::endl;
    
    bits += out_stream.WriteLong(1000, 64);  // first timestamp
    std::cout << "  WriteLong(1000, 64): " << bits << " bits" << std::endl;
    
    out_stream.Flush();
    auto buffer = out_stream.GetBuffer(1000);
    std::cout << "总共写入: " << bits << " bits, buffer大小: " << buffer.length() << " bytes" << std::endl;
    
    std::cout << "\n读取数据..." << std::endl;
    InputBitStream in_stream(buffer.begin(), buffer.length());
    
    std::cout << "  ReadInt(16)..." << std::endl;
    int block_size = in_stream.ReadInt(16);
    std::cout << "    block_size = " << block_size << std::endl;
    
    std::cout << "  ReadLong(64) for max_diff..." << std::endl;
    uint64_t max_diff_bits = in_stream.ReadLong(64);
    double max_diff = Double::LongBitsToDouble(max_diff_bits);
    std::cout << "    max_diff = " << max_diff << std::endl;
    
    std::cout << "  ReadLong(64) for timestamp..." << std::endl;
    uint64_t timestamp = in_stream.ReadLong(64);
    std::cout << "    timestamp = " << timestamp << std::endl;
    
    std::cout << "\n✓ 成功！" << std::endl;
    return 0;
}
