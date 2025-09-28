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

// GPS轨迹矢量预测器
class GPSVectorPredictor {
private:
    std::vector<SerfQtGpsConfigurableCompressor::GpsPoint> points_;
    std::vector<MotionVector> motions_;
    double error_bound_;
    
public:
    GPSVectorPredictor(double error_bound) : error_bound_(error_bound) {}
    
    // 添加点并返回预测是否准确
    bool AddPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& point) {
        if (points_.size() < 2) {
            // 前两个点，直接添加
            points_.push_back(point);
            if (points_.size() == 2) {
                motions_.push_back(CalculateMotionVector(points_[0], points_[1]));
            }
            return false; // 前两个点不算预测
        }
        
        // 进行矢量预测（零阶预测：重复上一次运动矢量）
        const auto& current_point = points_.back();
        const auto& current_motion = motions_.back();
        
        auto predicted_point = CalculateDestinationPoint(current_point, current_motion);
        
        // 计算预测误差
        double prediction_error = Calculate2DDistance(point, predicted_point);
        bool accurate = (prediction_error <= error_bound_);
        
        // 更新状态
        points_.push_back(point);
        if (points_.size() >= 2) {
            motions_.push_back(CalculateMotionVector(points_[points_.size()-2], points_.back()));
        }
        
        return accurate;
    }
    
    int GetPredictionCount() const {
        return std::max(0, static_cast<int>(points_.size()) - 2);
    }
};

// 线性预测器
class LinearPredictor {
private:
    std::vector<double> longitudes_;
    std::vector<double> latitudes_;
    double error_bound_1d_;
    double error_bound_2d_;
    
public:
    LinearPredictor(double error_bound_1d) : error_bound_1d_(error_bound_1d) {
        // 计算等效的2D误差界限
        error_bound_2d_ = error_bound_1d * std::sqrt(2.0);
    }
    
    // 添加点并返回预测是否准确
    bool AddPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& point) {
        if (longitudes_.size() < 2) {
            // 前两个点，直接添加
            longitudes_.push_back(point.longitude);
            latitudes_.push_back(point.latitude);
            return false; // 前两个点不算预测
        }
        
        // 进行线性预测（一阶预测：2*prev1 - prev2）
        double predicted_lon = 2.0 * longitudes_.back() - longitudes_[longitudes_.size()-2];
        double predicted_lat = 2.0 * latitudes_.back() - latitudes_[latitudes_.size()-2];
        
        SerfQtGpsConfigurableCompressor::GpsPoint predicted_point(predicted_lon, predicted_lat);
        
        // 计算预测误差
        double prediction_error_2d = Calculate2DDistance(point, predicted_point);
        bool accurate_2d = (prediction_error_2d <= error_bound_2d_);
        
        // 也计算1D误差用于分析
        double error_lon = std::abs(point.longitude - predicted_lon);
        double error_lat = std::abs(point.latitude - predicted_lat);
        bool accurate_1d = (error_lon <= error_bound_1d_) && (error_lat <= error_bound_1d_);
        
        // 更新状态
        longitudes_.push_back(point.longitude);
        latitudes_.push_back(point.latitude);
        
        // 返回2D误差评估结果（与GPS矢量预测保持一致）
        return accurate_2d;
    }
    
    int GetPredictionCount() const {
        return std::max(0, static_cast<int>(longitudes_.size()) - 2);
    }
    
    double GetError2DBound() const { return error_bound_2d_; }
    double GetError1DBound() const { return error_bound_1d_; }
};

