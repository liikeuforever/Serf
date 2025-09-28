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

// 精确模拟GPS轨迹压缩器的逻辑
struct GPSCompressorSimulator {
    double kEMax, kEpsilonV, kEpsilonTheta;
    SerfQtGpsConfigurableCompressor::GpsPoint current_reconstructed_point;
    SerfQtGpsConfigurableCompressor::MotionVector current_motion_vector;
    std::vector<std::pair<SerfQtGpsConfigurableCompressor::GpsPoint, 
                         SerfQtGpsConfigurableCompressor::MotionVector>> history_states;
    int point_index = 0;
    
    GPSCompressorSimulator(double e_max, double epsilon_v, double epsilon_theta) 
        : kEMax(e_max * 0.999), kEpsilonV(epsilon_v), kEpsilonTheta(epsilon_theta) {}
    
    // 计算运动矢量
    SerfQtGpsConfigurableCompressor::MotionVector CalculateMotionVector(
        const SerfQtGpsConfigurableCompressor::GpsPoint& start, 
        const SerfQtGpsConfigurableCompressor::GpsPoint& end) {
        
        double dx = end.longitude - start.longitude;
        double dy = end.latitude - start.latitude;
        double velocity = std::sqrt(dx * dx + dy * dy);
        double theta = std::atan2(dy, dx);
        
        return SerfQtGpsConfigurableCompressor::MotionVector(velocity, theta);
    }
    
    // 计算目标点
    SerfQtGpsConfigurableCompressor::GpsPoint CalculateDestinationPoint(
        const SerfQtGpsConfigurableCompressor::GpsPoint& start, 
        const SerfQtGpsConfigurableCompressor::MotionVector& motion) {
        
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
    
    // 高级预测（完全按照压缩器逻辑）
    std::pair<SerfQtGpsConfigurableCompressor::GpsPoint, SerfQtGpsConfigurableCompressor::MotionVector> 
    AdvancedPrediction() {
        if (history_states.empty()) {
            return {current_reconstructed_point, SerfQtGpsConfigurableCompressor::MotionVector(0, 0)};
        }
        
        if (history_states.size() == 1) {
            return {current_reconstructed_point, SerfQtGpsConfigurableCompressor::MotionVector(0, 0)};
        } else {
            auto predicted_point = CalculateDestinationPoint(current_reconstructed_point, current_motion_vector);
            return {predicted_point, current_motion_vector};
        }
    }
    
    // 处理一个GPS点
    bool ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& point) {
        if (point_index == 0) {
            // 第一个点
            current_reconstructed_point = point;
            current_motion_vector = SerfQtGpsConfigurableCompressor::MotionVector(0, 0);
            history_states.emplace_back(point, current_motion_vector);
            point_index++;
            return true; // 第一个点总是"成功"
        } else if (point_index == 1) {
            // 第二个点，使用完全校正
            auto true_motion = CalculateMotionVector(current_reconstructed_point, point);
            
            // 量化运动矢量（按照压缩器逻辑）
            int64_t qv = static_cast<int64_t>(std::round(true_motion.velocity / kEpsilonV));
            int64_t qtheta = static_cast<int64_t>(std::round(true_motion.theta / kEpsilonTheta));
            
            // 重构
            SerfQtGpsConfigurableCompressor::MotionVector reconstructed_motion(qv * kEpsilonV, qtheta * kEpsilonTheta);
            auto reconstructed_point = CalculateDestinationPoint(current_reconstructed_point, reconstructed_motion);
            
            // 更新状态
            current_reconstructed_point = reconstructed_point;
            current_motion_vector = reconstructed_motion;
            history_states.emplace_back(reconstructed_point, reconstructed_motion);
            
            point_index++;
            return false; // 第二个点使用完全校正
        } else {
            // 第三个点及以后，进行预测
            auto [predicted_point, predicted_motion] = AdvancedPrediction();
            
            // 检查零校正是否满足误差要求（完全按照压缩器逻辑）
            double zero_correction_error = CalculateDistance(point, predicted_point);
            bool use_zero_correction = (zero_correction_error <= kEMax);
            
            if (use_zero_correction) {
                // 使用零校正，更新状态但不改变运动矢量
                current_reconstructed_point = predicted_point;
                // current_motion_vector 保持不变
                history_states.emplace_back(predicted_point, current_motion_vector);
            } else {
                // 需要校正，这里简化处理，假设使用完全校正
                auto true_motion = CalculateMotionVector(current_reconstructed_point, point);
                
                // 量化
                int64_t qv = static_cast<int64_t>(std::round(true_motion.velocity / kEpsilonV));
                int64_t qtheta = static_cast<int64_t>(std::round(true_motion.theta / kEpsilonTheta));
                
                // 重构
                SerfQtGpsConfigurableCompressor::MotionVector reconstructed_motion(qv * kEpsilonV, qtheta * kEpsilonTheta);
                auto reconstructed_point = CalculateDestinationPoint(current_reconstructed_point, reconstructed_motion);
                
                // 更新状态
                current_reconstructed_point = reconstructed_point;
                current_motion_vector = reconstructed_motion;
                history_states.emplace_back(reconstructed_point, reconstructed_motion);
            }
            
            point_index++;
            return use_zero_correction;
        }
    }
};

