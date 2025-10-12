#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

// 加载GPS点
vector<GeoPoint> LoadPoints(const string& filename, int max_points = 1000) {
    vector<GeoPoint> points;
    ifstream file(filename);
    if (!file.is_open()) return points;
    
    string line;
    getline(file, line);
    
    int count = 0;
    while (getline(file, line) && count < max_points) {
        stringstream ss(line);
        string lon_str, lat_str;
        if (getline(ss, lon_str, ',') && getline(ss, lat_str, ',')) {
            points.emplace_back(stod(lon_str), stod(lat_str));
            count++;
        }
    }
    return points;
}

// 测试1：编码-解码往返精度测试
void TestEncodeDecode() {
    cout << "\n========================================" << endl;
    cout << "测试1: Hilbert编码-解码往返精度" << endl;
    cout << "========================================" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    
    int encoding_bits = 40;
    int order = encoding_bits / 2;
    
    // 测试一些已知点
    vector<GeoPoint> test_points = {
        GeoPoint(116.3, 39.9),
        GeoPoint(116.31, 39.91),
        GeoPoint(116.32, 39.92),
        GeoPoint(120.0, 35.0),
        GeoPoint(121.0, 40.0)
    };
    
    double max_error = 0.0;
    bool all_pass = true;
    
    for (size_t i = 0; i < test_points.size(); i++) {
        const auto& p = test_points[i];
        
        // 编码
        uint64_t code = SpaceFillingCurve::EncodeHilbert(p, order, bbox);
        
        // 解码
        GeoPoint decoded = SpaceFillingCurve::DecodeHilbert(code, order, bbox);
        
        // 计算误差
        double dx = decoded.longitude - p.longitude;
        double dy = decoded.latitude - p.latitude;
        double error = sqrt(dx * dx + dy * dy);
        max_error = max(max_error, error);
        
        bool pass = error <= 1e-5;
        
        cout << "点 " << i << ": (" << fixed << setprecision(6) << p.longitude << ", " << p.latitude << ")" << endl;
        cout << "  编码: " << code << endl;
        cout << "  解码: (" << decoded.longitude << ", " << decoded.latitude << ")" << endl;
        cout << "  误差: " << scientific << setprecision(6) << error << " 度 " 
             << (pass ? "✓" : "✗") << endl;
        
        if (!pass) all_pass = false;
    }
    
    cout << "\n最大误差: " << scientific << max_error << " 度" << endl;
    cout << "精度测试: " << (all_pass ? "✓ 通过" : "✗ 失败") << endl;
}

// 测试2：空间局部性验证
void TestSpatialLocality() {
    cout << "\n========================================" << endl;
    cout << "测试2: Hilbert空间局部性验证" << endl;
    cout << "========================================" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 0.0;
    bbox.max_lon = 1.0;
    bbox.min_lat = 0.0;
    bbox.max_lat = 1.0;
    
    int order = 4;  // 16x16网格
    int n = 1 << order;
    
    cout << "使用 " << n << "x" << n << " 网格测试\n" << endl;
    
    // 创建网格点并编码
    vector<tuple<uint64_t, int, int>> encoded_points;
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            double px = (x + 0.5) / n;
            double py = (y + 0.5) / n;
            GeoPoint p(px, py);
            uint64_t code = SpaceFillingCurve::EncodeHilbert(p, order, bbox);
            encoded_points.push_back({code, x, y});
        }
    }
    
    // 按编码值排序
    sort(encoded_points.begin(), encoded_points.end());
    
    // 检查连续性：相邻编码的点在空间上应该也相邻
    int adjacent_count = 0;
    int total_count = encoded_points.size() - 1;
    
    for (size_t i = 1; i < encoded_points.size(); i++) {
        auto [code1, x1, y1] = encoded_points[i-1];
        auto [code2, x2, y2] = encoded_points[i];
        
        // 曼哈顿距离
        int manhattan = abs(x2 - x1) + abs(y2 - y1);
        
        if (manhattan == 1) {
            adjacent_count++;
        } else if (i <= 20) {
            // 显示前20个不相邻的情况
            cout << "  编码 " << code1 << "->  " << code2 
                 << ": (" << x1 << "," << y1 << ") -> (" << x2 << "," << y2 
                 << ") 距离=" << manhattan << endl;
        }
    }
    
    double adjacent_ratio = 100.0 * adjacent_count / total_count;
    cout << "\n相邻性统计:" << endl;
    cout << "  相邻的点对: " << adjacent_count << " / " << total_count 
         << " (" << fixed << setprecision(1) << adjacent_ratio << "%)" << endl;
    
    // Hilbert曲线应该有很高的相邻性（通常>90%）
    bool pass = adjacent_ratio > 90.0;
    cout << "  空间局部性: " << (pass ? "✓ 良好" : "✗ 较差") << endl;
}

