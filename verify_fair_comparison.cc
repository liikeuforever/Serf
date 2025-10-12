#include "compressor/space_filling_curve.h"
#include "compressor/serf_qt_compressor.h"
#include "utils/elias_delta_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/output_bit_stream.h"
#include "utils/double.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>

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
    int test_size = 1000;
    
    cout << "========================================" << endl;
    cout << "验证公平对比 (" << test_size << " 个GPS点)" << endl;
    cout << "========================================\n" << endl;
    
    vector<GeoPoint> points = LoadPoints(filename, test_size);
    if (points.empty()) {
        cerr << "无法加载数据" << endl;
        return 1;
    }
    
    int original_bytes = points.size() * 16;  // 每个点 = 2个double = 16字节
    cout << "原始数据: " << original_bytes << " 字节 (" << (points.size() * 128) << " 比特)\n" << endl;
    
    // ============ Hilbert+Delta 详细分析 ============
    cout << "【方案1】Hilbert + ZigZag + Elias Delta" << endl;
    cout << "----------------------------------------" << endl;
    
    BoundingBox bbox;
    bbox.min_lon = 116.201597;
    bbox.max_lon = 121.470629;
    bbox.min_lat = 31.167749;
    bbox.max_lat = 40.081138;
    int encoding_bits = 38;
    
    OutputBitStream output1(1024 * 1024);
    int total_bits = 0;
    
    // Header
    total_bits += output1.WriteInt(2, 8);
    total_bits += output1.WriteLong(Double::DoubleToLongBits(1e-5), 64);
    total_bits += output1.WriteInt(encoding_bits, 8);
    total_bits += output1.WriteLong(Double::DoubleToLongBits(bbox.min_lon), 64);
    total_bits += output1.WriteLong(Double::DoubleToLongBits(bbox.max_lon), 64);
    total_bits += output1.WriteLong(Double::DoubleToLongBits(bbox.min_lat), 64);
    total_bits += output1.WriteLong(Double::DoubleToLongBits(bbox.max_lat), 64);
    total_bits += output1.WriteInt(points.size(), 32);
    int header_bits = total_bits;
    
    // 编码
    int order = encoding_bits / 2;
    vector<uint64_t> codes;
    for (const auto& p : points) {
        codes.push_back(SpaceFillingCurve::EncodeHilbert(p, order, bbox));
    }
    
    // 第一个点
    total_bits += output1.WriteLong(codes[0], encoding_bits);
    int first_point_bits = total_bits - header_bits;
    
    // 差值编码
    int data_bits = 0;
    for (size_t i = 1; i < codes.size(); i++) {
        int64_t diff = static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]);
        uint64_t zigzag = ZigZagCodec::Encode(diff);
        int bits = EliasDeltaCodec::Encode(zigzag + 1, &output1);
        data_bits += bits;
    }
    total_bits += data_bits;
    
    output1.Flush();
    int calculated_bytes = (total_bits + 7) / 8;
    Array<uint8_t> compressed_data1 = output1.GetBuffer(calculated_bytes);
    int actual_bytes1 = compressed_data1.length();
    
    cout << "  Header: " << header_bits << " 比特 = " << (header_bits / 8) << " 字节" << endl;
    cout << "  第一个点: " << first_point_bits << " 比特 = " << (first_point_bits / 8) << " 字节" << endl;
    cout << "  差值数据: " << data_bits << " 比特 = " << (data_bits / 8) << " 字节" << endl;
    cout << "  总计(手动累加): " << total_bits << " 比特" << endl;
    cout << "  计算字节数: " << calculated_bytes << " 字节" << endl;
    cout << "  GetBuffer()返回: " << actual_bytes1 << " 字节" << endl;
    cout << "  压缩率: " << fixed << setprecision(2) 
         << (100.0 * actual_bytes1 / original_bytes) << "%" << endl;
    cout << "  压缩比: " << (double)original_bytes / actual_bytes1 << ":1" << endl;
    
    // ============ Serf-QT 详细分析 ============
    cout << "\n【方案2】Serf-QT (分别压缩经纬度)" << endl;
    cout << "----------------------------------------" << endl;
    
    vector<double> longitudes, latitudes;
    for (const auto& p : points) {
        longitudes.push_back(p.longitude);
        latitudes.push_back(p.latitude);
    }
    
    double max_error = 1.0e-5;
    int block_size = 1000;
    
    // 压缩经度
    SerfQtCompressor lon_compressor(block_size, max_error);
    for (double lon : longitudes) {
        lon_compressor.AddValue(lon);
    }
    lon_compressor.Close();
    Array<uint8_t> lon_compressed = lon_compressor.compressed_bytes();
    long lon_bits = lon_compressor.get_compressed_size_in_bits();
    
    // 压缩纬度
    SerfQtCompressor lat_compressor(block_size, max_error);
    for (double lat : latitudes) {
        lat_compressor.AddValue(lat);
    }
    lat_compressor.Close();
    Array<uint8_t> lat_compressed = lat_compressor.compressed_bytes();
    long lat_bits = lat_compressor.get_compressed_size_in_bits();
    
    int total_bytes2 = lon_compressed.length() + lat_compressed.length();
    long total_bits2 = lon_bits + lat_bits;
    
    cout << "  经度压缩: " << lon_bits << " 比特 = " << lon_compressed.length() << " 字节" << endl;
    cout << "  纬度压缩: " << lat_bits << " 比特 = " << lat_compressed.length() << " 字节" << endl;
    cout << "  总计(手动累加): " << total_bits2 << " 比特" << endl;
    cout << "  compressed_bytes()返回: " << total_bytes2 << " 字节" << endl;
    cout << "  压缩率: " << fixed << setprecision(2) 
         << (100.0 * total_bytes2 / original_bytes) << "%" << endl;
    cout << "  压缩比: " << (double)original_bytes / total_bytes2 << ":1" << endl;
    
    // ============ 对比验证 ============
    cout << "\n========================================" << endl;
    cout << "公平性验证" << endl;
    cout << "========================================" << endl;
    cout << "✓ 两种方法都使用 GetBuffer() / compressed_bytes()" << endl;
    cout << "✓ 两种方法都基于实际数组长度计算" << endl;
    cout << "✓ 原始大小基准相同: " << original_bytes << " 字节" << endl;
    cout << "\n最终结论:" << endl;
    cout << "  Hilbert+Delta: " << actual_bytes1 << " 字节 (压缩率 " 
         << fixed << setprecision(2) << (100.0 * actual_bytes1 / original_bytes) << "%)" << endl;
    cout << "  Serf-QT: " << total_bytes2 << " 字节 (压缩率 " 
         << (100.0 * total_bytes2 / original_bytes) << "%)" << endl;
    cout << "  Serf-QT 节省: " << (actual_bytes1 - total_bytes2) << " 字节 (" 
         << (100.0 * (actual_bytes1 - total_bytes2) / actual_bytes1) << "%)" << endl;
    
    return 0;
}


