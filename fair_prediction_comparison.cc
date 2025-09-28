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

// 计算2D欧几里得距离
double Calculate2DDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1, 
                          const SerfQtGpsConfigurableCompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

// 模拟GPS轨迹压缩器的预测（使用重构点）
struct GPSTrajectoryPredictor {
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> reconstructed_points;
    std::vector<std::pair<double, double>> motion_vectors; // velocity, theta
    double epsilon_v, epsilon_theta;
    
    GPSTrajectoryPredictor(double ev, double et) : epsilon_v(ev), epsilon_theta(et) {}
    
    // 量化运动矢量
    std::pair<double, double> QuantizeMotionVector(double velocity, double theta) {
        int64_t qv = static_cast<int64_t>(std::round(velocity / epsilon_v));
        int64_t qtheta = static_cast<int64_t>(std::round(theta / epsilon_theta));
        return {qv * epsilon_v, qtheta * epsilon_theta};
    }
    
    // 计算目标点
    SerfQtGpsConfigurableCompressor::GpsPoint CalculateDestinationPoint(
        const SerfQtGpsConfigurableCompressor::GpsPoint& start, 
        double velocity, double theta) {
        double dx = velocity * std::cos(theta);
        double dy = velocity * std::sin(theta);
        return SerfQtGpsConfigurableCompressor::GpsPoint(start.longitude + dx, start.latitude + dy);
    }
    
    // 处理一个点并返回预测结果
    std::pair<bool, double> ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& actual_point, double error_bound) {
        if (reconstructed_points.size() < 2) {
            // 前两个点，直接重构（简化处理）
            reconstructed_points.push_back(actual_point);
            if (reconstructed_points.size() == 2) {
                // 计算第一个运动矢量
                double dx = actual_point.longitude - reconstructed_points[0].longitude;
                double dy = actual_point.latitude - reconstructed_points[0].latitude;
                double velocity = std::sqrt(dx * dx + dy * dy);
                double theta = std::atan2(dy, dx);
                
                // 量化
                auto quantized = QuantizeMotionVector(velocity, theta);
                motion_vectors.push_back(quantized);
                
                // 重构第二个点
                auto reconstructed_second = CalculateDestinationPoint(reconstructed_points[0], quantized.first, quantized.second);
                reconstructed_points.back() = reconstructed_second;
            }
            return {false, 0.0}; // 前两个点不算预测
        }
        
        // 进行预测（零阶预测：重复上一次运动矢量）
        const auto& current_reconstructed = reconstructed_points.back();
        const auto& current_motion = motion_vectors.back();
        
        auto predicted_point = CalculateDestinationPoint(current_reconstructed, current_motion.first, current_motion.second);
        
        // 计算预测误差
        double prediction_error = Calculate2DDistance(actual_point, predicted_point);
        bool accurate = (prediction_error <= error_bound);
        
        // 重构当前点（简化：使用实际运动矢量的量化版本）
        double actual_dx = actual_point.longitude - current_reconstructed.longitude;
        double actual_dy = actual_point.latitude - current_reconstructed.latitude;
        double actual_velocity = std::sqrt(actual_dx * actual_dx + actual_dy * actual_dy);
        double actual_theta = std::atan2(actual_dy, actual_dx);
        
        auto quantized_motion = QuantizeMotionVector(actual_velocity, actual_theta);
        auto reconstructed_point = CalculateDestinationPoint(current_reconstructed, quantized_motion.first, quantized_motion.second);
        
        reconstructed_points.push_back(reconstructed_point);
        motion_vectors.push_back(quantized_motion);
        
        return {accurate, prediction_error};
    }
};

// 模拟线性压缩器的预测（使用重构值）
struct LinearPredictor {
    std::vector<double> reconstructed_longitudes;
    std::vector<double> reconstructed_latitudes;
    double max_diff;
    
    LinearPredictor(double md) : max_diff(md) {}
    
    // 量化值
    double QuantizeValue(double predicted, double actual) {
        long q = static_cast<long>(std::round((actual - predicted) / (2 * max_diff)));
        return predicted + 2 * max_diff * static_cast<double>(q);
    }
    