// 测试3：真实GPS数据编码差值分析
void TestRealGPSDifferences(const string& filename) {
    cout << "\n========================================" << endl;
    cout << "测试3: 真实GPS数据编码差值分析" << endl;
    cout << "========================================" << endl;
    
    vector<GeoPoint> points = LoadPoints(filename, 1000);
    if (points.empty()) {
        cout << "无法加载数据" << endl;
        return;
    }
    
    cout << "加载了 " << points.size() << " 个GPS点\n" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    
    int encoding_bits = 40;
    int order = encoding_bits / 2;
    
    // 编码所有点
    vector<uint64_t> codes;
    for (const auto& p : points) {
        codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    
    // 计算差值
    vector<int64_t> diffs;
    for (size_t i = 1; i < codes.size(); i++) {
        int64_t diff = static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]);
        diffs.push_back(diff);
    }
    
    // 统计差值分布
    int64_t min_diff = *min_element(diffs.begin(), diffs.end());
    int64_t max_diff = *max_element(diffs.begin(), diffs.end());
    double avg_abs_diff = 0;
    for (auto d : diffs) avg_abs_diff += abs(d);
    avg_abs_diff /= diffs.size();
    
    cout << "差值统计:" << endl;
    cout << "  最小差值: " << min_diff << endl;
    cout << "  最大差值: " << max_diff << endl;
    cout << "  平均|差值|: " << fixed << setprecision(0) << avg_abs_diff << endl;
    
    // 差值分布
    int small_diff = 0, medium_diff = 0, large_diff = 0;
    for (auto d : diffs) {
        int64_t abs_d = abs(d);
        if (abs_d < 1000) small_diff++;
        else if (abs_d < 1000000) medium_diff++;
        else large_diff++;
    }
    
    cout << "\n差值分布:" << endl;
    cout << "  < 1K: " << small_diff << " (" 
         << fixed << setprecision(1) << (100.0 * small_diff / diffs.size()) << "%)" << endl;
    cout << "  1K-1M: " << medium_diff << " (" 
         << (100.0 * medium_diff / diffs.size()) << "%)" << endl;
    cout << "  > 1M: " << large_diff << " (" 
         << (100.0 * large_diff / diffs.size()) << "%)" << endl;
    
    // 显示前20个点的详细信息
    cout << "\n前20个点的详细信息:" << endl;
    cout << "索引 | 经度      | 纬度     | Hilbert编码      | 差值" << endl;
    cout << "-----+-----------+----------+------------------+--------------" << endl;
    for (size_t i = 0; i < min(size_t(20), points.size()); i++) {
        cout << setw(4) << i << " | "
             << fixed << setprecision(5) << setw(9) << points[i].longitude << " | "
             << setw(8) << points[i].latitude << " | "
             << setw(16) << codes[i] << " | ";
        if (i > 0) {
            cout << setw(12) << (static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]));
        } else {
            cout << "      -";
        }
        cout << endl;
    }
    
    // 计算相邻点的实际距离
    cout << "\n相邻点的空间距离分析:" << endl;
    double total_geo_dist = 0;
    for (size_t i = 1; i < min(size_t(20), points.size()); i++) {
        double dx = points[i].longitude - points[i-1].longitude;
        double dy = points[i].latitude - points[i-1].latitude;
        double geo_dist = sqrt(dx * dx + dy * dy);
        total_geo_dist += geo_dist;
        
        int64_t code_diff = abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]));
        
        cout << "  点 " << i << ": 地理距离=" << scientific << setprecision(3) << geo_dist 
             << " 度, 编码差=" << fixed << setprecision(0) << code_diff << endl;
    }
    
    double avg_geo_dist = total_geo_dist / 19;
    cout << "\n平均地理距离: " << scientific << setprecision(3) << avg_geo_dist << " 度" << endl;
    
    // 评估
    cout << "\n评估:" << endl;
    if (avg_abs_diff < 100000) {
        cout << "  ✓ 编码差值较小，空间局部性良好" << endl;
    } else {
        cout << "  ⚠ 编码差值较大，可能存在以下原因：" << endl;
        cout << "    - GPS轨迹有跳跃（不连续的轨迹段）" << endl;
        cout << "    - 编码比特数过高导致精度过细" << endl;
        cout << "    - 数据集跨越较大的地理范围" << endl;
    }
}

