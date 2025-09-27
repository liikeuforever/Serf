#include <iostream>
#include <vector>
#include <iomanip>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "decompressor/serf_qt_gps_configurable_decompressor.h"

// 计算GPS距离（度）
double CalculateGpsDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1,
                           const SerfQtGpsConfigurableDecompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    std::cout << "=== 可配置压缩器调试测试 ===" << std::endl;
    
    // 创建简单的测试数据
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> test_points = {
        {116.318417, 39.984702},
        {116.318450, 39.984683},
        {116.318417, 39.984686},
        {116.318385, 39.984688},
        {116.318263, 39.984655}
    };
    
    std::cout << "测试点数: " << test_points.size() << std::endl;
    std::cout << "使用参数: εv=1e-6, εθ=0.0001, error_bound=1e-5" << std::endl;
    
    // 压缩
    SerfQtGpsConfigurableCompressor compressor(test_points.size(), 1e-5, 1e-6, 0.0001);
    
    std::cout << "\n压缩过程:" << std::endl;
    for (size_t i = 0; i < test_points.size(); ++i) {
        std::cout << "添加点 " << i << ": (" << std::fixed << std::setprecision(6) 
                  << test_points[i].longitude << ", " << test_points[i].latitude << ")" << std::endl;
        compressor.AddGpsPoint(test_points[i]);
    }
    
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    std::cout << "压缩完成，数据大小: " << compressed_data.length() << " 字节" << std::endl;
    
    // 解压缩
    SerfQtGpsConfigurableDecompressor decompressor(compressed_data);
    std::vector<SerfQtGpsConfigurableDecompressor::GpsPoint> decompressed_points;
    
    std::cout << "\n解压过程:" << std::endl;
    for (size_t i = 0; i < test_points.size(); ++i) {
        auto point = decompressor.GetNextGpsPoint();
        decompressed_points.push_back(point);
        std::cout << "解压点 " << i << ": (" << std::fixed << std::setprecision(6) 
                  << point.longitude << ", " << point.latitude << ")" << std::endl;
    }
    
    // 误差分析
    std::cout << "\n误差分析:" << std::endl;
    std::cout << "点号  原始经度    原始纬度    重构经度    重构纬度    误差(度)" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;
    
    double max_error = 0.0;
    for (size_t i = 0; i < test_points.size(); ++i) {
        double error = CalculateGpsDistance(test_points[i], decompressed_points[i]);
        max_error = std::max(max_error, error);
        
        std::cout << std::setw(4) << i 
                  << "  " << std::fixed << std::setprecision(6) << test_points[i].longitude
                  << "  " << test_points[i].latitude
                  << "  " << decompressed_points[i].longitude
                  << "  " << decompressed_points[i].latitude
                  << "  " << std::scientific << std::setprecision(3) << error << std::endl;
    }
    
    std::cout << "\n总结:" << std::endl;
    std::cout << "最大误差: " << std::scientific << max_error << " 度" << std::endl;
    std::cout << "误差界限: " << 1e-5 << " 度" << std::endl;
    
    if (max_error <= 1e-5) {
        std::cout << "✅ 误差控制正常" << std::endl;
    } else {
        std::cout << "❌ 误差超出界限" << std::endl;
    }
    
    return 0;
}
