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

// GPS轨迹预测器（完整模拟压缩器逻辑）
class GPSTrajectoryPredictor {
private:
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> reconstructed_points_;
    std::vector<MotionVector> motion_vectors_;
    double kEMax_;
    double kEpsilonV_;
    double kEpsilonTheta_;
    int point_index_;
    
public:
    GPSTrajectoryPredictor(double e_max, double epsilon_v, double epsilon_theta) 
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
            // 第二个点，使用完全校正
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
            // 第三个点及以后，进行预测
            const auto& current_reconstructed = reconstructed_points_.back();
            const auto& current_motion = motion_vectors_.back();
            
            // 进行预测（零阶预测：重复上一次运动矢量）
            auto predicted_point = CalculateDestinationPoint(current_reconstructed, current_motion);
            double prediction_error = Calculate2DDistance(actual_point, predicted_point);
            
            result.predicted_point = predicted_point;
            result.predicted_motion = current_motion;
            result.prediction_error = prediction_error;
            
            if (prediction_error <= kEMax_) {
                // 使用零校正
                result.used_zero_correction = true;
                result.reconstructed_point = predicted_point;
                result.reconstructed_motion = current_motion;
                result.status = "零校正";
                
                reconstructed_points_.push_back(predicted_point);
                motion_vectors_.push_back(current_motion);
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

// 线性预测器
class LinearPredictor {
private:
    std::vector<double> reconstructed_longitudes_;
    std::vector<double> reconstructed_latitudes_;
    double max_diff_;
    int point_index_;
    
public:
    LinearPredictor(double max_diff) : max_diff_(max_diff * 0.999), point_index_(0) {}
    
    struct LinearPredictionResult {
        SerfQtGpsConfigurableCompressor::GpsPoint predicted_point;
        SerfQtGpsConfigurableCompressor::GpsPoint reconstructed_point;
        double prediction_error_2d;
        bool used_zero_correction;
        std::string status;
    };
    
    LinearPredictionResult ProcessPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& actual_point) {
        LinearPredictionResult result;
        result.predicted_point = SerfQtGpsConfigurableCompressor::GpsPoint(0, 0);
        result.reconstructed_point = actual_point;
        result.prediction_error_2d = 0.0;
        result.used_zero_correction = false;
        
        if (point_index_ < 2) {
            // 前两个点，简化处理
            double lon_predicted = (reconstructed_longitudes_.empty()) ? 2.0 : reconstructed_longitudes_.back();
            double lat_predicted = (reconstructed_latitudes_.empty()) ? 2.0 : reconstructed_latitudes_.back();
            
            // 量化重构
            long q_lon = static_cast<long>(std::round((actual_point.longitude - lon_predicted) / (2 * max_diff_)));
            long q_lat = static_cast<long>(std::round((actual_point.latitude - lat_predicted) / (2 * max_diff_)));
            
            double reconstructed_lon = lon_predicted + 2 * max_diff_ * static_cast<double>(q_lon);
            double reconstructed_lat = lat_predicted + 2 * max_diff_ * static_cast<double>(q_lat);
            
            reconstructed_longitudes_.push_back(reconstructed_lon);
            reconstructed_latitudes_.push_back(reconstructed_lat);
            
            result.reconstructed_point = SerfQtGpsConfigurableCompressor::GpsPoint(reconstructed_lon, reconstructed_lat);
            result.status = (point_index_ == 0) ? "第一个点" : "第二个点";
            point_index_++;
            return result;
        } else {
            // 第三个点及以后，进行线性预测
            double predicted_lon = 2.0 * reconstructed_longitudes_.back() - reconstructed_longitudes_[reconstructed_longitudes_.size()-2];
            double predicted_lat = 2.0 * reconstructed_latitudes_.back() - reconstructed_latitudes_[reconstructed_latitudes_.size()-2];
            
            SerfQtGpsConfigurableCompressor::GpsPoint predicted_point(predicted_lon, predicted_lat);
            
            // 计算2D预测误差
            double prediction_error_2d = Calculate2DDistance(actual_point, predicted_point);
            
            // 检查1D误差（线性预测的实际判断标准）
            double error_lon = std::abs(actual_point.longitude - predicted_lon);
            double error_lat = std::abs(actual_point.latitude - predicted_lat);
            bool accurate_1d = (error_lon <= max_diff_) && (error_lat <= max_diff_);
            
            result.predicted_point = predicted_point;
            result.prediction_error_2d = prediction_error_2d;
            result.used_zero_correction = accurate_1d;
            
            // 量化和重构
            long q_lon = static_cast<long>(std::round((actual_point.longitude - predicted_lon) / (2 * max_diff_)));
            long q_lat = static_cast<long>(std::round((actual_point.latitude - predicted_lat) / (2 * max_diff_)));
            
            double reconstructed_lon = predicted_lon + 2 * max_diff_ * static_cast<double>(q_lon);
            double reconstructed_lat = predicted_lat + 2 * max_diff_ * static_cast<double>(q_lat);
            
            reconstructed_longitudes_.push_back(reconstructed_lon);
            reconstructed_latitudes_.push_back(reconstructed_lat);
            
            result.reconstructed_point = SerfQtGpsConfigurableCompressor::GpsPoint(reconstructed_lon, reconstructed_lat);
            result.status = accurate_1d ? "零校正" : "需要校正";
            
            point_index_++;
            return result;
        }
    }
    
    int GetPointIndex() const { return point_index_; }
};

int main() {
    std::cout << "=== 逐点详细分析：GPS轨迹预测 vs 线性预测 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 20;  // 只分析前20个点，便于详细观察
    
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
    const double linear_max_diff = 1e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 1e-5;
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  GPS轨迹误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  线性预测误差界限: " << linear_max_diff << " 度" << std::endl;
    std::cout << "  GPS量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    // 初始化预测器
    GPSTrajectoryPredictor gps_predictor(gps_error_bound, epsilon_v, epsilon_theta);
    LinearPredictor linear_predictor(linear_max_diff);
    
    std::cout << "\n=== 逐点详细分析 ===" << std::endl;
    
    // 打印表头
    std::cout << std::setw(4) << "点" 
              << std::setw(15) << "真实经度" 
              << std::setw(15) << "真实纬度" 
              << std::setw(15) << "GPS预测经度" 
              << std::setw(15) << "GPS预测纬度" 
              << std::setw(15) << "线性预测经度" 
              << std::setw(15) << "线性预测纬度" 
              << std::setw(12) << "GPS误差" 
              << std::setw(12) << "线性误差" 
              << std::setw(12) << "GPS状态" 
              << std::setw(12) << "线性状态" << std::endl;
    std::cout << std::string(170, '-') << std::endl;
    
    int gps_zero_corrections = 0, linear_zero_corrections = 0;
    int gps_predictions = 0, linear_predictions = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        const auto& actual_point = points[i];
        
        // GPS轨迹预测
        auto gps_result = gps_predictor.ProcessPoint(actual_point);
        
        // 线性预测
        auto linear_result = linear_predictor.ProcessPoint(actual_point);
        
        // 统计预测结果
        if (gps_predictor.GetPointIndex() > 2) {
            gps_predictions++;
            if (gps_result.used_zero_correction) {
                gps_zero_corrections++;
            }
        }
        
        if (linear_predictor.GetPointIndex() > 2) {
            linear_predictions++;
            if (linear_result.used_zero_correction) {
                linear_zero_corrections++;
            }
        }
        
        // 打印详细信息
        std::cout << std::setw(4) << i
                  << std::setw(15) << std::fixed << std::setprecision(6) << actual_point.longitude
                  << std::setw(15) << actual_point.latitude
                  << std::setw(15) << gps_result.predicted_point.longitude
                  << std::setw(15) << gps_result.predicted_point.latitude
                  << std::setw(15) << linear_result.predicted_point.longitude
                  << std::setw(15) << linear_result.predicted_point.latitude
                  << std::setw(12) << std::scientific << std::setprecision(2) << gps_result.prediction_error
                  << std::setw(12) << linear_result.prediction_error_2d
                  << std::setw(12) << gps_result.status
                  << std::setw(12) << linear_result.status << std::endl;
        
        // 详细分析前几个预测点
        if (i >= 2 && i < 8) {
            std::cout << "\n📊 点" << i << "详细分析:" << std::endl;
            
            // GPS轨迹预测分析
            std::cout << "  GPS轨迹预测:" << std::endl;
            std::cout << "    预测运动矢量: v=" << std::fixed << std::setprecision(8) << gps_result.predicted_motion.velocity 
                      << ", θ=" << gps_result.predicted_motion.theta << std::endl;
            std::cout << "    预测点: (" << gps_result.predicted_point.longitude << ", " << gps_result.predicted_point.latitude << ")" << std::endl;
            std::cout << "    真实点: (" << actual_point.longitude << ", " << actual_point.latitude << ")" << std::endl;
            std::cout << "    重构点: (" << gps_result.reconstructed_point.longitude << ", " << gps_result.reconstructed_point.latitude << ")" << std::endl;
            std::cout << "    预测误差: " << std::scientific << gps_result.prediction_error << " 度" << std::endl;
            std::cout << "    状态: " << gps_result.status << std::endl;
            
            // 线性预测分析
            std::cout << "  线性预测:" << std::endl;
            std::cout << "    预测点: (" << std::fixed << std::setprecision(8) << linear_result.predicted_point.longitude 
                      << ", " << linear_result.predicted_point.latitude << ")" << std::endl;
            std::cout << "    真实点: (" << actual_point.longitude << ", " << actual_point.latitude << ")" << std::endl;
            std::cout << "    重构点: (" << linear_result.reconstructed_point.longitude << ", " << linear_result.reconstructed_point.latitude << ")" << std::endl;
            std::cout << "    预测误差: " << std::scientific << linear_result.prediction_error_2d << " 度" << std::endl;
            std::cout << "    状态: " << linear_result.status << std::endl;
            
            // 对比分析
            std::cout << "  对比分析:" << std::endl;
            double prediction_diff = std::abs(gps_result.prediction_error - linear_result.prediction_error_2d);
            std::cout << "    预测误差差异: " << prediction_diff << " 度" << std::endl;
            
            if (prediction_diff < 1e-10) {
                std::cout << "    ✅ 预测误差几乎相同！" << std::endl;
            } else if (prediction_diff < 1e-6) {
                std::cout << "    ✅ 预测误差非常接近" << std::endl;
            } else {
                std::cout << "    ❓ 预测误差存在差异" << std::endl;
            }
            
            std::cout << std::endl;
        }
    }
    
