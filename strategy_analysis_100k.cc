#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <map>
#include <chrono>

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "decompressor/serf_qt_gps_configurable_decompressor.h"

// 策略统计结构
struct StrategyStats {
    int zero_corr_count = 0;
    int v_only_count = 0;
    int theta_only_count = 0;
    int both_count = 0;
    int total_points = 0;
    
    // 编码成本统计
    int total_bits = 0;
    int strategy_bits = 0;
    int quantization_bits = 0;
    
    void AddStrategy(int strategy, int bits_used) {
        total_points++;
        total_bits += bits_used;
        
        switch (strategy) {
            case 0: // FLAG_ZERO_CORR
                zero_corr_count++;
                strategy_bits += 1; // 1 bit for "0"
                break;
            case 1: // FLAG_V_ONLY
                v_only_count++;
                strategy_bits += 2; // 2 bits for "10"
                quantization_bits += (bits_used - 2);
                break;
            case 2: // FLAG_THETA_ONLY
                theta_only_count++;
                strategy_bits += 3; // 3 bits for "110"
                quantization_bits += (bits_used - 3);
                break;
            case 3: // FLAG_BOTH
                both_count++;
                strategy_bits += 3; // 3 bits for "111"
                quantization_bits += (bits_used - 3);
                break;
        }
    }
    
    void PrintStats() const {
        std::cout << "\n=== 策略分布统计 ===" << std::endl;
        std::cout << "总点数: " << total_points << std::endl;
        
        std::cout << "\n策略使用分布:" << std::endl;
        std::cout << "  FLAG_ZERO_CORR (零校正):   " << std::setw(8) << zero_corr_count 
                  << " (" << std::fixed << std::setprecision(2) 
                  << (100.0 * zero_corr_count / total_points) << "%)" << std::endl;
        std::cout << "  FLAG_V_ONLY (速度校正):    " << std::setw(8) << v_only_count 
                  << " (" << (100.0 * v_only_count / total_points) << "%)" << std::endl;
        std::cout << "  FLAG_THETA_ONLY (角度校正): " << std::setw(8) << theta_only_count 
                  << " (" << (100.0 * theta_only_count / total_points) << "%)" << std::endl;
        std::cout << "  FLAG_BOTH (完全校正):      " << std::setw(8) << both_count 
                  << " (" << (100.0 * both_count / total_points) << "%)" << std::endl;
        
        std::cout << "\n编码成本分析:" << std::endl;
        std::cout << "  总编码比特数: " << total_bits << " bits" << std::endl;
        std::cout << "  策略标志比特: " << strategy_bits << " bits (" 
                  << (100.0 * strategy_bits / total_bits) << "%)" << std::endl;
        std::cout << "  量化数据比特: " << quantization_bits << " bits (" 
                  << (100.0 * quantization_bits / total_bits) << "%)" << std::endl;
        
        std::cout << "\n平均每点编码成本:" << std::endl;
        std::cout << "  平均总成本: " << std::setprecision(2) << (static_cast<double>(total_bits) / total_points) << " bits/点" << std::endl;
        std::cout << "  平均策略成本: " << (static_cast<double>(strategy_bits) / total_points) << " bits/点" << std::endl;
        std::cout << "  平均量化成本: " << (static_cast<double>(quantization_bits) / total_points) << " bits/点" << std::endl;
        
        // 分析压缩效率
        int original_bits = total_points * 128; // 每个点16字节 = 128比特
        double compression_ratio = static_cast<double>(original_bits) / total_bits;
        
        std::cout << "\n压缩效率分析:" << std::endl;
        std::cout << "  原始数据: " << original_bits << " bits" << std::endl;
        std::cout << "  压缩数据: " << total_bits << " bits" << std::endl;
        std::cout << "  压缩比: " << std::setprecision(2) << compression_ratio << ":1" << std::endl;
        std::cout << "  空间节省: " << (100.0 * (1.0 - static_cast<double>(total_bits) / original_bits)) << "%" << std::endl;
    }
};

// 带统计功能的可配置压缩器
class AnalyzingCompressor : public SerfQtGpsConfigurableCompressor {
public:
    AnalyzingCompressor(int block_size, double e_max, double epsilon_v, double epsilon_theta)
        : SerfQtGpsConfigurableCompressor(block_size, e_max, epsilon_v, epsilon_theta) {}
    
    StrategyStats GetStrategyStats() const { return stats_; }
    
    // 重写编码方法以收集统计信息
    void EncodeStrategyWithStats(CorrectionFlag strategy, int64_t qv, int64_t qtheta) {
        int bits_before = GetCompressedSizeInBits();
        EncodeStrategy(strategy, qv, qtheta);
        int bits_after = GetCompressedSizeInBits();
        int bits_used = bits_after - bits_before;
        
        stats_.AddStrategy(static_cast<int>(strategy), bits_used);
    }
    
protected:
    // 重写父类方法以添加统计
    void EncodeStrategy(CorrectionFlag strategy, int64_t qv, int64_t qtheta) override {
        int bits_before = compressed_size_in_bits_;
        
        // 调用父类方法
        SerfQtGpsConfigurableCompressor::EncodeStrategy(strategy, qv, qtheta);
        
        int bits_after = compressed_size_in_bits_;
        int bits_used = bits_after - bits_before;
        
        stats_.AddStrategy(static_cast<int>(strategy), bits_used);
    }
    
private:
    StrategyStats stats_;
};

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
                continue; // 跳过无效数据
            }
        }
    }
    
    return points;
}

