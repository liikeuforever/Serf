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

// 精确模拟GPS轨迹压缩器的预测行为
struct GPSCompressionState {
    SerfQtGpsConfigurableCompressor::GpsPoint current_reconstructed_point;
    SerfQtGpsConfigurableCompressor::MotionVector current_motion_vector;
    std::vector<std::pair<SerfQtGpsConfigurableCompressor::GpsPoint, 
                         SerfQtGpsConfigurableCompressor::MotionVector>> history_states;
    int point_index = 0;
    bool first_point = true;
    
    double kEMax;
    double kEpsilonV;
    double kEpsilonTheta;
    
    GPSCompressionState(double e_max, double epsilon_v, double epsilon_theta) 
        : kEMax(e_max * 0.999), kEpsilonV(epsilon_v), kEpsilonTheta(epsilon_theta) {}
    
    // 计算运动矢量
    SerfQtGpsConfigurableCompressor::MotionVector CalculateMotionVector(
        const SerfQtGpsConfigurableCompressor::GpsPoint& start, 
        const SerfQtGpsConfigurableCompressor::GpsPoint& end) const {
        
        double dx = end.longitude - start.longitude;
        double dy = end.latitude - start.latitude;
        double velocity = std::sqrt(dx * dx + dy * dy);
        double theta = std::atan2(dy, dx);
        
        return SerfQtGpsConfigurableCompressor::MotionVector(velocity, theta);
    }
    
    // 计算目标点
    SerfQtGpsConfigurableCompressor::GpsPoint CalculateDestinationPoint(
        const SerfQtGpsConfigurableCompressor::GpsPoint& start, 
        const SerfQtGpsConfigurableCompressor::MotionVector& motion) const {
        
        double dx = motion.velocity * std::cos(motion.theta);
        double dy = motion.velocity * std::sin(motion.theta);
        
        return SerfQtGpsConfigurableCompressor::GpsPoint(start.longitude + dx, start.latitude + dy);
    }
    
