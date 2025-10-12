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

// 计算Elias Gamma编码所需的位数
int EliasGammaBits(uint64_t n) {
    if (n == 0) return 1;  // 特殊处理0
    if (n == 1) return 1;
    int log2n = static_cast<int>(floor(log2(n)));
    return 2 * log2n + 1;
}

// 计算Elias Delta编码所需的位数
int EliasDeltaBits(uint64_t n) {
    if (n == 0) return 1;
    if (n == 1) return 1;
    int log2n = static_cast<int>(floor(log2(n)));
    int log2log2n = static_cast<int>(floor(log2(log2n + 1)));
    return 2 * log2log2n + 1 + log2n;
}

// 计算Rice编码所需的位数（使用最优k参数）
int RiceBits(uint64_t n, int k) {
    return (n >> k) + 1 + k;  // 商的一元编码 + k位余数
}

// ZigZag编码
uint64_t ZigZagEncode(int64_t value) {
    return (value << 1) ^ (value >> 63);
}

// 分析编码效率
void AnalyzeEncodingEfficiency(const string& method_name, const vector<int64_t>& diffs) {
    cout << "\n========================================" << endl;
    cout << method_name << " - 编码方案对比" << endl;
    cout << "========================================" << endl;
    
    // 统计数据
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
    distribution[">100M"] = 0;
    
    // 固定长度编码
    int64_t max_abs_diff = 0;
    for (auto d : diffs) {
        int64_t abs_d = abs(d);
        max_abs_diff = max(max_abs_diff, abs_d);
        
        if (abs_d == 0) distribution["0"]++;
        else if (abs_d <= 10) distribution["1-10"]++;
        else if (abs_d <= 100) distribution["11-100"]++;
        else if (abs_d <= 1000) distribution["101-1K"]++;
        else if (abs_d <= 10000) distribution["1K-10K"]++;
        else if (abs_d <= 100000) distribution["10K-100K"]++;
        else if (abs_d <= 1000000) distribution["100K-1M"]++;
        else if (abs_d <= 10000000) distribution["1M-10M"]++;
        else if (abs_d <= 100000000) distribution["10M-100M"]++;
        else distribution[">100M"]++;
    }
    
    int fixed_bits = static_cast<int>(ceil(log2(max_abs_diff + 1))) + 1;  // +1 for sign
    
    // 计算各种编码方案的总位数
    uint64_t total_fixed = 0;
    uint64_t total_elias_gamma = 0;
    uint64_t total_elias_delta = 0;
    uint64_t total_rice_k8 = 0;
    uint64_t total_rice_k12 = 0;
    uint64_t total_rice_k16 = 0;
    
    // 用于找最优Rice参数
    vector<uint64_t> rice_totals(32, 0);
    
    for (auto d : diffs) {
        int64_t abs_d = abs(d);
        uint64_t zigzag = ZigZagEncode(d);
        
        // 固定长度
        total_fixed += fixed_bits;
        
        // Elias Gamma (需要+1因为不能编码0)
        total_elias_gamma += EliasGammaBits(zigzag + 1);
        
        // Elias Delta
        total_elias_delta += EliasDeltaBits(zigzag + 1);
        
        // Rice with different k
        total_rice_k8 += RiceBits(zigzag, 8);
        total_rice_k12 += RiceBits(zigzag, 12);
        total_rice_k16 += RiceBits(zigzag, 16);
        
        // 计算所有k的Rice编码
        for (int k = 0; k < 32; k++) {
            rice_totals[k] += RiceBits(zigzag, k);
        }
    }
    
    // 找最优Rice参数
    int optimal_k = 0;
    uint64_t min_rice_total = rice_totals[0];
    for (int k = 1; k < 32; k++) {
        if (rice_totals[k] < min_rice_total) {
            min_rice_total = rice_totals[k];
            optimal_k = k;
        }
    }
    
    // 输出结果
    cout << "\n数据分布：" << endl;
    vector<string> keys = {"0", "1-10", "11-100", "101-1K", "1K-10K", 
                           "10K-100K", "100K-1M", "1M-10M", "10M-100M", ">100M"};
    for (const auto& key : keys) {
        int count = distribution[key];
        if (count > 0) {
            cout << "  " << setw(10) << left << key << ": " 
                 << setw(8) << right << count 
                 << " (" << fixed << setprecision(2) << (100.0 * count / diffs.size()) << "%)" << endl;
        }
    }
    
    cout << "\n编码方案对比：" << endl;
    cout << "  样本数: " << diffs.size() << endl;
    cout << "  最大|差值|: " << max_abs_diff << endl;
    cout << endl;
    
    double avg_fixed = (double)total_fixed / diffs.size();
    double avg_elias_gamma = (double)total_elias_gamma / diffs.size();
    double avg_elias_delta = (double)total_elias_delta / diffs.size();
    double avg_rice_k8 = (double)total_rice_k8 / diffs.size();
    double avg_rice_k12 = (double)total_rice_k12 / diffs.size();
    double avg_rice_k16 = (double)total_rice_k16 / diffs.size();
    double avg_rice_optimal = (double)min_rice_total / diffs.size();
    
    cout << "方案                    总bits          平均bits/差值    相对固定长度" << endl;
    cout << "-----------------------------------------------------------------------" << endl;
    cout << "固定长度(" << fixed_bits << "位)";
    cout << setw(15) << total_fixed 
         << setw(18) << fixed << setprecision(2) << avg_fixed 
         << "       100.0%" << endl;
    
    cout << "Elias Gamma         ";
    cout << setw(15) << total_elias_gamma 
         << setw(18) << fixed << setprecision(2) << avg_elias_gamma 
         << setw(12) << fixed << setprecision(1) << (100.0 * total_elias_gamma / total_fixed) << "%" << endl;
    
    cout << "Elias Delta         ";
    cout << setw(15) << total_elias_delta 
         << setw(18) << fixed << setprecision(2) << avg_elias_delta 
         << setw(12) << fixed << setprecision(1) << (100.0 * total_elias_delta / total_fixed) << "%" << endl;
    
    cout << "Rice (k=8)          ";
    cout << setw(15) << total_rice_k8 
         << setw(18) << fixed << setprecision(2) << avg_rice_k8 
         << setw(12) << fixed << setprecision(1) << (100.0 * total_rice_k8 / total_fixed) << "%" << endl;
    
    cout << "Rice (k=12)         ";
    cout << setw(15) << total_rice_k12 
         << setw(18) << fixed << setprecision(2) << avg_rice_k12 
         << setw(12) << fixed << setprecision(1) << (100.0 * total_rice_k12 / total_fixed) << "%" << endl;
    
    cout << "Rice (k=16)         ";
    cout << setw(15) << total_rice_k16 
         << setw(18) << fixed << setprecision(2) << avg_rice_k16 
         << setw(12) << fixed << setprecision(1) << (100.0 * total_rice_k16 / total_fixed) << "%" << endl;
    
    cout << "Rice (最优k=" << optimal_k << ")    ";
    cout << setw(15) << min_rice_total 
         << setw(18) << fixed << setprecision(2) << avg_rice_optimal 
         << setw(12) << fixed << setprecision(1) << (100.0 * min_rice_total / total_fixed) << "%" << endl;
    
    // 推荐方案
    cout << "\n推荐编码方案：" << endl;
    uint64_t min_total = min({total_elias_gamma, total_elias_delta, min_rice_total});
    if (min_total == total_elias_gamma) {
        cout << "  ✓ Elias Gamma（节省 " << fixed << setprecision(1) 
             << (100.0 * (total_fixed - total_elias_gamma) / total_fixed) << "%）" << endl;
    } else if (min_total == total_elias_delta) {
        cout << "  ✓ Elias Delta（节省 " << fixed << setprecision(1) 
             << (100.0 * (total_fixed - total_elias_delta) / total_fixed) << "%）" << endl;
    } else {
        cout << "  ✓ Rice k=" << optimal_k << "（节省 " << fixed << setprecision(1) 
             << (100.0 * (total_fixed - min_rice_total) / total_fixed) << "%）" << endl;
    }
}

