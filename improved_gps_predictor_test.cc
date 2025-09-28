#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>

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

// 改进的GPS轨迹预测器（使用一阶线性预测）
class ImprovedGPSTrajectoryPredictor {
private:
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> reconstructed_points_;
    std::vector<MotionVector> motion_vectors_;
    double kEMax_;
    double kEpsilonV_;
    double kEpsilonTheta_;
    int point_index_;
    
public:
    ImprovedGPSTrajectoryPredictor(double e_max, double epsilon_v, double epsilon_theta) 
        : kEMax_(e_max * 0.999), kEpsilonV_(epsilon_v), kEpsilonTheta_(epsilon_theta), point_index_(0) {}
    
    struct PredictionResult {
        SerfQtGpsConfigurableCompressor::GpsPoint predicted_point;
        SerfQtGpsConfigurableCompressor::GpsPoint reconstructed_point;
        MotionVector predicted_motion;
        MotionVector reconstructed_motion;
        double prediction_error;
        bool used_zero_correction;
        std::string status;
    };
    
    PredictionResult ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& actual_point) {
        PredictionResult result;
        result.predicted_point = SerfQtGpsConfigurableCompressor::GpsPoint(0, 0);
        result.reconstructed_point = actual_point;
        result.predicted_motion = MotionVector(0, 0);
        result.reconstructed_motion = MotionVector(0, 0);
        result.prediction_error = 0.0;
        result.used_zero_correction = false;
        
        if (point_index_ == 0) {
            // 第一个点
            reconstructed_points_.push_back(actual_point);
            motion_vectors_.emplace_back(0, 0);
            result.status = "第一个点";
            point_index_++;
            return result;
        } else if (point_index_ == 1) {
            // 第二个点
            const auto& prev_point = reconstructed_points_.back();
            MotionVector true_motion = CalculateMotionVector(prev_point, actual_point);
            MotionVector quantized_motion = QuantizeMotionVector(true_motion, kEpsilonV_, kEpsilonTheta_);
            
            auto reconstructed_point = CalculateDestinationPoint(prev_point, quantized_motion);
            
            reconstructed_points_.push_back(reconstructed_point);
            motion_vectors_.push_back(quantized_motion);
            
            result.reconstructed_point = reconstructed_point;
            result.reconstructed_motion = quantized_motion;
            result.status = "第二个点(完全校正)";
            point_index_++;
            return result;
        } else {
            // 第三个点及以后，使用一阶线性预测
            const auto& current_reconstructed = reconstructed_points_.back();
            const auto& current_motion = motion_vectors_.back();
            const auto& prev_motion = motion_vectors_[motion_vectors_.size()-2];
            
            // 一阶线性预测运动矢量
            MotionVector predicted_motion;
            predicted_motion.velocity = 2.0 * current_motion.velocity - prev_motion.velocity;
            predicted_motion.theta = 2.0 * current_motion.theta - prev_motion.theta;
            
            // 处理角度的周期性
            while (predicted_motion.theta > M_PI) predicted_motion.theta -= 2 * M_PI;
            while (predicted_motion.theta < -M_PI) predicted_motion.theta += 2 * M_PI;
            
            // 确保速度非负
            if (predicted_motion.velocity < 0) predicted_motion.velocity = 0;
            
            auto predicted_point = CalculateDestinationPoint(current_reconstructed, predicted_motion);
            double prediction_error = Calculate2DDistance(actual_point, predicted_point);
            
            result.predicted_point = predicted_point;
            result.predicted_motion = predicted_motion;
            result.prediction_error = prediction_error;
            
            if (prediction_error <= kEMax_) {
                // 使用零校正
                result.used_zero_correction = true;
                result.reconstructed_point = predicted_point;
                result.reconstructed_motion = predicted_motion;
                result.status = "零校正(一阶预测)";
                
                reconstructed_points_.push_back(predicted_point);
                motion_vectors_.push_back(predicted_motion);
            } else {
                // 需要校正
                MotionVector true_motion = CalculateMotionVector(current_reconstructed, actual_point);
                MotionVector quantized_motion = QuantizeMotionVector(true_motion, kEpsilonV_, kEpsilonTheta_);
                auto reconstructed_point = CalculateDestinationPoint(current_reconstructed, quantized_motion);
                
                result.used_zero_correction = false;
                result.reconstructed_point = reconstructed_point;
                result.reconstructed_motion = quantized_motion;
                result.status = "需要校正";
                
                reconstructed_points_.push_back(reconstructed_point);
                motion_vectors_.push_back(quantized_motion);
            }
            
            point_index_++;
            return result;
        }
    }
    
    int GetPointIndex() const { return point_index_; }
};

// 原始GPS轨迹预测器（零阶预测）
class OriginalGPSTrajectoryPredictor {
private:
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> reconstructed_points_;
    std::vector<MotionVector> motion_vectors_;
    double kEMax_;
    double kEpsilonV_;
    double kEpsilonTheta_;
    int point_index_;
    
public:
    OriginalGPSTrajectoryPredictor(double e_max, double epsilon_v, double epsilon_theta) 
        : kEMax_(e_max * 0.999), kEpsilonV_(epsilon_v), kEpsilonTheta_(epsilon_theta), point_index_(0) {}
    
