#include <iostream>
#include "src/utils/elias_gamma_codec.h"
#include "src/utils/zig_zag_codec.h"
#include "src/utils/output_bit_stream.h"
#include "src/utils/input_bit_stream.h"

int main() {
    OutputBitStream out(1000);
    
    // 编码一个值
    int64_t value = 5;
    uint64_t encoded = ZigZagCodec::Encode(value);
    std::cout << "编码值 " << value << " -> ZigZag: " << encoded << std::endl;
    
    int bits = EliasGammaCodec::Encode(encoded + 1, &out);
    std::cout << "EliasGamma编码: " << bits << " bits" << std::endl;
    
    out.Flush();
    auto buffer = out.GetBuffer(1000);
    
    // 解码
    InputBitStream in(buffer.begin(), buffer.length());
    std::cout << "\n解码..." << std::endl;
    
    uint64_t decoded_gamma = EliasGammaCodec::Decode(&in);
    std::cout << "EliasGamma解码: " << decoded_gamma << std::endl;
    
    int64_t decoded_value = ZigZagCodec::Decode(decoded_gamma - 1);
    std::cout << "ZigZag解码: " << decoded_value << std::endl;
    
    if (decoded_value == value) {
        std::cout << "✓ 成功！" << std::endl;
    } else {
        std::cout << "✗ 失败！" << std::endl;
    }
    
    return 0;
}
