#include "compressor/sfc_compressor.h"
#include "compressor/sfc_decompressor.h"
#include <iostream>
#include <vector>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;

int main() {
    cout << "测试MBR-GeoHash压缩解压缩" << endl;
    
    // 创建简单的测试数据
    vector<GeoPoint> points;
    for (int i = 0; i < 10; i++) {
        points.emplace_back(116.3 + i * 0.001, 39.9 + i * 0.001);
    }
    
    cout << "创建了 " << points.size() << " 个测试点" << endl;
    
    // 步骤1：先计算MBR
    SpaceFillingCurve::BoundingBox bbox;
    for (const auto& p : points) {
        bbox.Update(p);
    }
    cout << "计算MBR: [" << bbox.min_lon << ", " << bbox.max_lon << "] × [" 
         << bbox.min_lat << ", " << bbox.max_lat << "]" << endl;
    
    // 步骤2：创建压缩器并设置MBR
    double max_error = 1.0e-5;
    SFCCompressor compressor(SFCCompressor::MBR_GEOHASH, max_error);
    compressor.SetBoundingBox(bbox);  // 必须先设置MBR！
    
    // 步骤3：压缩所有点
    cout << "开始压缩..." << endl;
    for (size_t i = 0; i < points.size(); i++) {
        cout << "  添加点 " << i << ": (" << points[i].longitude << ", " << points[i].latitude << ")" << endl;
        compressor.AddPoint(points[i]);
    }
    
    Array<uint8_t> compressed = compressor.GetCompressedData();
    cout << "压缩完成，压缩后大小: " << compressed.length() << " 字节" << endl;
    cout << "编码比特数: " << compressor.GetEncodingBits() << endl;
    
    // 解压缩
    cout << "\n开始解压缩..." << endl;
    SFCDecompressor decompressor(compressed);
    
    cout << "读取的压缩方法: " << static_cast<int>(decompressor.GetMethod()) << endl;
    cout << "读取的最大误差: " << decompressor.GetMaxError() << endl;
    
    cout << "调用DecompressAll..." << endl;
    try {
        vector<GeoPoint> decompressed = decompressor.DecompressAll(points.size());
        cout << "解压缩完成，解压了 " << decompressed.size() << " 个点" << endl;
        
        // 验证
        for (size_t i = 0; i < min(size_t(10), decompressed.size()); i++) {
            cout << "点 " << i << ": 原始(" << points[i].longitude << ", " << points[i].latitude 
                 << ") -> 解压(" << decompressed[i].longitude << ", " << decompressed[i].latitude << ")" << endl;
        }
    } catch (const exception& e) {
        cout << "解压缩异常: " << e.what() << endl;
    }
    
    return 0;
}