    // 处理一个点并返回预测结果
    std::pair<bool, double> ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& actual_point, double error_bound_2d) {
        if (reconstructed_longitudes.size() < 2) {
            // 前两个点，简化处理
            double lon_predicted = (reconstructed_longitudes.empty()) ? 2.0 : reconstructed_longitudes.back();
            double lat_predicted = (reconstructed_latitudes.empty()) ? 2.0 : reconstructed_latitudes.back();
            
            // 量化重构
            double reconstructed_lon = QuantizeValue(lon_predicted, actual_point.longitude);
            double reconstructed_lat = QuantizeValue(lat_predicted, actual_point.latitude);
            
            reconstructed_longitudes.push_back(reconstructed_lon);
            reconstructed_latitudes.push_back(reconstructed_lat);
            
            return {false, 0.0}; // 前两个点不算预测
        }
        
        // 线性预测
        double predicted_lon = 2.0 * reconstructed_longitudes.back() - reconstructed_longitudes[reconstructed_longitudes.size()-2];
        double predicted_lat = 2.0 * reconstructed_latitudes.back() - reconstructed_latitudes[reconstructed_latitudes.size()-2];
        
        SerfQtGpsConfigurableCompressor::GpsPoint predicted_point(predicted_lon, predicted_lat);
        
        // 计算2D预测误差
        double prediction_error_2d = Calculate2DDistance(actual_point, predicted_point);
        bool accurate_2d = (prediction_error_2d <= error_bound_2d);
        
        // 量化和重构
        double reconstructed_lon = QuantizeValue(predicted_lon, actual_point.longitude);
        double reconstructed_lat = QuantizeValue(predicted_lat, actual_point.latitude);
        
        reconstructed_longitudes.push_back(reconstructed_lon);
        reconstructed_latitudes.push_back(reconstructed_lat);
        
        return {accurate_2d, prediction_error_2d};
    }
};

