#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

// 测试在一个小区域内沿着一条线移动
void TestLinearMovement(const string& method_name, 
                        function<uint64_t(const GeoPoint&)> encode_func) {
    cout << "\n=== " << method_name << " - 线性移动测试 ===" << endl;
    
    // 在一个1度×1度的区域内，从(0,0)沿对角线移动到(0.001, 0.001)
    double step = 0.0001;  // 每步0.0001度
    int steps = 10;
    
    cout << "从 (0, 0) 沿对角线移动到 (" << (step * steps) << ", " << (step * steps) << ")" << endl;
    cout << "每步移动: " << scientific << setprecision(2) << step << " 度" << endl;
    
    vector<uint64_t> codes;
    for (int i = 0; i <= steps; i++) {
        double x = i * step;
        double y = i * step;
        GeoPoint point(x, y);
        codes.push_back(encode_func(point));
    }
    
    cout << "\n编码值序列:" << endl;
    for (size_t i = 0; i < codes.size(); i++) {
        cout << "  步" << i << " (" << fixed << setprecision(4) << (i * step) 
             << ", " << (i * step) << "): code=" << codes[i];
        
        if (i > 0) {
            int64_t diff = static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]);
            cout << ", diff=" << diff << ", |diff|=" << abs(diff);
        }
        cout << endl;
    }
    
    // 统计差值
    vector<int64_t> diffs;
    for (size_t i = 1; i < codes.size(); i++) {
        diffs.push_back(abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1])));
    }
    
    int64_t min_diff = *min_element(diffs.begin(), diffs.end());
    int64_t max_diff = *max_element(diffs.begin(), diffs.end());
    double avg_diff = 0;
    for (auto d : diffs) avg_diff += d;
    avg_diff /= diffs.size();
    
    cout << "\n差值统计:" << endl;
    cout << "  最小差值: " << min_diff << endl;
    cout << "  最大差值: " << max_diff << endl;
    cout << "  平均差值: " << fixed << setprecision(0) << avg_diff << endl;
    
    // 判断空间局部性
    bool good_locality = (max_diff < 1000000);  // 如果最大差值小于100万，认为局部性好
    cout << "\n空间局部性: " << (good_locality ? "✓ 良好" : "✗ 差") << endl;
}

// 测试网格编码的连续性
void TestGridContinuity(const string& method_name,
                       function<uint64_t(const GeoPoint&)> encode_func) {
    cout << "\n=== " << method_name << " - 网格连续性测试 ===" << endl;
    
    // 在一个小网格内测试
    int grid_size = 4;
    double cell_size = 0.01;  // 每个格子0.01度
    
    cout << grid_size << "×" << grid_size << " 网格，格子大小=" << cell_size << "度" << endl;
    
    // 编码所有格点
    uint64_t grid[4][4];
    for (int y = 0; y < grid_size; y++) {
        for (int x = 0; x < grid_size; x++) {
            GeoPoint point(x * cell_size, y * cell_size);
            grid[y][x] = encode_func(point);
        }
    }
    
    // 显示编码值（从上到下）
    cout << "\n编码值矩阵:" << endl;
    for (int y = grid_size-1; y >= 0; y--) {
        cout << "y=" << y << ": ";
        for (int x = 0; x < grid_size; x++) {
            cout << setw(18) << grid[y][x] << " ";
        }
        cout << endl;
    }
    
    // 计算相邻格点的最大差值
    uint64_t max_neighbor_diff = 0;
    for (int y = 0; y < grid_size; y++) {
        for (int x = 0; x < grid_size; x++) {
            // 检查右邻居
            if (x < grid_size - 1) {
                uint64_t diff = abs(static_cast<int64_t>(grid[y][x+1]) - static_cast<int64_t>(grid[y][x]));
                max_neighbor_diff = max(max_neighbor_diff, diff);
            }
            // 检查上邻居
            if (y < grid_size - 1) {
                uint64_t diff = abs(static_cast<int64_t>(grid[y+1][x]) - static_cast<int64_t>(grid[y][x]));
                max_neighbor_diff = max(max_neighbor_diff, diff);
            }
        }
    }
    
    cout << "\n相邻格点最大差值: " << max_neighbor_diff << endl;
    bool good = (max_neighbor_diff < 10000000);  // 小于1000万认为合理
    cout << "评价: " << (good ? "✓ 编码连续" : "✗ 编码跳跃过大") << endl;
}

