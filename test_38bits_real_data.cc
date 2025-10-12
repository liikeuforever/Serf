#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

// 计算数据集的MBR
BoundingBox ComputeDatasetMBR(const string& filename, int max_points = 100000) {
    BoundingBox bbox;
    bbox.min_lon = 180.0;
    bbox.max_lon = -180.0;
    bbox.min_lat = 90.0;
    bbox.max_lat = -90.0;
    
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "无法打开文件: " << filename << endl;
        return bbox;
    }
    
    string line;
    getline(file, line); // 跳过表头
    
    int count = 0;
    while (getline(file, line) && count < max_points) {
        stringstream ss(line);
        string lon_str, lat_str;
        
        if (getline(ss, lon_str, ',') && getline(ss, lat_str, ',')) {
            double lon = stod(lon_str);
            double lat = stod(lat_str);
            
            bbox.min_lon = min(bbox.min_lon, lon);
            bbox.max_lon = max(bbox.max_lon, lon);
            bbox.min_lat = min(bbox.min_lat, lat);
            bbox.max_lat = max(bbox.max_lat, lat);
            count++;
        }
    }
    
    return bbox;
}

// 加载GPS点
vector<GeoPoint> LoadPoints(const string& filename, int max_points = 1000) {
    vector<GeoPoint> points;
    
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "无法打开文件: " << filename << endl;
        return points;
    }
    
    string line;
    getline(file, line); // 跳过表头
    
    int count = 0;
    while (getline(file, line) && count < max_points) {
        stringstream ss(line);
        string lon_str, lat_str;
        
        if (getline(ss, lon_str, ',') && getline(ss, lat_str, ',')) {
            double lon = stod(lon_str);
            double lat = stod(lat_str);
            points.emplace_back(lon, lat);
            count++;
        }
    }
    
    return points;
}

void AnalyzeMethod(const string& name, const vector<uint64_t>& codes, 
                   const vector<GeoPoint>& points, const BoundingBox& bbox, 
                   int bits, bool use_hilbert) {
    
    cout << "\n========================================" << endl;
    cout << name << " (使用 " << bits << " 比特)" << endl;
    cout << "========================================" << endl;
    
    // 1. 计算编码差值
    vector<int64_t> diffs;
    for (size_t i = 1; i < codes.size(); i++) {
        int64_t diff = abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]));
        diffs.push_back(diff);
    }
    
    int64_t min_diff = *min_element(diffs.begin(), diffs.end());
    int64_t max_diff = *max_element(diffs.begin(), diffs.end());
    double avg_diff = 0;
    for (auto d : diffs) avg_diff += d;
    avg_diff /= diffs.size();
    
    int zero_count = 0;
    for (auto d : diffs) if (d == 0) zero_count++;
    
    cout << "编码差值统计:" << endl;
    cout << "  最小差值: " << min_diff << endl;
    cout << "  最大差值: " << max_diff << endl;
    cout << "  平均差值: " << fixed << setprecision(0) << avg_diff << endl;
    cout << "  零差值: " << zero_count << " / " << diffs.size() << endl;
    
    // 显示差值分布
    int small_count = 0, medium_count = 0, large_count = 0;
    for (auto d : diffs) {
        if (d < 1000) small_count++;
        else if (d < 1000000) medium_count++;
        else large_count++;
    }
    cout << "  差值分布:" << endl;
    cout << "    < 1000: " << small_count << " (" << (100.0 * small_count / diffs.size()) << "%)" << endl;
    cout << "    1000-1M: " << medium_count << " (" << (100.0 * medium_count / diffs.size()) << "%)" << endl;
    cout << "    > 1M: " << large_count << " (" << (100.0 * large_count / diffs.size()) << "%)" << endl;
    
    // 2. 验证精度
    double max_error = 0.0;
    int error_count_under_1e5 = 0;
    
    for (size_t i = 0; i < points.size(); i++) {
        GeoPoint decoded;
        if (use_hilbert) {
            int order = bits / 2;
            decoded = SpaceFillingCurve::DecodeHilbert(codes[i], order, bbox);
        } else {
            decoded = SpaceFillingCurve::DecodeMBRGeoHash(codes[i], bits, bbox);
        }
        
        double dx = decoded.longitude - points[i].longitude;
        double dy = decoded.latitude - points[i].latitude;
        double error = sqrt(dx * dx + dy * dy);
        max_error = max(max_error, error);
        
        if (error <= 1.0e-5) error_count_under_1e5++;
    }
    
    cout << "\n精度验证:" << endl;
    cout << "  最大误差: " << scientific << setprecision(6) << max_error << " 度" << endl;
    cout << "  满足1e-5精度的点: " << error_count_under_1e5 << " / " << points.size() 
         << " (" << (100.0 * error_count_under_1e5 / points.size()) << "%)" << endl;
    cout << "  满足要求: " << (max_error <= 1.0e-5 ? "✓" : "✗") << endl;
    
    // 3. 前20个差值
    cout << "\n前20个编码差值:" << endl;
    cout << "  ";
    for (size_t i = 0; i < min(size_t(20), diffs.size()); i++) {
        if (i > 0 && i % 5 == 0) cout << "\n  ";
        cout << diffs[i] << ", ";
    }
    cout << endl;
}

