#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

void TestBitsForMBR(int bits) {
    cout << "\n========================================" << endl;
    cout << "测试 " << bits << " 比特编码" << endl;
    cout << "========================================" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 0.0;
    bbox.max_lon = 1.0;
    bbox.min_lat = 0.0;
    bbox.max_lat = 1.0;
    
    // 计算格子大小
    int lon_bits = (bits + 1) / 2;
    int lat_bits = bits / 2;
    int lon_cells = 1 << lon_bits;
    int lat_cells = 1 << lat_bits;
    double lon_cell_size = 1.0 / lon_cells;
    double lat_cell_size = 1.0 / lat_cells;
    
    cout << "格子大小: " << scientific << setprecision(2) 
         << lon_cell_size << " × " << lat_cell_size << endl;
    
    // 测试相邻点
    double step = 0.0001;
    vector<uint64_t> codes;
    
    for (int i = 0; i < 10; i++) {
        GeoPoint p(i * step, i * step);
        codes.push_back(SpaceFillingCurve::EncodeMBRGeoHash(p, bits, bbox));
    }
    
    // 计算差值
    vector<int64_t> diffs;
    for (size_t i = 1; i < codes.size(); i++) {
        diffs.push_back(abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1])));
    }
    
    int64_t max_diff = *max_element(diffs.begin(), diffs.end());
    double avg_diff = 0;
    for (auto d : diffs) avg_diff += d;
    avg_diff /= diffs.size();
    
    cout << "平均差值: " << fixed << setprecision(0) << avg_diff << endl;
    cout << "最大差值: " << max_diff << endl;
    
    // 评估
    bool good = (avg_diff < 100000);
    cout << "评价: " << (good ? "✓ 良好" : "✗ 较差") << endl;
    
    // 计算理论上0.0001度跨越多少格子
    int cells_crossed = (int)(step / lon_cell_size);
    cout << "0.0001度跨越约 " << cells_crossed << " 个格子" << endl;
}

int main() {
    cout << "========================================" << endl;
    cout << "不同比特数对MBR-GeoHash的影响" << endl;
    cout << "========================================" << endl;
    
    // 测试不同的比特数
    int bit_options[] = {20, 24, 28, 30, 32, 36, 40, 44, 48, 52};
    
    for (int bits : bit_options) {
        TestBitsForMBR(bits);
    }
    
    cout << "\n========================================" << endl;
    cout << "建议" << endl;
    cout << "========================================" << endl;
    cout << "对于5度×9度的GeoLife数据集：" << endl;
    cout << "- 如果要求精度1e-5度，需要约32-36比特" << endl;
    cout << "- 52比特导致格子过小，差值编码效果极差" << endl;
    cout << "- 标准GeoHash在全球范围使用52比特是合理的" << endl;
    
    return 0;
}


