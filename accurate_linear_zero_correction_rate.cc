#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "decompressor/serf_qt_gps_configurable_decompressor.h"

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

// 线性预测零校正率计算器
class LinearZeroCorrectionCalculator {
private:
    std::vector<double> longitudes_;
    std::vector<double> latitudes_;
    double max_diff_;
    int predictions_;
    int zero_corrections_;
    
public:
    LinearZeroCorrectionCalculator(double max_diff) : max_diff_(max_diff), predictions_(0), zero_corrections_(0) {}
    
    void AddPoint(const SerfQtGpsConfigurableCompressor::GpsPoint& point) {
        if (longitudes_.size() < 2) {
            // 前两个点，直接添加
            longitudes_.push_back(point.longitude);
            latitudes_.push_back(point.latitude);
            return;
        }
        
        // 进行线性预测
        predictions_++;
        
        double predicted_lon = 2.0 * longitudes_.back() - longitudes_[longitudes_.size()-2];
        double predicted_lat = 2.0 * latitudes_.back() - latitudes_[latitudes_.size()-2];
        
        // 计算预测误差
        double error_lon = point.longitude - predicted_lon;
        double error_lat = point.latitude - predicted_lat;
        
        // 检查是否在误差界限内（这就是零校正的条件）
        bool accurate_lon = std::abs(error_lon) <= max_diff_;
        bool accurate_lat = std::abs(error_lat) <= max_diff_;
        
        if (accurate_lon && accurate_lat) {
            // 预测准确，量化为0，这就是零校正
            zero_corrections_++;
        }
        
        // 更新状态（模拟压缩器的量化重构过程）
        long q_lon = static_cast<long>(std::round(error_lon / (2 * max_diff_)));
        long q_lat = static_cast<long>(std::round(error_lat / (2 * max_diff_)));
        
        double reconstructed_lon = predicted_lon + 2 * max_diff_ * static_cast<double>(q_lon);
        double reconstructed_lat = predicted_lat + 2 * max_diff_ * static_cast<double>(q_lat);
        
        longitudes_.push_back(reconstructed_lon);
        latitudes_.push_back(reconstructed_lat);
    }
    
    double GetZeroCorrectionRate() const {
        return (predictions_ > 0) ? static_cast<double>(zero_corrections_) / predictions_ * 100.0 : 0.0;
    }
    
    int GetPredictions() const { return predictions_; }
    int GetZeroCorrections() const { return zero_corrections_; }
};

