#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

#include "compressor/serf_qt_gps_configurable_compressor.h"

// 读取GPS点数据
std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> ReadGpsPoints(const std::string& filename, int max_points) {
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    std::string line;
    std::getline(file, line); // 跳过标题行
    
    int count = 0;
    while (std::getline(file, line) && count < max_points) {
        std::istringstream iss(line);
        std::string longitude_str, latitude_str;
        
        if (std::getline(iss, longitude_str, ',') && std::getline(iss, latitude_str)) {
            try {
                double longitude = std::stod(longitude_str);
                double latitude = std::stod(latitude_str);
                points.emplace_back(longitude, latitude);
                count++;
            } catch (const std::exception& e) {
                continue;
            }
        }
    }
    
    return points;
}

// 计算运动矢量
struct MotionVector {
    double velocity;
    double theta;
    MotionVector(double v, double t) : velocity(v), theta(t) {}
};

MotionVector CalculateMotionVector(const SerfQtGpsConfigurableCompressor::GpsPoint& start, 
                                  const SerfQtGpsConfigurableCompressor::GpsPoint& end) {
    double dx = end.longitude - start.longitude;
    double dy = end.latitude - start.latitude;
    double velocity = std::sqrt(dx * dx + dy * dy);
    double theta = std::atan2(dy, dx);
    return MotionVector(velocity, theta);
}

// 计算目标点
SerfQtGpsConfigurableCompressor::GpsPoint CalculateDestinationPoint(
    const SerfQtGpsConfigurableCompressor::GpsPoint& start, const MotionVector& motion) {
    double dx = motion.velocity * std::cos(motion.theta);
    double dy = motion.velocity * std::sin(motion.theta);
    return SerfQtGpsConfigurableCompressor::GpsPoint(start.longitude + dx, start.latitude + dy);
}