    struct PredictionResult {
        SerfQtGpsConfigurableCompressor::GpsPoint predicted_point;
        SerfQtGpsConfigurableCompressor::GpsPoint reconstructed_point;
        MotionVector predicted_motion;
        MotionVector reconstructed_motion;
        double prediction_error;
        bool used_zero_correction;
        std::string status;
    };
    
    PredictionResult ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& actual_point) {
        PredictionResult result;
        result.predicted_point = SerfQtGpsConfigurableCompressor::GpsPoint(0, 0);
        result.reconstructed_point = actual_point;
        result.predicted_motion = MotionVector(0, 0);
        result.reconstructed_motion = MotionVector(0, 0);
        result.prediction_error = 0.0;
        result.used_zero_correction = false;
        
        if (point_index_ == 0) {
            reconstructed_points_.push_back(actual_point);
            motion_vectors_.emplace_back(0, 0);
            result.status = "第一个点";
            point_index_++;
            return result;
        } else if (point_index_ == 1) {
            const auto& prev_point = reconstructed_points_.back();
            MotionVector true_motion = CalculateMotionVector(prev_point, actual_point);
            MotionVector quantized_motion = QuantizeMotionVector(true_motion, kEpsilonV_, kEpsilonTheta_);
            
            auto reconstructed_point = CalculateDestinationPoint(prev_point, quantized_motion);
            
            reconstructed_points_.push_back(reconstructed_point);
            motion_vectors_.push_back(quantized_motion);
            
            result.reconstructed_point = reconstructed_point;
            result.reconstructed_motion = quantized_motion;
            result.status = "第二个点(完全校正)";
            point_index_++;
            return result;
        } else {
            // 零阶预测（重复上一次运动矢量）
            const auto& current_reconstructed = reconstructed_points_.back();
            const auto& current_motion = motion_vectors_.back();
            
            auto predicted_point = CalculateDestinationPoint(current_reconstructed, current_motion);
            double prediction_error = Calculate2DDistance(actual_point, predicted_point);
            
            result.predicted_point = predicted_point;
            result.predicted_motion = current_motion;
            result.prediction_error = prediction_error;
            
            if (prediction_error <= kEMax_) {
                result.used_zero_correction = true;
                result.reconstructed_point = predicted_point;
                result.reconstructed_motion = current_motion;
                result.status = "零校正(零阶预测)";
                
                reconstructed_points_.push_back(predicted_point);
                motion_vectors_.push_back(current_motion);
            } else {
                MotionVector true_motion = CalculateMotionVector(current_reconstructed, actual_point);
                MotionVector quantized_motion = QuantizeMotionVector(true_motion, kEpsilonV_, kEpsilonTheta_);
                auto reconstructed_point = CalculateDestinationPoint(current_reconstructed, quantized_motion);
                
                result.used_zero_correction = false;
                result.reconstructed_point = reconstructed_point;
                result.reconstructed_motion = quantized_motion;
                result.status = "需要校正";
                
                reconstructed_points_.push_back(reconstructed_point);
                motion_vectors_.push_back(quantized_motion);
            }
            
            point_index_++;
            return result;
        }
    }
    
    int GetPointIndex() const { return point_index_; }
};

