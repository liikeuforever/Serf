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

// 模拟GPS轨迹压缩器的预测逻辑
struct GPSPredictionResult {
    SerfQtGpsConfigurableCompressor::GpsPoint predicted_point;
    double prediction_error;
    bool within_error_bound;
};

GPSPredictionResult SimulateGPSPrediction(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points, 
                                         int index, double error_bound) {
    GPSPredictionResult result;
    
    if (index < 2) {
        // 前两个点无法预测
        result.predicted_point = points[index];
        result.prediction_error = 0.0;
        result.within_error_bound = true;
        return result;
    }
    
    // 模拟GPS轨迹压缩器的预测逻辑
    // 基于运动矢量的线性预测
    const auto& prev_point = points[index - 1];
    const auto& prev_prev_point = points[index - 2];
    
    // 计算前一步的运动矢量
    double dx = prev_point.longitude - prev_prev_point.longitude;
    double dy = prev_point.latitude - prev_prev_point.latitude;
    
    // 线性外推预测当前点
    result.predicted_point.longitude = prev_point.longitude + dx;
    result.predicted_point.latitude = prev_point.latitude + dy;
    
    // 计算预测误差
    double actual_dx = points[index].longitude - result.predicted_point.longitude;
    double actual_dy = points[index].latitude - result.predicted_point.latitude;
    result.prediction_error = std::sqrt(actual_dx * actual_dx + actual_dy * actual_dy);
    
    result.within_error_bound = (result.prediction_error <= error_bound);
    
    return result;
}

// 模拟线性压缩器的预测逻辑
struct LinearPredictionResult {
    double predicted_longitude;
    double predicted_latitude;
    double longitude_error;
    double latitude_error;
    double total_2d_error;
    bool longitude_within_bound;
    bool latitude_within_bound;
    bool both_within_bound;
};

LinearPredictionResult SimulateLinearPrediction(const std::vector<SerfQtGpsConfigurableCompressor::GpsPoint>& points,
                                               int index, double max_diff) {
    LinearPredictionResult result;
    
    if (index < 2) {
        // 前两个点无法预测
        result.predicted_longitude = points[index].longitude;
        result.predicted_latitude = points[index].latitude;
        result.longitude_error = 0.0;
        result.latitude_error = 0.0;
        result.total_2d_error = 0.0;
        result.longitude_within_bound = true;
        result.latitude_within_bound = true;
        result.both_within_bound = true;
        return result;
    }
    
    // 线性预测: predicted = 2 * prev1 - prev2
    result.predicted_longitude = 2.0 * points[index - 1].longitude - points[index - 2].longitude;
    result.predicted_latitude = 2.0 * points[index - 1].latitude - points[index - 2].latitude;
    
    // 计算误差
    result.longitude_error = std::abs(points[index].longitude - result.predicted_longitude);
    result.latitude_error = std::abs(points[index].latitude - result.predicted_latitude);
    
    double dx = points[index].longitude - result.predicted_longitude;
    double dy = points[index].latitude - result.predicted_latitude;
    result.total_2d_error = std::sqrt(dx * dx + dy * dy);
    
    result.longitude_within_bound = (result.longitude_error <= max_diff);
    result.latitude_within_bound = (result.latitude_error <= max_diff);
    result.both_within_bound = result.longitude_within_bound && result.latitude_within_bound;
    
    return result;
}

