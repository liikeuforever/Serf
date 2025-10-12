#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

void CompareMethod(const string& name, vector<uint64_t>& codes, const vector<GeoPoint>& points) {
    // 计算差值
    vector<int64_t> diffs;
    for (size_t i = 1; i < codes.size(); i++) {
        diffs.push_back(abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1])));
    }
    
    int64_t min_diff = *min_element(diffs.begin(), diffs.end());
    int64_t max_diff = *max_element(diffs.begin(), diffs.end());
    double avg_diff = 0;
    for (auto d : diffs) avg_diff += d;
    avg_diff /= diffs.size();
    
    // 计算有多少个零差值
    int zero_count = 0;
    for (auto d : diffs) if (d == 0) zero_count++;
    
    cout << name << ":" << endl;
    cout << "  最小差值: " << min_diff << endl;
    cout << "  最大差值: " << max_diff << endl;
    cout << "  平均差值: " << fixed << setprecision(0) << avg_diff << endl;
    cout << "  零差值: " << zero_count << " / " << diffs.size() << endl;
    
    // 显示前20个差值
    cout << "  前20个差值: ";
    for (size_t i = 0; i < min(size_t(20), diffs.size()); i++) {
        if (i > 0) cout << ", ";
        cout << diffs[i];
    }
    cout << endl << endl;
}

void TestWithBits(int bits) {
    cout << "\n========================================" << endl;
    cout << "测试 " << bits << " 比特编码" << endl;
    cout << "========================================" << endl;
    
    // 使用归一化的边界框
    BoundingBox bbox;
    bbox.min_lon = 0.0;
    bbox.max_lon = 1.0;
    bbox.min_lat = 0.0;
    bbox.max_lat = 1.0;
    
    // 生成测试点：沿对角线移动
    vector<GeoPoint> points;
    double step = 0.0001;
    for (int i = 0; i < 100; i++) {
        points.emplace_back(i * step, i * step);
    }
    
    cout << "测试数据: 100个点，从(0,0)沿对角线移动到(" << (99 * step) << ", " << (99 * step) << ")" << endl;
    cout << "每步移动: " << scientific << setprecision(2) << step << " 度" << endl << endl;
    
    // 标准GeoHash（使用同样的bbox归一化到[0,1]）
    vector<uint64_t> std_codes;
    for (const auto& p : points) {
        // 将[0,1]映射到全球坐标[-180,180]×[-90,90]
        GeoPoint global_p(p.longitude * 360 - 180, p.latitude * 180 - 90);
        std_codes.push_back(SpaceFillingCurve::EncodeStandardGeoHash(global_p, bits));
    }
    CompareMethod("标准GeoHash", std_codes, points);
    
    // MBR-GeoHash
    vector<uint64_t> mbr_codes;
    for (const auto& p : points) {
        mbr_codes.push_back(SpaceFillingCurve::EncodeMBRGeoHash(p, bits, bbox));
    }
    CompareMethod("MBR-GeoHash", mbr_codes, points);
    
    // Hilbert曲线
    int order = bits / 2;
    vector<uint64_t> hilbert_codes;
    for (const auto& p : points) {
        hilbert_codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    CompareMethod("Hilbert曲线", hilbert_codes, points);
    
    // 可视化：显示前16个点的编码值（4x4网格）
    cout << "前16个点的编码值对比:" << endl;
    cout << "\n标准GeoHash:" << endl;
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) cout << "  ";
        cout << setw(15) << std_codes[i] << " ";
        if (i % 4 == 3) cout << endl;
    }
    
    cout << "\nMBR-GeoHash:" << endl;
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) cout << "  ";
        cout << setw(15) << mbr_codes[i] << " ";
        if (i % 4 == 3) cout << endl;
    }
    
    cout << "\nHilbert曲线:" << endl;
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) cout << "  ";
        cout << setw(15) << hilbert_codes[i] << " ";
        if (i % 4 == 3) cout << endl;
    }
}

// 专门测试Hilbert曲线的Z字形路径
void TestHilbertZPattern() {
    cout << "\n========================================" << endl;
    cout << "Hilbert曲线Z字形路径测试" << endl;
    cout << "========================================" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 0.0;
    bbox.max_lon = 1.0;
    bbox.min_lat = 0.0;
    bbox.max_lat = 1.0;
    
    int order = 3;  // 8x8网格
    int n = 1 << order;
    
    cout << "生成" << n << "x" << n << "网格的Hilbert路径" << endl;
    
    // 创建整个网格的编码
    vector<pair<uint64_t, pair<int,int>>> code_pos;
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            double px = (x + 0.5) / n;
            double py = (y + 0.5) / n;
            GeoPoint p(px, py);
            uint64_t code = SpaceFillingCurve::EncodeHilbert(p, order, bbox);
            code_pos.push_back({code, {x, y}});
        }
    }
    
    // 按编码值排序
    sort(code_pos.begin(), code_pos.end());
    
    // 检查是否连续
    bool all_continuous = true;
    for (size_t i = 0; i < code_pos.size(); i++) {
        if (code_pos[i].first != i) {
            cout << "错误: 编码值不连续！位置(" << code_pos[i].second.first 
                 << "," << code_pos[i].second.second << ") 编码=" << code_pos[i].first 
                 << ", 期望=" << i << endl;
            all_continuous = false;
        }
    }
    
    if (all_continuous) {
        cout << "✓ 所有编码值0-" << (code_pos.size()-1) << "连续" << endl;
    }
    
    // 显示Hilbert路径顺序（前30个点）
    cout << "\nHilbert路径前30个点的顺序:" << endl;
    for (size_t i = 0; i < min(size_t(30), code_pos.size()); i++) {
        cout << i << ":(" << code_pos[i].second.first << "," << code_pos[i].second.second << ") ";
        if (i % 5 == 4) cout << endl;
    }
    cout << endl;
    
    // 计算路径上相邻点的曼哈顿距离
    cout << "\n相邻点的曼哈顿距离（应该都是1）:" << endl;
    int non_adjacent = 0;
    for (size_t i = 1; i < code_pos.size(); i++) {
        int dx = abs(code_pos[i].second.first - code_pos[i-1].second.first);
        int dy = abs(code_pos[i].second.second - code_pos[i-1].second.second);
        int manhattan = dx + dy;
        
        if (manhattan != 1) {
            cout << "  步" << i << ": 距离=" << manhattan << " (从(" 
                 << code_pos[i-1].second.first << "," << code_pos[i-1].second.second 
                 << ")到(" << code_pos[i].second.first << "," << code_pos[i].second.second << "))" << endl;
            non_adjacent++;
        }
    }
    
    if (non_adjacent == 0) {
        cout << "  ✓ 所有相邻编码的点在空间上也相邻（曼哈顿距离=1）" << endl;
    } else {
        cout << "  ✗ 发现 " << non_adjacent << " 对不相邻的点！Hilbert实现可能有bug" << endl;
    }
}

int main() {
    cout << "========================================" << endl;
    cout << "相同比特数下三种方法的对比测试" << endl;
    cout << "========================================" << endl;
    
    // 测试不同的比特数
    TestWithBits(24);
    TestWithBits(32);
    TestWithBits(40);
    
    // 专门测试Hilbert曲线的正确性
    TestHilbertZPattern();
    
    cout << "\n========================================" << endl;
    cout << "结论" << endl;
    cout << "========================================" << endl;
    cout << "如果Hilbert曲线的差值比GeoHash大很多，说明实现有bug。" << endl;
    cout << "正确的Hilbert曲线应该有最好的空间局部性。" << endl;
    
    return 0;
}

