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

// 量化运动矢量
MotionVector QuantizeMotionVector(const MotionVector& motion, double epsilon_v, double epsilon_theta) {
    int64_t qv = static_cast<int64_t>(std::round(motion.velocity / epsilon_v));
    int64_t qtheta = static_cast<int64_t>(std::round(motion.theta / epsilon_theta));
    return MotionVector(qv * epsilon_v, qtheta * epsilon_theta);
}

// 模拟GPS轨迹压缩器的完整预测过程（包括量化重构）
class GPSCompressorSimulator {
private:
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> reconstructed_points_;
    std::vector<MotionVector> motion_vectors_;
    double kEMax_;
    double kEpsilonV_;
    double kEpsilonTheta_;
    int point_index_;
    bool first_point_;
    
public:
    GPSCompressorSimulator(double e_max, double epsilon_v, double epsilon_theta) 
        : kEMax_(e_max * 0.999), kEpsilonV_(epsilon_v), kEpsilonTheta_(epsilon_theta), 
          point_index_(0), first_point_(true) {}
    
    // 添加点并返回预测是否准确
    bool AddPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& point) {
        if (first_point_) {
            // 第一个点
            first_point_ = false;
            reconstructed_points_.push_back(point);
            motion_vectors_.emplace_back(0, 0);
            point_index_ = 1;
            return false; // 第一个点不算预测
        } else if (point_index_ == 1) {
            // 第二个点，使用完全校正
            const auto& current_reconstructed = reconstructed_points_.back();
            MotionVector true_motion = CalculateMotionVector(current_reconstructed, point);
            
            // 量化运动矢量
            MotionVector quantized_motion = QuantizeMotionVector(true_motion, kEpsilonV_, kEpsilonTheta_);
            
            // 重构点
            auto reconstructed_point = CalculateDestinationPoint(current_reconstructed, quantized_motion);
            
            // 更新状态
            reconstructed_points_.push_back(reconstructed_point);
            motion_vectors_.push_back(quantized_motion);
            
            point_index_++;
            return false; // 第二个点使用完全校正，不算预测
        } else {
            // 第三个点及以后，进行预测
            const auto& current_reconstructed = reconstructed_points_.back();
            const auto& current_motion = motion_vectors_.back();
            
            // 进行预测（零阶预测：重复上一次运动矢量）
            auto predicted_point = CalculateDestinationPoint(current_reconstructed, current_motion);
            
            // 检查零校正是否满足误差要求
            double zero_correction_error = Calculate2DDistance(point, predicted_point);
            bool use_zero_correction = (zero_correction_error <= kEMax_);
            
            if (use_zero_correction) {
                // 使用零校正，预测点就是重构点
                reconstructed_points_.push_back(predicted_point);
                motion_vectors_.push_back(current_motion); // 运动矢量不变
            } else {
                // 需要校正，计算真实运动矢量并量化
                MotionVector true_motion = CalculateMotionVector(current_reconstructed, point);
                MotionVector quantized_motion = QuantizeMotionVector(true_motion, kEpsilonV_, kEpsilonTheta_);
                
                // 重构点
                auto reconstructed_point = CalculateDestinationPoint(current_reconstructed, quantized_motion);
                
                // 更新状态
                reconstructed_points_.push_back(reconstructed_point);
                motion_vectors_.push_back(quantized_motion);
            }
            
            point_index_++;
            return use_zero_correction;
        }
    }
    
    int GetPredictionCount() const {
        return std::max(0, point_index_ - 2);
    }
    
    // 获取当前重构点（用于调试）
    SerfQtGpsConfigurableCompressor::GpsPoint GetCurrentReconstructedPoint() const {
        return reconstructed_points_.empty() ? SerfQtGpsConfigurableCompressor::GpsPoint(0, 0) : reconstructed_points_.back();
    }
    
    // 获取当前运动矢量（用于调试）
    MotionVector GetCurrentMotionVector() const {
        return motion_vectors_.empty() ? MotionVector(0, 0) : motion_vectors_.back();
    }
};

