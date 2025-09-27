#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>

// 模拟轨迹压缩器接口（简化版演示）
class SimpleTrajectoryDemo {
public:
    // 模拟原始Serf-QT算法
    static double CompressWithOriginal(const std::vector<double>& data) {
        double total_residual = 0.0;
        double prev_value = data[0];
        
        for (size_t i = 1; i < data.size(); i++) {
            double residual = std::abs(data[i] - prev_value);
            total_residual += residual;
            prev_value = data[i];
        }
        
        return total_residual / data.size();
    }
    
    // 模拟线性预测优化
    static double CompressWithLinear(const std::vector<double>& data) {
        if (data.size() < 3) return CompressWithOriginal(data);
        
        double total_residual = 0.0;
        
        for (size_t i = 2; i < data.size(); i++) {
            // 线性外推预测
            double predicted = 2 * data[i-1] - data[i-2];
            double residual = std::abs(data[i] - predicted);
            total_residual += residual;
        }
        
        return total_residual / (data.size() - 2);
    }
    
    // 模拟二次预测优化
    static double CompressWithQuadratic(const std::vector<double>& data) {
        if (data.size() < 4) return CompressWithLinear(data);
        
        double total_residual = 0.0;
        
        for (size_t i = 3; i < data.size(); i++) {
            // 简化的二次预测（使用三点拟合抛物线）
            double x1 = i-3, y1 = data[i-3];
            double x2 = i-2, y2 = data[i-2];
            double x3 = i-1, y3 = data[i-1];
            double x = i;
            
            // 拉格朗日插值预测
            double predicted = y1 * (x-x2)*(x-x3)/((x1-x2)*(x1-x3)) +
                              y2 * (x-x1)*(x-x3)/((x2-x1)*(x2-x3)) +
                              y3 * (x-x1)*(x-x2)/((x3-x1)*(x3-x2));
            
            double residual = std::abs(data[i] - predicted);
            total_residual += residual;
        }
        
        return total_residual / (data.size() - 3);
    }
    
    // 生成模拟轨迹数据
    static std::vector<double> GenerateTrajectoryData(int count, const std::string& pattern) {
        std::vector<double> data;
        data.reserve(count);
        
        if (pattern == "linear") {
            // 线性轨迹（匀速直线）
            double start = 39.9;
            double step = 0.001;
            for (int i = 0; i < count; i++) {
                data.push_back(start + i * step);
            }
        } else if (pattern == "curve") {
            // 曲线轨迹（转弯）
            double center = 39.9;
            double amplitude = 0.01;
            for (int i = 0; i < count; i++) {
                double angle = i * 0.1;
                data.push_back(center + amplitude * std::sin(angle));
            }
        } else if (pattern == "acceleration") {
            // 加速轨迹
            double start = 39.9;
            for (int i = 0; i < count; i++) {
                double t = i * 0.1;
                data.push_back(start + 0.001 * t + 0.0001 * t * t);
            }
        }
        
        return data;
    }
};

int main() {
    std::cout << "🚗 轨迹数据压缩算法演示" << std::endl;
    std::cout << "================================" << std::endl;
    
    std::vector<std::string> patterns = {"linear", "curve", "acceleration"};
    std::vector<std::string> pattern_names = {"直线轨迹", "转弯轨迹", "加速轨迹"};
    
    for (size_t p = 0; p < patterns.size(); p++) {
        std::cout << "\n📍 测试场景: " << pattern_names[p] << std::endl;
        std::cout << "--------------------------------" << std::endl;
        
        auto data = SimpleTrajectoryDemo::GenerateTrajectoryData(100, patterns[p]);
        
        // 计算不同算法的平均预测误差
        double original_error = SimpleTrajectoryDemo::CompressWithOriginal(data);
        double linear_error = SimpleTrajectoryDemo::CompressWithLinear(data);
        double quadratic_error = SimpleTrajectoryDemo::CompressWithQuadratic(data);
        
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "原始算法平均误差:   " << original_error << std::endl;
        std::cout << "线性预测平均误差:   " << linear_error << std::endl;
        std::cout << "二次预测平均误差:   " << quadratic_error << std::endl;
        
        std::cout << std::setprecision(1);
        std::cout << "\n📊 改进效果:" << std::endl;
        std::cout << "线性预测改进: " << (1 - linear_error/original_error) * 100 << "%" << std::endl;
        std::cout << "二次预测改进: " << (1 - quadratic_error/original_error) * 100 << "%" << std::endl;
        
        // 估算压缩比改进（简化计算）
        double compression_improvement = (original_error / quadratic_error - 1) * 100;
        std::cout << "预估压缩比提升: " << compression_improvement << "%" << std::endl;
    }
    
    std::cout << "\n🎯 总结" << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "✓ 多项式预测显著减少了预测误差" << std::endl;
    std::cout << "✓ 不同轨迹模式需要不同的预测策略" << std::endl;
    std::cout << "✓ 二次预测在大多数情况下效果最佳" << std::endl;
    std::cout << "✓ 实际压缩比改进取决于数据特性" << std::endl;
    
    std::cout << "\n📚 完整实现请参考:" << std::endl;
    std::cout << "- src/compressor/serf_qt_trajectory_compressor.*" << std::endl;
    std::cout << "- src/decompressor/serf_qt_trajectory_decompressor.*" << std::endl;
    std::cout << "- test/trajectory_compression_test.cc" << std::endl;
    
    return 0;
}
