#include "compressor/space_filling_curve.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

// 先扫描所有点获取边界框
BoundingBox ComputeBoundingBox(const string& filepath) {
    BoundingBox bbox;
    ifstream file(filepath);
    
    if (!file.is_open()) {
        cerr << "无法打开文件: " << filepath << endl;
        return bbox;
    }
    
    string line;
    int count = 0;
    while (getline(file, line)) {
        istringstream iss(line);
        string lon_str, lat_str;
        
        if (getline(iss, lon_str, ',') && getline(iss, lat_str, ',')) {
            try {
                double lon = stod(lon_str);
                double lat = stod(lat_str);
                GeoPoint point(lon, lat);
                bbox.Update(point);
                count++;
            } catch (...) {
                continue;
            }
        }
    }
    
    file.close();
    cout << "扫描了 " << count << " 个点以计算边界框" << endl;
    return bbox;
}

vector<GeoPoint> LoadData(const string& filepath, int max_points = 100) {
    vector<GeoPoint> points;
    ifstream file(filepath);
    
    if (!file.is_open()) {
        cerr << "无法打开文件: " << filepath << endl;
        return points;
    }
    
    string line;
    int count = 0;
    while (getline(file, line) && count < max_points) {
        istringstream iss(line);
        string lon_str, lat_str;
        
        if (getline(iss, lon_str, ',') && getline(iss, lat_str, ',')) {
            try {
                double lon = stod(lon_str);
                double lat = stod(lat_str);
                points.emplace_back(lon, lat);
                count++;
            } catch (...) {
                continue;
            }
        }
    }
    
    file.close();
    return points;
}

void AnalyzeAdjacentDifferences(const vector<GeoPoint>& points, const string& method_name,
                                 const vector<uint64_t>& codes) {
    cout << "\n=== " << method_name << " ===" << endl;
    
    // 计算相邻差值
    vector<int64_t> diffs;
    for (size_t i = 1; i < codes.size(); i++) {
        int64_t diff = static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]);
        diffs.push_back(diff);
    }
    
    // 统计
    int64_t sum_abs = 0;
    int64_t max_abs = 0;
    int64_t min_abs = abs(diffs[0]);
    int zero_count = 0;
    
    for (int64_t diff : diffs) {
        int64_t abs_diff = abs(diff);
        sum_abs += abs_diff;
        max_abs = max(max_abs, abs_diff);
        min_abs = min(min_abs, abs_diff);
        if (diff == 0) zero_count++;
    }
    
    double avg_abs = static_cast<double>(sum_abs) / diffs.size();
    
    cout << "差值统计 (前" << diffs.size() << "个差值):" << endl;
    cout << "  最小绝对差值: " << min_abs << endl;
    cout << "  最大绝对差值: " << max_abs << endl;
    cout << "  平均绝对差值: " << fixed << setprecision(0) << avg_abs << endl;
    cout << "  零差值数量: " << zero_count << " (" << setprecision(1) 
         << (100.0 * zero_count / diffs.size()) << "%)" << endl;
    
    // 显示前20个差值
    cout << "\n前20个相邻点的编码差值:" << endl;
    for (size_t i = 0; i < min(size_t(20), diffs.size()); i++) {
        // 计算实际空间距离
        double spatial_dist = SpaceFillingCurve::CalculateDistance(points[i], points[i+1]);
        
        cout << "  " << setw(2) << i << "→" << setw(2) << (i+1) 
             << ": diff=" << setw(15) << diffs[i]
             << ", |diff|=" << setw(15) << abs(diffs[i])
             << ", 空间距离=" << scientific << setprecision(2) << spatial_dist << " 度";
        
        if (diffs[i] == 0) cout << " [零差值]";
        cout << endl;
    }
    
    // 差值分布
    vector<int> bins(10, 0);  // 分成10个区间
    for (int64_t diff : diffs) {
        int64_t abs_diff = abs(diff);
        if (abs_diff == 0) {
            bins[0]++;
        } else {
            int bin = min(9, static_cast<int>(log10(static_cast<double>(abs_diff)) + 1));
            bins[bin]++;
        }
    }
    
    cout << "\n差值绝对值分布:" << endl;
    cout << "  0          : " << bins[0] << endl;
    for (int i = 1; i < 10; i++) {
        if (bins[i] > 0) {
            cout << "  10^" << (i-1) << " - 10^" << i << ": " << bins[i] << endl;
        }
    }
}

