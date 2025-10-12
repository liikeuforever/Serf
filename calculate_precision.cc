#include "compressor/space_filling_curve.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

// 计算实际数据集的MBR
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
    
    cout << "读取了 " << count << " 个点" << endl;
    return bbox;
}

// 计算给定比特数下的最大误差
double CalculateMaxError(int bits, const BoundingBox& bbox, bool use_hilbert) {
    double max_error = 0.0;
    int test_points = 1000;
    
    // 在MBR内均匀采样测试点
    for (int i = 0; i < test_points; i++) {
        double lon = bbox.min_lon + (bbox.max_lon - bbox.min_lon) * i / test_points;
        double lat = bbox.min_lat + (bbox.max_lat - bbox.min_lat) * i / test_points;
        
        GeoPoint original(lon, lat);
        GeoPoint decoded;
        
        if (use_hilbert) {
            int order = bits / 2;
            uint64_t code = SpaceFillingCurve::EncodeHilbert(original, order, bbox);
            decoded = SpaceFillingCurve::DecodeHilbert(code, order, bbox);
        } else {
            uint64_t code = SpaceFillingCurve::EncodeMBRGeoHash(original, bits, bbox);
            decoded = SpaceFillingCurve::DecodeMBRGeoHash(code, bits, bbox);
        }
        
        // 计算对角线距离
        double dx = decoded.longitude - original.longitude;
        double dy = decoded.latitude - original.latitude;
        double error = sqrt(dx * dx + dy * dy);
        max_error = max(max_error, error);
    }
    
    return max_error;
}

int main() {
    cout << fixed << setprecision(10);
    
    // 1. 读取数据集并计算MBR
    string filename = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    cout << "========================================" << endl;
    cout << "计算数据集的MBR" << endl;
    cout << "========================================" << endl;
    
    BoundingBox bbox = ComputeDatasetMBR(filename);
    cout << "MBR: [" << bbox.min_lon << ", " << bbox.max_lon << "] × [" 
         << bbox.min_lat << ", " << bbox.max_lat << "]" << endl;
    cout << "宽度: " << bbox.GetWidth() << " 度" << endl;
    cout << "高度: " << bbox.GetHeight() << " 度" << endl;
    cout << "宽高比: " << (bbox.GetWidth() / bbox.GetHeight()) << endl;
    cout << endl;
    
    // 2. 计算不同比特数下的精度
    cout << "========================================" << endl;
    cout << "不同比特数下的最大误差（对角线距离）" << endl;
    cout << "目标精度: 1.0e-05 度" << endl;
    cout << "========================================" << endl;
    
    cout << "\n--- MBR-GeoHash ---" << endl;
    cout << "比特数    最大误差(度)       满足1e-5?" << endl;
    for (int bits = 20; bits <= 40; bits += 2) {
        double error = CalculateMaxError(bits, bbox, false);
        bool ok = error <= 1.0e-5;
        cout << setw(4) << bits << "    " << scientific << setprecision(6) << error 
             << "    " << (ok ? "✓" : "✗") << endl;
    }
    
    cout << "\n--- Hilbert曲线 ---" << endl;
    cout << "比特数    阶数    最大误差(度)       满足1e-5?" << endl;
    for (int bits = 20; bits <= 40; bits += 2) {
        int order = bits / 2;
        double error = CalculateMaxError(bits, bbox, true);
        bool ok = error <= 1.0e-5;
        cout << setw(4) << bits << "    " << setw(4) << order << "    " 
             << scientific << setprecision(6) << error 
             << "    " << (ok ? "✓" : "✗") << endl;
    }
    
    // 3. 找到最小满足要求的比特数
    cout << "\n========================================" << endl;
    cout << "寻找最小满足1e-5精度的比特数" << endl;
    cout << "========================================" << endl;
    
    // MBR-GeoHash
    int min_bits_mbr = -1;
    for (int bits = 10; bits <= 50; bits++) {
        double error = CalculateMaxError(bits, bbox, false);
        if (error <= 1.0e-5) {
            min_bits_mbr = bits;
            cout << "MBR-GeoHash: 至少需要 " << bits << " 比特 (误差=" 
                 << scientific << setprecision(6) << error << ")" << endl;
            break;
        }
    }
    
    // Hilbert
    int min_bits_hilbert = -1;
    for (int bits = 10; bits <= 50; bits += 2) {
        int order = bits / 2;
        double error = CalculateMaxError(bits, bbox, true);
        if (error <= 1.0e-5) {
            min_bits_hilbert = bits;
            cout << "Hilbert曲线: 至少需要 " << bits << " 比特 (阶数=" << order 
                 << ", 误差=" << scientific << setprecision(6) << error << ")" << endl;
            break;
        }
    }
    
    // 4. 理论分析
    cout << "\n========================================" << endl;
    cout << "理论分析" << endl;
    cout << "========================================" << endl;
    
    double max_dimension = max(bbox.GetWidth(), bbox.GetHeight());
    double required_cells = max_dimension / 1.0e-5;
    double required_bits_per_dim = log2(required_cells);
    
    cout << "MBR最大维度: " << fixed << setprecision(6) << max_dimension << " 度" << endl;
    cout << "要达到1e-5精度需要的格子数: " << scientific << required_cells << endl;
    cout << "每个维度需要的比特数: " << fixed << setprecision(2) << required_bits_per_dim << endl;
    cout << "总比特数（两个维度）: " << fixed << setprecision(2) << (required_bits_per_dim * 2) << endl;
    
    return 0;
}


