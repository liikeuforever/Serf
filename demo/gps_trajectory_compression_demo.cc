#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "compressor/serf_qt_gps_trajectory_compressor.h"
#include "decompressor/serf_qt_gps_trajectory_decompressor.h"

/**
 * GPS轨迹压缩算法演示程序
 * 
 * 演示基于固定步长与几何剪枝的率-失真优化GPS轨迹压缩算法
 * 输入格式：经度,纬度 (例如: 116.321572,40.008773)
 */

// 从CSV文件读取GPS轨迹数据
std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> LoadGpsDataFromCSV(const std::string& filename) {
    std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> points;
    std::ifstream file(filename);
    std::string line;
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    // 跳过标题行（如果有）
    if (std::getline(file, line) && line.find("longitude") != std::string::npos) {
        // 这是标题行，跳过
    } else {
        // 不是标题行，重置文件指针
        file.clear();
        file.seekg(0);
    }
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str)) {
            try {
                double longitude = std::stod(lon_str);
                double latitude = std::stod(lat_str);
                points.emplace_back(longitude, latitude);
            } catch (const std::exception& e) {
                std::cerr << "解析GPS坐标失败: " << line << " - " << e.what() << std::endl;
            }
        }
    }
    
    file.close();
    std::cout << "成功加载 " << points.size() << " 个GPS轨迹点" << std::endl;
    return points;
}

// 创建示例GPS轨迹数据
std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> CreateSampleGpsData() {
    std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> points;
    
    // 模拟一条从北京天安门广场开始的轨迹
    double base_lon = 116.397128;  // 天安门广场经度
    double base_lat = 39.916527;   // 天安门广场纬度
    
    // 生成一条向东北方向移动的轨迹
    for (int i = 0; i < 100; ++i) {
        double lon = base_lon + i * 0.001 + (i % 10) * 0.0001;  // 主要向东，带小幅波动
        double lat = base_lat + i * 0.0008 + (i % 7) * 0.00008; // 主要向北，带小幅波动
        points.emplace_back(lon, lat);
    }
    
    std::cout << "创建了 " << points.size() << " 个示例GPS轨迹点" << std::endl;
    return points;
}

// 计算压缩率
double CalculateCompressionRatio(size_t original_size, size_t compressed_size) {
    return static_cast<double>(original_size) / static_cast<double>(compressed_size);
}

// 计算重构误差统计
struct ErrorStats {
    double max_error = 0.0;
    double avg_error = 0.0;
    double total_error = 0.0;
    int point_count = 0;
};

ErrorStats CalculateReconstructionError(
    const std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint>& original,
    const std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint>& reconstructed) {
    
    ErrorStats stats;
    size_t min_size = std::min(original.size(), reconstructed.size());
    
    for (size_t i = 0; i < min_size; ++i) {
        double dx = original[i].longitude - reconstructed[i].longitude;
        double dy = original[i].latitude - reconstructed[i].latitude;
        double error = std::sqrt(dx * dx + dy * dy);
        
        stats.total_error += error;
        stats.max_error = std::max(stats.max_error, error);
        stats.point_count++;
    }
    
    if (stats.point_count > 0) {
        stats.avg_error = stats.total_error / stats.point_count;
    }
    
    return stats;
}