    // 统计结果
    std::cout << "\n=== 统计结果 ===" << std::endl;
    
    double gps_zero_rate = (gps_predictions > 0) ? static_cast<double>(gps_zero_corrections) / gps_predictions * 100.0 : 0.0;
    double linear_zero_rate = (linear_predictions > 0) ? static_cast<double>(linear_zero_corrections) / linear_predictions * 100.0 : 0.0;
    
    std::cout << "📊 GPS轨迹预测:" << std::endl;
    std::cout << "  预测次数: " << gps_predictions << std::endl;
    std::cout << "  零校正次数: " << gps_zero_corrections << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << gps_zero_rate << "%" << std::endl;
    
    std::cout << "\n📊 线性预测:" << std::endl;
    std::cout << "  预测次数: " << linear_predictions << std::endl;
    std::cout << "  零校正次数: " << linear_zero_corrections << std::endl;
    std::cout << "  零校正率: " << linear_zero_rate << "%" << std::endl;
    
    std::cout << "\n=== 结论 ===" << std::endl;
    
    std::cout << "💡 关键发现:" << std::endl;
    std::cout << "1. 两种预测方法都是基于历史重构点的线性预测" << std::endl;
    std::cout << "2. GPS轨迹预测：基于运动矢量的空间线性预测" << std::endl;
    std::cout << "3. 线性预测：基于坐标的独立线性预测" << std::endl;
    std::cout << "4. 两者在数学本质上都是线性外推，但在不同的坐标系统中" << std::endl;
    
    if (std::abs(gps_zero_rate - linear_zero_rate) < 5.0) {
        std::cout << "5. ✅ 在小样本测试中，两种方法的零校正率相近" << std::endl;
    } else {
        std::cout << "5. ❓ 两种方法的零校正率存在差异，需要进一步分析" << std::endl;
    }
    
    return 0;
}