int main() {
    std::cout << "=== 调试GPS轨迹压缩器实现问题 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 10000;  // 使用1万条数据进行详细调试
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double gps_error_bound = 1.4e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 2e-4;
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  εv = " << epsilon_v << ", εθ = " << epsilon_theta << std::endl;
    
    std::cout << "\n=== 对比测试 ===" << std::endl;
    
    // 测试1: 实际GPS轨迹压缩器
    std::cout << "\n🔄 测试1: 实际GPS轨迹压缩器..." << std::endl;
    
    SerfQtGpsConfigurableCompressor actual_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        actual_compressor.AddGpsPoint(point);
    }
    
    const auto& actual_stats = actual_compressor.GetStrategyStats();
    double actual_zero_rate = static_cast<double>(actual_stats.zero_corr_count) / actual_stats.GetTotalPoints() * 100.0;
    
    std::cout << "实际压缩器结果:" << std::endl;
    std::cout << "  总处理点数: " << actual_stats.GetTotalPoints() << std::endl;
    std::cout << "  零校正次数: " << actual_stats.zero_corr_count << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << actual_zero_rate << "%" << std::endl;
    
    // 测试2: 模拟GPS轨迹压缩器（精确复现逻辑）
    std::cout << "\n🔄 测试2: 模拟GPS轨迹压缩器..." << std::endl;
    
    GPSCompressorSimulator simulator(gps_error_bound, epsilon_v, epsilon_theta);
    int sim_accurate_predictions = 0;
    
    for (const auto& point : points) {
        bool accurate = simulator.AddPoint(point);
        if (simulator.GetPredictionCount() > 0 && accurate) {
            sim_accurate_predictions++;
        }
    }
    
    int sim_total_predictions = simulator.GetPredictionCount();
    double sim_zero_rate = static_cast<double>(sim_accurate_predictions) / sim_total_predictions * 100.0;
    
    std::cout << "模拟压缩器结果:" << std::endl;
    std::cout << "  总预测次数: " << sim_total_predictions << std::endl;
    std::cout << "  零校正次数: " << sim_accurate_predictions << std::endl;
    std::cout << "  零校正率: " << sim_zero_rate << "%" << std::endl;
    
    // 测试3: 纯预测（使用真实点）
    std::cout << "\n🔄 测试3: 纯预测（使用真实点）..." << std::endl;
    
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> pure_points;
    std::vector<MotionVector> pure_motions;
    int pure_accurate_predictions = 0;
    int pure_total_predictions = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        if (i < 2) {
            pure_points.push_back(points[i]);
            if (i == 1) {
                pure_motions.push_back(CalculateMotionVector(points[0], points[1]));
            }
        } else {
            // 使用真实点进行预测
            const auto& current_point = pure_points.back();
            const auto& current_motion = pure_motions.back();
            
            auto predicted_point = CalculateDestinationPoint(current_point, current_motion);
            double error = Calculate2DDistance(points[i], predicted_point);
            
            pure_total_predictions++;
            if (error <= gps_error_bound) {
                pure_accurate_predictions++;
            }
            
            // 更新状态（使用真实点）
            pure_points.push_back(points[i]);
            pure_motions.push_back(CalculateMotionVector(pure_points[pure_points.size()-2], pure_points.back()));
        }
    }
    
    double pure_zero_rate = static_cast<double>(pure_accurate_predictions) / pure_total_predictions * 100.0;
    
    std::cout << "纯预测结果:" << std::endl;
    std::cout << "  总预测次数: " << pure_total_predictions << std::endl;
    std::cout << "  准确预测次数: " << pure_accurate_predictions << std::endl;
    std::cout << "  预测准确率: " << pure_zero_rate << "%" << std::endl;
    
    // 分析差异
    std::cout << "\n=== 差异分析 ===" << std::endl;
    
    double actual_vs_sim = std::abs(actual_zero_rate - sim_zero_rate);
    double sim_vs_pure = std::abs(sim_zero_rate - pure_zero_rate);
    double actual_vs_pure = std::abs(actual_zero_rate - pure_zero_rate);
    
    std::cout << "实际压缩器 vs 模拟压缩器差异: " << std::setprecision(2) << actual_vs_sim << "%" << std::endl;
    std::cout << "模拟压缩器 vs 纯预测差异: " << sim_vs_pure << "%" << std::endl;
    std::cout << "实际压缩器 vs 纯预测差异: " << actual_vs_pure << "%" << std::endl;
    
    if (actual_vs_sim > 2.0) {
        std::cout << "\n🚨 实际压缩器与模拟压缩器存在显著差异！" << std::endl;
        std::cout << "可能的问题:" << std::endl;
        std::cout << "1. 几何剪枝算法的影响" << std::endl;
        std::cout << "2. 策略选择逻辑的复杂性" << std::endl;
        std::cout << "3. 编码成本评估的影响" << std::endl;
        std::cout << "4. 状态管理的差异" << std::endl;
    } else {
        std::cout << "\n✅ 实际压缩器与模拟压缩器基本一致" << std::endl;
    }
    
    if (sim_vs_pure > 5.0) {
        std::cout << "\n🎯 量化重构是主要问题！" << std::endl;
        std::cout << "量化重构导致预测准确率下降: " << sim_vs_pure << "%" << std::endl;
        std::cout << "建议优化量化参数或改进重构策略" << std::endl;
    } else {
        std::cout << "\n✅ 量化重构影响较小" << std::endl;
    }
    
    // 详细分析前100个点的预测情况
    std::cout << "\n=== 详细调试前100个点 ===" << std::endl;
    
    GPSCompressorSimulator debug_sim(gps_error_bound, epsilon_v, epsilon_theta);
    int debug_count = 0;
    int debug_zero_corrections = 0;
    
    for (int i = 0; i < std::min(100, static_cast<int>(points.size())); ++i) {
        bool accurate = debug_sim.AddPoint(points[i]);
        
        if (debug_sim.GetPredictionCount() > 0) {
            debug_count++;
            if (accurate) {
                debug_zero_corrections++;
            }
            
            if (debug_count <= 10) {  // 只打印前10个预测
                auto current_reconstructed = debug_sim.GetCurrentReconstructedPoint();
                auto current_motion = debug_sim.GetCurrentMotionVector();
                
                std::cout << "点" << i << ": " 
                          << (accurate ? "零校正" : "需校正") 
                          << ", 重构点(" << std::setprecision(6) << current_reconstructed.longitude 
                          << "," << current_reconstructed.latitude << ")"
                          << ", 运动矢量(v=" << current_motion.velocity << ",θ=" << current_motion.theta << ")" << std::endl;
            }
        }
    }
    
    double debug_rate = static_cast<double>(debug_zero_corrections) / debug_count * 100.0;
    std::cout << "前100个点的零校正率: " << debug_rate << "%" << std::endl;
    
    return 0;
}
