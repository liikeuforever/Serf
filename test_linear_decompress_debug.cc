#include <iostream>
#include "src/decompressor/serf_qt_linear_decompressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include "utils/double.h"

int main() {
    // 创建一个小测试：3个点
    std::vector<double> test_values = {116.3, 116.4, 116.5};
    std::vector<uint64_t> test_timestamps = {1000, 1001, 1002};
    
    std::cout << "压缩3个点..." << std::endl;
    SerfQtLinearCompressor compressor(3, 1e-5);
    for (size_t i = 0; i < test_values.size(); i++) {
        std::cout << "  添加点 " << i << ": value=" << test_values[i] << ", ts=" << test_timestamps[i] << std::endl;
        compressor.AddValue(test_values[i], test_timestamps[i]);
    }
    compressor.Close();
    
    std::cout << "压缩完成，大小: " << compressor.get_compressed_size_in_bits() << " bits" << std::endl;
    
    auto compressed = compressor.compressed_bytes();
    std::cout << "压缩数据: " << compressed.length() << " bytes" << std::endl;
    
    std::cout << "\n开始解压..." << std::endl;
    SerfQtLinearDecompressor decompressor;
    
    std::cout << "调用Decompress..." << std::endl;
    auto decompressed = decompressor.Decompress(compressed);
    
    std::cout << "解压完成！得到 " << decompressed.size() << " 个点" << std::endl;
    for (size_t i = 0; i < decompressed.size(); i++) {
        std::cout << "  点 " << i << ": " << decompressed[i] << std::endl;
    }
    
    return 0;
}
