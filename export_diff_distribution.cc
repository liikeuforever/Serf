#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

// 加载GPS点
vector<GeoPoint> LoadPoints(const string& filename, int max_points = 100000) {
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

// 计算需要的比特数
int CalculateRequiredBits(double max_dimension, double target_error) {
    double required_cells = max_dimension / target_error;
    double bits_per_dim = log2(required_cells);
    int total_bits = static_cast<int>(ceil(bits_per_dim * 2));
    return total_bits;
}

// 输出详细差值数据
void ExportDetailedDiffs(const string& filename, 
                        const vector<GeoPoint>& points,
                        const vector<uint64_t>& std_codes,
                        const vector<uint64_t>& mbr_codes,
                        const vector<uint64_t>& hilbert_codes) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "无法创建文件: " << filename << endl;
        return;
    }
    
    // 写入表头
    file << "index,longitude,latitude,"
         << "std_geohash_code,std_geohash_diff,std_geohash_abs_diff,"
         << "mbr_geohash_code,mbr_geohash_diff,mbr_geohash_abs_diff,"
         << "hilbert_code,hilbert_diff,hilbert_abs_diff\n";
    
    // 写入数据
    for (size_t i = 0; i < points.size(); i++) {
        file << i << ","
             << fixed << setprecision(8) << points[i].longitude << ","
             << points[i].latitude << ",";
        
        // 标准GeoHash
        file << std_codes[i] << ",";
        if (i > 0) {
            int64_t diff = static_cast<int64_t>(std_codes[i]) - static_cast<int64_t>(std_codes[i-1]);
            file << diff << "," << abs(diff);
        } else {
            file << "0,0";
        }
        file << ",";
        
        // MBR-GeoHash
        file << mbr_codes[i] << ",";
        if (i > 0) {
            int64_t diff = static_cast<int64_t>(mbr_codes[i]) - static_cast<int64_t>(mbr_codes[i-1]);
            file << diff << "," << abs(diff);
        } else {
            file << "0,0";
        }
        file << ",";
        
        // Hilbert曲线
        file << hilbert_codes[i] << ",";
        if (i > 0) {
            int64_t diff = static_cast<int64_t>(hilbert_codes[i]) - static_cast<int64_t>(hilbert_codes[i-1]);
            file << diff << "," << abs(diff);
        } else {
            file << "0,0";
        }
        file << "\n";
    }
    
    file.close();
    cout << "✓ 详细差值数据已导出到: " << filename << endl;
}

// 输出差值分布统计
void ExportDistributionStats(const string& filename,
                             const vector<int64_t>& std_diffs,
                             const vector<int64_t>& mbr_diffs,
                             const vector<int64_t>& hilbert_diffs) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "无法创建文件: " << filename << endl;
        return;
    }
    
    // 定义区间
    vector<pair<string, pair<int64_t, int64_t>>> ranges = {
        {"0", {0, 0}},
        {"1-10", {1, 10}},
        {"11-100", {11, 100}},
        {"101-1K", {101, 1000}},
        {"1K-10K", {1001, 10000}},
        {"10K-100K", {10001, 100000}},
        {"100K-1M", {100001, 1000000}},
        {"1M-10M", {1000001, 10000000}},
        {"10M-100M", {10000001, 100000000}},
        {"100M-1B", {100000001, 1000000000}},
        {">1B", {1000000001, LLONG_MAX}}
    };
    
    // 写入表头
    file << "range,std_geohash_count,std_geohash_percent,"
         << "mbr_geohash_count,mbr_geohash_percent,"
         << "hilbert_count,hilbert_percent\n";
    
    // 统计各区间
    for (const auto& range : ranges) {
        string name = range.first;
        int64_t min_val = range.second.first;
        int64_t max_val = range.second.second;
        
        int std_count = 0, mbr_count = 0, hilbert_count = 0;
        
        for (auto d : std_diffs) {
            int64_t abs_d = abs(d);
            if (abs_d >= min_val && abs_d <= max_val) std_count++;
        }
        
        for (auto d : mbr_diffs) {
            int64_t abs_d = abs(d);
            if (abs_d >= min_val && abs_d <= max_val) mbr_count++;
        }
        
        for (auto d : hilbert_diffs) {
            int64_t abs_d = abs(d);
            if (abs_d >= min_val && abs_d <= max_val) hilbert_count++;
        }
        
        double std_percent = 100.0 * std_count / std_diffs.size();
        double mbr_percent = 100.0 * mbr_count / mbr_diffs.size();
        double hilbert_percent = 100.0 * hilbert_count / hilbert_diffs.size();
        
        file << name << ","
             << std_count << "," << fixed << setprecision(2) << std_percent << ","
             << mbr_count << "," << mbr_percent << ","
             << hilbert_count << "," << hilbert_percent << "\n";
    }
    
    file.close();
    cout << "✓ 分布统计已导出到: " << filename << endl;
}