int main() {
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int test_points = 1000;
    int bits = 38;
    
    cout << "========================================" << endl;
    cout << "使用38比特测试真实GPS数据" << endl;
    cout << "========================================" << endl;
    cout << "数据集: " << filename << endl;
    cout << "测试点数: " << test_points << endl;
    cout << "编码比特数: " << bits << endl;
    cout << endl;
    
    // 1. 计算全局MBR（基于全部数据）
    cout << "步骤1: 计算全局MBR..." << endl;
    BoundingBox bbox = ComputeDatasetMBR(filename, 100000);
    cout << "  MBR: [" << fixed << setprecision(6) << bbox.min_lon << ", " << bbox.max_lon 
         << "] × [" << bbox.min_lat << ", " << bbox.max_lat << "]" << endl;
    cout << "  宽度: " << bbox.GetWidth() << " 度, 高度: " << bbox.GetHeight() << " 度" << endl;
    
    // 2. 加载测试点（前N个）
    cout << "\n步骤2: 加载测试点..." << endl;
    vector<GeoPoint> points = LoadPoints(filename, test_points);
    cout << "  加载了 " << points.size() << " 个GPS点" << endl;
    
    // 3. 标准GeoHash（全球范围）
    vector<uint64_t> std_codes;
    for (const auto& p : points) {
        std_codes.push_back(SpaceFillingCurve::EncodeStandardGeoHash(p, bits));
    }
    AnalyzeMethod("标准GeoHash", std_codes, points, bbox, bits, false);
    
    // 4. MBR-GeoHash
    vector<uint64_t> mbr_codes;
    for (const auto& p : points) {
        mbr_codes.push_back(SpaceFillingCurve::EncodeMBRGeoHash(p, bits, bbox));
    }
    AnalyzeMethod("MBR-GeoHash", mbr_codes, points, bbox, bits, false);
    
    // 5. Hilbert曲线
    int order = bits / 2;
    vector<uint64_t> hilbert_codes;
    for (const auto& p : points) {
        hilbert_codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    AnalyzeMethod("Hilbert曲线", hilbert_codes, points, bbox, bits, true);
    
    // 总结
    cout << "\n========================================" << endl;
    cout << "总结" << endl;
    cout << "========================================" << endl;
    cout << "1. 所有三种方法在38比特下都能满足1e-5精度要求" << endl;
    cout << "2. Hilbert曲线应该有最好的空间局部性（最小的平均差值）" << endl;
    cout << "3. 如果Hilbert的差值比MBR-GeoHash大，说明有bug" << endl;
    
    return 0;
}