void TestStandardGeoHash(const vector<GeoPoint>& points) {
    int encoding_bits = 52;
    vector<uint64_t> codes;
    
    cout << "\n编码前10个点 (标准GeoHash):" << endl;
    for (size_t i = 0; i < min(size_t(10), points.size()); i++) {
        uint64_t code = SpaceFillingCurve::EncodeStandardGeoHash(points[i], encoding_bits);
        codes.push_back(code);
        
        cout << "  点" << i << ": (" << fixed << setprecision(6) 
             << points[i].longitude << ", " << points[i].latitude 
             << ") -> " << code << endl;
    }
    
    // 编码剩余点
    for (size_t i = 10; i < points.size(); i++) {
        codes.push_back(SpaceFillingCurve::EncodeStandardGeoHash(points[i], encoding_bits));
    }
    
    AnalyzeAdjacentDifferences(points, "标准 GeoHash (全球坐标)", codes);
}

void TestMBRGeoHash(const vector<GeoPoint>& points, const BoundingBox& bbox) {
    int encoding_bits = 52;
    vector<uint64_t> codes;
    
    cout << "\n编码前10个点 (MBR-GeoHash):" << endl;
    for (size_t i = 0; i < min(size_t(10), points.size()); i++) {
        uint64_t code = SpaceFillingCurve::EncodeMBRGeoHash(points[i], encoding_bits, bbox);
        codes.push_back(code);
        
        cout << "  点" << i << ": (" << fixed << setprecision(6) 
             << points[i].longitude << ", " << points[i].latitude 
             << ") -> " << code << endl;
    }
    
    for (size_t i = 10; i < points.size(); i++) {
        codes.push_back(SpaceFillingCurve::EncodeMBRGeoHash(points[i], encoding_bits, bbox));
    }
    
    AnalyzeAdjacentDifferences(points, "MBR-GeoHash (边界框优化)", codes);
}

void TestHilbert(const vector<GeoPoint>& points, const BoundingBox& bbox) {
    int order = 26;  // 52 bits total
    vector<uint64_t> codes;
    
    cout << "\n编码前10个点 (Hilbert曲线):" << endl;
    for (size_t i = 0; i < min(size_t(10), points.size()); i++) {
        uint64_t code = SpaceFillingCurve::EncodeHilbert(points[i], order, bbox);
        codes.push_back(code);
        
        cout << "  点" << i << ": (" << fixed << setprecision(6) 
             << points[i].longitude << ", " << points[i].latitude 
             << ") -> " << code << endl;
    }
    
    for (size_t i = 10; i < points.size(); i++) {
        codes.push_back(SpaceFillingCurve::EncodeHilbert(points[i], order, bbox));
    }
    
    AnalyzeAdjacentDifferences(points, "Hilbert 曲线 (边界框优化)", codes);
}

int main() {
    cout << "========================================" << endl;
    cout << "相邻GPS点编码差值分析" << endl;
    cout << "========================================" << endl;
    
    string data_file = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    
    // 第一步：扫描所有点获取正确的边界框
    cout << "\n第一步：扫描所有数据点计算边界框..." << endl;
    BoundingBox bbox = ComputeBoundingBox(data_file);
    
    cout << "\n完整数据集的边界框:" << endl;
    cout << "  经度: [" << fixed << setprecision(6) << bbox.min_lon << ", " << bbox.max_lon << "]" << endl;
    cout << "  纬度: [" << bbox.min_lat << ", " << bbox.max_lat << "]" << endl;
    cout << "  宽度: " << bbox.GetWidth() << " 度" << endl;
    cout << "  高度: " << bbox.GetHeight() << " 度" << endl;
    cout << "  宽高比: " << setprecision(2) << (bbox.GetWidth() / bbox.GetHeight()) << endl;
    
    // 第二步：加载前N个点进行测试
    cout << "\n第二步：加载前100个点进行编码测试..." << endl;
    auto points = LoadData(data_file, 100);
    
    if (points.empty()) {
        cerr << "无法加载数据!" << endl;
        return 1;
    }
    
    cout << "成功加载 " << points.size() << " 个点" << endl;
    
    // 显示前几个点的实际坐标
    cout << "\n前5个点的坐标:" << endl;
    for (size_t i = 0; i < min(size_t(5), points.size()); i++) {
        cout << "  点" << i << ": (" << fixed << setprecision(6)
             << points[i].longitude << ", " << points[i].latitude << ")";
        
        if (i > 0) {
            double dist = SpaceFillingCurve::CalculateDistance(points[i-1], points[i]);
            cout << " [距上一点: " << scientific << setprecision(2) << dist << " 度]";
        }
        cout << endl;
    }
    
    // 测试三种方法（使用完整数据集的边界框）
    TestStandardGeoHash(points);
    TestMBRGeoHash(points, bbox);
    TestHilbert(points, bbox);
    
    cout << "\n========================================" << endl;
    cout << "分析完成" << endl;
    cout << "========================================" << endl;
    
    return 0;
}

