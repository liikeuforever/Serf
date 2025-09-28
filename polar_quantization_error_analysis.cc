#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <random>

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
    MotionVector(double v, double t) : velocity(v), theta(t) {}
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

// 计算距离
double CalculateDistance(const SerfQtGpsConfigurableCompressor::GpsPoint& p1, 
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

// 量化坐标差值
std::pair<double, double> QuantizeCoordinateDiff(double dx, double dy, double max_diff) {
    long qx = static_cast<long>(std::round(dx / (2 * max_diff)));
    long qy = static_cast<long>(std::round(dy / (2 * max_diff)));
    return {qx * 2 * max_diff, qy * 2 * max_diff};
}

int main() {
    std::cout << "=== 极坐标量化误差放大效应分析 ===" << std::endl;
    
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
    
    // 设置参数
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 2e-4;
    const double max_diff = 1e-5;
    
    std::cout << "\n📏 参数设置:" << std::endl;
    std::cout << "  极坐标量化: εv=" << std::scientific << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    std::cout << "  直角坐标量化: max_diff=" << max_diff << std::endl;
    
    // 统计变量
    std::vector<double> polar_quantization_errors;
    std::vector<double> cartesian_quantization_errors;
    std::vector<double> polar_conversion_errors;
    std::vector<double> velocity_errors;
    std::vector<double> theta_errors;
    
    std::cout << "\n🔄 分析量化误差..." << std::endl;
    
    for (int i = 1; i < std::min(5000, static_cast<int>(points.size())); ++i) {
        const auto& start_point = points[i-1];
        const auto& end_point = points[i];
        
        // 计算真实运动矢量
        MotionVector true_motion = CalculateMotionVector(start_point, end_point);
        
        // 计算真实坐标差值
        double true_dx = end_point.longitude - start_point.longitude;
        double true_dy = end_point.latitude - start_point.latitude;
        
        // === 方法1: 极坐标量化 ===
        MotionVector quantized_motion = QuantizeMotionVector(true_motion, epsilon_v, epsilon_theta);
        
        // 转换回直角坐标
        double polar_dx = quantized_motion.velocity * std::cos(quantized_motion.theta);
        double polar_dy = quantized_motion.velocity * std::sin(quantized_motion.theta);
        
        // 计算极坐标量化误差
        double polar_error = std::sqrt((true_dx - polar_dx) * (true_dx - polar_dx) + 
                                      (true_dy - polar_dy) * (true_dy - polar_dy));
        polar_quantization_errors.push_back(polar_error);
        
        // === 方法2: 直角坐标量化 ===
        auto [quantized_dx, quantized_dy] = QuantizeCoordinateDiff(true_dx, true_dy, max_diff);
        
        // 计算直角坐标量化误差
        double cartesian_error = std::sqrt((true_dx - quantized_dx) * (true_dx - quantized_dx) + 
                                          (true_dy - quantized_dy) * (true_dy - quantized_dy));
        cartesian_quantization_errors.push_back(cartesian_error);
        
        // === 分析极坐标转换精度损失 ===
        // 先转换为极坐标再转换回来（无量化）
        double converted_dx = true_motion.velocity * std::cos(true_motion.theta);
        double converted_dy = true_motion.velocity * std::sin(true_motion.theta);
        
        double conversion_error = std::sqrt((true_dx - converted_dx) * (true_dx - converted_dx) + 
                                           (true_dy - converted_dy) * (true_dy - converted_dy));
        polar_conversion_errors.push_back(conversion_error);
        
        // === 分析各个分量的量化误差 ===
        double velocity_error = std::abs(true_motion.velocity - quantized_motion.velocity);
        double theta_error = std::abs(true_motion.theta - quantized_motion.theta);
        
        velocity_errors.push_back(velocity_error);
        theta_errors.push_back(theta_error);
    }
    
    // 计算统计结果
    auto calculate_stats = [](const std::vector<double>& data) {
        if (data.empty()) return std::make_tuple(0.0, 0.0, 0.0, 0.0, 0.0);
        
        std::vector<double> sorted_data = data;
        std::sort(sorted_data.begin(), sorted_data.end());
        
        double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
        double median = sorted_data[sorted_data.size() / 2];
        double p95 = sorted_data[static_cast<size_t>(0.95 * sorted_data.size())];
        double max_val = sorted_data.back();
        
        return std::make_tuple(mean, median, p95, max_val, static_cast<double>(sorted_data.size()));
    };
    
    auto [polar_mean, polar_median, polar_p95, polar_max, polar_count] = calculate_stats(polar_quantization_errors);
    auto [cart_mean, cart_median, cart_p95, cart_max, cart_count] = calculate_stats(cartesian_quantization_errors);
    auto [conv_mean, conv_median, conv_p95, conv_max, conv_count] = calculate_stats(polar_conversion_errors);
    auto [vel_mean, vel_median, vel_p95, vel_max, vel_count] = calculate_stats(velocity_errors);
    auto [theta_mean, theta_median, theta_p95, theta_max, theta_count] = calculate_stats(theta_errors);
    
    // 打印结果
    std::cout << "\n=== 量化误差对比分析 ===" << std::endl;
    
    std::cout << "\n📊 极坐标量化误差统计:" << std::endl;
    std::cout << "  样本数: " << static_cast<int>(polar_count) << std::endl;
    std::cout << "  平均误差: " << std::scientific << std::setprecision(3) << polar_mean << " 度" << std::endl;
    std::cout << "  中位数误差: " << polar_median << " 度" << std::endl;
    std::cout << "  95%分位数: " << polar_p95 << " 度" << std::endl;
    std::cout << "  最大误差: " << polar_max << " 度" << std::endl;
    
    std::cout << "\n📊 直角坐标量化误差统计:" << std::endl;
    std::cout << "  样本数: " << static_cast<int>(cart_count) << std::endl;
    std::cout << "  平均误差: " << cart_mean << " 度" << std::endl;
    std::cout << "  中位数误差: " << cart_median << " 度" << std::endl;
    std::cout << "  95%分位数: " << cart_p95 << " 度" << std::endl;
    std::cout << "  最大误差: " << cart_max << " 度" << std::endl;
    
    std::cout << "\n📊 极坐标转换精度损失统计 (无量化):" << std::endl;
    std::cout << "  样本数: " << static_cast<int>(conv_count) << std::endl;
    std::cout << "  平均损失: " << conv_mean << " 度" << std::endl;
    std::cout << "  中位数损失: " << conv_median << " 度" << std::endl;
    std::cout << "  95%分位数: " << conv_p95 << " 度" << std::endl;
    std::cout << "  最大损失: " << conv_max << " 度" << std::endl;
    
    // 误差放大分析
    std::cout << "\n=== 误差放大效应分析 ===" << std::endl;
    
    double error_amplification = polar_mean / cart_mean;
    std::cout << "误差放大倍数 (极坐标/直角坐标): " << std::fixed << std::setprecision(2) << error_amplification << "x" << std::endl;
    
    if (error_amplification > 2.0) {
        std::cout << "🚨 极坐标量化误差被显著放大！" << std::endl;
    } else if (error_amplification > 1.5) {
        std::cout << "⚠️ 极坐标量化误差有一定放大" << std::endl;
    } else if (error_amplification > 1.1) {
        std::cout << "📈 极坐标量化误差略有放大" << std::endl;
    } else {
        std::cout << "✅ 极坐标量化误差无明显放大" << std::endl;
    }
    
    // 转换精度损失分析
    double conversion_impact = conv_mean / cart_mean;
    std::cout << "转换精度损失影响 (转换损失/直角坐标误差): " << conversion_impact << "x" << std::endl;
    
    if (conversion_impact > 0.1) {
        std::cout << "🚨 极坐标转换存在显著精度损失！" << std::endl;
    } else if (conversion_impact > 0.01) {
        std::cout << "⚠️ 极坐标转换有一定精度损失" << std::endl;
    } else {
        std::cout << "✅ 极坐标转换精度损失很小" << std::endl;
    }
    
    // 分量误差分析
    std::cout << "\n=== 极坐标分量量化误差分析 ===" << std::endl;
    
    std::cout << "📊 速度量化误差:" << std::endl;
    std::cout << "  平均误差: " << std::scientific << vel_mean << " 度/步" << std::endl;
    std::cout << "  最大误差: " << vel_max << " 度/步" << std::endl;
    std::cout << "  量化步长: " << epsilon_v << " 度/步" << std::endl;
    std::cout << "  相对误差: " << std::fixed << (vel_mean / epsilon_v * 100) << "% of εv" << std::endl;
    
    std::cout << "\n📊 角度量化误差:" << std::endl;
    std::cout << "  平均误差: " << std::scientific << theta_mean << " 弧度" << std::endl;
    std::cout << "  最大误差: " << theta_max << " 弧度" << std::endl;
    std::cout << "  量化步长: " << epsilon_theta << " 弧度" << std::endl;
    std::cout << "  相对误差: " << std::fixed << (theta_mean / epsilon_theta * 100) << "% of εθ" << std::endl;
    
    // 分析哪个分量贡献更大的误差
    std::cout << "\n=== 误差贡献分析 ===" << std::endl;
    
    // 计算速度误差和角度误差对最终坐标误差的贡献
    double avg_velocity = 0.0;
    double avg_theta = 0.0;
    int valid_count = 0;
    
    for (int i = 1; i < std::min(1000, static_cast<int>(points.size())); ++i) {
        MotionVector motion = CalculateMotionVector(points[i-1], points[i]);
        if (motion.velocity > 1e-10) {  // 避免除零
            avg_velocity += motion.velocity;
            avg_theta += std::abs(motion.theta);
            valid_count++;
        }
    }
    
    if (valid_count > 0) {
        avg_velocity /= valid_count;
        avg_theta /= valid_count;
        
        // 估算各分量对坐标误差的贡献
        double velocity_contribution = epsilon_v;  // 速度误差直接转换为坐标误差
        double theta_contribution = avg_velocity * epsilon_theta;  // 角度误差乘以平均速度
        
        std::cout << "平均运动速度: " << std::scientific << avg_velocity << " 度/步" << std::endl;
        std::cout << "平均角度: " << avg_theta << " 弧度" << std::endl;
        std::cout << "速度量化对坐标误差贡献: ~" << velocity_contribution << " 度" << std::endl;
        std::cout << "角度量化对坐标误差贡献: ~" << theta_contribution << " 度" << std::endl;
        
        if (theta_contribution > velocity_contribution) {
            std::cout << "🎯 角度量化误差是主要误差源！" << std::endl;
            std::cout << "建议减小εθ以提高预测准确率" << std::endl;
        } else {
            std::cout << "🎯 速度量化误差是主要误差源！" << std::endl;
            std::cout << "建议减小εv以提高预测准确率" << std::endl;
        }
    }
    
    std::cout << "\n=== 结论 ===" << std::endl;
    
    std::cout << "1. 极坐标量化误差放大倍数: " << error_amplification << "x" << std::endl;
    std::cout << "2. 极坐标转换精度损失: " << std::scientific << conv_mean << " 度" << std::endl;
    std::cout << "3. 这解释了为什么运动矢量预测准确率低于直角坐标预测" << std::endl;
    
    if (error_amplification > 1.5 || conversion_impact > 0.01) {
        std::cout << "4. 🚨 极坐标方法确实存在精度劣势！" << std::endl;
        std::cout << "5. 建议考虑直接在直角坐标系进行预测和量化" << std::endl;
    } else {
        std::cout << "4. ✅ 极坐标方法精度损失在可接受范围内" << std::endl;
        std::cout << "5. 预测准确率差异可能有其他原因" << std::endl;
    }
    
    return 0;
}