int main() {
    std::cout << "=== 准确计算线性预测的零校正率 ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const int test_count = 50000;  // 使用5万条数据
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 设置测试参数
    const double gps_error_bound = 1.4e-5;
    const double linear_max_diff = 1e-5;
    const double epsilon_v = 2e-5;
    const double epsilon_theta = 1e-5;
    
    std::cout << "\n📏 测试参数:" << std::endl;
    std::cout << "  GPS轨迹预测误差界限: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "  线性预测误差界限: " << linear_max_diff << " 度" << std::endl;
    std::cout << "  GPS量化参数: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    std::cout << "  测试数据点数: " << points.size() << std::endl;
    
    std::cout << "\n=== GPS轨迹预测零校正率（实际测量）===" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 测试GPS轨迹压缩器
    SerfQtGpsConfigurableCompressor gps_compressor(points.size(), gps_error_bound, epsilon_v, epsilon_theta);
    
    for (const auto& point : points) {
        gps_compressor.AddGpsPoint(point);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto gps_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    Array<uint8_t> gps_compressed_data = gps_compressor.GetCompressedData();
    const auto& gps_stats = gps_compressor.GetStrategyStats();
    
    // 计算GPS压缩器结果
    double gps_compression_ratio = static_cast<double>(points.size() * 16) / gps_compressed_data.length();
    double gps_avg_bits = static_cast<double>(gps_compressed_data.length() * 8) / points.size();
    double gps_zero_rate = static_cast<double>(gps_stats.zero_corr_count) / gps_stats.GetTotalPoints() * 100.0;
    
    std::cout << "📊 GPS轨迹预测结果:" << std::endl;
    std::cout << "  零校正次数: " << gps_stats.zero_corr_count << std::endl;
    std::cout << "  总预测次数: " << gps_stats.GetTotalPoints() << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << gps_zero_rate << "%" << std::endl;
    std::cout << "  压缩比: " << gps_compression_ratio << ":1" << std::endl;
    std::cout << "  平均成本: " << gps_avg_bits << " bits/点" << std::endl;
    std::cout << "  压缩时间: " << gps_duration.count() << " 毫秒" << std::endl;
    
    std::cout << "\n=== 线性预测零校正率（准确计算）===" << std::endl;
    
    start_time = std::chrono::high_resolution_clock::now();
    
    // 计算线性预测的真实零校正率
    LinearZeroCorrectionCalculator linear_calculator(linear_max_diff);
    
    for (const auto& point : points) {
        linear_calculator.AddPoint(point);
    }
    
    end_time = std::chrono::high_resolution_clock::now();
    auto linear_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    double linear_zero_rate = linear_calculator.GetZeroCorrectionRate();
    
    std::cout << "📊 线性预测结果:" << std::endl;
    std::cout << "  零校正次数: " << linear_calculator.GetZeroCorrections() << std::endl;
    std::cout << "  总预测次数: " << linear_calculator.GetPredictions() << std::endl;
    std::cout << "  零校正率: " << std::fixed << std::setprecision(2) << linear_zero_rate << "%" << std::endl;
    std::cout << "  计算时间: " << linear_duration.count() << " 毫秒" << std::endl;
    
    std::cout << "\n=== 准确的零校正率对比 ===" << std::endl;
    
    double zero_rate_gap = gps_zero_rate - linear_zero_rate;
    
    std::cout << "🎯 零校正率对比:" << std::endl;
    std::cout << "  GPS轨迹预测: " << std::setprecision(2) << gps_zero_rate << "%" << std::endl;
    std::cout << "  线性预测: " << linear_zero_rate << "%" << std::endl;
    std::cout << "  差距: " << std::showpos << zero_rate_gap << "%" << std::noshowpos << std::endl;
    
    if (std::abs(zero_rate_gap) < 2.0) {
        std::cout << "  ⚖️ 两种预测方法的零校正率基本相同" << std::endl;
        std::cout << "  💡 这完全验证了您的观点：预测本质相同，差异在编码方式" << std::endl;
    } else if (gps_zero_rate > linear_zero_rate) {
        std::cout << "  📈 GPS轨迹预测的零校正率更高" << std::endl;
        std::cout << "  🤔 但压缩比仍然较低，问题确实在编码开销" << std::endl;
    } else {
        std::cout << "  📉 GPS轨迹预测的零校正率较低" << std::endl;
        std::cout << "  🔍 可能的原因分析:" << std::endl;
        std::cout << "    1. 误差界限设置不同 (1.4e-5 vs 1e-5)" << std::endl;
        std::cout << "    2. 2D欧几里得距离 vs 1D独立误差的判断标准不同" << std::endl;
        std::cout << "    3. 量化精度的细微差异" << std::endl;
    }
    
    std::cout << "\n=== 详细分析 ===" << std::endl;
    
    std::cout << "💡 关键发现:" << std::endl;
    std::cout << "1. GPS轨迹预测零校正率: " << gps_zero_rate << "%" << std::endl;
    std::cout << "2. 线性预测零校正率: " << linear_zero_rate << "%" << std::endl;
    std::cout << "3. 之前的估算(" << 56.09 << "%)完全错误，基于压缩比估算是不准确的" << std::endl;
    std::cout << "4. 真实的零校正率对比才能反映预测算法的本质差异" << std::endl;
    
    // 误差界限标准化对比
    std::cout << "\n🔍 误差界限标准化分析:" << std::endl;
    std::cout << "GPS轨迹预测使用2D欧几里得距离: " << std::scientific << gps_error_bound << " 度" << std::endl;
    std::cout << "线性预测使用1D独立误差: " << linear_max_diff << " 度" << std::endl;
    
    // 计算等效的2D误差界限
    double linear_equivalent_2d = linear_max_diff * std::sqrt(2.0);
    std::cout << "线性预测等效2D误差界限: " << linear_equivalent_2d << " 度" << std::endl;
    
    double bound_ratio = gps_error_bound / linear_equivalent_2d;
    std::cout << "误差界限比值: " << std::fixed << std::setprecision(3) << bound_ratio << std::endl;
    
    if (std::abs(bound_ratio - 1.0) < 0.1) {
        std::cout << "✅ 误差界限基本等效，零校正率对比有意义" << std::endl;
    } else {
        std::cout << "⚠️ 误差界限不等效，需要考虑这个因素" << std::endl;
    }
    
    std::cout << "\n=== 最终结论 ===" << std::endl;
    
    std::cout << "🎯 **您的观点验证结果**:" << std::endl;
    std::cout << "1. ✅ 正确指出了我的估算方法错误" << std::endl;
    std::cout << "2. ✅ 零校正率确实应该基于预测准确性计算" << std::endl;
    std::cout << "3. 📊 真实零校正率对比: " << gps_zero_rate << "% vs " << linear_zero_rate << "%" << std::endl;
    
    if (std::abs(zero_rate_gap) < 5.0) {
        std::cout << "4. ✅ **完全验证了您的核心观点**：预测本质相同，差异在量化和编码方式" << std::endl;
        std::cout << "5. 🎯 GPS轨迹压缩器性能差的主要原因是编码开销，不是预测准确性" << std::endl;
    } else {
        std::cout << "4. 📊 零校正率存在差异，需要进一步分析原因" << std::endl;
        std::cout << "5. 🔍 可能是误差界限标准或量化方式的影响" << std::endl;
    }
    
    return 0;
}