// 精确模拟线性压缩器的逻辑
struct LinearCompressorSimulator {
    double kMaxDiff;
    bool first = true;
    bool second = true;
    double prev_value1 = 2.0;  // most recent value (重构值)
    double prev_value2 = 2.0;  // second most recent value (重构值)
    
    LinearCompressorSimulator(double max_diff) : kMaxDiff(max_diff * 0.999) {}
    
    // 线性预测（完全按照压缩器逻辑）
    double LinearPredict() {
        if (first) {
            return 2.0;  // default value for first point
        } else if (second) {
            return prev_value1;  // use previous value for second point
        } else {
            // Linear prediction: predicted = 2 * prev1 - prev2
            return 2.0 * prev_value1 - prev_value2;
        }
    }
    
    // 处理一个值
    bool ProcessValue(double v, double error_bound_2d) {
        if (first) {
            first = false;
            // 量化和重构第一个值
            long q = static_cast<long>(std::round((v - 2.0) / (2 * kMaxDiff)));
            double recoverValue = 2.0 + 2 * kMaxDiff * static_cast<double>(q);
            prev_value1 = recoverValue;
            return false; // 第一个值不算预测
        }
        
        if (second) {
            second = false;
            // 第二个值使用前一个值作为预测
            long q = static_cast<long>(std::round((v - prev_value1) / (2 * kMaxDiff)));
            double recoverValue = prev_value1 + 2 * kMaxDiff * static_cast<double>(q);
            prev_value2 = prev_value1;
            prev_value1 = recoverValue;
            return false; // 第二个值不算预测
        }
        
        // 第三个值及以后，使用线性预测
        double predicted = LinearPredict();
        
        // 检查预测准确性（使用1D误差界限）
        double error_1d = std::abs(v - predicted);
        bool accurate_1d = (error_1d <= kMaxDiff);
        
        // 量化和重构
        long q = static_cast<long>(std::round((v - predicted) / (2 * kMaxDiff)));
        double recoverValue = predicted + 2 * kMaxDiff * static_cast<double>(q);
        
        // 更新历史
        prev_value2 = prev_value1;
        prev_value1 = recoverValue;
        
        return accurate_1d;
    }
};

