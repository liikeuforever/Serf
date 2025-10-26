#include <iostream>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

int main() {
    std::cout << "创建3个测试点...\n";
    std::vector<TrajCompressSPAdaptiveSimpleCompressor::GpsPoint> data;
    data.push_back({116.3, 39.9, 1000});
    data.push_back({116.301, 39.901, 1001});
    data.push_back({116.302, 39.902, 1002});
    
    std::cout << "开始压缩...\n";
    TrajCompressSPAdaptiveSimpleCompressor compressor(3, 1e-5, 96);
    
    for (int i = 0; i < 3; i++) {
        std::cout << "  添加点 " << i << "...\n";
        std::cout.flush();
        compressor.AddGpsPoint(data[i]);
        std::cout << "  添加点 " << i << " 完成\n";
        std::cout.flush();
    }
    
    std::cout << "所有点添加完成，准备调用Close()...\n";
    std::cout.flush();
    compressor.Close();
    std::cout << "Close()完成\n";
    std::cout.flush();
    
    auto compressed = compressor.GetCompressedData();
    std::cout << "压缩完成: " << compressed.length() << " bytes\n";
    
    return 0;
}
