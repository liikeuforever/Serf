#include "compressor/sfc_compressor.h"
#include "compressor/sfc_decompressor.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;

int main() {
    cout << "使用合理比特数测试压缩解压缩" << endl;
    
    // 创建测试数据
    vector<GeoPoint> points;
    for (int i = 0; i < 10; i++) {
        points.emplace_back(116.3 + i * 0.001, 39.9 + i * 0.001);
    }
    cout << "创建了 " << points.size() << " 个测试点\n" << endl;
    
    // 步骤1：计算MBR
    SpaceFillingCurve::BoundingBox bbox;
    for (const auto& p : points) {
        bbox.Update(p);
    }
    cout << "MBR: [" << bbox.min_lon << ", " << bbox.max_lon << "] × [" 
         << bbox.min_lat << ", " << bbox.max_lat << "]" << endl;
    cout << "宽度: " << bbox.GetWidth() << " 度, 高度: " << bbox.GetHeight() << " 度\n" << endl;
    
    // 测试不同的比特数
    vector<int> bit_counts = {24, 30, 38};
    
    for (int bits : bit_counts) {
        cout << "\n========================================" << endl;
        cout << "测试 " << bits << " 比特编码" << endl;
        cout << "========================================" << endl;
        
        // 计算理论网格大小
        int n = 1 << (bits / 2);
        double cell_width = bbox.GetWidth() / n;
        double cell_height = bbox.GetHeight() / n;
        double diagonal = sqrt(cell_width * cell_width + cell_height * cell_height);
        cout << "网格大小: " << n << "x" << n << endl;
        cout << "格子对角线: " << scientific << diagonal << " 度\n" << endl;
        
        // 编码并计算差值
        vector<uint64_t> codes;
        for (const auto& p : points) {
            uint64_t code = SpaceFillingCurve::EncodeMBRGeoHash(p, bits, bbox);
            codes.push_back(code);
        }
        
        cout << "编码值: ";
        for (size_t i = 0; i < min(size_t(5), codes.size()); i++) {
            cout << codes[i] << " ";
        }
        cout << "..." << endl;
        
        cout << "前5个差值: ";
        for (size_t i = 1; i < min(size_t(6), codes.size()); i++) {
            int64_t diff = static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]);
            cout << diff << " ";
        }
        cout << endl;
        
        // 计算差值平均值
        double avg_diff = 0;
        for (size_t i = 1; i < codes.size(); i++) {
            avg_diff += abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]));
        }
        avg_diff /= (codes.size() - 1);
        cout << "平均差值: " << fixed << setprecision(0) << avg_diff << endl;
    }
    
    cout << "\n========================================" << endl;
    cout << "完整压缩解压缩测试（使用38比特）" << endl;
    cout << "========================================" << endl;
    
    // 直接使用38比特（满足1e-5精度的最小比特数）
    int target_bits = 38;
    
    // 压缩 - 使用新的构造函数，直接指定比特数和bbox
    SFCCompressor compressor(SFCCompressor::MBR_GEOHASH, target_bits, bbox);
    
    for (const auto& p : points) {
        compressor.AddPoint(p);
    }
    
    Array<uint8_t> compressed = compressor.GetCompressedData();
    int actual_bits = compressor.GetEncodingBits();
    
    cout << "实际使用的编码比特数: " << actual_bits << endl;
    cout << "压缩后大小: " << compressed.length() << " 字节" << endl;
    cout << "压缩率: " << fixed << setprecision(2) 
         << (100.0 * compressed.length() / (points.size() * 2 * 8)) << "%" << endl;
    
    // 解压缩
    SFCDecompressor decompressor(compressed);
    vector<GeoPoint> decompressed = decompressor.DecompressAll(points.size());
    
    cout << "\n解压缩验证:" << endl;
    double max_err = 0.0;
    for (size_t i = 0; i < points.size(); i++) {
        double dx = decompressed[i].longitude - points[i].longitude;
        double dy = decompressed[i].latitude - points[i].latitude;
        double err = sqrt(dx * dx + dy * dy);
        max_err = max(max_err, err);
        
        if (i < 5 || err > 1e-5) {
            cout << "  点 " << i << ": 原始(" << points[i].longitude << ", " << points[i].latitude 
                 << ") -> 解压(" << decompressed[i].longitude << ", " << decompressed[i].latitude 
                 << ") 误差=" << scientific << err << endl;
        }
    }
    
    cout << "\n最大误差: " << scientific << max_err << " 度" << endl;
    cout << "满足1e-5精度: " << (max_err <= 1.0e-5 ? "✓ 是" : "✗ 否") << endl;
    
    return 0;
}

