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

int main() {
    std::cout << "=== 调试零校正失败的具体原因 ===" << std::endl;
    
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
    
    // 设置不同的误差界限进行测试
    std::vector<double> error_bounds = {1e-5, 2e-5, 5e-5, 1e-4, 2e-4};
    const double epsilon_v = 5e-6;
    const double epsilon_theta = 1e-4;
    
    std::cout << "\n=== 不同误差界限下的零校正率 ===" << std::endl;
    
    for (double error_bound : error_bounds) {
        std::cout << "\n📏 误差界限: " << std::scientific << error_bound << " 度" << std::endl;
        
        SerfQtGpsConfigurableCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
        
        for (const auto& point : points) {
            compressor.AddGpsPoint(point);
        }
        
        const auto& stats = compressor.GetStrategyStats();
        int total_points = stats.GetTotalPoints();
        double zero_correction_rate = static_cast<double>(stats.zero_corr_count) / total_points * 100.0;
        
        std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << zero_correction_rate << "%" << std::endl;
        std::cout << "  零校正次数: " << stats.zero_corr_count << " / " << total_points << std::endl;
    }
    
    // 分析误差分布
    std::cout << "\n=== 预测误差分布分析 ===" << std::endl;
    
    const double target_error_bound = 1e-5;
    
    // 手动模拟预测过程，记录每次预测的误差
    std::vector<double> prediction_errors;
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> reconstructed_points;
    std::vector<std::pair<double, double>> motion_vectors; // velocity, theta
    
    for (int i = 0; i < std::min(500, static_cast<int>(points.size())); ++i) {
        if (i < 2) {
            // 前两个点直接添加（简化处理）
            reconstructed_points.push_back(points[i]);
            if (i == 1) {
                double dx = points[1].longitude - points[0].longitude;
                double dy = points[1].latitude - points[0].latitude;
                double velocity = std::sqrt(dx * dx + dy * dy);
                double theta = std::atan2(dy, dx);
                motion_vectors.emplace_back(velocity, theta);
            }
            continue;
        }
        
        // 进行预测
        const auto& current_reconstructed = reconstructed_points.back();
        const auto& current_motion = motion_vectors.back();
        
        // GPS轨迹压缩器的预测方法
        double predicted_lon = current_reconstructed.longitude + current_motion.first * std::cos(current_motion.second);
        double predicted_lat = current_reconstructed.latitude + current_motion.first * std::sin(current_motion.second);
        
        SerfQtGpsConfigurableCompressor::GpsPoint predicted_point(predicted_lon, predicted_lat);
        
        // 计算预测误差
        double dx = points[i].longitude - predicted_point.longitude;
        double dy = points[i].latitude - predicted_point.latitude;
        double error = std::sqrt(dx * dx + dy * dy);
        
        prediction_errors.push_back(error);
        
        // 更新状态（简化：假设完美重构）
        reconstructed_points.push_back(points[i]);
        
        // 计算新的运动矢量
        const auto& prev_point = reconstructed_points[reconstructed_points.size() - 2];
        const auto& curr_point = reconstructed_points.back();
        double new_dx = curr_point.longitude - prev_point.longitude;
        double new_dy = curr_point.latitude - prev_point.latitude;
        double new_velocity = std::sqrt(new_dx * new_dx + new_dy * new_dy);
        double new_theta = std::atan2(new_dy, new_dx);
        motion_vectors.emplace_back(new_velocity, new_theta);
    }
    
    // 分析误差分布
    std::sort(prediction_errors.begin(), prediction_errors.end());
    
    int total_predictions = prediction_errors.size();
    int within_bound = std::count_if(prediction_errors.begin(), prediction_errors.end(), 
                                    [target_error_bound](double error) { return error <= target_error_bound; });
    
    std::cout << "\n📊 预测误差统计 (前500个点):" << std::endl;
    std::cout << "  总预测次数: " << total_predictions << std::endl;
    std::cout << "  误差 ≤ " << std::scientific << target_error_bound << ": " << within_bound 
              << " (" << std::fixed << std::setprecision(2) << (100.0 * within_bound / total_predictions) << "%)" << std::endl;
    
    // 误差分位数
    std::vector<double> percentiles = {0.5, 0.75, 0.9, 0.95, 0.99};
    std::cout << "\n📈 误差分位数:" << std::endl;
    for (double p : percentiles) {
        int index = static_cast<int>(p * (total_predictions - 1));
        std::cout << "  " << std::setprecision(0) << (p * 100) << "%分位数: " 
                  << std::scientific << std::setprecision(3) << prediction_errors[index] << " 度" << std::endl;
    }
    
    std::cout << "  最小误差: " << prediction_errors[0] << " 度" << std::endl;
    std::cout << "  最大误差: " << prediction_errors.back() << " 度" << std::endl;
    
    // 分析误差界限的合理性
    std::cout << "\n=== 误差界限合理性分析 ===" << std::endl;
    
    double median_error = prediction_errors[total_predictions / 2];
    double p75_error = prediction_errors[static_cast<int>(0.75 * (total_predictions - 1))];
    double p90_error = prediction_errors[static_cast<int>(0.90 * (total_predictions - 1))];
    
    std::cout << "当前误差界限: " << target_error_bound << " 度" << std::endl;
    std::cout << "中位数误差: " << median_error << " 度" << std::endl;
    
    if (target_error_bound < median_error) {
        std::cout << "🚨 误差界限过于严格！" << std::endl;
        std::cout << "  当前界限小于中位数误差 " << (median_error / target_error_bound) << " 倍" << std::endl;
        std::cout << "  建议将误差界限调整到至少: " << median_error << " 度" << std::endl;
    } else if (target_error_bound < p75_error) {
        std::cout << "⚠️  误差界限较为严格" << std::endl;
        std::cout << "  只有25%的预测能满足当前界限" << std::endl;
        std::cout << "  如果调整到75%分位数: " << p75_error << " 度，零校正率可达75%" << std::endl;
    } else if (target_error_bound < p90_error) {
        std::cout << "✅ 误差界限相对合理" << std::endl;
        std::cout << "  约有75-90%的预测能满足当前界限" << std::endl;
    } else {
        std::cout << "✅ 误差界限较为宽松" << std::endl;
        std::cout << "  超过90%的预测能满足当前界限" << std::endl;
    }
    
    // 建议最优误差界限
    std::vector<double> target_rates = {0.3, 0.5, 0.7, 0.9};
    std::cout << "\n💡 不同零校正率目标对应的建议误差界限:" << std::endl;
    for (double rate : target_rates) {
        int index = static_cast<int>(rate * (total_predictions - 1));
        double suggested_bound = prediction_errors[index];
        std::cout << "  零校正率 " << std::setprecision(0) << (rate * 100) << "%: " 
                  << std::scientific << std::setprecision(2) << suggested_bound << " 度" << std::endl;
    }
    
    return 0;
}
