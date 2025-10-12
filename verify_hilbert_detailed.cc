#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include "src/compressor/space_filling_curve.h"

// 打印希尔伯特曲线网格
void PrintHilbertGrid(int order) {
    int n = 1 << order;
    std::cout << "\n=== " << n << "x" << n << " 希尔伯特曲线网格 ===" << std::endl;
    
    // 创建网格
    std::vector<std::vector<uint64_t>> grid(n, std::vector<uint64_t>(n, 0));
    
    // 填充网格
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            grid[y][x] = SpaceFillingCurve::XYToHilbert(n, x, y);
        }
    }
    
    // 打印网格（从上到下，y从大到小）
    for (int y = n - 1; y >= 0; y--) {
        for (int x = 0; x < n; x++) {
            std::cout << std::setw(4) << grid[y][x];
        }
        std::cout << std::endl;
    }
}

// 验证编码解码的一致性
bool VerifyEncodeDecode(int order) {
    int n = 1 << order;
    int total_cells = n * n;
    int errors = 0;
    
    std::cout << "\n=== 验证 " << order << " 阶希尔伯特曲线编码/解码 ===" << std::endl;
    
    // 测试每个坐标点
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            // 编码
            uint64_t d = SpaceFillingCurve::XYToHilbert(n, x, y);
            
            // 解码
            int decoded_x, decoded_y;
            SpaceFillingCurve::HilbertToXY(n, d, &decoded_x, &decoded_y);
            
            // 检查是否一致
            if (decoded_x != x || decoded_y != y) {
                std::cout << "❌ 错误: (" << x << "," << y << ") -> d=" << d 
                          << " -> (" << decoded_x << "," << decoded_y << ")" << std::endl;
                errors++;
            }
        }
    }
    
    if (errors == 0) {
        std::cout << "✅ 所有 " << total_cells << " 个点编码/解码一致！" << std::endl;
        return true;
    } else {
        std::cout << "❌ 发现 " << errors << " 个错误！" << std::endl;
        return false;
    }
}

// 验证希尔伯特曲线的连续性（相邻的d值对应相邻的坐标）
bool VerifyHilbertContinuity(int order) {
    int n = 1 << order;
    int total_cells = n * n;
    
    std::cout << "\n=== 验证 " << order << " 阶希尔伯特曲线连续性 ===" << std::endl;
    
    std::vector<std::pair<int, int>> coords(total_cells);
    
    // 获取每个d值对应的坐标
    for (uint64_t d = 0; d < total_cells; d++) {
        int x, y;
        SpaceFillingCurve::HilbertToXY(n, d, &x, &y);
        coords[d] = {x, y};
    }
    
    // 检查相邻的d值是否对应相邻的坐标（曼哈顿距离=1）
    int discontinuities = 0;
    for (uint64_t d = 0; d < total_cells - 1; d++) {
        int x1 = coords[d].first, y1 = coords[d].second;
        int x2 = coords[d+1].first, y2 = coords[d+1].second;
        
        int manhattan_dist = std::abs(x2 - x1) + std::abs(y2 - y1);
        
        if (manhattan_dist != 1) {
            if (discontinuities < 10) {  // 只打印前10个
                std::cout << "❌ 不连续: d=" << d << " (" << x1 << "," << y1 
                          << ") -> d=" << (d+1) << " (" << x2 << "," << y2 
                          << "), 曼哈顿距离=" << manhattan_dist << std::endl;
            }
            discontinuities++;
        }
    }
    
    if (discontinuities == 0) {
        std::cout << "✅ 曲线完全连续！所有 " << (total_cells-1) << " 个相邻点对都是相邻的。" << std::endl;
        return true;
    } else {
        std::cout << "❌ 发现 " << discontinuities << " 个不连续点！" << std::endl;
        return false;
    }
}