int main() {
    std::cout << "=== 精确模拟压缩器逻辑的预测对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 5000;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double gps_error_bound = 1e-5;
    const double linear_error_bound = gps_error_bound / std::sqrt(2.0);
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  GPS轨迹压缩器误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  线性压缩器误差界限: " << linear_error_bound << " 度" << std::endl;
    std::cout << "  GPS量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    // 初始化模拟器
    GPSCompressorSimulator gps_sim(gps_error_bound, epsilon_v, epsilon_theta);
    LinearCompressorSimulator linear_lon_sim(linear_error_bound);
    LinearCompressorSimulator linear_lat_sim(linear_error_bound);
    
    // 统计变量
    int gps_predictions = 0, gps_accurate = 0;
    int linear_predictions = 0, linear_accurate = 0;
    
    std::cout << "\n🔄 精确模拟压缩器逻辑..." << std::endl;
    
    for (const auto& point : points) {
        // GPS轨迹压缩器模拟
        bool gps_zero_correction = gps_sim.ProcessPoint(point);
        if (gps_sim.point_index >= 3) { // 只统计第三个点及以后
            gps_predictions++;
            if (gps_zero_correction) {
                gps_accurate++;
            }
        }
        
        // 线性压缩器模拟（经度和纬度分别处理）
        bool lon_accurate = linear_lon_sim.ProcessValue(point.longitude, linear_error_bound);
        bool lat_accurate = linear_lat_sim.ProcessValue(point.latitude, linear_error_bound);
        
        if (!linear_lon_sim.first && !linear_lon_sim.second) { // 第三个值及以后
            linear_predictions++;
            // 只有当经度和纬度都预测准确时，才算准确
            if (lon_accurate && lat_accurate) {
                linear_accurate++;
            }
        }
    }
    
    // 计算结果
    double gps_accuracy_rate = static_cast<double>(gps_accurate) / gps_predictions * 100.0;
    double linear_accuracy_rate = static_cast<double>(linear_accurate) / linear_predictions * 100.0;
    
    // 打印结果
    std::cout << "\n=== 精确模拟结果 ===" << std::endl;
    
    std::cout << "\n📊 GPS轨迹压缩器模拟:" << std::endl;
    std::cout << "  预测次数: " << gps_predictions << std::endl;
    std::cout << "  零校正次数: " << gps_accurate << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << gps_accuracy_rate << "%" << std::endl;
    
    std::cout << "\n📊 线性压缩器模拟:" << std::endl;
    std::cout << "  预测次数: " << linear_predictions << std::endl;
    std::cout << "  准确预测次数: " << linear_accurate << std::endl;
    std::cout << "  预测准确率: " << linear_accuracy_rate << "%" << std::endl;
    
    // 对比实际压缩器结果
    std::cout << "\n=== 与实际压缩器对比 ===" << std::endl;
    
    // 运行实际GPS轨迹压缩器
    SerfQtGpsConfigurableCompressor actual_gps_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    for (const auto& point : points) {
        actual_gps_compressor.AddGpsPoint(point);
    }
    const auto& actual_gps_stats = actual_gps_compressor.GetStrategyStats();
    double actual_gps_zero_rate = static_cast<double>(actual_gps_stats.zero_corr_count) / actual_gps_stats.GetTotalPoints() * 100.0;
    
    std::cout << "GPS轨迹压缩器:" << std::endl;
    std::cout << "  模拟零校正率: " << gps_accuracy_rate << "%" << std::endl;
    std::cout << "  实际零校正率: " << actual_gps_zero_rate << "%" << std::endl;
    std::cout << "  差异: " << std::abs(gps_accuracy_rate - actual_gps_zero_rate) << "%" << std::endl;
    
    if (std::abs(gps_accuracy_rate - actual_gps_zero_rate) < 1.0) {
        std::cout << "✅ 模拟结果与实际结果高度一致" << std::endl;
    } else {
        std::cout << "❌ 模拟结果与实际结果存在差异" << std::endl;
        std::cout << "可能的原因:" << std::endl;
        std::cout << "1. 策略选择逻辑的复杂性" << std::endl;
        std::cout << "2. 几何剪枝算法的影响" << std::endl;
        std::cout << "3. 编码成本评估的影响" << std::endl;
    }
    
    // 分析预测准确率差异
    std::cout << "\n=== 预测准确率差异分析 ===" << std::endl;
    
    double accuracy_diff = gps_accuracy_rate - linear_accuracy_rate;
    std::cout << "GPS轨迹压缩器 vs 线性压缩器准确率差异: " << std::setprecision(2) << accuracy_diff << "%" << std::endl;
    
    if (std::abs(accuracy_diff) < 2.0) {
        std::cout << "✅ 两种方法的预测准确率基本相同" << std::endl;
        std::cout << "这说明预测算法本身不是性能差异的主要原因" << std::endl;
    } else {
        if (accuracy_diff > 0) {
            std::cout << "📈 GPS轨迹压缩器预测更准确" << std::endl;
            std::cout << "原因：2D运动矢量预测比1D独立预测更适合轨迹数据" << std::endl;
        } else {
            std::cout << "📈 线性压缩器预测更准确" << std::endl;
            std::cout << "原因：可能是量化参数或误差界限设置的影响" << std::endl;
        }
    }
    
    return 0;
}
