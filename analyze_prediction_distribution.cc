/**
 * 分析预测误差的分布特性
 * 找出为什么 ZP 预测误差小但编码开销大
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>

struct GpsPoint {
    double longitude;
    double latitude;
    
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
    
    GpsPoint operator-(const GpsPoint& other) const {
        return GpsPoint(longitude - other.longitude, latitude - other.latitude);
    }
    
    GpsPoint operator+(const GpsPoint& other) const {
        return GpsPoint(longitude + other.longitude, latitude + other.latitude);
    }
};

std::vector<GpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) return points;
    
    std::string line;
    int count = 0;
    
    while (std::getline(file, line) && count < max_points) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                points.emplace_back(std::stod(lon_str), std::stod(lat_str));
                count++;
            } catch (...) {}
        }
    }
    
    return points;
}

int main() {
    std::string dataset_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int max_points = 100000;
    double epsilon = 1e-5 * std::sqrt(2);
    
    auto gps_data = LoadGpsDataFromCSV(dataset_path, max_points);
    if (gps_data.size() < 3) return 1;
    
    std::cout << "分析预测误差分布特性" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "数据点数: " << gps_data.size() << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    // 分析 LDR
    std::vector<double> ldr_residual_magnitudes;
    std::vector<int64_t> ldr_quantized_lon, ldr_quantized_lat;
    GpsPoint current = gps_data[0], prev = gps_data[0];
    
    for (size_t i = 1; i < gps_data.size(); i++) {
        // LDR 预测
        GpsPoint velocity(current.longitude - prev.longitude, current.latitude - prev.latitude);
        GpsPoint predicted(current.longitude + velocity.longitude, current.latitude + velocity.latitude);
        
        // 残差
        GpsPoint residual = gps_data[i] - predicted;
        double mag = std::sqrt(residual.longitude * residual.longitude + residual.latitude * residual.latitude);
        ldr_residual_magnitudes.push_back(mag);
        
        // 量化
        int64_t q_lon = static_cast<int64_t>(std::round(residual.longitude / epsilon));
        int64_t q_lat = static_cast<int64_t>(std::round(residual.latitude / epsilon));
        ldr_quantized_lon.push_back(q_lon);
        ldr_quantized_lat.push_back(q_lat);
        
        // 重构
        GpsPoint reconstructed(predicted.longitude + q_lon * epsilon, predicted.latitude + q_lat * epsilon);
        prev = current;
        current = reconstructed;
    }
    
    // 分析 ZP
    std::vector<double> zp_residual_magnitudes;
    std::vector<int64_t> zp_quantized_lon, zp_quantized_lat;
    current = gps_data[0];
    
    for (size_t i = 1; i < gps_data.size(); i++) {
        // ZP 预测（前值）
        GpsPoint predicted = current;
        
        // 残差
        GpsPoint residual = gps_data[i] - predicted;
        double mag = std::sqrt(residual.longitude * residual.longitude + residual.latitude * residual.latitude);
        zp_residual_magnitudes.push_back(mag);
        
        // 量化
        int64_t q_lon = static_cast<int64_t>(std::round(residual.longitude / epsilon));
        int64_t q_lat = static_cast<int64_t>(std::round(residual.latitude / epsilon));
        zp_quantized_lon.push_back(q_lon);
        zp_quantized_lat.push_back(q_lat);
        
        // 重构
        GpsPoint reconstructed(predicted.longitude + q_lon * epsilon, predicted.latitude + q_lat * epsilon);
        current = reconstructed;
    }
    
    // 统计
    auto calc_stats = [](const std::vector<int64_t>& data) {
        std::vector<int64_t> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        
        double mean = 0;
        for (auto v : data) mean += std::abs(v);
        mean /= data.size();
        
        size_t p50 = sorted.size() * 0.50;
        size_t p90 = sorted.size() * 0.90;
        size_t p99 = sorted.size() * 0.99;
        
        int zeros = std::count(sorted.begin(), sorted.end(), 0);
        
        return std::make_tuple(mean, sorted[p50], sorted[p90], sorted[p99], zeros, *sorted.rbegin());
    };
    
    auto [ldr_mean_lon, ldr_p50_lon, ldr_p90_lon, ldr_p99_lon, ldr_zeros_lon, ldr_max_lon] = calc_stats(ldr_quantized_lon);
    auto [ldr_mean_lat, ldr_p50_lat, ldr_p90_lat, ldr_p99_lat, ldr_zeros_lat, ldr_max_lat] = calc_stats(ldr_quantized_lat);
    auto [zp_mean_lon, zp_p50_lon, zp_p90_lon, zp_p99_lon, zp_zeros_lon, zp_max_lon] = calc_stats(zp_quantized_lon);
    auto [zp_mean_lat, zp_p50_lat, zp_p90_lat, zp_p99_lat, zp_zeros_lat, zp_max_lat] = calc_stats(zp_quantized_lat);
    
    std::cout << "\n=== LDR (线性预测) ===" << std::endl;
    std::cout << "经度量化值统计:" << std::endl;
    std::cout << "  平均绝对值: " << std::fixed << std::setprecision(2) << ldr_mean_lon << std::endl;
    std::cout << "  P50: " << ldr_p50_lon << ", P90: " << ldr_p90_lon << ", P99: " << ldr_p99_lon << std::endl;
    std::cout << "  最大值: " << ldr_max_lon << ", 零值数量: " << ldr_zeros_lon 
              << " (" << std::setprecision(1) << (100.0 * ldr_zeros_lon / ldr_quantized_lon.size()) << "%)" << std::endl;
    
    std::cout << "纬度量化值统计:" << std::endl;
    std::cout << "  平均绝对值: " << std::fixed << std::setprecision(2) << ldr_mean_lat << std::endl;
    std::cout << "  P50: " << ldr_p50_lat << ", P90: " << ldr_p90_lat << ", P99: " << ldr_p99_lat << std::endl;
    std::cout << "  最大值: " << ldr_max_lat << ", 零值数量: " << ldr_zeros_lat 
              << " (" << std::setprecision(1) << (100.0 * ldr_zeros_lat / ldr_quantized_lat.size()) << "%)" << std::endl;
    
    std::cout << "\n=== ZP (零预测/前值) ===" << std::endl;
    std::cout << "经度量化值统计:" << std::endl;
    std::cout << "  平均绝对值: " << std::fixed << std::setprecision(2) << zp_mean_lon << std::endl;
    std::cout << "  P50: " << zp_p50_lon << ", P90: " << zp_p90_lon << ", P99: " << zp_p99_lon << std::endl;
    std::cout << "  最大值: " << zp_max_lon << ", 零值数量: " << zp_zeros_lon 
              << " (" << std::setprecision(1) << (100.0 * zp_zeros_lon / zp_quantized_lon.size()) << "%)" << std::endl;
    
    std::cout << "纬度量化值统计:" << std::endl;
    std::cout << "  平均绝对值: " << std::fixed << std::setprecision(2) << zp_mean_lat << std::endl;
    std::cout << "  P50: " << zp_p50_lat << ", P90: " << zp_p90_lat << ", P99: " << zp_p99_lat << std::endl;
    std::cout << "  最大值: " << zp_max_lat << ", 零值数量: " << zp_zeros_lat 
              << " (" << std::setprecision(1) << (100.0 * zp_zeros_lat / zp_quantized_lat.size()) << "%)" << std::endl;
    
    std::cout << "\n=== 关键发现 ===" << std::endl;
    std::cout << "平均量化值 (绝对值):" << std::endl;
    std::cout << "  LDR: " << std::setprecision(2) << (ldr_mean_lon + ldr_mean_lat) / 2 << std::endl;
    std::cout << "  ZP:  " << (zp_mean_lon + zp_mean_lat) / 2 << std::endl;
    std::cout << "  差异: " << std::setprecision(1) 
              << (((zp_mean_lon + zp_mean_lat) - (ldr_mean_lon + ldr_mean_lat)) / (ldr_mean_lon + ldr_mean_lat) * 100) 
              << "%" << std::endl;
    
    std::cout << "\n零值占比（Elias Gamma 编码最省space）:" << std::endl;
    std::cout << "  LDR: " << std::setprecision(1) 
              << (100.0 * (ldr_zeros_lon + ldr_zeros_lat) / (ldr_quantized_lon.size() + ldr_quantized_lat.size())) 
              << "%" << std::endl;
    std::cout << "  ZP:  " 
              << (100.0 * (zp_zeros_lon + zp_zeros_lat) / (zp_quantized_lon.size() + zp_quantized_lat.size())) 
              << "%" << std::endl;
    
    return 0;
}

