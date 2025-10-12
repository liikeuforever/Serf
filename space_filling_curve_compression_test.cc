#include "compressor/space_filling_curve.h"
#include "compressor/sfc_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>

using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

// 从CSV文件读取经纬度数据
std::vector<GeoPoint> LoadGeolifeData(const std::string& filepath) {
    std::vector<GeoPoint> points;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filepath << std::endl;
        return points;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(iss, lon_str, ',') && std::getline(iss, lat_str, ',')) {
            try {
                double lon = std::stod(lon_str);
                double lat = std::stod(lat_str);
                points.emplace_back(lon, lat);
            } catch (const std::exception& e) {
                // 跳过无效行
                continue;
            }
        }
    }
    
    file.close();
    return points;
}

// 验证压缩精度
void VerifyCompressionAccuracy(const std::vector<GeoPoint>& original_points,
                               SFCCompressor::CompressionMethod method,
                               double max_error) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "验证压缩精度: " << (method == SFCCompressor::STANDARD_GEOHASH ? "Standard GeoHash" :
                                      method == SFCCompressor::MBR_GEOHASH ? "MBR GeoHash" :
                                      "Hilbert Curve") << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 创建压缩器
    SFCCompressor compressor(method, max_error);
    
    // 第一遍：收集边界框信息
    for (const auto& point : original_points) {
        compressor.AddPoint(point);
    }
    
    BoundingBox bbox = compressor.GetBoundingBox();
    int encoding_bits = compressor.GetEncodingBits();
    
    std::cout << "\n边界框信息:" << std::endl;
    std::cout << "  经度范围: [" << std::fixed << std::setprecision(6) 
              << bbox.min_lon << ", " << bbox.max_lon << "]" << std::endl;
    std::cout << "  纬度范围: [" << bbox.min_lat << ", " << bbox.max_lat << "]" << std::endl;
    std::cout << "  编码比特数: " << encoding_bits << " bits" << std::endl;
    
    // 计算格子的最大对角线距离
    double diagonal = SpaceFillingCurve::CalculateGeoHashMaxDiagonal(
        encoding_bits, method == SFCCompressor::STANDARD_GEOHASH ? nullptr : &bbox);
    std::cout << "  格子最大对角线距离: " << std::scientific << std::setprecision(2) 
              << diagonal << " 度 (要求: <= " << max_error << ")" << std::endl;
    
    if (diagonal > max_error) {
        std::cout << "  ⚠️  警告: 格子对角线超过最大误差要求!" << std::endl;
    } else {
        std::cout << "  ✅ 格子对角线满足精度要求" << std::endl;
    }
    
    // 验证编码和解码的精度
    std::cout << "\n验证编码/解码精度 (前10个点):" << std::endl;
    
    double max_reconstruction_error = 0.0;
    double sum_reconstruction_error = 0.0;
    int points_exceeding_error = 0;
    
    for (size_t i = 0; i < std::min(size_t(10), original_points.size()); ++i) {
        const GeoPoint& original = original_points[i];
        
        // 编码
        uint64_t code;
        if (method == SFCCompressor::STANDARD_GEOHASH) {
            code = SpaceFillingCurve::EncodeStandardGeoHash(original, encoding_bits);
        } else if (method == SFCCompressor::MBR_GEOHASH) {
            code = SpaceFillingCurve::EncodeMBRGeoHash(original, encoding_bits, bbox);
        } else {
            int order = encoding_bits / 2;
            code = SpaceFillingCurve::EncodeHilbert(original, order, bbox);
        }
        
        // 解码
        GeoPoint decoded;
        if (method == SFCCompressor::STANDARD_GEOHASH) {
            decoded = SpaceFillingCurve::DecodeStandardGeoHash(code, encoding_bits);
        } else if (method == SFCCompressor::MBR_GEOHASH) {
            decoded = SpaceFillingCurve::DecodeMBRGeoHash(code, encoding_bits, bbox);
        } else {
            int order = encoding_bits / 2;
            decoded = SpaceFillingCurve::DecodeHilbert(code, order, bbox);
        }
        
        // 计算重构误差
        double error = SpaceFillingCurve::CalculateDistance(original, decoded);
        
        std::cout << "  点 " << i << ": (" << std::fixed << std::setprecision(6)
                  << original.longitude << ", " << original.latitude << ") -> 编码=" << code
                  << " -> (" << decoded.longitude << ", " << decoded.latitude 
                  << "), 误差=" << std::scientific << std::setprecision(2) << error << " 度";
        
        if (error > max_error) {
            std::cout << " ⚠️";
            points_exceeding_error++;
        }
        std::cout << std::endl;
        
        max_reconstruction_error = std::max(max_reconstruction_error, error);
        sum_reconstruction_error += error;
    }
    
    // 验证所有点的精度
    std::cout << "\n验证所有点的精度..." << std::endl;
    for (size_t i = 10; i < original_points.size(); ++i) {
        const GeoPoint& original = original_points[i];
        
        uint64_t code;
        if (method == SFCCompressor::STANDARD_GEOHASH) {
            code = SpaceFillingCurve::EncodeStandardGeoHash(original, encoding_bits);
        } else if (method == SFCCompressor::MBR_GEOHASH) {
            code = SpaceFillingCurve::EncodeMBRGeoHash(original, encoding_bits, bbox);
        } else {
            int order = encoding_bits / 2;
            code = SpaceFillingCurve::EncodeHilbert(original, order, bbox);
        }
        
        GeoPoint decoded;
        if (method == SFCCompressor::STANDARD_GEOHASH) {
            decoded = SpaceFillingCurve::DecodeStandardGeoHash(code, encoding_bits);
        } else if (method == SFCCompressor::MBR_GEOHASH) {
            decoded = SpaceFillingCurve::DecodeMBRGeoHash(code, encoding_bits, bbox);
        } else {
            int order = encoding_bits / 2;
            decoded = SpaceFillingCurve::DecodeHilbert(code, order, bbox);
        }
        
        double error = SpaceFillingCurve::CalculateDistance(original, decoded);
        max_reconstruction_error = std::max(max_reconstruction_error, error);
        sum_reconstruction_error += error;
        
        if (error > max_error) {
            points_exceeding_error++;
        }
    }
    
    double avg_reconstruction_error = sum_reconstruction_error / original_points.size();
    
    std::cout << "\n精度验证结果:" << std::endl;
    std::cout << "  最大重构误差: " << std::scientific << std::setprecision(2) 
              << max_reconstruction_error << " 度" << std::endl;
    std::cout << "  平均重构误差: " << avg_reconstruction_error << " 度" << std::endl;
    std::cout << "  超过误差限制的点数: " << points_exceeding_error << " / " 
              << original_points.size() << " (" << std::fixed << std::setprecision(2)
              << (100.0 * points_exceeding_error / original_points.size()) << "%)" << std::endl;
    
    if (max_reconstruction_error <= max_error) {
        std::cout << "  ✅ 所有点的重构误差都满足精度要求!" << std::endl;
    } else {
        std::cout << "  ⚠️  存在超过精度要求的点!" << std::endl;
    }
}

