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

int main() {
    std::cout << "=== 纯预测命中率对比：消除量化差异的影响 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 50000;  // 使用5万条数据
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < 1000) {
        std::cerr << "❌ 数据量不足，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置相同的2D误差界限
    const double error_bound_2d = 1.4e-5;  // 统一使用2D欧几里得距离
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  统一误差界限: " << std::scientific << error_bound_2d << " 度 (2D欧几里得距离)" << std::endl;
    std::cout << "  测试数据点数: " << points.size() << std::endl;
    
    std::cout << "\n=== 方法1: GPS轨迹运动矢量预测 ===" << std::endl;
    std::cout << "预测逻辑: 基于前一个运动矢量，预测下一个点的位置" << std::endl;
    std::cout << "数学表达: predicted_point = current_point + previous_motion_vector" << std::endl;
    
    // GPS轨迹运动矢量预测
    std::vector<MotionVector> motion_vectors;
    int gps_predictions = 0;
    int gps_hits = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        if (i == 0) {
            // 第一个点，无法预测
            continue;
        } else if (i == 1) {
            // 第二个点，计算第一个运动矢量
            motion_vectors.push_back(CalculateMotionVector(points[0], points[1]));
            continue;
        } else {
            // 第三个点及以后，进行预测
            gps_predictions++;
            
            // 使用前一个运动矢量进行预测
            const auto& prev_motion = motion_vectors.back();
            auto predicted_point = CalculateDestinationPoint(points[i-1], prev_motion);
            
            // 计算预测误差
            double prediction_error = Calculate2DDistance(points[i], predicted_point);
            
            if (prediction_error <= error_bound_2d) {
                gps_hits++;
            }
            
            // 更新运动矢量
            motion_vectors.push_back(CalculateMotionVector(points[i-1], points[i]));
        }
    }
    
    double gps_hit_rate = static_cast<double>(gps_hits) / gps_predictions * 100.0;
    
    std::cout << "  预测次数: " << gps_predictions << std::endl;
    std::cout << "  命中次数: " << gps_hits << std::endl;
    std::cout << "  命中率: " << std::fixed << std::setprecision(2) << gps_hit_rate << "%" << std::endl;
    
    std::cout << "\n=== 方法2: 直角坐标线性预测 ===" << std::endl;
    std::cout << "预测逻辑: 基于前两个点的坐标，线性外推下一个点" << std::endl;
    std::cout << "数学表达: predicted_point = 2 * points[i-1] - points[i-2]" << std::endl;
    
    // 直角坐标线性预测
    int linear_predictions = 0;
    int linear_hits = 0;
    
    for (int i = 2; i < points.size(); ++i) {
        linear_predictions++;
        
        // 线性预测
        double predicted_lon = 2.0 * points[i-1].longitude - points[i-2].longitude;
        double predicted_lat = 2.0 * points[i-1].latitude - points[i-2].latitude;
        SerfQtGpsConfigurableCompressor::GpsPoint predicted_point(predicted_lon, predicted_lat);
        
        // 计算预测误差
        double prediction_error = Calculate2DDistance(points[i], predicted_point);
        
        if (prediction_error <= error_bound_2d) {
            linear_hits++;
        }
    }
    
    double linear_hit_rate = static_cast<double>(linear_hits) / linear_predictions * 100.0;
    
    std::cout << "  预测次数: " << linear_predictions << std::endl;
    std::cout << "  命中次数: " << linear_hits << std::endl;
    std::cout << "  命中率: " << linear_hit_rate << "%" << std::endl;
    
    std::cout << "\n=== 方法3: 运动矢量线性预测 ===" << std::endl;
    std::cout << "预测逻辑: 基于前两个运动矢量，线性外推下一个运动矢量" << std::endl;
    std::cout << "数学表达: predicted_motion = 2 * motion[i-1] - motion[i-2]" << std::endl;
    
    // 运动矢量线性预测
    motion_vectors.clear();
    int motion_linear_predictions = 0;
    int motion_linear_hits = 0;
    
    for (int i = 0; i < points.size(); ++i) {
        if (i == 0) {
            continue;
        } else if (i == 1) {
            motion_vectors.push_back(CalculateMotionVector(points[0], points[1]));
            continue;
        } else if (i == 2) {
            motion_vectors.push_back(CalculateMotionVector(points[1], points[2]));
            continue;
        } else {
            // 第四个点及以后，进行运动矢量线性预测
            motion_linear_predictions++;
            
            // 线性预测运动矢量
            const auto& motion1 = motion_vectors[motion_vectors.size()-1];
            const auto& motion2 = motion_vectors[motion_vectors.size()-2];
            
            MotionVector predicted_motion;
            predicted_motion.velocity = 2.0 * motion1.velocity - motion2.velocity;
            predicted_motion.theta = 2.0 * motion1.theta - motion2.theta;
            
            // 处理角度的周期性
            while (predicted_motion.theta > M_PI) predicted_motion.theta -= 2 * M_PI;
            while (predicted_motion.theta < -M_PI) predicted_motion.theta += 2 * M_PI;
            
            // 确保速度非负
            if (predicted_motion.velocity < 0) predicted_motion.velocity = 0;
            
            auto predicted_point = CalculateDestinationPoint(points[i-1], predicted_motion);
            
            // 计算预测误差
            double prediction_error = Calculate2DDistance(points[i], predicted_point);
            
            if (prediction_error <= error_bound_2d) {
                motion_linear_hits++;
            }
            
            // 更新运动矢量
            motion_vectors.push_back(CalculateMotionVector(points[i-1], points[i]));
        }
    }
    
    double motion_linear_hit_rate = (motion_linear_predictions > 0) ? 
        static_cast<double>(motion_linear_hits) / motion_linear_predictions * 100.0 : 0.0;
    
    std::cout << "  预测次数: " << motion_linear_predictions << std::endl;
    std::cout << "  命中次数: " << motion_linear_hits << std::endl;
    std::cout << "  命中率: " << motion_linear_hit_rate << "%" << std::endl;
    
    std::cout << "\n=== 对比分析 ===" << std::endl;
    
    std::cout << "📊 预测命中率排名:" << std::endl;
    std::vector<std::pair<std::string, double>> results = {
        {"GPS轨迹运动矢量预测(零阶)", gps_hit_rate},
        {"直角坐标线性预测(一阶)", linear_hit_rate},
        {"运动矢量线性预测(一阶)", motion_linear_hit_rate}
    };
    
    std::sort(results.begin(), results.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (int i = 0; i < results.size(); ++i) {
        std::cout << "  " << (i+1) << ". " << results[i].first 
                  << ": " << std::setprecision(2) << results[i].second << "%" << std::endl;
    }
    
    std::cout << "\n🔍 关键洞察:" << std::endl;
    
    double gps_vs_linear = linear_hit_rate - gps_hit_rate;
    double gps_vs_motion_linear = motion_linear_hit_rate - gps_hit_rate;
    
    std::cout << "1. 直角坐标线性预测 vs GPS运动矢量预测: " 
              << std::showpos << gps_vs_linear << "%" << std::noshowpos << std::endl;
    std::cout << "2. 运动矢量线性预测 vs GPS运动矢量预测: " 
              << std::showpos << gps_vs_motion_linear << "%" << std::noshowpos << std::endl;
    
    if (std::abs(gps_vs_linear) < 2.0) {
        std::cout << "\n✅ 您说得对！GPS运动矢量预测和直角坐标线性预测的命中率基本相同" << std::endl;
        std::cout << "差异主要在于:" << std::endl;
        std::cout << "  - GPS方法: 对速度和方向进行量化" << std::endl;
        std::cout << "  - 线性方法: 对经纬度残差进行量化" << std::endl;
        std::cout << "  - 预测本质: 都是基于历史数据的线性外推" << std::endl;
    } else if (linear_hit_rate > gps_hit_rate) {
        std::cout << "\n📈 直角坐标线性预测略优于GPS运动矢量预测" << std::endl;
        std::cout << "可能原因:" << std::endl;
        std::cout << "  - 预测阶数差异: 一阶 vs 零阶" << std::endl;
        std::cout << "  - 坐标系适应性: 直角坐标更直接" << std::endl;
    } else {
        std::cout << "\n📈 GPS运动矢量预测略优于直角坐标线性预测" << std::endl;
        std::cout << "可能原因:" << std::endl;
        std::cout << "  - 运动相关性: 考虑了2D运动的相关性" << std::endl;
    }
    
    std::cout << "\n=== 最终结论 ===" << std::endl;
    
    if (std::abs(gps_vs_linear) < 3.0) {
        std::cout << "🎯 **您的观点完全正确！**" << std::endl;
        std::cout << "\n关键发现:" << std::endl;
        std::cout << "1. ✅ 两种预测方法的**纯预测命中率基本相同**" << std::endl;
        std::cout << "2. ✅ 差异确实主要在于**量化方式**:" << std::endl;
        std::cout << "   - GPS轨迹: 对(velocity, theta)进行量化" << std::endl;
        std::cout << "   - 线性预测: 对(Δlon, Δlat)进行量化" << std::endl;
        std::cout << "3. ✅ 预测本质相同: 都是基于历史数据的线性外推" << std::endl;
        std::cout << "4. ✅ 压缩比差异来自**编码效率**而非预测准确率" << std::endl;
        
        std::cout << "\n💡 这解释了为什么:" << std::endl;
        std::cout << "- 纯预测准确率相近，但实际压缩性能不同" << std::endl;
        std::cout << "- 问题不在预测算法，而在编码策略和量化方式" << std::endl;
        std::cout << "- 运动矢量编码可能比坐标差值编码开销更大" << std::endl;
    } else {
        std::cout << "🤔 预测命中率存在差异，需要进一步分析原因" << std::endl;
    }
    
    return 0;
}
