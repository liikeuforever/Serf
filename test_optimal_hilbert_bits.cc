#include "compressor/space_filling_curve.h"
#include "utils/elias_delta_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/output_bit_stream.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

vector<GeoPoint> LoadPoints(const string& filename, int max_points = 10000) {
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

// 测试指定编码位数的压缩效果
void TestCompressionWithBits(const vector<GeoPoint>& points, 
                             int encoding_bits, 
                             const BoundingBox& bbox) {
    OutputBitStream output(1024 * 1024);
    int total_bits = 0;
    
    int order = encoding_bits / 2;
    
    // 编码所有点
    vector<uint64_t> codes;
    for (const auto& p : points) {
        codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    
    // 计算差值统计
    double avg_abs_diff = 0;
    int64_t max_abs_diff = 0;
    for (size_t i = 1; i < codes.size(); i++) {
        int64_t diff = abs(static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]));
        avg_abs_diff += diff;
        max_abs_diff = max(max_abs_diff, diff);
    }
    avg_abs_diff /= (codes.size() - 1);
    
    // 第一个点
    total_bits += encoding_bits;
    
    // 差值编码
    for (size_t i = 1; i < codes.size(); i++) {
        int64_t diff = static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]);
        uint64_t zigzag = ZigZagCodec::Encode(diff);
        total_bits += EliasDeltaCodec::Encode(zigzag + 1, &output);
    }
    
    // 检查精度
    double max_error = 0.0;
    for (size_t i = 0; i < points.size(); i++) {
        GeoPoint decoded = SpaceFillingCurve::DecodeHilbert(codes[i], order, bbox);
        double dx = decoded.longitude - points[i].longitude;
        double dy = decoded.latitude - points[i].latitude;
        double error = sqrt(dx * dx + dy * dy);
        max_error = max(max_error, error);
    }
    
    int byte_length = (total_bits + 7) / 8;
    int original_bytes = points.size() * 16;
    
    // 网格大小
    int grid_size = 1 << order;
    double lon_range = bbox.max_lon - bbox.min_lon;
    double lat_range = bbox.max_lat - bbox.min_lat;
    double cell_lon = lon_range / grid_size;
    double cell_lat = lat_range / grid_size;
    double diagonal = sqrt(cell_lon * cell_lon + cell_lat * cell_lat);
    
    cout << setw(6) << encoding_bits << " | "
         << setw(11) << grid_size << " | "
         << scientific << setprecision(2) << setw(9) << diagonal << " | "
         << setw(9) << max_error << " | "
         << fixed << setprecision(0) << setw(10) << avg_abs_diff << " | "
         << setw(11) << max_abs_diff << " | "
         << setw(6) << byte_length << " | "
         << setprecision(2) << setw(5) << (100.0 * byte_length / original_bytes) << "% | ";
    
    if (max_error <= 1e-5) {
        cout << "✓";
    } else {
        cout << "✗";
    }
    cout << endl;
}

int main() {
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    
    cout << "========================================" << endl;
    cout << "寻找Hilbert编码的最优比特数" << endl;
    cout << "========================================" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    
    cout << "\nMBR范围: " << endl;
    cout << "  经度: " << fixed << setprecision(6) << bbox.min_lon << " ~ " << bbox.max_lon 
         << " (跨度 " << (bbox.max_lon - bbox.min_lon) << " 度)" << endl;
    cout << "  纬度: " << bbox.min_lat << " ~ " << bbox.max_lat 
         << " (跨度 " << (bbox.max_lat - bbox.min_lat) << " 度)" << endl;
    
    // 测试不同数据规模
    vector<int> test_sizes = {1000, 10000};
    
    for (int size : test_sizes) {
        cout << "\n========================================" << endl;
        cout << "测试规模: " << size << " 个GPS点" << endl;
        cout << "========================================" << endl;
        
        vector<GeoPoint> points = LoadPoints(filename, size);
        if (points.empty()) {
            cout << "无法加载数据" << endl;
            continue;
        }
        
        int original_bytes = points.size() * 16;
        cout << "原始大小: " << original_bytes << " 字节 (" << (points.size() * 128) << " 比特)\n" << endl;
        
        cout << "编码位 |  网格大小   | 对角线  | 最大误差 |  平均差值  |  最大差值   | 压缩后 | 压缩率 | 精度" << endl;
        cout << "-------+-------------+----------+----------+------------+-------------+--------+--------+-----" << endl;
        
        // 测试不同的编码位数
        for (int bits = 20; bits <= 46; bits += 2) {
            TestCompressionWithBits(points, bits, bbox);
        }
    }
    
    cout << "\n========================================" << endl;
    cout << "结论" << endl;
    cout << "========================================" << endl;
    cout << "关键发现：" << endl;
    cout << "  • 编码位数越高，网格越细，精度越高" << endl;
    cout << "  • 但编码位数越高，相邻编码差值越大" << endl;
    cout << "  • 差值越大，Elias Delta编码效率越低" << endl;
    cout << "  • 需要找到「满足精度要求的最小编码位数」" << endl;
    cout << "\n最优策略：" << endl;
    cout << "  选择满足max_error <= 1e-5的最小编码位数" << endl;
    cout << "  这样可以在保证精度的同时，最小化编码差值" << endl;
    
    return 0;
}