// 验证希尔伯特曲线的唯一性（每个d值唯一对应一个坐标）
bool VerifyHilbertUniqueness(int order) {
    int n = 1 << order;
    int total_cells = n * n;
    
    std::cout << "\n=== 验证 " << order << " 阶希尔伯特曲线唯一性 ===" << std::endl;
    
    std::vector<bool> d_used(total_cells, false);
    int duplicates = 0;
    
    // 检查每个坐标产生的d值是否唯一
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            uint64_t d = SpaceFillingCurve::XYToHilbert(n, x, y);
            
            if (d >= total_cells) {
                std::cout << "❌ d值越界: (" << x << "," << y << ") -> d=" << d 
                          << " (应该 < " << total_cells << ")" << std::endl;
                duplicates++;
            } else if (d_used[d]) {
                std::cout << "❌ d值重复: (" << x << "," << y << ") -> d=" << d << std::endl;
                duplicates++;
            } else {
                d_used[d] = true;
            }
        }
    }
    
    // 检查是否所有d值都被使用
    int missing = 0;
    for (uint64_t d = 0; d < total_cells; d++) {
        if (!d_used[d]) {
            if (missing < 10) {  // 只打印前10个
                std::cout << "❌ d值缺失: d=" << d << " 未被任何坐标映射" << std::endl;
            }
            missing++;
        }
    }
    
    if (duplicates == 0 && missing == 0) {
        std::cout << "✅ 所有 d 值唯一且完整！" << std::endl;
        return true;
    } else {
        std::cout << "❌ 发现 " << duplicates << " 个重复和 " << missing << " 个缺失！" << std::endl;
        return false;
    }
}

// 测试实际经纬度编码
void TestGeoEncoding() {
    std::cout << "\n=== 测试实际经纬度编码 ===" << std::endl;
    
    // 创建一个小范围的边界框
    SpaceFillingCurve::BoundingBox bbox;
    bbox.min_lon = 116.0;
    bbox.max_lon = 117.0;
    bbox.min_lat = 39.0;
    bbox.max_lat = 40.0;
    
    int order = 4;  // 16x16 网格
    
    std::cout << "边界框: [" << bbox.min_lon << ", " << bbox.max_lon << "] x [" 
              << bbox.min_lat << ", " << bbox.max_lat << "]" << std::endl;
    std::cout << "阶数: " << order << " (网格: " << (1 << order) << "x" << (1 << order) << ")" << std::endl;
    
    // 测试几个点
    std::vector<SpaceFillingCurve::GeoPoint> test_points = {
        {116.0, 39.0},     // 左下角
        {117.0, 40.0},     // 右上角
        {116.5, 39.5},     // 中心
        {116.25, 39.25},   // 四分之一点
        {116.75, 39.75}    // 四分之三点
    };
    
    std::cout << "\n测试点编码/解码:" << std::endl;
    for (const auto& point : test_points) {
        uint64_t h = SpaceFillingCurve::EncodeHilbert(point, order, bbox);
        auto decoded = SpaceFillingCurve::DecodeHilbert(h, order, bbox);
        
        double error = SpaceFillingCurve::CalculateDistance(point, decoded);
        
        std::cout << "原始: (" << std::fixed << std::setprecision(6) 
                  << point.longitude << ", " << point.latitude << ")"
                  << " -> h=" << h
                  << " -> 解码: (" << decoded.longitude << ", " << decoded.latitude << ")"
                  << " 误差=" << std::scientific << error << " 度" << std::endl;
    }
}