// 测试理论格子大小
void TestCellSize(const string& method_name, int bits, const BoundingBox* bbox = nullptr) {
    cout << "\n=== " << method_name << " - 格子大小分析 ===" << endl;
    cout << "编码比特数: " << bits << " bits" << endl;
    
    double lon_range = bbox ? bbox->GetWidth() : 360.0;
    double lat_range = bbox ? bbox->GetHeight() : 180.0;
    
    int lon_bits = (bits + 1) / 2;
    int lat_bits = bits / 2;
    
    int lon_cells = 1 << lon_bits;
    int lat_cells = 1 << lat_bits;
    
    double lon_cell_size = lon_range / lon_cells;
    double lat_cell_size = lat_range / lat_cells;
    
    cout << "经度: " << lon_cells << " 个格子，每格 " << scientific << setprecision(2) 
         << lon_cell_size << " 度" << endl;
    cout << "纬度: " << lat_cells << " 个格子，每格 " << lat_cell_size << " 度" << endl;
    
    // 计算理论上相邻点的编码差值
    // 对于GeoHash位交错编码，相邻格子的差值取决于具体位置
    // 最小差值通常是1（在同一行/列内移动）
    // 最大差值可能很大（跨越大范围）
    
    cout << "\n理论分析:" << endl;
    cout << "如果两个GPS点相距0.0001度，它们大约在 " 
         << (int)(0.0001 / lon_cell_size) << " 个经度格子内" << endl;
    
    // 位交错的最坏情况：需要改变高位
    uint64_t total_cells = static_cast<uint64_t>(lon_cells) * lat_cells;
    cout << "总格子数: " << total_cells << endl;
    cout << "编码值范围: 0 ~ " << (total_cells - 1) << endl;
}

int main() {
    cout << "========================================" << endl;
    cout << "空间编码局部性验证测试" << endl;
    cout << "========================================" << endl;
    
    // 准备边界框
    BoundingBox full_bbox;  // 全球
    full_bbox.min_lon = -180.0;
    full_bbox.max_lon = 180.0;
    full_bbox.min_lat = -90.0;
    full_bbox.max_lat = 90.0;
    
    BoundingBox local_bbox;  // GeoLife数据集
    local_bbox.min_lon = 116.201597;
    local_bbox.max_lon = 121.470629;
    local_bbox.min_lat = 31.167749;
    local_bbox.max_lat = 40.081138;
    
    int bits = 52;
    int order = 26;
    
    // ========================================
    // 测试1: 标准GeoHash
    // ========================================
    cout << "\n========================================" << endl;
    cout << "标准 GeoHash" << endl;
    cout << "========================================" << endl;
    
    TestCellSize("标准GeoHash", bits, nullptr);
    
    auto std_encode = [bits](const GeoPoint& p) {
        return SpaceFillingCurve::EncodeStandardGeoHash(p, bits);
    };
    
    TestLinearMovement("标准GeoHash", std_encode);
    TestGridContinuity("标准GeoHash", std_encode);
    
    // ========================================
    // 测试2: MBR-GeoHash
    // ========================================
    cout << "\n========================================" << endl;
    cout << "MBR-GeoHash (使用GeoLife边界框)" << endl;
    cout << "========================================" << endl;
    cout << "边界框: [" << fixed << setprecision(2) << local_bbox.min_lon << ", " 
         << local_bbox.max_lon << "] × [" << local_bbox.min_lat << ", " 
         << local_bbox.max_lat << "]" << endl;
    
    TestCellSize("MBR-GeoHash", bits, &local_bbox);
    
    // 创建一个归一化的bbox用于测试（将GeoLife区域映射到[0,1]×[0,1]）
    BoundingBox test_bbox;
    test_bbox.min_lon = 0.0;
    test_bbox.max_lon = 1.0;
    test_bbox.min_lat = 0.0;
    test_bbox.max_lat = 1.0;
    
    auto mbr_encode = [bits, test_bbox](const GeoPoint& p) {
        return SpaceFillingCurve::EncodeMBRGeoHash(p, bits, test_bbox);
    };
    
    TestLinearMovement("MBR-GeoHash", mbr_encode);
    TestGridContinuity("MBR-GeoHash", mbr_encode);
    
    // ========================================
    // 测试3: Hilbert曲线
    // ========================================
    cout << "\n========================================" << endl;
    cout << "Hilbert 曲线 (使用GeoLife边界框)" << endl;
    cout << "========================================" << endl;
    
    auto hilbert_encode = [order, test_bbox](const GeoPoint& p) {
        return SpaceFillingCurve::EncodeHilbert(p, order, test_bbox);
    };
    
    TestLinearMovement("Hilbert曲线", hilbert_encode);
    TestGridContinuity("Hilbert曲线", hilbert_encode);
    
    cout << "\n========================================" << endl;
    cout << "测试完成" << endl;
    cout << "========================================" << endl;
    
    return 0;
}