// 计算距离
double CalculateDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1, 
                        const SerfQtGpsConfigurableCompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    std::cout << "=== 量化误差对GPS轨迹预测影响分析 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 1000;  // 使用较少数据进行详细分析
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double error_bound = 1e-5;
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << error_bound << " 度" << std::endl;
    std::cout << "  εv = " << epsilon_v << ", εθ = " << epsilon_theta << std::endl;
    
    // 分析不同预测方法的准确率
    std::cout << "\n=== 预测方法对比分析 ===" << std::endl;
    
    // 方法1: 理想预测（使用真实点，无量化误差）
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> ideal_points;
    std::vector<MotionVector> ideal_motions;
    int ideal_predictions = 0, ideal_accurate = 0;
    
    // 方法2: 量化预测（使用量化后的重构点）
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> quantized_points;
    std::vector<MotionVector> quantized_motions;
    int quantized_predictions = 0, quantized_accurate = 0;
    
    // 方法3: Serf-QT风格的线性预测（使用量化后的坐标）
    std::vector<double> linear_lons, linear_lats;
    int linear_predictions = 0, linear_accurate = 0;
    const double linear_max_diff = error_bound / std::sqrt(2.0);
    
    std::cout << "\n🔄 进行预测对比..." << std::endl;
    
    for (int i = 0; i < points.size(); ++i) {
        // === 方法1: 理想预测 ===
        if (i < 2) {
            ideal_points.push_back(points[i]);
            if (i == 1) {
                ideal_motions.push_back(CalculateMotionVector(points[0], points[1]));
            }
        } else {
            // 使用真实点进行预测（零阶预测）
            const auto& current_point = ideal_points.back();
            const auto& current_motion = ideal_motions.back();
            
            auto predicted_point = CalculateDestinationPoint(current_point, current_motion);
            double error = CalculateDistance(points[i], predicted_point);
            
            ideal_predictions++;
            if (error <= error_bound) {
                ideal_accurate++;
            }
            
            // 更新状态（使用真实点）
            ideal_points.push_back(points[i]);
            if (ideal_points.size() >= 2) {
                ideal_motions.push_back(CalculateMotionVector(
                    ideal_points[ideal_points.size() - 2], 
                    ideal_points.back()
                ));
            }
        }
        
        // === 方法2: 量化预测 ===
        if (i < 2) {
            quantized_points.push_back(points[i]);
            if (i == 1) {
                auto true_motion = CalculateMotionVector(points[0], points[1]);
                // 量化运动矢量
                int64_t qv = static_cast<int64_t>(std::round(true_motion.velocity / epsilon_v));
                int64_t qtheta = static_cast<int64_t>(std::round(true_motion.theta / epsilon_theta));
                MotionVector quantized_motion(qv * epsilon_v, qtheta * epsilon_theta);
                quantized_motions.push_back(quantized_motion);
                
                // 重构第二个点
                auto reconstructed_second = CalculateDestinationPoint(quantized_points[0], quantized_motion);
                quantized_points.back() = reconstructed_second;
            }
        } else {
            // 使用量化后的重构点进行预测
            const auto& current_point = quantized_points.back();
            const auto& current_motion = quantized_motions.back();
            
            auto predicted_point = CalculateDestinationPoint(current_point, current_motion);
            double error = CalculateDistance(points[i], predicted_point);
            
            quantized_predictions++;
            if (error <= error_bound) {
                quantized_accurate++;
            }
            
            // 量化并重构当前点
            auto true_motion = CalculateMotionVector(current_point, points[i]);
            int64_t qv = static_cast<int64_t>(std::round(true_motion.velocity / epsilon_v));
            int64_t qtheta = static_cast<int64_t>(std::round(true_motion.theta / epsilon_theta));
            MotionVector quantized_motion(qv * epsilon_v, qtheta * epsilon_theta);
            
            auto reconstructed_point = CalculateDestinationPoint(current_point, quantized_motion);
            quantized_points.push_back(reconstructed_point);
            quantized_motions.push_back(quantized_motion);
        }
        
        // === 方法3: 线性预测 ===
        if (i < 2) {
            // 简化处理前两个点
            linear_lons.push_back(points[i].longitude);
            linear_lats.push_back(points[i].latitude);
        } else {
            // 线性预测
            double predicted_lon = 2.0 * linear_lons.back() - linear_lons[linear_lons.size() - 2];
            double predicted_lat = 2.0 * linear_lats.back() - linear_lats[linear_lats.size() - 2];
            
            // 计算2D误差
            double dx = points[i].longitude - predicted_lon;
            double dy = points[i].latitude - predicted_lat;
            double error_2d = std::sqrt(dx * dx + dy * dy);
            
            linear_predictions++;
            if (error_2d <= error_bound) {
                linear_accurate++;
            }
            
            // 量化和重构（模拟Serf-QT的处理）
            long q_lon = static_cast<long>(std::round((points[i].longitude - predicted_lon) / (2 * linear_max_diff)));
            long q_lat = static_cast<long>(std::round((points[i].latitude - predicted_lat) / (2 * linear_max_diff)));
            
            double reconstructed_lon = predicted_lon + 2 * linear_max_diff * static_cast<double>(q_lon);
            double reconstructed_lat = predicted_lat + 2 * linear_max_diff * static_cast<double>(q_lat);
            
            linear_lons.push_back(reconstructed_lon);
            linear_lats.push_back(reconstructed_lat);
        }
    }
    
    // 计算结果
    double ideal_rate = static_cast<double>(ideal_accurate) / ideal_predictions * 100.0;
    double quantized_rate = static_cast<double>(quantized_accurate) / quantized_predictions * 100.0;
    double linear_rate = static_cast<double>(linear_accurate) / linear_predictions * 100.0;
    
    // 打印结果
    std::cout << "\n=== 预测准确率对比结果 ===" << std::endl;
    
    std::cout << "\n📊 方法1 - 理想运动矢量预测 (无量化误差):" << std::endl;
    std::cout << "  预测次数: " << ideal_predictions << std::endl;
    std::cout << "  准确预测: " << ideal_accurate << std::endl;
    std::cout << "  准确率: " << std::fixed << std::setprecision(2) << ideal_rate << "%" << std::endl;
    
    std::cout << "\n📊 方法2 - 量化运动矢量预测 (有量化误差):" << std::endl;
    std::cout << "  预测次数: " << quantized_predictions << std::endl;
    std::cout << "  准确预测: " << quantized_accurate << std::endl;
    std::cout << "  准确率: " << quantized_rate << "%" << std::endl;
    
    std::cout << "\n📊 方法3 - 线性坐标预测 (Serf-QT风格):" << std::endl;
    std::cout << "  预测次数: " << linear_predictions << std::endl;
    std::cout << "  准确预测: " << linear_accurate << std::endl;
    std::cout << "  准确率: " << linear_rate << "%" << std::endl;
    
    // 分析量化误差的影响
    std::cout << "\n=== 量化误差影响分析 ===" << std::endl;
    
    double quantization_impact = ideal_rate - quantized_rate;
    std::cout << "量化误差对运动矢量预测的影响: " << std::setprecision(2) << quantization_impact << "%" << std::endl;
    
    if (quantization_impact > 5.0) {
        std::cout << "🚨 量化误差严重影响预测准确率！" << std::endl;
        std::cout << "原因分析:" << std::endl;
        std::cout << "1. εv=" << epsilon_v << " 可能过大，导致速度量化误差" << std::endl;
        std::cout << "2. εθ=" << epsilon_theta << " 可能过大，导致角度量化误差" << std::endl;
        std::cout << "3. 极坐标量化误差在转换为直角坐标时被放大" << std::endl;
    } else if (quantization_impact > 2.0) {
        std::cout << "⚠️ 量化误差有一定影响" << std::endl;
    } else {
        std::cout << "✅ 量化误差影响较小" << std::endl;
    }
    
    // 对比不同方法
    std::cout << "\n=== 方法对比分析 ===" << std::endl;
    
    if (linear_rate > quantized_rate) {
        double advantage = linear_rate - quantized_rate;
        std::cout << "📈 线性坐标预测优于量化运动矢量预测 " << advantage << "%" << std::endl;
        std::cout << "可能原因:" << std::endl;
        std::cout << "1. 直角坐标量化比极坐标量化误差更小" << std::endl;
        std::cout << "2. 线性预测比零阶预测更适合GPS轨迹" << std::endl;
        std::cout << "3. 1D独立预测在某些情况下比2D相关预测更稳定" << std::endl;
    } else {
        std::cout << "📈 量化运动矢量预测优于线性坐标预测" << std::endl;
    }
    
    if (ideal_rate > linear_rate) {
        std::cout << "💡 理想运动矢量预测最优，说明2D预测方法本身是正确的" << std::endl;
        std::cout << "问题在于量化参数的选择和量化方式" << std::endl;
    }
    
    return 0;
}
