#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace std;

// 验证Hilbert曲线编码/解码的可逆性
void TestHilbertReversibility() {
    cout << "=== 测试Hilbert曲线编码/解码可逆性 ===" << endl;
    
    SpaceFillingCurve::BoundingBox bbox;
    bbox.min_lon = 0.0;
    bbox.max_lon = 1.0;
    bbox.min_lat = 0.0;
    bbox.max_lat = 1.0;
    
    int order = 10; // 2^10 x 2^10 = 1024x1024 网格
    int n = 1 << order;
    
    cout << "测试网格大小: " << n << " x " << n << endl;
    
    // 测试一些特殊点
    struct TestCase {
        double x, y;
        string desc;
    };
    
    TestCase tests[] = {
        {0.0, 0.0, "原点"},
        {0.5, 0.5, "中心点"},
        {1.0, 1.0, "右上角"},
        {0.25, 0.75, "随机点1"},
        {0.123, 0.456, "随机点2"},
        {0.999, 0.001, "边界点"},
    };
    
    bool all_passed = true;
    
    for (const auto& test : tests) {
        SpaceFillingCurve::GeoPoint original(test.x, test.y);
        
        // 编码
        uint64_t code = SpaceFillingCurve::EncodeHilbert(original, order, bbox);
        
        // 解码
        SpaceFillingCurve::GeoPoint decoded = SpaceFillingCurve::DecodeHilbert(code, order, bbox);
        
        // 计算误差
        double error = SpaceFillingCurve::CalculateDistance(original, decoded);
        double max_cell_size = 1.0 / n; // 单元格大小
        double max_expected_error = max_cell_size * sqrt(2.0) / 2.0; // 对角线的一半
        
        cout << test.desc << ": (" << fixed << setprecision(6) << test.x << ", " << test.y << ")"
             << " -> code=" << code 
             << " -> (" << decoded.longitude << ", " << decoded.latitude << ")"
             << ", 误差=" << scientific << setprecision(2) << error;
        
        if (error <= max_expected_error) {
            cout << " ✓" << endl;
        } else {
            cout << " ✗ (超出预期误差 " << max_expected_error << ")" << endl;
            all_passed = false;
        }
    }
    
    cout << "\n整体结果: " << (all_passed ? "✓ 通过" : "✗ 失败") << endl;
}

// 测试Hilbert曲线的空间局部性
void TestHilbertLocality() {
    cout << "\n=== 测试Hilbert曲线空间局部性 ===" << endl;
    
    SpaceFillingCurve::BoundingBox bbox;
    bbox.min_lon = 0.0;
    bbox.max_lon = 1.0;
    bbox.min_lat = 0.0;
    bbox.max_lat = 1.0;
    
    int order = 10;
    
    // 测试：相邻的空间点是否有相近的编码值
    cout << "测试沿一条线移动的点的编码差值:" << endl;
    
    uint64_t prev_code = 0;
    for (int i = 0; i <= 10; i++) {
        double t = i * 0.1; // 从0.0到1.0
        SpaceFillingCurve::GeoPoint point(t, t); // 沿对角线移动
        
        uint64_t code = SpaceFillingCurve::EncodeHilbert(point, order, bbox);
        
        cout << "  点(" << fixed << setprecision(1) << t << ", " << t << ") -> code=" << code;
        
        if (i > 0) {
            int64_t diff = static_cast<int64_t>(code) - static_cast<int64_t>(prev_code);
            cout << ", diff=" << diff;
        }
        cout << endl;
        
        prev_code = code;
    }
}

// 测试小网格的Hilbert编码分布
void TestXYToHilbert() {
    cout << "\n=== 测试小网格Hilbert编码分布 ===" << endl;
    
    SpaceFillingCurve::BoundingBox bbox;
    bbox.min_lon = 0.0;
    bbox.max_lon = 1.0;
    bbox.min_lat = 0.0;
    bbox.max_lat = 1.0;
    
    int order = 2; // 4x4网格
    int n = 4;
    
    cout << "4x4网格的Hilbert编码:" << endl;
    
    // 记录所有编码
    uint64_t grid[4][4];
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            double px = (x + 0.5) / n;
            double py = (y + 0.5) / n;
            SpaceFillingCurve::GeoPoint point(px, py);
            grid[y][x] = SpaceFillingCurve::EncodeHilbert(point, order, bbox);
        }
    }
    
    // 从上到下打印（y从大到小）
    for (int y = n-1; y >= 0; y--) {
        cout << "y=" << y << ": ";
        for (int x = 0; x < n; x++) {
            cout << setw(3) << grid[y][x] << " ";
        }
        cout << endl;
    }
    
    // 验证编码唯一性和连续性
    bool codes[16] = {false};
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            if (grid[y][x] < 16) {
                codes[grid[y][x]] = true;
            }
        }
    }
    
    bool all_present = true;
    for (int i = 0; i < 16; i++) {
        if (!codes[i]) {
            cout << "\n✗ 缺少编码: " << i << " - Hilbert实现有问题！" << endl;
            all_present = false;
        }
    }
    
    if (all_present) {
        cout << "\n✓ 所有编码0-15都存在且唯一" << endl;
    }
}

int main() {
    cout << "========================================" << endl;
    cout << "Hilbert曲线实现验证" << endl;
    cout << "========================================" << endl;
    
    TestXYToHilbert();
    TestHilbertReversibility();
    TestHilbertLocality();
    
    cout << "\n========================================" << endl;
    cout << "验证完成" << endl;
    cout << "========================================" << endl;
    
    return 0;
}