int main() {
    std::cout << "=== GPS轨迹预测 vs 线性预测准确性对比 ===" << std::endl;
    
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
    
    // 设置相同的2D误差界限
    const double target_2d_error_bound = 1e-5;
    const double linear_max_diff = target_2d_error_bound / std::sqrt(2.0); // 7.07e-6
    
    std::cout << "\n📏 误差界限设置:" << std::endl;
    std::cout << "  GPS轨迹预测2D误差界限: " << std::scientific << target_2d_error_bound << " 度" << std::endl;
    std::cout << "  线性预测单维度误差界限: " << linear_max_diff << " 度" << std::endl;
    std::cout << "  线性预测理论最大2D误差: " << (linear_max_diff * std::sqrt(2.0)) << " 度" << std::endl;
    
    // 统计变量
    int gps_predictions = 0;
    int gps_accurate_predictions = 0;
    std::vector<double> gps_errors;
    
    int linear_predictions = 0;
    int linear_accurate_predictions = 0;
    std::vector<double> linear_errors;
    
    // 逐点对比预测准确性
    std::cout << "\n🔄 分析预测准确性..." << std::endl;
    
    for (int i = 2; i < points.size(); ++i) {
        // GPS轨迹预测
        GPSPredictionResult gps_result = SimulateGPSPrediction(points, i, target_2d_error_bound);
        gps_predictions++;
        gps_errors.push_back(gps_result.prediction_error);
        if (gps_result.within_error_bound) {
            gps_accurate_predictions++;
        }
        
        // 线性预测
        LinearPredictionResult linear_result = SimulateLinearPrediction(points, i, linear_max_diff);
        linear_predictions++;
        linear_errors.push_back(linear_result.total_2d_error);
        if (linear_result.both_within_bound) {
            linear_accurate_predictions++;
        }
    }
    
    // 计算统计结果
    double gps_accuracy_rate = static_cast<double>(gps_accurate_predictions) / gps_predictions * 100.0;
    double linear_accuracy_rate = static_cast<double>(linear_accurate_predictions) / linear_predictions * 100.0;
    
    double gps_avg_error = std::accumulate(gps_errors.begin(), gps_errors.end(), 0.0) / gps_errors.size();
    double linear_avg_error = std::accumulate(linear_errors.begin(), linear_errors.end(), 0.0) / linear_errors.size();
    
    double gps_max_error = *std::max_element(gps_errors.begin(), gps_errors.end());
    double linear_max_error = *std::max_element(linear_errors.begin(), linear_errors.end());
    
    // 打印对比结果
    std::cout << "\n=== 预测准确性对比结果 ===" << std::endl;
    
    std::cout << "\n📊 GPS轨迹预测 (运动矢量线性外推):" << std::endl;
    std::cout << "  预测点数: " << gps_predictions << std::endl;
    std::cout << "  准确预测: " << gps_accurate_predictions << " (" << std::fixed << std::setprecision(2) << gps_accuracy_rate << "%)" << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << gps_avg_error << " 度" << std::endl;
    std::cout << "  最大误差: " << gps_max_error << " 度" << std::endl;
    
    std::cout << "\n📊 线性预测 (2*prev1 - prev2):" << std::endl;
    std::cout << "  预测点数: " << linear_predictions << std::endl;
    std::cout << "  准确预测: " << linear_accurate_predictions << " (" << std::fixed << std::setprecision(2) << linear_accuracy_rate << "%)" << std::endl;
    std::cout << "  平均2D误差: " << std::scientific << std::setprecision(3) << linear_avg_error << " 度" << std::endl;
    std::cout << "  最大2D误差: " << linear_max_error << " 度" << std::endl;
    
    // 分析差异
    std::cout << "\n=== 预测算法对比分析 ===" << std::endl;
    
    if (std::abs(gps_accuracy_rate - linear_accuracy_rate) < 5.0) {
        std::cout << "🎯 预测准确率基本相同!" << std::endl;
        std::cout << "  GPS轨迹预测: " << gps_accuracy_rate << "%" << std::endl;
        std::cout << "  线性预测: " << linear_accuracy_rate << "%" << std::endl;
        std::cout << "  差异: " << std::abs(gps_accuracy_rate - linear_accuracy_rate) << "%" << std::endl;
        
        std::cout << "\n💡 关键洞察:" << std::endl;
        std::cout << "1. 两种预测算法本质上都是线性预测" << std::endl;
        std::cout << "2. 在相同的2D误差界限下，预测准确率应该相似" << std::endl;
        std::cout << "3. GPS轨迹压缩器零校正率低的原因不是预测算法本身" << std::endl;
        
        std::cout << "\n🔍 可能的原因:" << std::endl;
        std::cout << "1. 量化误差累积 - GPS轨迹压缩器使用重构点进行预测" << std::endl;
        std::cout << "2. 运动矢量量化 - εv和εθ的量化引入额外误差" << std::endl;
        std::cout << "3. 坐标系转换 - 极坐标与直角坐标转换的精度损失" << std::endl;
        std::cout << "4. 误差界限检查方式不同" << std::endl;
        
    } else {
        std::cout << "📈 预测准确率存在显著差异:" << std::endl;
        if (gps_accuracy_rate > linear_accuracy_rate) {
            std::cout << "  🏆 GPS轨迹预测更准确，优势: " << (gps_accuracy_rate - linear_accuracy_rate) << "%" << std::endl;
        } else {
            std::cout << "  🏆 线性预测更准确，优势: " << (linear_accuracy_rate - gps_accuracy_rate) << "%" << std::endl;
        }
    }
    
    // 详细分析误差分布
    std::cout << "\n=== 误差分布分析 ===" << std::endl;
    
    // 计算误差分布
    std::sort(gps_errors.begin(), gps_errors.end());
    std::sort(linear_errors.begin(), linear_errors.end());
    
    auto percentile = [](const std::vector<double>& data, double p) {
        size_t index = static_cast<size_t>(p * (data.size() - 1));
        return data[index];
    };
    
    std::cout << "\nGPS轨迹预测误差分布:" << std::endl;
    std::cout << "  50%: " << std::scientific << std::setprecision(3) << percentile(gps_errors, 0.5) << " 度" << std::endl;
    std::cout << "  90%: " << percentile(gps_errors, 0.9) << " 度" << std::endl;
    std::cout << "  95%: " << percentile(gps_errors, 0.95) << " 度" << std::endl;
    std::cout << "  99%: " << percentile(gps_errors, 0.99) << " 度" << std::endl;
    
    std::cout << "\n线性预测误差分布:" << std::endl;
    std::cout << "  50%: " << percentile(linear_errors, 0.5) << " 度" << std::endl;
    std::cout << "  90%: " << percentile(linear_errors, 0.9) << " 度" << std::endl;
    std::cout << "  95%: " << percentile(linear_errors, 0.95) << " 度" << std::endl;
    std::cout << "  99%: " << percentile(linear_errors, 0.99) << " 度" << std::endl;
    
    return 0;
}