// 输出百分位数据
void ExportPercentiles(const string& filename,
                      const vector<int64_t>& std_diffs,
                      const vector<int64_t>& mbr_diffs,
                      const vector<int64_t>& hilbert_diffs) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "无法创建文件: " << filename << endl;
        return;
    }
    
    // 排序
    vector<int64_t> std_sorted, mbr_sorted, hilbert_sorted;
    for (auto d : std_diffs) std_sorted.push_back(abs(d));
    for (auto d : mbr_diffs) mbr_sorted.push_back(abs(d));
    for (auto d : hilbert_diffs) hilbert_sorted.push_back(abs(d));
    
    sort(std_sorted.begin(), std_sorted.end());
    sort(mbr_sorted.begin(), mbr_sorted.end());
    sort(hilbert_sorted.begin(), hilbert_sorted.end());
    
    // 写入表头
    file << "percentile,std_geohash,mbr_geohash,hilbert\n";
    
    // 计算百分位
    vector<int> percentiles = {0, 1, 5, 10, 25, 50, 75, 90, 95, 99, 100};
    
    for (int p : percentiles) {
        size_t idx = (std_sorted.size() - 1) * p / 100;
        
        file << "P" << p << ","
             << std_sorted[idx] << ","
             << mbr_sorted[idx] << ","
             << hilbert_sorted[idx] << "\n";
    }
    
    file.close();
    cout << "✓ 百分位数据已导出到: " << filename << endl;
}

// 输出基本统计
void ExportBasicStats(const string& filename,
                     const vector<int64_t>& std_diffs,
                     const vector<int64_t>& mbr_diffs,
                     const vector<int64_t>& hilbert_diffs) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "无法创建文件: " << filename << endl;
        return;
    }
    
    auto calc_stats = [](const vector<int64_t>& diffs) {
        int64_t min_val = *min_element(diffs.begin(), diffs.end());
        int64_t max_val = *max_element(diffs.begin(), diffs.end());
        
        double sum = 0, sum_abs = 0;
        for (auto d : diffs) {
            sum += d;
            sum_abs += abs(d);
        }
        double avg = sum / diffs.size();
        double avg_abs = sum_abs / diffs.size();
        
        int zero_count = 0;
        for (auto d : diffs) if (d == 0) zero_count++;
        
        return make_tuple(min_val, max_val, avg, avg_abs, zero_count);
    };
    
    auto [std_min, std_max, std_avg, std_avg_abs, std_zero] = calc_stats(std_diffs);
    auto [mbr_min, mbr_max, mbr_avg, mbr_avg_abs, mbr_zero] = calc_stats(mbr_diffs);
    auto [hil_min, hil_max, hil_avg, hil_avg_abs, hil_zero] = calc_stats(hilbert_diffs);
    
    // 写入表头
    file << "metric,std_geohash,mbr_geohash,hilbert\n";
    
    // 写入统计数据
    file << "sample_count," << std_diffs.size() << "," << mbr_diffs.size() << "," << hilbert_diffs.size() << "\n";
    file << "min_diff," << std_min << "," << mbr_min << "," << hil_min << "\n";
    file << "max_diff," << std_max << "," << mbr_max << "," << hil_max << "\n";
    file << "avg_diff," << fixed << setprecision(2) << std_avg << "," << mbr_avg << "," << hil_avg << "\n";
    file << "avg_abs_diff," << std_avg_abs << "," << mbr_avg_abs << "," << hil_avg_abs << "\n";
    file << "zero_count," << std_zero << "," << mbr_zero << "," << hil_zero << "\n";
    file << "zero_percent," << setprecision(2) << (100.0 * std_zero / std_diffs.size()) << ","
         << (100.0 * mbr_zero / mbr_diffs.size()) << ","
         << (100.0 * hil_zero / hilbert_diffs.size()) << "\n";
    
    file.close();
    cout << "✓ 基本统计已导出到: " << filename << endl;
}

