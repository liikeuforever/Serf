#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

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
    std::cout << "=== GPS轨迹压缩器预测模型 vs 真正线性预测对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 10000;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    const double error_bound = 1e-5;
    
    // 统计变量
    int gps_current_predictions = 0;  // GPS轨迹压缩器当前预测方法
    int gps_current_accurate = 0;
    
    int gps_linear_predictions = 0;   // GPS轨迹压缩器改用线性预测
    int gps_linear_accurate = 0;
    
    int linear_predictions = 0;       // 标准线性预测
    int linear_accurate = 0;
    
    std::vector<double> gps_current_errors, gps_linear_errors, linear_errors;
    
    // 模拟状态
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> reconstructed_points;
    std::vector<MotionVector> motion_vectors;
    
    std::cout << "\n🔄 对比不同预测方法..." << std::endl;
    
    for (int i = 0; i < points.size(); ++i) {
        if (i < 2) {
            // 前两个点，直接重构（简化处理）
            reconstructed_points.push_back(points[i]);
            if (i == 1) {
                motion_vectors.push_back(CalculateMotionVector(points[0], points[1]));
            }
            continue;
        }
        
        // 从第三个点开始进行预测对比
        const auto& current_point = reconstructed_points.back();
        
        // 方法1: GPS轨迹压缩器当前方法（零阶预测 - 重复上一次运动）
        if (motion_vectors.size() > 0) {
            auto predicted_point_current = CalculateDestinationPoint(current_point, motion_vectors.back());
            double error_current = CalculateDistance(points[i], predicted_point_current);
            
            gps_current_predictions++;
            gps_current_errors.push_back(error_current);
            if (error_current <= error_bound) {
                gps_current_accurate++;
            }
        }
        
        // 方法2: GPS轨迹压缩器改用线性预测（一阶预测）
        if (motion_vectors.size() >= 2) {
            // 线性外推运动矢量: predicted_motion = 2 * current - previous
            const auto& current_motion = motion_vectors.back();
            const auto& prev_motion = motion_vectors[motion_vectors.size() - 2];
            
            MotionVector predicted_motion_linear(
                2.0 * current_motion.velocity - prev_motion.velocity,
                2.0 * current_motion.theta - prev_motion.theta
            );
            
            auto predicted_point_linear = CalculateDestinationPoint(current_point, predicted_motion_linear);
            double error_linear = CalculateDistance(points[i], predicted_point_linear);
            
            gps_linear_predictions++;
            gps_linear_errors.push_back(error_linear);
            if (error_linear <= error_bound) {
                gps_linear_accurate++;
            }
        }
        
        // 方法3: 标准线性预测（直接在坐标上）
        if (reconstructed_points.size() >= 2) {
            const auto& prev1 = reconstructed_points.back();
            const auto& prev2 = reconstructed_points[reconstructed_points.size() - 2];
            
            SerfQtGpsConfigurableCompressor::GpsPoint predicted_point_standard(
                2.0 * prev1.longitude - prev2.longitude,
                2.0 * prev1.latitude - prev2.latitude
            );
            
            double error_standard = CalculateDistance(points[i], predicted_point_standard);
            
            linear_predictions++;
            linear_errors.push_back(error_standard);
            if (error_standard <= error_bound) {
                linear_accurate++;
            }
        }
        
        // 更新状态（简化：假设完美重构）
        reconstructed_points.push_back(points[i]);
        if (reconstructed_points.size() >= 2) {
            motion_vectors.push_back(CalculateMotionVector(
                reconstructed_points[reconstructed_points.size() - 2], 
                reconstructed_points.back()
            ));
        }
    }
    
    // 计算统计结果
    double gps_current_rate = static_cast<double>(gps_current_accurate) / gps_current_predictions * 100.0;
    double gps_linear_rate = static_cast<double>(gps_linear_accurate) / gps_linear_predictions * 100.0;
    double linear_rate = static_cast<double>(linear_accurate) / linear_predictions * 100.0;
    
    double gps_current_avg = std::accumulate(gps_current_errors.begin(), gps_current_errors.end(), 0.0) / gps_current_errors.size();
    double gps_linear_avg = std::accumulate(gps_linear_errors.begin(), gps_linear_errors.end(), 0.0) / gps_linear_errors.size();
    double linear_avg = std::accumulate(linear_errors.begin(), linear_errors.end(), 0.0) / linear_errors.size();
    
    // 打印结果
    std::cout << "\n=== 预测方法对比结果 ===" << std::endl;
    
    std::cout << "\n📊 GPS轨迹压缩器当前方法 (零阶预测 - 重复运动矢量):" << std::endl;
    std::cout << "  预测次数: " << gps_current_predictions << std::endl;
    std::cout << "  准确预测: " << gps_current_accurate << " (" << std::fixed << std::setprecision(2) << gps_current_rate << "%)" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << gps_current_avg << " 度" << std::endl;
    
    std::cout << "\n📊 GPS轨迹压缩器改用线性预测 (一阶预测 - 线性外推运动矢量):" << std::endl;
    std::cout << "  预测次数: " << gps_linear_predictions << std::endl;
    std::cout << "  准确预测: " << gps_linear_accurate << " (" << std::fixed << std::setprecision(2) << gps_linear_rate << "%)" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << gps_linear_avg << " 度" << std::endl;
    
    std::cout << "\n📊 标准线性预测 (直接在坐标上线性外推):" << std::endl;
    std::cout << "  预测次数: " << linear_predictions << std::endl;
    std::cout << "  准确预测: " << linear_accurate << " (" << std::fixed << std::setprecision(2) << linear_rate << "%)" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << linear_avg << " 度" << std::endl;
    
    // 分析结果
    std::cout << "\n=== 分析结论 ===" << std::endl;
    
    std::cout << "\n🎯 关键发现:" << std::endl;
    std::cout << "1. GPS轨迹压缩器使用的是零阶预测（重复运动），不是线性预测！" << std::endl;
    std::cout << "2. 零阶预测准确率: " << gps_current_rate << "%" << std::endl;
    std::cout << "3. 线性预测准确率: " << linear_rate << "%" << std::endl;
    
    if (linear_rate > gps_current_rate * 1.5) {
        double improvement = (linear_rate / gps_current_rate - 1) * 100;
        std::cout << "\n🚀 改用线性预测的潜在收益:" << std::endl;
        std::cout << "  预测准确率可提升: " << std::setprecision(1) << improvement << "%" << std::endl;
        std::cout << "  从 " << gps_current_rate << "% 提升到 " << linear_rate << "%" << std::endl;
        
        // 估算压缩比提升
        std::cout << "\n💡 对压缩比的影响:" << std::endl;
        std::cout << "  如果零校正率从 " << gps_current_rate << "% 提升到 " << linear_rate << "%" << std::endl;
        
        // 简化估算：假设当前平均25 bits/点，零校正1 bit/点，其他校正平均30 bits/点
        double current_avg_bits = gps_current_rate/100.0 * 1.0 + (1.0 - gps_current_rate/100.0) * 30.0;
        double improved_avg_bits = linear_rate/100.0 * 1.0 + (1.0 - linear_rate/100.0) * 30.0;
        
        double current_compression_ratio = 128.0 / current_avg_bits;
        double improved_compression_ratio = 128.0 / improved_avg_bits;
        double compression_improvement = (improved_compression_ratio / current_compression_ratio - 1) * 100;
        
        std::cout << "  压缩比可能从 " << std::setprecision(2) << current_compression_ratio 
                  << ":1 提升到 " << improved_compression_ratio << ":1" << std::endl;
        std::cout << "  压缩比提升: " << std::setprecision(1) << compression_improvement << "%" << std::endl;
    }
    
    std::cout << "\n🔧 建议的改进:" << std::endl;
    std::cout << "将GPS轨迹压缩器的预测从:" << std::endl;
    std::cout << "  predicted_motion = current_motion_vector_;" << std::endl;
    std::cout << "改为:" << std::endl;
    std::cout << "  predicted_motion = 2 * current_motion_vector_ - previous_motion_vector_;" << std::endl;
    
    return 0;
}