// 测试空间局部性
void TestSpatialLocality() {
    std::cout << "\n=== 测试空间局部性 ===" << std::endl;
    
    SpaceFillingCurve::BoundingBox bbox;
    bbox.min_lon = 116.0;
    bbox.max_lon = 117.0;
    bbox.min_lat = 39.0;
    bbox.max_lat = 40.0;
    
    int order = 10;  // 1024x1024 网格
    
    // 测试两个空间上相近的点
    SpaceFillingCurve::GeoPoint p1(116.5, 39.5);
    SpaceFillingCurve::GeoPoint p2(116.5001, 39.5001);  // 非常接近
    
    uint64_t h1 = SpaceFillingCurve::EncodeHilbert(p1, order, bbox);
    uint64_t h2 = SpaceFillingCurve::EncodeHilbert(p2, order, bbox);
    
    double spatial_dist = SpaceFillingCurve::CalculateDistance(p1, p2);
    int64_t hilbert_dist = std::abs(static_cast<int64_t>(h1) - static_cast<int64_t>(h2));
    
    std::cout << "点1: (" << p1.longitude << ", " << p1.latitude << ") -> h=" << h1 << std::endl;
    std::cout << "点2: (" << p2.longitude << ", " << p2.latitude << ") -> h=" << h2 << std::endl;
    std::cout << "空间距离: " << std::scientific << spatial_dist << " 度" << std::endl;
    std::cout << "希尔伯特距离: " << hilbert_dist << std::endl;
    
    // 测试两个空间上相远的点
    SpaceFillingCurve::GeoPoint p3(116.1, 39.1);
    SpaceFillingCurve::GeoPoint p4(116.9, 39.9);
    
    uint64_t h3 = SpaceFillingCurve::EncodeHilbert(p3, order, bbox);
    uint64_t h4 = SpaceFillingCurve::EncodeHilbert(p4, order, bbox);
    
    double spatial_dist2 = SpaceFillingCurve::CalculateDistance(p3, p4);
    int64_t hilbert_dist2 = std::abs(static_cast<int64_t>(h3) - static_cast<int64_t>(h4));
    
    std::cout << "\n点3: (" << p3.longitude << ", " << p3.latitude << ") -> h=" << h3 << std::endl;
    std::cout << "点4: (" << p4.longitude << ", " << p4.latitude << ") -> h=" << h4 << std::endl;
    std::cout << "空间距离: " << std::scientific << spatial_dist2 << " 度" << std::endl;
    std::cout << "希尔伯特距离: " << hilbert_dist2 << std::endl;
    
    if (spatial_dist < spatial_dist2 && hilbert_dist < hilbert_dist2) {
        std::cout << "\n✅ 空间局部性验证通过：近的点希尔伯特距离小，远的点希尔伯特距离大" << std::endl;
    } else {
        std::cout << "\n⚠️  空间局部性可能有问题" << std::endl;
    }
}

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "   希尔伯特曲线编码详细验证" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    // 1. 打印小网格可视化
    PrintHilbertGrid(2);  // 4x4
    PrintHilbertGrid(3);  // 8x8
    
    // 2. 验证编码解码一致性
    bool test1 = VerifyEncodeDecode(2);
    bool test2 = VerifyEncodeDecode(4);
    bool test3 = VerifyEncodeDecode(8);
    
    // 3. 验证唯一性
    bool test4 = VerifyHilbertUniqueness(2);
    bool test5 = VerifyHilbertUniqueness(4);
    bool test6 = VerifyHilbertUniqueness(8);
    
    // 4. 验证连续性
    bool test7 = VerifyHilbertContinuity(2);
    bool test8 = VerifyHilbertContinuity(4);
    bool test9 = VerifyHilbertContinuity(8);
    
    // 5. 测试实际经纬度编码
    TestGeoEncoding();
    
    // 6. 测试空间局部性
    TestSpatialLocality();
    
    // 总结
    std::cout << "\n=====================================" << std::endl;
    std::cout << "             测试总结" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    bool all_passed = test1 && test2 && test3 && test4 && test5 && test6 && test7 && test8 && test9;
    
    if (all_passed) {
        std::cout << "✅ 所有测试通过！希尔伯特曲线编码正确。" << std::endl;
    } else {
        std::cout << "❌ 部分测试失败！请检查希尔伯特曲线实现。" << std::endl;
    }
    
    return all_passed ? 0 : 1;
}