int main() {
    std::cout << "=== 改进GPS轨迹预测器测试：零阶 vs 一阶预测 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 1000;  // 使用1000个点进行测试
    
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
    const double epsilon_theta = 1e-5;
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  GPS轨迹误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  GPS量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    // 初始化预测器
    OriginalGPSTrajectoryPredictor original_predictor(gps_error_bound, epsilon_v, epsilon_theta);
    ImprovedGPSTrajectoryPredictor improved_predictor(gps_error_bound, epsilon_v, epsilon_theta);
    
    std::cout << "\n=== 对比测试 ===" << std::endl;
    
    int original_zero_corrections = 0, improved_zero_corrections = 0;
    int original_predictions = 0, improved_predictions = 0;
    
    double original_total_error = 0.0, improved_total_error = 0.0;
    
    // 只显示前10个点的详细信息
    std::cout << "\n前10个点的详细对比:" << std::endl;
    std::cout << std::setw(4) << "点" 
              << std::setw(15) << "原始预测误差" 
              << std::setw(15) << "改进预测误差" 
              << std::setw(15) << "原始状态" 
              << std::setw(15) << "改进状态" 
              << std::setw(15) << "误差改进" << std::endl;
    std::cout << std::string(90, '-') << std::endl;
    
    for (int i = 0; i < points.size(); ++i) {
        const auto& actual_point = points[i];
        
        // 原始GPS轨迹预测
        auto original_result = original_predictor.ProcessPoint(actual_point);
        
        // 改进GPS轨迹预测
        auto improved_result = improved_predictor.ProcessPoint(actual_point);
        
        // 统计预测结果
        if (original_predictor.GetPointIndex() > 2) {
            original_predictions++;
            if (original_result.used_zero_correction) {
                original_zero_corrections++;
            }
            original_total_error += original_result.prediction_error;
        }
        
        if (improved_predictor.GetPointIndex() > 2) {
            improved_predictions++;
            if (improved_result.used_zero_correction) {
                improved_zero_corrections++;
            }
            improved_total_error += improved_result.prediction_error;
        }
        
        // 显示前10个点的详细信息
        if (i < 10) {
            double error_improvement = original_result.prediction_error - improved_result.prediction_error;
            std::cout << std::setw(4) << i
                      << std::setw(15) << std::scientific << std::setprecision(2) << original_result.prediction_error
                      << std::setw(15) << improved_result.prediction_error
                      << std::setw(15) << original_result.status.substr(0, 12)
                      << std::setw(15) << improved_result.status.substr(0, 12)
                      << std::setw(15) << error_improvement << std::endl;
        }
    }
    
    // 统计结果
    std::cout << "\n=== 统计结果 ===" << std::endl;
    
    double original_zero_rate = (original_predictions > 0) ? static_cast<double>(original_zero_corrections) / original_predictions * 100.0 : 0.0;
    double improved_zero_rate = (improved_predictions > 0) ? static_cast<double>(improved_zero_corrections) / improved_predictions * 100.0 : 0.0;
    
    double original_avg_error = (original_predictions > 0) ? original_total_error / original_predictions : 0.0;
    double improved_avg_error = (improved_predictions > 0) ? improved_total_error / improved_predictions : 0.0;
    
    std::cout << "📊 原始GPS轨迹预测（零阶）:" << std::endl;
    std::cout << "  预测次数: " << original_predictions << std::endl;
    std::cout << "  零校正次数: " << original_zero_corrections << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << original_zero_rate << "%" << std::endl;
    std::cout << "  平均预测误差: " << std::scientific << original_avg_error << " 度" << std::endl;
    
    std::cout << "\n📊 改进GPS轨迹预测（一阶）:" << std::endl;
    std::cout << "  预测次数: " << improved_predictions << std::endl;
    std::cout << "  零校正次数: " << improved_zero_corrections << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << improved_zero_rate << "%" << std::endl;
    std::cout << "  平均预测误差: " << std::scientific << improved_avg_error << " 度" << std::endl;
    
    // 改进效果分析
    std::cout << "\n=== 改进效果分析 ===" << std::endl;
    
    double zero_rate_improvement = improved_zero_rate - original_zero_rate;
    double error_reduction = original_avg_error - improved_avg_error;
    double error_reduction_percent = (original_avg_error > 0) ? (error_reduction / original_avg_error * 100.0) : 0.0;
    
    std::cout << "🎯 零校正率改进: " << std::showpos << zero_rate_improvement << "%" << std::noshowpos << std::endl;
    std::cout << "🎯 平均误差减少: " << std::scientific << error_reduction << " 度 (" 
              << std::fixed << std::setprecision(1) << error_reduction_percent << "%)" << std::endl;
    
    if (zero_rate_improvement > 5.0) {
        std::cout << "✅ 一阶预测显著提升了零校正率！" << std::endl;
    } else if (zero_rate_improvement > 0.0) {
        std::cout << "✅ 一阶预测略微提升了零校正率" << std::endl;
    } else {
        std::cout << "❌ 一阶预测未能提升零校正率" << std::endl;
    }
    
    if (error_reduction_percent > 10.0) {
        std::cout << "✅ 一阶预测显著减少了预测误差！" << std::endl;
    } else if (error_reduction_percent > 0.0) {
        std::cout << "✅ 一阶预测略微减少了预测误差" << std::endl;
    } else {
        std::cout << "❌ 一阶预测未能减少预测误差" << std::endl;
    }
    
    std::cout << "\n=== 结论 ===" << std::endl;
    
    std::cout << "💡 关键发现:" << std::endl;
    std::cout << "1. 原始GPS轨迹预测使用零阶预测（重复运动矢量）" << std::endl;
    std::cout << "2. 改进GPS轨迹预测使用一阶线性预测（运动矢量外推）" << std::endl;
    std::cout << "3. 预测阶数的差异是影响性能的重要因素" << std::endl;
    
    if (zero_rate_improvement > 5.0) {
        std::cout << "4. ✅ 一阶预测确实能显著提升GPS轨迹压缩器的性能！" << std::endl;
        std::cout << "5. 🎯 建议将GPS轨迹压缩器的预测模型升级为一阶线性预测" << std::endl;
    } else {
        std::cout << "4. ❓ 一阶预测的改进效果有限，可能需要其他优化策略" << std::endl;
        std::cout << "5. 🤔 运动矢量的不规律性可能限制了预测效果" << std::endl;
    }
    
    return 0;
}