// 测试压缩效果
void TestCompressionMethod(const std::vector<GeoPoint>& points,
                          SFCCompressor::CompressionMethod method,
                          double max_error) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试压缩方法: ";
    
    SFCCompressor compressor(method, max_error);
    std::cout << compressor.GetMethodName() << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "最大允许误差: " << std::scientific << max_error << " 度" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 执行压缩
    std::cout << "\n执行压缩..." << std::endl;
    for (const auto& point : points) {
        compressor.AddPoint(point);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    BoundingBox bbox = compressor.GetBoundingBox();
    std::cout << "  边界框: [" << std::fixed << std::setprecision(6)
              << bbox.min_lon << ", " << bbox.max_lon << "] x ["
              << bbox.min_lat << ", " << bbox.max_lat << "]" << std::endl;
    
    // 获取压缩数据
    Array<uint8_t> compressed_data = compressor.GetCompressedData();
    
    std::cout << "  压缩完成，耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "  压缩数据大小: " << compressed_data.length() << " bytes" << std::endl;
    
    // 计算并打印统计信息
    auto stats = compressor.GetStatistics();
    const_cast<SFCCompressor::Statistics&>(stats).Calculate();
    stats.Print();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "空间填充曲线压缩测试程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 加载数据
    std::string data_file = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    std::cout << "\n加载数据集: " << data_file << std::endl;
    
    auto points = LoadGeolifeData(data_file);
    
    if (points.empty()) {
        std::cerr << "错误: 无法加载数据或数据为空!" << std::endl;
        return 1;
    }
    
    std::cout << "成功加载 " << points.size() << " 个点" << std::endl;
    
    // 设置最大误差为1e-5度（对应格子对角线距离）
    double max_error = 1e-5;
    
    std::cout << "\n目标精度: 格子最大对角线距离 <= " << std::scientific 
              << max_error << " 度" << std::endl;
    
    // 验证三种方法的精度
    std::cout << "\n======================================" << std::endl;
    std::cout << "第一部分: 精度验证" << std::endl;
    std::cout << "======================================" << std::endl;
    
    VerifyCompressionAccuracy(points, SFCCompressor::STANDARD_GEOHASH, max_error);
    VerifyCompressionAccuracy(points, SFCCompressor::MBR_GEOHASH, max_error);
    VerifyCompressionAccuracy(points, SFCCompressor::HILBERT_CURVE, max_error);
    
    // 测试三种压缩方法
    std::cout << "\n======================================" << std::endl;
    std::cout << "第二部分: 压缩效果对比" << std::endl;
    std::cout << "======================================" << std::endl;
    
    TestCompressionMethod(points, SFCCompressor::STANDARD_GEOHASH, max_error);
    TestCompressionMethod(points, SFCCompressor::MBR_GEOHASH, max_error);
    TestCompressionMethod(points, SFCCompressor::HILBERT_CURVE, max_error);
    
    // 总结对比
    std::cout << "\n========================================" << std::endl;
    std::cout << "总结" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "本测试实现了三种空间填充曲线压缩方法：" << std::endl;
    std::cout << "1. 标准GeoHash - 全球坐标系统，经纬度交错编码" << std::endl;
    std::cout << "2. MBR优化GeoHash - 基于数据集边界框优化的GeoHash" << std::endl;
    std::cout << "3. Hilbert曲线 - 基于数据集边界框的Hilbert空间填充曲线" << std::endl;
    std::cout << "\n所有方法都：" << std::endl;
    std::cout << "- 确保格子对角线距离不超过1e-5度" << std::endl;
    std::cout << "- 将二维经纬度降维到一维编码" << std::endl;
    std::cout << "- 使用差值 + ZigZag + Elias Gamma编码压缩" << std::endl;
    std::cout << "- 参考Serf-QT的差值编码思想" << std::endl;
    
    return 0;
}