int main() {
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    double target_error = 1.0e-5;
    
    // 使用预先统计的MBR
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    
    cout << "========================================" << endl;
    cout << "导出空间编码差值分布CSV文件" << endl;
    cout << "========================================" << endl;
    cout << "数据集: " << filename << endl;
    cout << "MBR: [" << fixed << setprecision(6) << bbox.min_lon << ", " << bbox.max_lon 
         << "] × [" << bbox.min_lat << ", " << bbox.max_lat << "]" << endl;
    
    // 加载数据
    cout << "\n正在加载GPS数据..." << endl;
    vector<GeoPoint> points = LoadPoints(filename, 100000);
    cout << "✓ 加载了 " << points.size() << " 个GPS点" << endl;
    
    // 计算各方法所需的比特数
    double global_max_dim = 360.0;
    double mbr_max_dim = max(bbox.GetWidth(), bbox.GetHeight());
    
    int std_geohash_bits = CalculateRequiredBits(global_max_dim, target_error);
    int mbr_geohash_bits = CalculateRequiredBits(mbr_max_dim, target_error);
    int hilbert_bits = mbr_geohash_bits;
    if (hilbert_bits % 2 != 0) hilbert_bits++;
    
    cout << "\n编码比特数：" << endl;
    cout << "  标准GeoHash: " << std_geohash_bits << " 比特" << endl;
    cout << "  MBR-GeoHash:  " << mbr_geohash_bits << " 比特" << endl;
    cout << "  Hilbert曲线:  " << hilbert_bits << " 比特" << endl;
    
    // 编码所有点
    cout << "\n正在编码..." << endl;
    
    vector<uint64_t> std_codes, mbr_codes, hilbert_codes;
    
    cout << "  - 标准GeoHash编码..." << flush;
    for (const auto& p : points) {
        std_codes.push_back(SpaceFillingCurve::EncodeStandardGeoHash(p, std_geohash_bits));
    }
    cout << " 完成" << endl;
    
    cout << "  - MBR-GeoHash编码..." << flush;
    for (const auto& p : points) {
        mbr_codes.push_back(SpaceFillingCurve::EncodeMBRGeoHash(p, mbr_geohash_bits, bbox));
    }
    cout << " 完成" << endl;
    
    cout << "  - Hilbert曲线编码..." << flush;
    int order = hilbert_bits / 2;
    for (const auto& p : points) {
        hilbert_codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    cout << " 完成" << endl;
    
    // 计算差值
    cout << "\n正在计算差值..." << endl;
    
    vector<int64_t> std_diffs, mbr_diffs, hilbert_diffs;
    
    for (size_t i = 1; i < std_codes.size(); i++) {
        std_diffs.push_back(static_cast<int64_t>(std_codes[i]) - static_cast<int64_t>(std_codes[i-1]));
    }
    
    for (size_t i = 1; i < mbr_codes.size(); i++) {
        mbr_diffs.push_back(static_cast<int64_t>(mbr_codes[i]) - static_cast<int64_t>(mbr_codes[i-1]));
    }
    
    for (size_t i = 1; i < hilbert_codes.size(); i++) {
        hilbert_diffs.push_back(static_cast<int64_t>(hilbert_codes[i]) - static_cast<int64_t>(hilbert_codes[i-1]));
    }
    
    cout << "✓ 差值计算完成" << endl;
    
    // 导出CSV文件
    cout << "\n正在导出CSV文件..." << endl;
    
    ExportDetailedDiffs("sfc_detailed_diffs.csv", points, std_codes, mbr_codes, hilbert_codes);
    ExportDistributionStats("sfc_distribution_stats.csv", std_diffs, mbr_diffs, hilbert_diffs);
    ExportPercentiles("sfc_percentiles.csv", std_diffs, mbr_diffs, hilbert_diffs);
    ExportBasicStats("sfc_basic_stats.csv", std_diffs, mbr_diffs, hilbert_diffs);
    
    cout << "\n========================================" << endl;
    cout << "导出完成！生成的文件：" << endl;
    cout << "========================================" << endl;
    cout << "1. sfc_detailed_diffs.csv      - 每个点的详细编码和差值数据" << endl;
    cout << "2. sfc_distribution_stats.csv  - 差值区间分布统计" << endl;
    cout << "3. sfc_percentiles.csv         - 百分位数据" << endl;
    cout << "4. sfc_basic_stats.csv         - 基本统计指标" << endl;
    cout << "\n所有文件已保存到当前目录。" << endl;
    
    return 0;
}