int main() {
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    
    cout << "========================================" << endl;
    cout << "空间编码差值 - 编码方案效率对比" << endl;
    cout << "========================================" << endl;
    
    cout << "正在加载和编码数据..." << endl;
    vector<GeoPoint> points = LoadPoints(filename, 100000);
    cout << "✓ 加载了 " << points.size() << " 个GPS点\n" << endl;
    
    // 计算编码比特数
    double target_error = 1.0e-5;
    double global_max_dim = 360.0;
    double mbr_max_dim = max(bbox.GetWidth(), bbox.GetHeight());
    
    auto calc_bits = [&](double max_dim) {
        double required_cells = max_dim / target_error;
        return static_cast<int>(ceil(log2(required_cells) * 2));
    };
    
    int std_bits = calc_bits(global_max_dim);
    int mbr_bits = calc_bits(mbr_max_dim);
    int hilbert_bits = mbr_bits + (mbr_bits % 2);
    
    // 1. 标准GeoHash
    cout << "编码标准GeoHash (" << std_bits << " 比特)..." << endl;
    vector<uint64_t> std_codes;
    for (const auto& p : points) {
        std_codes.push_back(SpaceFillingCurve::EncodeStandardGeoHash(p, std_bits));
    }
    vector<int64_t> std_diffs;
    for (size_t i = 1; i < std_codes.size(); i++) {
        std_diffs.push_back(static_cast<int64_t>(std_codes[i]) - static_cast<int64_t>(std_codes[i-1]));
    }
    AnalyzeEncodingEfficiency("标准GeoHash", std_diffs);
    
    // 2. MBR-GeoHash
    cout << "\n\n编码MBR-GeoHash (" << mbr_bits << " 比特)..." << endl;
    vector<uint64_t> mbr_codes;
    for (const auto& p : points) {
        mbr_codes.push_back(SpaceFillingCurve::EncodeMBRGeoHash(p, mbr_bits, bbox));
    }
    vector<int64_t> mbr_diffs;
    for (size_t i = 1; i < mbr_codes.size(); i++) {
        mbr_diffs.push_back(static_cast<int64_t>(mbr_codes[i]) - static_cast<int64_t>(mbr_codes[i-1]));
    }
    AnalyzeEncodingEfficiency("MBR-GeoHash", mbr_diffs);
    
    // 3. Hilbert曲线
    cout << "\n\n编码Hilbert曲线 (" << hilbert_bits << " 比特)..." << endl;
    vector<uint64_t> hilbert_codes;
    int order = hilbert_bits / 2;
    for (const auto& p : points) {
        hilbert_codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    vector<int64_t> hilbert_diffs;
    for (size_t i = 1; i < hilbert_codes.size(); i++) {
        hilbert_diffs.push_back(static_cast<int64_t>(hilbert_codes[i]) - static_cast<int64_t>(hilbert_codes[i-1]));
    }
    AnalyzeEncodingEfficiency("Hilbert曲线", hilbert_diffs);
    
    return 0;
}