int main() {
    std::cout << "=== GPS轨迹压缩器(2D) vs 线性压缩器(1D) 公平预测对比 ===" << std::endl;
    
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
    const double target_2d_error_bound = 1e-5;  // 2D误差界限
    const double linear_1d_error_bound = target_2d_error_bound / std::sqrt(2.0);  // 等效1D误差界限
    
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  目标2D误差界限: " << std::scientific << target_2d_error_bound << " 度" << std::endl;
    std::cout << "  线性压缩器1D误差界限: " << linear_1d_error_bound << " 度" << std::endl;
    std::cout << "  GPS轨迹压缩器量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    // 初始化预测器
    GPSTrajectoryPredictor gps_predictor(epsilon_v, epsilon_theta);
    LinearPredictor linear_predictor(linear_1d_error_bound);
    
    // 统计变量
    int gps_predictions = 0, gps_accurate = 0;
    int linear_predictions = 0, linear_accurate = 0;
    
    std::vector<double> gps_errors, linear_errors;
    
    std::cout << "\n🔄 进行预测对比..." << std::endl;
    
    for (const auto& point : points) {
        // GPS轨迹压缩器预测
        auto [gps_acc, gps_err] = gps_predictor.ProcessPoint(point, target_2d_error_bound);
        if (gps_predictor.reconstructed_points.size() > 2) {
            gps_predictions++;
            gps_errors.push_back(gps_err);
            if (gps_acc) gps_accurate++;
        }
        
        // 线性压缩器预测（使用2D误差界限进行评估）
        auto [linear_acc, linear_err] = linear_predictor.ProcessPoint(point, target_2d_error_bound);
        if (linear_predictor.reconstructed_longitudes.size() > 2) {
            linear_predictions++;
            linear_errors.push_back(linear_err);
            if (linear_acc) linear_accurate++;
        }
    }
    
    // 计算统计结果
    double gps_accuracy_rate = static_cast<double>(gps_accurate) / gps_predictions * 100.0;
    double linear_accuracy_rate = static_cast<double>(linear_accurate) / linear_predictions * 100.0;
    
    double gps_avg_error = std::accumulate(gps_errors.begin(), gps_errors.end(), 0.0) / gps_errors.size();
    double linear_avg_error = std::accumulate(linear_errors.begin(), linear_errors.end(), 0.0) / linear_errors.size();
    
    // 计算误差分位数
    std::sort(gps_errors.begin(), gps_errors.end());
    std::sort(linear_errors.begin(), linear_errors.end());
    
    double gps_median = gps_errors[gps_errors.size() / 2];
    double linear_median = linear_errors[linear_errors.size() / 2];
    
    // 打印结果
    std::cout << "\n=== 公平预测对比结果 ===" << std::endl;
    
    std::cout << "\n📊 GPS轨迹压缩器 (2D预测，使用重构点):" << std::endl;
    std::cout << "  预测次数: " << gps_predictions << std::endl;
    std::cout << "  准确预测: " << gps_accurate << " (" << std::fixed << std::setprecision(2) << gps_accuracy_rate << "%)" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << gps_avg_error << " 度" << std::endl;
    std::cout << "  中位数误差: " << gps_median << " 度" << std::endl;
    
    std::cout << "\n📊 线性压缩器 (1D预测，使用重构值，2D误差评估):" << std::endl;
    std::cout << "  预测次数: " << linear_predictions << std::endl;
    std::cout << "  准确预测: " << linear_accurate << " (" << linear_accuracy_rate << "%)" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << linear_avg_error << " 度" << std::endl;
    std::cout << "  中位数误差: " << linear_median << " 度" << std::endl;
    
    // 分析差异
    std::cout << "\n=== 差异分析 ===" << std::endl;
    
    double accuracy_diff = std::abs(gps_accuracy_rate - linear_accuracy_rate);
    double error_ratio = gps_avg_error / linear_avg_error;
    
    std::cout << "预测准确率差异: " << std::fixed << std::setprecision(2) << accuracy_diff << "%" << std::endl;
    std::cout << "平均误差比值 (GPS/线性): " << std::setprecision(3) << error_ratio << std::endl;
    
    if (accuracy_diff < 3.0) {
        std::cout << "\n✅ 两种预测方法的准确率基本相同" << std::endl;
        std::cout << "这说明预测算法本身不是压缩比差异的主要原因" << std::endl;
    } else {
        if (gps_accuracy_rate > linear_accuracy_rate) {
            std::cout << "\n📈 GPS轨迹压缩器预测更准确" << std::endl;
            std::cout << "优势: " << (gps_accuracy_rate - linear_accuracy_rate) << "%" << std::endl;
        } else {
            std::cout << "\n📈 线性压缩器预测更准确" << std::endl;
            std::cout << "优势: " << (linear_accuracy_rate - gps_accuracy_rate) << "%" << std::endl;
        }
        
        std::cout << "\n🔍 可能的原因分析:" << std::endl;
        
        if (gps_accuracy_rate < linear_accuracy_rate) {
            std::cout << "GPS轨迹压缩器预测准确率较低的可能原因:" << std::endl;
            std::cout << "1. 量化误差累积：εv和εθ量化导致重构点偏差" << std::endl;
            std::cout << "2. 极坐标转换精度损失：sin/cos计算的舍入误差" << std::endl;
            std::cout << "3. 零阶预测vs一阶预测：重复运动矢量 vs 线性外推" << std::endl;
            std::cout << "4. 2D运动矢量量化 vs 1D坐标量化的不同影响" << std::endl;
        } else {
            std::cout << "线性压缩器预测准确率较低的可能原因:" << std::endl;
            std::cout << "1. 1D独立预测忽略了2D运动的相关性" << std::endl;
            std::cout << "2. 线性外推在轨迹转弯时误差较大" << std::endl;
        }
    }
    
    // 详细误差分布对比
    std::cout << "\n=== 误差分布对比 ===" << std::endl;
    
    std::vector<double> percentiles = {0.25, 0.5, 0.75, 0.9, 0.95};
    std::cout << "\n误差分位数对比:" << std::endl;
    std::cout << std::setw(10) << "分位数" << std::setw(15) << "GPS轨迹" << std::setw(15) << "线性压缩" << std::setw(10) << "比值" << std::endl;
    
    for (double p : percentiles) {
        int gps_idx = static_cast<int>(p * (gps_errors.size() - 1));
        int linear_idx = static_cast<int>(p * (linear_errors.size() - 1));
        
        double gps_p_error = gps_errors[gps_idx];
        double linear_p_error = linear_errors[linear_idx];
        double ratio = gps_p_error / linear_p_error;
        
        std::cout << std::setw(10) << std::setprecision(0) << (p * 100) << "%" 
                  << std::setw(15) << std::scientific << std::setprecision(2) << gps_p_error
                  << std::setw(15) << linear_p_error
                  << std::setw(10) << std::fixed << std::setprecision(2) << ratio << std::endl;
    }
    
    return 0;
}
