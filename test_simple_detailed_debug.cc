#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>
#include "src/compressor/trajcompress_sp_adaptive_simple_compressor.h"

using namespace std;

struct GpsPoint {
    double longitude;
    double latitude;
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
};

vector<GpsPoint> LoadGpsData(const string& filepath, int max_points) {
    vector<GpsPoint> data;
    ifstream file(filepath);
    string line;
    
    getline(file, line);  // Skip header
    
    while (getline(file, line) && (max_points == -1 || data.size() < (size_t)max_points)) {
        stringstream ss(line);
        string lon_str, lat_str;
        
        if (getline(ss, lon_str, ',') && getline(ss, lat_str, ',')) {
            data.emplace_back(stod(lon_str), stod(lat_str));
        }
    }
    
    return data;
}

int main(int argc, char* argv[]) {
    int test_points = (argc > 1) ? atoi(argv[1]) : 10;
    
    cout << "===== 详细调试测试：Simple压缩解压 =====" << endl;
    cout << "测试点数: " << test_points << endl << endl;
    
    // 加载数据
    string dataset_path = "test/data_set/Geolife_100k_longitude_latitude.csv";
    auto gps_data = LoadGpsData(dataset_path, test_points);
    cout << "✓ 加载了 " << gps_data.size() << " 个GPS点" << endl << endl;
    
    // 显示原始数据
    cout << "===== 原始数据（前10个点）=====" << endl;
    for (size_t i = 0; i < min((size_t)10, gps_data.size()); i++) {
        cout << "点 " << (i + 1) << ": (" 
             << fixed << setprecision(8) << gps_data[i].longitude << ", " 
             << gps_data[i].latitude << ")" << endl;
    }
    cout << endl;
    
    // 压缩
    cout << "===== 开始压缩 =====" << endl;
    TrajCompressSPAdaptiveSimpleCompressor compressor(test_points, 1e-5, 96);
    
    for (size_t i = 0; i < gps_data.size(); i++) {
        compressor.AddGpsPoint(TrajCompressSPAdaptiveSimpleCompressor::GpsPoint(
            gps_data[i].longitude, gps_data[i].latitude));
        
        auto stats = compressor.GetStats();
        
        // 在关键点打印详细信息
        if (i < 5 || i == 95 || i == 96 || i == gps_data.size() - 1) {
            cout << "压缩点 " << (i + 1) << ":" << endl;
            cout << "  坐标: (" << fixed << setprecision(8) 
                 << gps_data[i].longitude << ", " << gps_data[i].latitude << ")" << endl;
            cout << "  当前模式: " << (stats.multi_predictor_mode_points > stats.ldr_only_mode_points ? "Multi" : "LDR-Only") << endl;
            cout << "  Multi模式点数: " << stats.multi_predictor_mode_points << endl;
            cout << "  LDR-Only模式点数: " << stats.ldr_only_mode_points << endl;
            cout << "  已压缩比特数: " << compressor.GetCompressedSizeInBits() << endl;
            cout << endl;
        }
        
        // 在窗口边界打印
        if ((i + 1) % 96 == 0) {
            cout << ">>> 评估窗口边界（第 " << (i + 1) << " 个点后）<<<" << endl;
            cout << "  模式切换次数: " << stats.mode_switch_count << endl;
            cout << endl;
        }
    }
    
    compressor.Close();
    auto buffer = compressor.GetCompressedData();
    
    cout << "✓ 压缩完成" << endl;
    cout << "  总大小: " << compressor.GetCompressedSizeInBits() << " bits" << endl;
    cout << "  压缩数据字节数: " << buffer.length() << " bytes" << endl;
    cout << "  平均: " << (double)compressor.GetCompressedSizeInBits() / test_points << " bits/点" << endl << endl;
    
    // 显示压缩数据的前几个字节（16进制）
    cout << "压缩数据前32字节（16进制）:" << endl;
    for (int i = 0; i < min(32, buffer.length()); i++) {
        cout << hex << setw(2) << setfill('0') << (int)(unsigned char)buffer[i] << " ";
        if ((i + 1) % 16 == 0) cout << endl;
    }
    cout << dec << endl << endl;
    
    // 解压
    cout << "===== 开始解压 =====" << endl;
    TrajCompressSPAdaptiveSimpleDecompressor decompressor(&buffer[0], buffer.length());
    
    vector<TrajCompressSPAdaptiveSimpleDecompressor::GpsPoint> decompressed;
    TrajCompressSPAdaptiveSimpleDecompressor::GpsPoint point;
    
    int point_count = 0;
    while (point_count < test_points) {
        bool success = decompressor.ReadNextPoint(point);
        
        if (!success) {
            cout << "! 解压在第 " << (point_count + 1) << " 个点失败（返回false）" << endl;
            break;
        }
        
        decompressed.push_back(point);
        point_count++;
        
        // 在关键点打印详细信息
        if (point_count <= 5 || point_count == 96 || point_count == 97 || point_count == test_points) {
            cout << "解压点 " << point_count << ":" << endl;
            cout << "  坐标: (" << fixed << setprecision(8) 
                 << point.longitude << ", " << point.latitude << ")" << endl;
            
            if (point_count <= gps_data.size()) {
                double error = sqrt(
                    pow(gps_data[point_count - 1].longitude - point.longitude, 2) +
                    pow(gps_data[point_count - 1].latitude - point.latitude, 2)
                );
                cout << "  误差: " << scientific << setprecision(6) << error << " 度" << endl;
            }
            cout << endl;
        }
        
        // 在窗口边界打印
        if (point_count % 96 == 0) {
            cout << ">>> 评估窗口边界（第 " << point_count << " 个点后）<<<" << endl << endl;
        }
    }
    
    cout << "✓ 解压完成，共 " << decompressed.size() << " 个点" << endl << endl;
    
    // 详细对比前10个点
    cout << "===== 详细对比（前10个点）=====" << endl;
    for (size_t i = 0; i < min((size_t)10, min(gps_data.size(), decompressed.size())); i++) {
        double error = sqrt(
            pow(gps_data[i].longitude - decompressed[i].longitude, 2) +
            pow(gps_data[i].latitude - decompressed[i].latitude, 2)
        );
        
        cout << "点 " << (i + 1) << ":" << endl;
        cout << "  原始: (" << fixed << setprecision(8) 
             << gps_data[i].longitude << ", " << gps_data[i].latitude << ")" << endl;
        cout << "  解压: (" << fixed << setprecision(8)
             << decompressed[i].longitude << ", " << decompressed[i].latitude << ")" << endl;
        cout << "  误差: " << scientific << setprecision(6) << error << " 度";
        if (error > 1e-5) {
            cout << " ⚠️ 超限！";
        }
        cout << endl << endl;
    }
    
    // 如果有后面的点，也显示最后几个
    if (decompressed.size() > 10) {
        cout << "===== 详细对比（最后3个点）=====" << endl;
        for (size_t i = max((size_t)0, decompressed.size() - 3); i < decompressed.size(); i++) {
            double error = sqrt(
                pow(gps_data[i].longitude - decompressed[i].longitude, 2) +
                pow(gps_data[i].latitude - decompressed[i].latitude, 2)
            );
            
            cout << "点 " << (i + 1) << ":" << endl;
            cout << "  原始: (" << fixed << setprecision(8) 
                 << gps_data[i].longitude << ", " << gps_data[i].latitude << ")" << endl;
            cout << "  解压: (" << fixed << setprecision(8)
                 << decompressed[i].longitude << ", " << decompressed[i].latitude << ")" << endl;
            cout << "  误差: " << scientific << setprecision(6) << error << " 度" << endl << endl;
        }
    }
    
    return 0;
}