int main() {
    std::cout << "=== GPS轨迹矢量预测 vs 线性预测 纯预测准确率对比 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 100000;  // 使用10万条数据
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < 1000) {
        std::cerr << "❌ 数据量不足，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置参数
    const double gps_error_bound = 1.4e-5;    // GPS轨迹预测误差界限
    const double linear_error_bound = 1e-5;   // 线性预测误差界限
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  GPS轨迹矢量预测误差界限: " << std::scientific << gps_error_bound << " 度 (2D欧几里得)" << std::endl;
    std::cout << "  线性预测误差界限: " << linear_error_bound << " 度 (1D，等效2D: " 
              << (linear_error_bound * std::sqrt(2.0)) << " 度)" << std::endl;
    std::cout << "  测试数据点数: " << points.size() << std::endl;
    
    // 初始化预测器
    GPSVectorPredictor gps_predictor(gps_error_bound);
    LinearPredictor linear_predictor(linear_error_bound);
    
    // 统计变量
    int gps_accurate_predictions = 0;
    int linear_accurate_predictions = 0;
    
    std::vector<double> gps_errors, linear_errors;
    
    std::cout << "\n🔄 进行预测对比..." << std::endl;
    
    // 逐点进行预测测试
    for (const auto& point : points) {
        // GPS轨迹矢量预测
        bool gps_accurate = gps_predictor.AddPoint(point);
        if (gps_predictor.GetPredictionCount() > 0) {
            if (gps_accurate) {
                gps_accurate_predictions++;
            }
        }
        
        // 线性预测
        bool linear_accurate = linear_predictor.AddPoint(point);
        if (linear_predictor.GetPredictionCount() > 0) {
            if (linear_accurate) {
                linear_accurate_predictions++;
            }
        }
    }
    
    // 计算结果
    int gps_total_predictions = gps_predictor.GetPredictionCount();
    int linear_total_predictions = linear_predictor.GetPredictionCount();
    
    double gps_accuracy_rate = static_cast<double>(gps_accurate_predictions) / gps_total_predictions * 100.0;
    double linear_accuracy_rate = static_cast<double>(linear_accurate_predictions) / linear_total_predictions * 100.0;
    
    // 打印结果
    std::cout << "\n=== 纯预测准确率对比结果 ===" << std::endl;
    
    std::cout << "\n📊 GPS轨迹矢量预测 (零阶预测 - 重复运动矢量):" << std::endl;
    std::cout << "  预测次数: " << gps_total_predictions << std::endl;
    std::cout << "  准确预测: " << gps_accurate_predictions << std::endl;
    std::cout << "  预测准确率: " << std::fixed << std::setprecision(2) << gps_accuracy_rate << "%" << std::endl;
    std::cout << "  误差界限: " << std::scientific << gps_error_bound << " 度 (2D)" << std::endl;
    
    std::cout << "\n📊 线性预测 (一阶预测 - 2*prev1-prev2):" << std::endl;
    std::cout << "  预测次数: " << linear_total_predictions << std::endl;
    std::cout << "  准确预测: " << linear_accurate_predictions << std::endl;
    std::cout << "  预测准确率: " << linear_accuracy_rate << "%" << std::endl;
    std::cout << "  误差界限: " << linear_predictor.GetError1DBound() << " 度 (1D), " 
              << linear_predictor.GetError2DBound() << " 度 (等效2D)" << std::endl;
    
    // 分析结果
    std::cout << "\n=== 预测准确率分析 ===" << std::endl;
    
    double accuracy_difference = linear_accuracy_rate - gps_accuracy_rate;
    
    std::cout << "预测准确率差异: " << std::showpos << std::setprecision(2) << accuracy_difference << "%" << std::noshowpos << std::endl;
    
    if (std::abs(accuracy_difference) < 2.0) {
        std::cout << "✅ 两种预测方法准确率基本相同" << std::endl;
        std::cout << "差异可能在误差界限的细微不同或数据特性" << std::endl;
    } else if (linear_accuracy_rate > gps_accuracy_rate) {
        std::cout << "📈 线性预测明显优于GPS轨迹矢量预测" << std::endl;
        std::cout << "优势: " << accuracy_difference << "%" << std::endl;
        
        std::cout << "\n🔍 可能的原因分析:" << std::endl;
        std::cout << "1. 预测阶数差异: 一阶线性预测 vs 零阶矢量重复" << std::endl;
        std::cout << "2. GPS轨迹特性: 坐标变化可能更适合线性外推" << std::endl;
        std::cout << "3. 误差界限差异: 虽然调整为等效2D，但仍可能有细微差别" << std::endl;
        std::cout << "4. 数据分布特征: Geolife数据可能更适合坐标级预测" << std::endl;
    } else {
        std::cout << "📈 GPS轨迹矢量预测优于线性预测" << std::endl;
        std::cout << "优势: " << -accuracy_difference << "%" << std::endl;
        
        std::cout << "\n🔍 可能的原因分析:" << std::endl;
        std::cout << "1. 2D运动相关性: 矢量预测考虑了经纬度变化的相关性" << std::endl;
        std::cout << "2. 运动连续性: 运动矢量重复假设在某些轨迹段更有效" << std::endl;
    }
    
    // 详细分析
    std::cout << "\n=== 详细分析 ===" << std::endl;
    
    std::cout << "🎯 关键发现:" << std::endl;
    std::cout << "1. 使用 " << points.size() << " 条Geolife GPS轨迹数据" << std::endl;
    std::cout << "2. GPS矢量预测使用零阶预测（重复运动矢量）" << std::endl;
    std::cout << "3. 线性预测使用一阶预测（线性外推坐标）" << std::endl;
    std::cout << "4. 两者使用近似等效的2D误差界限" << std::endl;
    
    if (linear_accuracy_rate > gps_accuracy_rate + 5.0) {
        std::cout << "\n💡 建议改进GPS轨迹预测:" << std::endl;
        std::cout << "1. 尝试一阶矢量预测: predicted_motion = 2*current - previous" << std::endl;
        std::cout << "2. 考虑混合预测策略" << std::endl;
        std::cout << "3. 根据轨迹特征自适应选择预测方法" << std::endl;
    }
    
    std::cout << "\n=== 结论 ===" << std::endl;
    
    if (accuracy_difference > 5.0) {
        std::cout << "🚨 线性预测在Geolife数据上显著优于GPS轨迹矢量预测" << std::endl;
        std::cout << "这解释了为什么Serf-QT压缩比更好" << std::endl;
    } else if (accuracy_difference < -5.0) {
        std::cout << "🚀 GPS轨迹矢量预测显著优于线性预测" << std::endl;
        std::cout << "GPS轨迹压缩器应该有更好的压缩比" << std::endl;
    } else {
        std::cout << "⚖️ 两种预测方法在纯预测准确率上相近" << std::endl;
        std::cout << "压缩比差异可能来自编码效率而非预测准确率" << std::endl;
    }
    
    return 0;
}
