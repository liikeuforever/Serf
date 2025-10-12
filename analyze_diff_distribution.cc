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

// 分析差值分布
void AnalyzeDiffDistribution(const string& method_name, const vector<int64_t>& diffs) {
    if (diffs.empty()) {
        cout << "无差值数据" << endl;
        return;
    }
    
    cout << "\n========================================" << endl;
    cout << method_name << " - 差值分布分析" << endl;
    cout << "========================================" << endl;
    
    // 基本统计
    int64_t min_diff = *min_element(diffs.begin(), diffs.end());
    int64_t max_diff = *max_element(diffs.begin(), diffs.end());
    
    double sum = 0, sum_abs = 0;
    for (auto d : diffs) {
        sum += d;
        sum_abs += abs(d);
    }
    double avg = sum / diffs.size();
    double avg_abs = sum_abs / diffs.size();
    
    // 排序用于计算中位数和百分位
    vector<int64_t> sorted_abs_diffs;
    for (auto d : diffs) {
        sorted_abs_diffs.push_back(abs(d));
    }
    sort(sorted_abs_diffs.begin(), sorted_abs_diffs.end());
    
    int64_t median = sorted_abs_diffs[sorted_abs_diffs.size() / 2];
    int64_t p25 = sorted_abs_diffs[sorted_abs_diffs.size() / 4];
    int64_t p75 = sorted_abs_diffs[sorted_abs_diffs.size() * 3 / 4];
    int64_t p90 = sorted_abs_diffs[sorted_abs_diffs.size() * 9 / 10];
    int64_t p95 = sorted_abs_diffs[sorted_abs_diffs.size() * 95 / 100];
    int64_t p99 = sorted_abs_diffs[sorted_abs_diffs.size() * 99 / 100];
    
    cout << "基本统计：" << endl;
    cout << "  样本数: " << diffs.size() << endl;
    cout << "  最小差值: " << min_diff << endl;
    cout << "  最大差值: " << max_diff << endl;
    cout << "  平均差值: " << fixed << setprecision(2) << avg << endl;
    cout << "  平均|差值|: " << fixed << setprecision(2) << avg_abs << endl;
    
    cout << "\n百分位数（绝对值）：" << endl;
    cout << "  P25  (25%): " << p25 << endl;
    cout << "  P50  (中位): " << median << endl;
    cout << "  P75  (75%): " << p75 << endl;
    cout << "  P90  (90%): " << p90 << endl;
    cout << "  P95  (95%): " << p95 << endl;
    cout << "  P99  (99%): " << p99 << endl;
    
    // 区间分布
    map<string, int> distribution;
    distribution["0"] = 0;
    distribution["1-10"] = 0;
    distribution["11-100"] = 0;
    distribution["101-1K"] = 0;
    distribution["1K-10K"] = 0;
    distribution["10K-100K"] = 0;
    distribution["100K-1M"] = 0;
    distribution["1M-10M"] = 0;
    distribution["10M-100M"] = 0;
    distribution["100M-1B"] = 0;
    distribution[">1B"] = 0;
    
    for (auto d : sorted_abs_diffs) {
        if (d == 0) distribution["0"]++;
        else if (d <= 10) distribution["1-10"]++;
        else if (d <= 100) distribution["11-100"]++;
        else if (d <= 1000) distribution["101-1K"]++;
        else if (d <= 10000) distribution["10K-100K"]++;
        else if (d <= 100000) distribution["100K-1M"]++;
        else if (d <= 1000000) distribution["1M-10M"]++;
        else if (d <= 10000000) distribution["10M-100M"]++;
        else if (d <= 100000000) distribution["100M-1B"]++;
        else if (d <= 1000000000) distribution[">1B"]++;
    }
    
    cout << "\n差值区间分布（绝对值）：" << endl;
    vector<string> keys = {"0", "1-10", "11-100", "101-1K", "1K-10K", "10K-100K", 
                           "100K-1M", "1M-10M", "10M-100M", "100M-1B", ">1B"};
    for (const auto& key : keys) {
        int count = distribution[key];
        if (count > 0) {
            double percent = 100.0 * count / diffs.size();
            cout << "  " << setw(12) << left << key << ": " 
                 << setw(8) << right << count 
                 << " (" << fixed << setprecision(2) << setw(6) << percent << "%)" << endl;
        }
    }
    
    // 零差值统计
    int zero_count = distribution["0"];
    cout << "\n零差值统计：" << endl;
    cout << "  零差值数量: " << zero_count << " / " << diffs.size() 
         << " (" << fixed << setprecision(2) << (100.0 * zero_count / diffs.size()) << "%)" << endl;
    
    // Elias Gamma编码所需位数估算
    double total_bits = 0;
    for (auto d : sorted_abs_diffs) {
        uint64_t abs_d = abs(d);
        // ZigZag编码
        uint64_t zigzag = (abs_d << 1) ^ (abs_d >> 63);
        // Elias Gamma: 2*floor(log2(n+1))+1 bits
        uint64_t value = zigzag + 1;
        int bits = (value <= 1) ? 1 : (2 * static_cast<int>(floor(log2(value))) + 1);
        total_bits += bits;
    }
    double avg_bits_per_diff = total_bits / diffs.size();
    
    cout << "\nElias Gamma编码估算：" << endl;
    cout << "  平均bits/差值: " << fixed << setprecision(2) << avg_bits_per_diff << endl;
    cout << "  总编码bits: " << fixed << setprecision(0) << total_bits << endl;
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
    cout << "空间编码差值分布分析" << endl;
    cout << "========================================" << endl;
    cout << "数据集: " << filename << endl;
    cout << "MBR: [" << fixed << setprecision(6) << bbox.min_lon << ", " << bbox.max_lon 
         << "] × [" << bbox.min_lat << ", " << bbox.max_lat << "]" << endl;
    cout << "宽度: " << bbox.GetWidth() << " 度, 高度: " << bbox.GetHeight() << " 度" << endl;
    cout << "目标精度: " << scientific << setprecision(6) << target_error << " 度" << endl;
    
    // 加载数据
    cout << "\n加载GPS数据..." << endl;
    vector<GeoPoint> points = LoadPoints(filename, 100000);
    cout << "加载了 " << points.size() << " 个GPS点" << endl;
    
    // 计算各方法所需的比特数
    double global_max_dim = 360.0;
    double mbr_max_dim = max(bbox.GetWidth(), bbox.GetHeight());
    
    int std_geohash_bits = CalculateRequiredBits(global_max_dim, target_error);
    int mbr_geohash_bits = CalculateRequiredBits(mbr_max_dim, target_error);
    int hilbert_bits = mbr_geohash_bits;
    if (hilbert_bits % 2 != 0) hilbert_bits++;
    
    cout << "\n各方法编码比特数：" << endl;
    cout << "  标准GeoHash: " << std_geohash_bits << " 比特（全球范围）" << endl;
    cout << "  MBR-GeoHash:  " << mbr_geohash_bits << " 比特（局部MBR）" << endl;
    cout << "  Hilbert曲线:  " << hilbert_bits << " 比特（局部MBR）" << endl;
    
    // 1. 标准GeoHash
    cout << "\n\n处理标准GeoHash编码..." << endl;
    vector<uint64_t> std_codes;
    for (const auto& p : points) {
        std_codes.push_back(SpaceFillingCurve::EncodeStandardGeoHash(p, std_geohash_bits));
    }
    
    vector<int64_t> std_diffs;
    for (size_t i = 1; i < std_codes.size(); i++) {
        std_diffs.push_back(static_cast<int64_t>(std_codes[i]) - static_cast<int64_t>(std_codes[i-1]));
    }
    
    AnalyzeDiffDistribution("标准GeoHash", std_diffs);
    
    // 2. MBR-GeoHash
    cout << "\n\n处理MBR-GeoHash编码..." << endl;
    vector<uint64_t> mbr_codes;
    for (const auto& p : points) {
        mbr_codes.push_back(SpaceFillingCurve::EncodeMBRGeoHash(p, mbr_geohash_bits, bbox));
    }
    
    vector<int64_t> mbr_diffs;
    for (size_t i = 1; i < mbr_codes.size(); i++) {
        mbr_diffs.push_back(static_cast<int64_t>(mbr_codes[i]) - static_cast<int64_t>(mbr_codes[i-1]));
    }
    
    AnalyzeDiffDistribution("MBR-GeoHash", mbr_diffs);
    
    // 3. Hilbert曲线
    cout << "\n\n处理Hilbert曲线编码..." << endl;
    int order = hilbert_bits / 2;
    vector<uint64_t> hilbert_codes;
    for (const auto& p : points) {
        hilbert_codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    
    vector<int64_t> hilbert_diffs;
    for (size_t i = 1; i < hilbert_codes.size(); i++) {
        hilbert_diffs.push_back(static_cast<int64_t>(hilbert_codes[i]) - static_cast<int64_t>(hilbert_codes[i-1]));
    }
    
    AnalyzeDiffDistribution("Hilbert曲线", hilbert_diffs);
    
    // 对比总结
    cout << "\n\n========================================" << endl;
    cout << "三种方法对比总结" << endl;
    cout << "========================================" << endl;
    
    // 计算各方法的平均绝对差值
    auto calc_avg_abs = [](const vector<int64_t>& diffs) {
        double sum = 0;
        for (auto d : diffs) sum += abs(d);
        return sum / diffs.size();
    };
    
    double std_avg = calc_avg_abs(std_diffs);
    double mbr_avg = calc_avg_abs(mbr_diffs);
    double hilbert_avg = calc_avg_abs(hilbert_diffs);
    
    cout << "\n平均|差值|对比：" << endl;
    cout << "  标准GeoHash: " << fixed << setprecision(0) << std_avg << endl;
    cout << "  MBR-GeoHash:  " << mbr_avg << " (减少 " << setprecision(1) << (100.0 * (1 - mbr_avg/std_avg)) << "%)" << endl;
    cout << "  Hilbert曲线:  " << hilbert_avg << " (减少 " << setprecision(1) << (100.0 * (1 - hilbert_avg/std_avg)) << "%)" << endl;
    
    cout << "\n关键发现：" << endl;
    cout << "  • MBR局部化将平均差值减少了 " << fixed << setprecision(1) 
         << (100.0 * (1 - mbr_avg/std_avg)) << "%" << endl;
    cout << "  • Hilbert的空间局部性比MBR-GeoHash进一步减少了 " 
         << fixed << setprecision(1) << (100.0 * (1 - hilbert_avg/mbr_avg)) << "%" << endl;
    cout << "  • Hilbert相比标准GeoHash总共减少了 " 
         << fixed << setprecision(1) << (100.0 * (1 - hilbert_avg/std_avg)) << "%" << endl;
    
    return 0;
}


