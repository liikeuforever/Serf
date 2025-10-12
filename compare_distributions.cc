#include "compressor/space_filling_curve.h"
#include "utils/zig_zag_codec.h"
#include "utils/double.h"
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

void AnalyzeDistribution(const string& name, const vector<int64_t>& values, bool skip_first = false) {
    size_t start_idx = skip_first ? 1 : 0;
    vector<int64_t> abs_values;
    
    for (size_t i = start_idx; i < values.size(); i++) {
        abs_values.push_back(abs(values[i]));
    }
    
    if (abs_values.empty()) return;
    
    sort(abs_values.begin(), abs_values.end());
    
    int64_t min_val = abs_values.front();
    int64_t max_val = abs_values.back();
    double avg_val = 0;
    for (auto v : abs_values) avg_val += v;
    avg_val /= abs_values.size();
    
    int64_t median = abs_values[abs_values.size() / 2];
    int64_t p90 = abs_values[abs_values.size() * 9 / 10];
    int64_t p99 = abs_values[abs_values.size() * 99 / 100];
    
    cout << name << (skip_first ? " (排除第1个点)" : "") << ":" << endl;
    cout << "  数量: " << abs_values.size() << endl;
    cout << "  最小: " << min_val << endl;
    cout << "  最大: " << max_val << endl;
    cout << "  平均: " << fixed << setprecision(1) << avg_val << endl;
    cout << "  中位数: " << median << endl;
    cout << "  P90: " << p90 << endl;
    cout << "  P99: " << p99 << endl;
    
    // 分布统计
    int small = 0, medium = 0, large = 0, huge = 0;
    for (auto v : abs_values) {
        if (v < 10) small++;
        else if (v < 100) medium++;
        else if (v < 10000) large++;
        else huge++;
    }
    
    cout << "  分布: < 10: " << small << " (" << (100.0 * small / abs_values.size()) << "%)"
         << ", 10-100: " << medium << " (" << (100.0 * medium / abs_values.size()) << "%)"
         << ", 100-10K: " << large << " (" << (100.0 * large / abs_values.size()) << "%)"
         << ", > 10K: " << huge << " (" << (100.0 * huge / abs_values.size()) << "%)" << endl;
}

int main() {
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int test_size = 1000;
    
    cout << "========================================" << endl;
    cout << "差值分布详细对比 (前" << test_size << "个点)" << endl;
    cout << "========================================\n" << endl;
    
    vector<GeoPoint> points = LoadPoints(filename, test_size);
    if (points.empty()) {
        cerr << "无法加载数据" << endl;
        return 1;
    }
    
    // Hilbert编码
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
    
    vector<int64_t> hilbert_diffs;
    for (size_t i = 1; i < hilbert_codes.size(); i++) {
        int64_t diff = static_cast<int64_t>(hilbert_codes[i]) - static_cast<int64_t>(hilbert_codes[i-1]);
        hilbert_diffs.push_back(diff);
    }
    
    // Serf-QT经度
    vector<double> longitudes;
    for (const auto& p : points) longitudes.push_back(p.longitude);
    
    vector<int64_t> lon_quantized;
    double max_error = 1.0e-5;
    double pre_lon = 2.0;
    for (size_t i = 0; i < longitudes.size(); i++) {
        int64_t q = static_cast<int64_t>(round((longitudes[i] - pre_lon) / (2 * max_error)));
        lon_quantized.push_back(q);
        pre_lon += 2 * max_error * static_cast<double>(q);
    }
    
    // Serf-QT纬度
    vector<double> latitudes;
    for (const auto& p : points) latitudes.push_back(p.latitude);
    
    vector<int64_t> lat_quantized;
    double pre_lat = 2.0;
    for (size_t i = 0; i < latitudes.size(); i++) {
        int64_t q = static_cast<int64_t>(round((latitudes[i] - pre_lat) / (2 * max_error)));
        lat_quantized.push_back(q);
        pre_lat += 2 * max_error * static_cast<double>(q);
    }
    
    // 分析Hilbert差值
    cout << "【Hilbert曲线差值分析】" << endl;
    cout << "========================================" << endl;
    AnalyzeDistribution("Hilbert差值", hilbert_diffs, false);
    
    cout << "\n【Serf-QT量化值分析】" << endl;
    cout << "========================================" << endl;
    AnalyzeDistribution("经度量化值", lon_quantized, false);
    cout << endl;
    AnalyzeDistribution("经度量化值", lon_quantized, true);
    cout << endl;
    AnalyzeDistribution("纬度量化值", lat_quantized, false);
    cout << endl;
    AnalyzeDistribution("纬度量化值", lat_quantized, true);
    
    // 关键对比
    cout << "\n========================================" << endl;
    cout << "关键发现" << endl;
    cout << "========================================" << endl;
    
    // 计算排除第一个点后的平均值
    double hilbert_avg_no_first = 0;
    for (auto d : hilbert_diffs) hilbert_avg_no_first += abs(d);
    hilbert_avg_no_first /= hilbert_diffs.size();
    
    double lon_avg_no_first = 0;
    for (size_t i = 1; i < lon_quantized.size(); i++) lon_avg_no_first += abs(lon_quantized[i]);
    lon_avg_no_first /= (lon_quantized.size() - 1);
    
    double lat_avg_no_first = 0;
    for (size_t i = 1; i < lat_quantized.size(); i++) lat_avg_no_first += abs(lat_quantized[i]);
    lat_avg_no_first /= (lat_quantized.size() - 1);
    
    cout << "\n排除第一个点后的平均|值|:" << endl;
    cout << "  Hilbert差值: " << fixed << setprecision(1) << hilbert_avg_no_first << endl;
    cout << "  经度量化值: " << lon_avg_no_first << endl;
    cout << "  纬度量化值: " << lat_avg_no_first << endl;
    cout << "  经度+纬度: " << (lon_avg_no_first + lat_avg_no_first) << endl;
    
    cout << "\n为什么Serf-QT压缩更好？" << endl;
    cout << "  1. Serf-QT大部分值 < 10 (高占比)" << endl;
    cout << "  2. Hilbert差值分布更广，大值更多" << endl;
    cout << "  3. Elias编码对小值更有效（指数级差异）" << endl;
    
    // Elias Gamma编码位数估算
    auto elias_bits = [](int64_t n) -> int {
        if (n == 0) return 1;
        n = abs(n);
        int len = 0;
        while (n > 0) { len++; n >>= 1; }
        return 2 * len - 1;
    };
    
    cout << "\nElias Gamma编码位数示例:" << endl;
    cout << "  值=1: " << elias_bits(1) << " 位" << endl;
    cout << "  值=10: " << elias_bits(10) << " 位" << endl;
    cout << "  值=100: " << elias_bits(100) << " 位" << endl;
    cout << "  值=1000: " << elias_bits(1000) << " 位" << endl;
    cout << "  值=10000: " << elias_bits(10000) << " 位" << endl;
    cout << "  值=68930: " << elias_bits(68930) << " 位" << endl;
    
    return 0;
}