int main(int argc, char* argv[]) {
    std::cout << "=== GPS轨迹压缩算法演示 ===" << std::endl;
    std::cout << "基于固定步长与几何剪枝的率-失真优化算法" << std::endl << std::endl;
    
    // 加载GPS数据
    std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> gps_points;
    
    if (argc > 1) {
        // 从命令行参数指定的文件加载数据
        gps_points = LoadGpsDataFromCSV(argv[1]);
    } else {
        // 使用示例数据
        std::cout << "未指定输入文件，使用示例数据..." << std::endl;
        gps_points = CreateSampleGpsData();
    }
    
    if (gps_points.empty()) {
        std::cerr << "没有GPS数据可供压缩" << std::endl;
        return 1;
    }
    
    // 设置压缩参数
    int block_size = 1000;
    double max_error = 1e-4;  // 最大允许误差（度）
    
    std::cout << "压缩参数:" << std::endl;
    std::cout << "  块大小: " << block_size << std::endl;
    std::cout << "  最大误差: " << max_error << " 度" << std::endl << std::endl;
    
    // 创建压缩器
    SerfQtGpsTrajectoryCompressor compressor(block_size, max_error);
    
    std::cout << "开始压缩..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 压缩GPS轨迹数据
    for (const auto& point : gps_points) {
        compressor.AddGpsPoint(point);
    }
    compressor.Close();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto compression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // 获取压缩结果
    Array<uint8_t> compressed_data = compressor.compressed_bytes();
    long compressed_bits = compressor.get_compressed_size_in_bits();
    
    std::cout << "压缩完成!" << std::endl;
    std::cout << "压缩时间: " << compression_time.count() << " 微秒" << std::endl;
    std::cout << "压缩后大小: " << compressed_data.length() << " 字节 (" << compressed_bits << " 比特)" << std::endl;
    
    // 计算原始数据大小（每个点16字节：8字节经度 + 8字节纬度）
    size_t original_size = gps_points.size() * 16;
    double compression_ratio = CalculateCompressionRatio(original_size, compressed_data.length());
    
    std::cout << "原始数据大小: " << original_size << " 字节" << std::endl;
    std::cout << "压缩率: " << std::fixed << std::setprecision(2) << compression_ratio << ":1" << std::endl << std::endl;
    
    // 解压缩测试
    std::cout << "开始解压缩..." << std::endl;
    start_time = std::chrono::high_resolution_clock::now();
    
    SerfQtGpsTrajectoryDecompressor decompressor(compressed_data);
    std::vector<SerfQtGpsTrajectoryCompressor::GpsPoint> reconstructed_points;
    
    // 解压缩所有点
    for (size_t i = 0; i < gps_points.size(); ++i) {
        auto point = decompressor.DecompressNextPoint();
        reconstructed_points.push_back(point);
    }
    
    end_time = std::chrono::high_resolution_clock::now();
    auto decompression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "解压缩完成!" << std::endl;
    std::cout << "解压缩时间: " << decompression_time.count() << " 微秒" << std::endl;
    std::cout << "重构点数: " << reconstructed_points.size() << std::endl << std::endl;
    
    // 计算重构误差
    ErrorStats error_stats = CalculateReconstructionError(gps_points, reconstructed_points);
    
    std::cout << "=== 重构误差统计 ===" << std::endl;
    std::cout << "最大误差: " << std::scientific << std::setprecision(6) << error_stats.max_error << " 度" << std::endl;
    std::cout << "平均误差: " << std::scientific << std::setprecision(6) << error_stats.avg_error << " 度" << std::endl;
    std::cout << "误差上界: " << std::scientific << std::setprecision(6) << max_error << " 度" << std::endl;
    
    if (error_stats.max_error <= max_error) {
        std::cout << "✓ 所有重构点都在误差上界内" << std::endl;
    } else {
        std::cout << "✗ 部分重构点超出误差上界" << std::endl;
    }
    
    std::cout << std::endl;
    
    // 显示前几个点的对比
    std::cout << "=== 前10个点的对比 ===" << std::endl;
    std::cout << std::setw(5) << "索引" << std::setw(15) << "原始经度" << std::setw(15) << "原始纬度" 
              << std::setw(15) << "重构经度" << std::setw(15) << "重构纬度" << std::setw(12) << "误差" << std::endl;
    std::cout << std::string(82, '-') << std::endl;
    
    for (size_t i = 0; i < std::min(size_t(10), gps_points.size()); ++i) {
        double dx = gps_points[i].longitude - reconstructed_points[i].longitude;
        double dy = gps_points[i].latitude - reconstructed_points[i].latitude;
        double error = std::sqrt(dx * dx + dy * dy);
        
        std::cout << std::setw(5) << i 
                  << std::setw(15) << std::fixed << std::setprecision(6) << gps_points[i].longitude
                  << std::setw(15) << std::fixed << std::setprecision(6) << gps_points[i].latitude
                  << std::setw(15) << std::fixed << std::setprecision(6) << reconstructed_points[i].longitude
                  << std::setw(15) << std::fixed << std::setprecision(6) << reconstructed_points[i].latitude
                  << std::setw(12) << std::scientific << std::setprecision(3) << error << std::endl;
    }
    
    std::cout << std::endl << "演示完成!" << std::endl;
    return 0;
}
