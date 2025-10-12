#include "compressor/space_filling_curve.h"
#include "utils/zig_zag_codec.h"
#include "utils/double.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

vector<GeoPoint> LoadPoints(const string& filename, int max_points) {
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

int main() {
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int display_count = 100;
    
    cout << "========================================" << endl;
    cout << "前" << display_count << "个GPS点的中间编码值对比" << endl;
    cout << "========================================\n" << endl;
    
    vector<GeoPoint> points = LoadPoints(filename, display_count);
    if (points.empty()) {
        cerr << "无法加载数据" << endl;
        return 1;
    }
    
    // ============ Hilbert 编码分析 ============
    cout << "【方案1】Hilbert曲线编码 (38位)" << endl;
    cout << "========================================" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    int encoding_bits = 38;
    int order = encoding_bits / 2;
    
    vector<uint64_t> hilbert_codes;
    for (const auto& p : points) {
        hilbert_codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    
    cout << "\nIdx | 经度       | 纬度      | Hilbert编码      | Delta差值      | ZigZag编码" << endl;
    cout << "----+------------+-----------+------------------+----------------+---------------" << endl;
    
    for (size_t i = 0; i < min(size_t(20), hilbert_codes.size()); i++) {
        cout << setw(3) << i << " | "
             << fixed << setprecision(5) << setw(10) << points[i].longitude << " | "
             << setw(9) << points[i].latitude << " | "
             << setw(16) << hilbert_codes[i] << " | ";
        
        if (i > 0) {
            int64_t diff = static_cast<int64_t>(hilbert_codes[i]) - static_cast<int64_t>(hilbert_codes[i-1]);
            uint64_t zigzag = ZigZagCodec::Encode(diff);
            cout << setw(14) << diff << " | " << setw(13) << zigzag;
        } else {
            cout << "      -       | " << "      -";
        }
        cout << endl;
    }
    
    // 统计Hilbert差值分布
    vector<int64_t> hilbert_diffs;
    for (size_t i = 1; i < hilbert_codes.size(); i++) {
        int64_t diff = static_cast<int64_t>(hilbert_codes[i]) - static_cast<int64_t>(hilbert_codes[i-1]);
        hilbert_diffs.push_back(diff);
    }
    
    int64_t h_min = *min_element(hilbert_diffs.begin(), hilbert_diffs.end());
    int64_t h_max = *max_element(hilbert_diffs.begin(), hilbert_diffs.end());
    double h_avg = 0;
    for (auto d : hilbert_diffs) h_avg += abs(d);
    h_avg /= hilbert_diffs.size();
    
    cout << "\nHilbert差值统计 (前" << display_count << "个点):" << endl;
    cout << "  最小差值: " << h_min << endl;
    cout << "  最大差值: " << h_max << endl;
    cout << "  平均|差值|: " << fixed << setprecision(0) << h_avg << endl;
    
    // ============ Serf-QT 编码分析 ============
    cout << "\n\n【方案2】Serf-QT编码 (分别编码经纬度)" << endl;
    cout << "========================================" << endl;
    
    double max_error = 1.0e-5;
    
    // 经度编码
    vector<double> longitudes;
    vector<int64_t> lon_quantized;
    vector<int64_t> lon_diffs;
    
    for (const auto& p : points) {
        longitudes.push_back(p.longitude);
    }
    
    double pre_lon = 2.0;  // Serf-QT的初始值
    for (size_t i = 0; i < longitudes.size(); i++) {
        int64_t q = static_cast<int64_t>(round((longitudes[i] - pre_lon) / (2 * max_error)));
        lon_quantized.push_back(q);
        lon_diffs.push_back(q);
        double recover = pre_lon + 2 * max_error * static_cast<double>(q);
        pre_lon = recover;
    }
    
    // 纬度编码
    vector<double> latitudes;
    vector<int64_t> lat_quantized;
    vector<int64_t> lat_diffs;
    
    for (const auto& p : points) {
        latitudes.push_back(p.latitude);
    }
    
    double pre_lat = 2.0;
    for (size_t i = 0; i < latitudes.size(); i++) {
        int64_t q = static_cast<int64_t>(round((latitudes[i] - pre_lat) / (2 * max_error)));
        lat_quantized.push_back(q);
        lat_diffs.push_back(q);
        double recover = pre_lat + 2 * max_error * static_cast<double>(q);
        pre_lat = recover;
    }
    
    cout << "\n【经度序列】" << endl;
    cout << "Idx | 原始经度   | 量化值(q) | ZigZag编码 | 恢复值" << endl;
    cout << "----+------------+-----------+------------+-----------" << endl;
    for (size_t i = 0; i < min(size_t(20), longitudes.size()); i++) {
        uint64_t zigzag = ZigZagCodec::Encode(lon_quantized[i]);
        
        // 重建恢复值
        double recover = 2.0;
        for (size_t j = 0; j <= i; j++) {
            recover += 2 * max_error * static_cast<double>(lon_quantized[j]);
        }
        
        cout << setw(3) << i << " | "
             << fixed << setprecision(5) << setw(10) << longitudes[i] << " | "
             << setw(9) << lon_quantized[i] << " | "
             << setw(10) << zigzag << " | "
             << setw(9) << recover << endl;
    }
    
    int64_t lon_min = *min_element(lon_quantized.begin(), lon_quantized.end());
    int64_t lon_max = *max_element(lon_quantized.begin(), lon_quantized.end());
    double lon_avg = 0;
    for (auto d : lon_quantized) lon_avg += abs(d);
    lon_avg /= lon_quantized.size();
    
    cout << "\n经度量化值统计:" << endl;
    cout << "  最小值: " << lon_min << endl;
    cout << "  最大值: " << lon_max << endl;
    cout << "  平均|值|: " << fixed << setprecision(2) << lon_avg << endl;
    
    cout << "\n【纬度序列】" << endl;
    cout << "Idx | 原始纬度   | 量化值(q) | ZigZag编码 | 恢复值" << endl;
    cout << "----+------------+-----------+------------+-----------" << endl;
    for (size_t i = 0; i < min(size_t(20), latitudes.size()); i++) {
        uint64_t zigzag = ZigZagCodec::Encode(lat_quantized[i]);
        
        // 重建恢复值
        double recover = 2.0;
        for (size_t j = 0; j <= i; j++) {
            recover += 2 * max_error * static_cast<double>(lat_quantized[j]);
        }
        
        cout << setw(3) << i << " | "
             << fixed << setprecision(5) << setw(10) << latitudes[i] << " | "
             << setw(9) << lat_quantized[i] << " | "
             << setw(10) << zigzag << " | "
             << setw(9) << recover << endl;
    }
    
    int64_t lat_min = *min_element(lat_quantized.begin(), lat_quantized.end());
    int64_t lat_max = *max_element(lat_quantized.begin(), lat_quantized.end());
    double lat_avg = 0;
    for (auto d : lat_quantized) lat_avg += abs(d);
    lat_avg /= lat_quantized.size();
    
    cout << "\n纬度量化值统计:" << endl;
    cout << "  最小值: " << lat_min << endl;
    cout << "  最大值: " << lat_max << endl;
    cout << "  平均|值|: " << fixed << setprecision(2) << lat_avg << endl;
    
    // ============ 对比分析 ============
    cout << "\n\n========================================" << endl;
    cout << "对比分析 (前" << display_count << "个点)" << endl;
    cout << "========================================" << endl;
    
    cout << "\n编码值范围对比:" << endl;
    cout << "  Hilbert编码: " << *min_element(hilbert_codes.begin(), hilbert_codes.end()) 
         << " ~ " << *max_element(hilbert_codes.begin(), hilbert_codes.end()) << endl;
    cout << "  经度量化值: " << lon_min << " ~ " << lon_max << endl;
    cout << "  纬度量化值: " << lat_min << " ~ " << lat_max << endl;
    
    cout << "\n差值/量化值对比:" << endl;
    cout << "  Hilbert平均|差值|: " << fixed << setprecision(0) << h_avg << endl;
    cout << "  经度平均|量化值|: " << lon_avg << endl;
    cout << "  纬度平均|量化值|: " << lat_avg << endl;
    cout << "  经度+纬度平均: " << (lon_avg + lat_avg) << endl;
    
    cout << "\n关键观察:" << endl;
    cout << "  • Hilbert差值量级: ~" << scientific << setprecision(0) << h_avg << endl;
    cout << "  • Serf-QT量化值量级: ~" << lon_avg << " (经度) + ~" << lat_avg << " (纬度)" << endl;
    
    double ratio = h_avg / (lon_avg + lat_avg);
    cout << "  • Hilbert差值是Serf-QT的 " << fixed << setprecision(1) << ratio << " 倍" << endl;
    
    cout << "\n结论:" << endl;
    if (ratio > 100) {
        cout << "  ⚠ Hilbert差值显著大于Serf-QT量化值" << endl;
        cout << "  → Elias编码对较小值更有效" << endl;
        cout << "  → 这解释了为什么Serf-QT压缩率更好" << endl;
    } else {
        cout << "  • 两种方法的编码值量级接近" << endl;
    }
    
    return 0;
}


