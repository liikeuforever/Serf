#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>
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

// 运动矢量结构
struct MotionVector {
    double velocity;
    double theta;
    MotionVector(double v = 0.0, double t = 0.0) : velocity(v), theta(t) {}
};

// 计算运动矢量
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

// 计算2D欧几里得距离
double Calculate2DDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1, 
                          const SerfQtGpsConfigurableCompressor::GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    std::cout << "=== 调试误差界限问题 ===" << std::endl;
    
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
    
    // 测试不同的误差界限
    std::vector<double> error_bounds = {
        1.4e-5,           // 原始设置
        1.4e-5 * 0.999,   // 实际GPS压缩器使用的界限
        1.414e-5,         // 线性预测的等效2D界限
        2e-5,             // 更宽松的界限
        3e-5              // 更宽松的界限
    };
    
    std::cout << "\n=== 不同误差界限下的纯预测准确率 ===" << std::endl;
    
    for (double error_bound : error_bounds) {
        std::cout << "\n📏 误差界限: " << std::scientific << std::setprecision(3) << error_bound << " 度" << std::endl;
        
        // 进行纯预测测试
        std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> test_points;
        std::vector<MotionVector> test_motions;
        int accurate_predictions = 0;
        int total_predictions = 0;
        
        for (int i = 0; i < points.size(); ++i) {
            if (i < 2) {
                test_points.push_back(points[i]);
                if (i == 1) {
                    test_motions.push_back(CalculateMotionVector(points[0], points[1]));
                }
            } else {
                // 使用真实点进行预测
                const auto& current_point = test_points.back();
                const auto& current_motion = test_motions.back();
                
                auto predicted_point = CalculateDestinationPoint(current_point, current_motion);
                double error = Calculate2DDistance(points[i], predicted_point);
                
                total_predictions++;
                if (error <= error_bound) {
                    accurate_predictions++;
                }
                
                // 更新状态（使用真实点）
                test_points.push_back(points[i]);
                test_motions.push_back(CalculateMotionVector(test_points[test_points.size()-2], test_points.back()));
            }
        }
        
        double accuracy_rate = static_cast<double>(accurate_predictions) / total_predictions * 100.0;
        std::cout << "  预测准确率: " << std::fixed << std::setprecision(2) << accuracy_rate << "%" << std::endl;
        std::cout << "  准确预测: " << accurate_predictions << " / " << total_predictions << std::endl;
    }
    
    // 对比实际GPS压缩器的结果
    std::cout << "\n=== 实际GPS压缩器对比 ===" << std::endl;
    
    const double gps_error_bound = 1.4e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 2e-4;
    
    SerfQtGpsConfigurableCompressor actual_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        actual_compressor.AddGpsPoint(point);
    }
    
    const auto& actual_stats = actual_compressor.GetStrategyStats();
    double actual_zero_rate = static_cast<double>(actual_stats.zero_corr_count) / actual_stats.GetTotalPoints() * 100.0;
    
    std::cout << "实际GPS压缩器零校正率: " << actual_zero_rate << "%" << std::endl;
    std::cout << "实际使用的误差界限: " << (gps_error_bound * 0.999) << " 度" << std::endl;
    
    // 分析误差分布
    std::cout << "\n=== 预测误差分布分析 ===" << std::endl;
    
    std::vector<double> prediction_errors;
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> analysis_points;
    std::vector<MotionVector> analysis_motions;
    
    for (int i = 0; i < std::min(1000, static_cast<int>(points.size())); ++i) {
        if (i < 2) {
            analysis_points.push_back(points[i]);
            if (i == 1) {
                analysis_motions.push_back(CalculateMotionVector(points[0], points[1]));
            }
        } else {
            const auto& current_point = analysis_points.back();
            const auto& current_motion = analysis_motions.back();
            
            auto predicted_point = CalculateDestinationPoint(current_point, current_motion);
            double error = Calculate2DDistance(points[i], predicted_point);
            
            prediction_errors.push_back(error);
            
            // 更新状态
            analysis_points.push_back(points[i]);
            analysis_motions.push_back(CalculateMotionVector(analysis_points[analysis_points.size()-2], analysis_points.back()));
        }
    }
    
    // 计算误差统计
    std::sort(prediction_errors.begin(), prediction_errors.end());
    
    if (!prediction_errors.empty()) {
        double mean_error = std::accumulate(prediction_errors.begin(), prediction_errors.end(), 0.0) / prediction_errors.size();
        double median_error = prediction_errors[prediction_errors.size() / 2];
        double p95_error = prediction_errors[static_cast<size_t>(0.95 * prediction_errors.size())];
        
        std::cout << "预测误差统计 (前1000个点):" << std::endl;
        std::cout << "  平均误差: " << std::scientific << mean_error << " 度" << std::endl;
        std::cout << "  中位数误差: " << median_error << " 度" << std::endl;
        std::cout << "  95%分位数误差: " << p95_error << " 度" << std::endl;
        std::cout << "  最小误差: " << prediction_errors[0] << " 度" << std::endl;
        std::cout << "  最大误差: " << prediction_errors.back() << " 度" << std::endl;
        
        // 分析误差界限的合理性
        std::cout << "\n💡 误差界限合理性分析:" << std::endl;
        std::cout << "当前GPS压缩器误差界限: " << (gps_error_bound * 0.999) << " 度" << std::endl;
        
        if (gps_error_bound * 0.999 < median_error) {
            std::cout << "🚨 误差界限小于中位数误差，过于严格！" << std::endl;
            std::cout << "建议调整到至少: " << median_error << " 度" << std::endl;
        } else if (gps_error_bound * 0.999 < mean_error) {
            std::cout << "⚠️ 误差界限小于平均误差，较为严格" << std::endl;
        } else {
            std::cout << "✅ 误差界限设置合理" << std::endl;
        }
    }
    
    return 0;
}
