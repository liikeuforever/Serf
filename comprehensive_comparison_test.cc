/**
 * 综合对比测试程序
 * 测试 LDR、CP、ZP 单独预测器、TrajCompress-SP 和 Serf-QT
 */

#include "src/compressor/trajcompress_sp_compressor.h"
#include "src/compressor/serf_qt_compressor.h"
#include "src/utils/output_bit_stream.h"
#include "src/utils/elias_gamma_codec.h"
#include "src/utils/zig_zag_codec.h"
#include "src/utils/double.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <memory>

using GpsPoint = TrajCompressSPCompressor::GpsPoint;

// 从CSV文件读取GPS数据
std::vector<GpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    std::string line;
    int count = 0;
    
    while (std::getline(file, line) && (max_points < 0 || count < max_points)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                double longitude = std::stod(lon_str);
                double latitude = std::stod(lat_str);
                points.emplace_back(longitude, latitude);
                count++;
            } catch (const std::exception& e) {
                // 跳过无效行
            }
        }
    }
    
    file.close();
    return points;
}

// 测试单个预测器
double TestSinglePredictor(const std::vector<GpsPoint>& gps_data, 
                          TrajCompressSPCompressor::PredictorType predictor_type,
                          double quantization_step) {
    if (gps_data.size() < 2) {
        return 0.0;
    }
    
    // 创建输出流
    auto output_stream = std::make_unique<OutputBitStream>();
    
    // 写入第一个点（完整精度）
    output_stream->WriteBits(DoubleBits::ExtractBits(gps_data[0].longitude), 64);
    output_stream->WriteBits(DoubleBits::ExtractBits(gps_data[0].latitude), 64);
    
    // 初始化状态
    GpsPoint current_point = gps_data[0];
    GpsPoint prev_point = gps_data[0];
    GpsPoint prev_prev_point = gps_data[0];
    
    // 压缩后续点
    for (size_t i = 1; i < gps_data.size(); ++i) {
        const GpsPoint& original_point = gps_data[i];
        
        // 预测
        GpsPoint predicted_point;
        if (predictor_type == TrajCompressSPCompressor::PREDICTOR_LDR) {
            // 线性预测
            GpsPoint velocity = current_point - prev_point;
            predicted_point = current_point + velocity;
        } else if (predictor_type == TrajCompressSPCompressor::PREDICTOR_CP) {
            // 曲线预测
            if (i >= 2) {
                GpsPoint velocity = current_point - prev_point;
                GpsPoint prev_velocity = prev_point - prev_prev_point;
                GpsPoint acceleration = velocity - prev_velocity;
                predicted_point = current_point + velocity + acceleration;
            } else {
                GpsPoint velocity = current_point - prev_point;
                predicted_point = current_point + velocity;
            }
        } else { // ZP
            // 零预测/前值
            predicted_point = current_point;
        }
        
        // 计算预测误差
        GpsPoint delta = original_point - predicted_point;
        
        // 量化
        long quantized_delta_lon = static_cast<long>(std::round(delta.longitude / quantization_step));
        long quantized_delta_lat = static_cast<long>(std::round(delta.latitude / quantization_step));
        
        // Elias Gamma 编码
        EliasGammaCodec::Encode(output_stream.get(), ZigZagCodec::Encode(quantized_delta_lon));
        EliasGammaCodec::Encode(output_stream.get(), ZigZagCodec::Encode(quantized_delta_lat));
        
        // 更新重构点
        GpsPoint reconstructed_delta(
            quantized_delta_lon * quantization_step,
            quantized_delta_lat * quantization_step
        );
        prev_prev_point = prev_point;
        prev_point = current_point;
        current_point = predicted_point + reconstructed_delta;
    }
    
    output_stream->Flush();
    size_t total_bits = output_stream->GetBitCount();
    
    return static_cast<double>(total_bits) / gps_data.size();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <数据文件> <最大点数> [epsilon]" << std::endl;
        return 1;
    }
    
    std::string dataset_path = argv[1];
    int max_points = std::stoi(argv[2]);
    double base_epsilon = (argc > 3) ? std::stod(argv[3]) : 1e-5;
    
    // 加载数据
    auto gps_data = LoadGpsDataFromCSV(dataset_path, max_points);
    
    if (gps_data.empty()) {
        std::cerr << "无法加载GPS数据" << std::endl;
        return 1;
    }
    
    std::cout << "数据点数: " << gps_data.size() << std::endl;
    
    // 使用2×epsilon作为量化步长（与Serf-QT对齐）
    double quantization_step = 2.0 * base_epsilon;
    
    // 测试单预测器
    double ldr_bits = TestSinglePredictor(gps_data, TrajCompressSPCompressor::PREDICTOR_LDR, quantization_step);
    double cp_bits = TestSinglePredictor(gps_data, TrajCompressSPCompressor::PREDICTOR_CP, quantization_step);
    double zp_bits = TestSinglePredictor(gps_data, TrajCompressSPCompressor::PREDICTOR_ZP, quantization_step);
    
    // 测试TrajCompress-SP
    TrajCompressSPCompressor sp_compressor(gps_data.size(), quantization_step);
    for (const auto& point : gps_data) {
        sp_compressor.AddGpsPoint(point);
    }
    const auto& compressed_data = sp_compressor.GetCompressedData();
    double sp_bits = static_cast<double>(compressed_data.size() * 8) / gps_data.size();
    
    // 测试Serf-QT
    SerfQTCompressor<double> qt_lon_compressor(base_epsilon);
    SerfQTCompressor<double> qt_lat_compressor(base_epsilon);
    for (const auto& point : gps_data) {
        qt_lon_compressor.AddValue(point.longitude);
        qt_lat_compressor.AddValue(point.latitude);
    }
    size_t qt_lon_bits = qt_lon_compressor.GetCompressedData().size() * 8;
    size_t qt_lat_bits = qt_lat_compressor.GetCompressedData().size() * 8;
    double qt_bits = static_cast<double>(qt_lon_bits + qt_lat_bits) / gps_data.size();
    
    // 输出结果
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "LDR: " << ldr_bits << std::endl;
    std::cout << "CP: " << cp_bits << std::endl;
    std::cout << "ZP: " << zp_bits << std::endl;
    std::cout << "TrajCompress-SP: " << sp_bits << std::endl;
    std::cout << "Serf-QT: " << qt_bits << std::endl;
    
    return 0;
}