int main() {
    std::cout << "=== GPS轨迹压缩策略分布分析 (10万个点) ===" << std::endl;
    
    const std::string data_file = "test/data_set/Geolife_100k_longitude_latitude.csv";
    const double error_bound = 1e-5;  // 1e-5度的严格误差界限
    const int test_count = 99999;     // 测试全部可用点数
    
    // 使用最佳参数配置进行分析
    const double epsilon_v = 5e-6;    // 角度精确配置
    const double epsilon_theta = 1e-4;
    
    std::cout << "数据文件: " << data_file << std::endl;
    std::cout << "测试点数: " << test_count << std::endl;
    std::cout << "误差界限: " << std::scientific << error_bound << " 度" << std::endl;
    std::cout << "参数配置: εv=" << epsilon_v << ", εθ=" << epsilon_theta << std::endl;
    
    // 读取GPS数据
    std::cout << "\n🔄 读取GPS数据..." << std::endl;
    auto points = ReadGpsPoints(data_file, test_count);
    
    if (points.size() < test_count) {
        std::cerr << "❌ 无法读取足够的测试数据，实际读取: " << points.size() << " 个点" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 成功读取 " << points.size() << " 个GPS点" << std::endl;
    
    // 使用带统计功能的压缩器
    std::cout << "\n🔄 开始压缩并统计策略分布..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    AnalyzingCompressor compressor(points.size(), error_bound, epsilon_v, epsilon_theta);
    
    for (size_t i = 0; i < points.size(); ++i) {
        if (i % 10000 == 0) {
            std::cout << "\r进度: " << i << "/" << points.size() 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (100.0 * i / points.size()) << "%)" << std::flush;
        }
        compressor.AddGpsPoint(points[i]);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\r✅ 压缩完成！处理时间: " << duration.count() << " ms" << std::endl;
    
    // 获取并打印统计信息
    StrategyStats stats = compressor.GetStrategyStats();
    stats.PrintStats();
    
    // 分析策略效率
    std::cout << "\n=== 策略效率分析 ===" << std::endl;
    
    double zero_corr_ratio = static_cast<double>(stats.zero_corr_count) / stats.total_points;
    double correction_ratio = 1.0 - zero_corr_ratio;
    
    std::cout << "零校正比例: " << std::fixed << std::setprecision(2) << (zero_corr_ratio * 100) << "%" << std::endl;
    std::cout << "需要校正比例: " << (correction_ratio * 100) << "%" << std::endl;
    
    if (correction_ratio > 0.5) {
        std::cout << "\n💡 优化建议:" << std::endl;
        std::cout << "  - 校正比例较高 (" << (correction_ratio * 100) << "%)，可能需要:" << std::endl;
        std::cout << "    1. 改进预测算法，提高预测准确性" << std::endl;
        std::cout << "    2. 调整量化步长，平衡精度与压缩比" << std::endl;
        std::cout << "    3. 考虑更复杂的预测模型（如二阶预测）" << std::endl;
    } else {
        std::cout << "\n✅ 预测效果良好，零校正比例达到 " << (zero_corr_ratio * 100) << "%" << std::endl;
    }
    
    // 分析各策略的平均成本
    if (stats.v_only_count > 0 || stats.theta_only_count > 0 || stats.both_count > 0) {
        std::cout << "\n=== 各策略平均成本分析 ===" << std::endl;
        
        // 估算各策略的平均比特成本
        double avg_v_only_cost = stats.v_only_count > 0 ? 
            (2.0 + static_cast<double>(stats.quantization_bits) * stats.v_only_count / (stats.v_only_count + stats.theta_only_count + stats.both_count)) : 0;
        double avg_theta_only_cost = stats.theta_only_count > 0 ? 
            (3.0 + static_cast<double>(stats.quantization_bits) * stats.theta_only_count / (stats.v_only_count + stats.theta_only_count + stats.both_count)) : 0;
        double avg_both_cost = stats.both_count > 0 ? 
            (3.0 + static_cast<double>(stats.quantization_bits) * stats.both_count / (stats.v_only_count + stats.theta_only_count + stats.both_count)) : 0;
        
        std::cout << "  零校正平均成本: 1.0 bits/点" << std::endl;
        if (stats.v_only_count > 0)
            std::cout << "  速度校正平均成本: " << std::setprecision(1) << avg_v_only_cost << " bits/点" << std::endl;
        if (stats.theta_only_count > 0)
            std::cout << "  角度校正平均成本: " << avg_theta_only_cost << " bits/点" << std::endl;
        if (stats.both_count > 0)
            std::cout << "  完全校正平均成本: " << avg_both_cost << " bits/点" << std::endl;
    }
    
    return 0;
}