// 测试4：对比不同编码比特数
void TestDifferentBits(const string& filename) {
    cout << "\n========================================" << endl;
    cout << "测试4: 不同编码比特数的影响" << endl;
    cout << "========================================" << endl;
    
    vector<GeoPoint> points = LoadPoints(filename, 100);
    if (points.empty()) return;
    
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    
    vector<int> bit_counts = {20, 30, 40};
    
    cout << "使用前100个GPS点测试\n" << endl;
    cout << "编码位数 | 平均|差值| | 最大|差值| | 网格大小" << endl;
    cout << "---------+-----------+------------+-----------" << endl;
    
    for (int bits : bit_counts) {
        int order = bits / 2;
        int grid_size = 1 << order;
        
        vector<uint64_t> codes;
        for (const auto& p : points) {
            codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
        }
        
        double avg_diff = 0;
        int64_t max_diff = 0;
        for (size_t i = 1; i < codes.size(); i++) {
            int64_t diff = abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]));
            avg_diff += diff;
            max_diff = max(max_diff, diff);
        }
        avg_diff /= (codes.size() - 1);
        
        cout << setw(8) << bits << " | "
             << fixed << setprecision(0) << setw(9) << avg_diff << " | "
             << setw(10) << max_diff << " | "
             << setw(5) << grid_size << "x" << grid_size << endl;
    }
    
    cout << "\n观察：编码位数越高，网格越细，差值可能越大" << endl;
}

int main() {
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    
    cout << "========================================" << endl;
    cout << "Hilbert编码正确性完整验证" << endl;
    cout << "========================================" << endl;
    
    // 测试1：编码解码精度
    TestEncodeDecode();
    
    // 测试2：空间局部性
    TestSpatialLocality();
    
    // 测试3：真实GPS数据
    TestRealGPSDifferences(filename);
    
    // 测试4：不同比特数
    TestDifferentBits(filename);
    
    cout << "\n========================================" << endl;
    cout << "总结" << endl;
    cout << "========================================" << endl;
    cout << "如果所有测试通过，说明Hilbert编码实现是正确的。" << endl;
    cout << "如果压缩效果不如Serf-QT，原因是：" << endl;
    cout << "  • GPS轨迹的时间连续性 > 空间局部性" << endl;
    cout << "  • Hilbert编码差值虽小，但仍大于直接的经纬度差值" << endl;
    cout << "  • 2D->1D映射不可避免地引入额外信息" << endl;
    
    return 0;
}