    // 计算距离
    double CalculateDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1, 
                           const SerfQtGpsConfigurableCompressor::GpsPoint& p2) const {
        double dx = p1.longitude - p2.longitude;
        double dy = p1.latitude - p2.latitude;
        return std::sqrt(dx * dx + dy * dy);
    }
    
    // 模拟高级预测
    std::pair<SerfQtGpsConfigurableCompressor::GpsPoint, SerfQtGpsConfigurableCompressor::MotionVector> 
    AdvancedPrediction() const {
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
    
    // 处理一个GPS点并返回是否使用零校正
    bool ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& point) {
        if (first_point) {
            // 第一个点
            first_point = false;
            current_reconstructed_point = point;
            current_motion_vector = SerfQtGpsConfigurableCompressor::MotionVector(0, 0);
            history_states.emplace_back(point, current_motion_vector);
            point_index = 1;
            return true; // 第一个点总是"成功"
        } else if (point_index == 1) {
            // 第二个点，使用完全校正
            auto true_motion = CalculateMotionVector(current_reconstructed_point, point);
            
            // 量化运动矢量
            int64_t qv = static_cast<int64_t>(std::round(true_motion.velocity / kEpsilonV));
            int64_t qtheta = static_cast<int64_t>(std::round(true_motion.theta / kEpsilonTheta));
            
            // 重构
            SerfQtGpsConfigurableCompressor::MotionVector reconstructed_motion(qv * kEpsilonV, qtheta * kEpsilonTheta);
            auto reconstructed_point = CalculateDestinationPoint(current_reconstructed_point, reconstructed_motion);
            
            // 更新状态
            current_reconstructed_point = reconstructed_point;
            current_motion_vector = reconstructed_motion;
            history_states.emplace_back(reconstructed_point, reconstructed_motion);
            
            point_index = 2;
            return false; // 第二个点使用完全校正
        } else {
            // 第三个点及以后，进行预测
            auto [predicted_point, predicted_motion] = AdvancedPrediction();
            
            // 检查零校正是否满足误差要求
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

// 模拟线性压缩器的行为（使用重构值）
struct LinearCompressionState {
    std::vector<double> reconstructed_longitudes;
    std::vector<double> reconstructed_latitudes;
    double max_diff;
    
    LinearCompressionState(double max_diff) : max_diff(max_diff) {}
    
    bool ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& point) {
        if (reconstructed_longitudes.size() < 2) {
            // 前两个点，进行量化重构
            double lon_predicted = (reconstructed_longitudes.empty()) ? 2.0 : reconstructed_longitudes.back();
            double lat_predicted = (reconstructed_latitudes.empty()) ? 2.0 : reconstructed_latitudes.back();
            
            // 量化
            long q_lon = static_cast<long>(std::round((point.longitude - lon_predicted) / (2 * max_diff)));
            long q_lat = static_cast<long>(std::round((point.latitude - lat_predicted) / (2 * max_diff)));
            
            // 重构
            double reconstructed_lon = lon_predicted + 2 * max_diff * static_cast<double>(q_lon);
            double reconstructed_lat = lat_predicted + 2 * max_diff * static_cast<double>(q_lat);
            
            reconstructed_longitudes.push_back(reconstructed_lon);
            reconstructed_latitudes.push_back(reconstructed_lat);
            
            return false; // 前两个点不算预测成功
        } else {
            // 线性预测
            double predicted_lon = 2.0 * reconstructed_longitudes.back() - reconstructed_longitudes[reconstructed_longitudes.size()-2];
            double predicted_lat = 2.0 * reconstructed_latitudes.back() - reconstructed_latitudes[reconstructed_latitudes.size()-2];
            
            // 检查预测准确性
            double lon_error = std::abs(point.longitude - predicted_lon);
            double lat_error = std::abs(point.latitude - predicted_lat);
            bool accurate = (lon_error <= max_diff) && (lat_error <= max_diff);
            
            // 量化和重构（无论预测是否准确）
            long q_lon = static_cast<long>(std::round((point.longitude - predicted_lon) / (2 * max_diff)));
            long q_lat = static_cast<long>(std::round((point.latitude - predicted_lat) / (2 * max_diff)));
            
            double reconstructed_lon = predicted_lon + 2 * max_diff * static_cast<double>(q_lon);
            double reconstructed_lat = predicted_lat + 2 * max_diff * static_cast<double>(q_lat);
            
            reconstructed_longitudes.push_back(reconstructed_lon);
            reconstructed_latitudes.push_back(reconstructed_lat);
            
            return accurate;
        }
    }
};

int main() {
    std::cout << "=== 精确模拟GPS轨迹压缩器 vs 线性压缩器预测行为 ===" << std::endl;
    
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
    const double target_2d_error_bound = 1e-5;
    const double linear_max_diff = target_2d_error_bound / std::sqrt(2.0);
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  GPS轨迹压缩器误差界限: " << std::scientific << target_2d_error_bound << " 度" << std::endl;
    std::cout << "  线性压缩器单维度误差界限: " << linear_max_diff << " 度" << std::endl;
    std::cout << "  εv = " << epsilon_v << ", εθ = " << epsilon_theta << std::endl;
    
    // 模拟GPS轨迹压缩器
    std::cout << "\n🔄 模拟GPS轨迹压缩器..." << std::endl;
    GPSCompressionState gps_state(target_2d_error_bound, epsilon_v, epsilon_theta);
    int gps_zero_corrections = 0;
    int gps_total_predictions = 0;
    
    for (const auto& point : points) {
        bool used_zero_correction = gps_state.ProcessPoint(point);
        if (gps_state.point_index >= 3) { // 只统计第三个点及以后
            gps_total_predictions++;
            if (used_zero_correction) {
                gps_zero_corrections++;
            }
        }
    }
    
    // 模拟线性压缩器
    std::cout << "\n🔄 模拟线性压缩器..." << std::endl;
    LinearCompressionState linear_state(linear_max_diff);
    int linear_accurate_predictions = 0;
    int linear_total_predictions = 0;
    
    for (const auto& point : points) {
        bool accurate = linear_state.ProcessPoint(point);
        if (linear_state.reconstructed_longitudes.size() >= 3) { // 只统计第三个点及以后
            linear_total_predictions++;
            if (accurate) {
                linear_accurate_predictions++;
            }
        }
    }
    
    // 计算结果
    double gps_zero_correction_rate = static_cast<double>(gps_zero_corrections) / gps_total_predictions * 100.0;
    double linear_accuracy_rate = static_cast<double>(linear_accurate_predictions) / linear_total_predictions * 100.0;
    
    // 打印结果
    std::cout << "\n=== 精确模拟结果对比 ===" << std::endl;
    
    std::cout << "\n📊 GPS轨迹压缩器 (使用重构点预测):" << std::endl;
    std::cout << "  总预测次数: " << gps_total_predictions << std::endl;
    std::cout << "  零校正次数: " << gps_zero_corrections << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << gps_zero_correction_rate << "%" << std::endl;
    
    std::cout << "\n📊 线性压缩器 (使用重构值预测):" << std::endl;
    std::cout << "  总预测次数: " << linear_total_predictions << std::endl;
    std::cout << "  准确预测次数: " << linear_accurate_predictions << std::endl;
    std::cout << "  预测准确率: " << linear_accuracy_rate << "%" << std::endl;
    
    std::cout << "\n=== 分析结论 ===" << std::endl;
    
    if (std::abs(gps_zero_correction_rate - linear_accuracy_rate) < 5.0) {
        std::cout << "🎯 两种算法的预测准确率基本相同！" << std::endl;
        std::cout << "  差异: " << std::abs(gps_zero_correction_rate - linear_accuracy_rate) << "%" << std::endl;
        std::cout << "\n💡 这证明了预测算法本身不是问题所在" << std::endl;
        std::cout << "  GPS轨迹压缩器零校正率低的原因必须在其他地方寻找" << std::endl;
    } else {
        std::cout << "📈 两种算法存在显著差异:" << std::endl;
        if (gps_zero_correction_rate < linear_accuracy_rate) {
            double diff = linear_accuracy_rate - gps_zero_correction_rate;
            std::cout << "  线性压缩器预测更准确，优势: " << diff << "%" << std::endl;
            std::cout << "\n🔍 可能的原因:" << std::endl;
            std::cout << "  1. 运动矢量量化误差影响预测准确性" << std::endl;
            std::cout << "  2. 极坐标与直角坐标转换的精度损失" << std::endl;
            std::cout << "  3. 不同的误差界限检查方式" << std::endl;
        } else {
            std::cout << "  GPS轨迹压缩器预测更准确" << std::endl;
        }
    }
    
    return 0;
}
