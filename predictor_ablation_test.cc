/**
 * 预测器消融实验
 * 
 * 测试单一预测器 vs 多预测器融合的效果
 * 对比指标：
 * 1. 量化数据大小（不包含预测器标志位）
 * 2. 总压缩大小（包含预测器标志位）
 * 3. 压缩比
 * 4. 预测精度
 */

#include "src/compressor/trajcompress_sp_compressor.h"
#include "src/utils/output_bit_stream.h"
#include "src/utils/elias_gamma_codec.h"
#include "src/utils/zig_zag_codec.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>

using GpsPoint = TrajCompressSPCompressor::GpsPoint;

// 从CSV文件读取GPS数据
std::vector<GpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return points;
    }
    
    std::string line;
    int count = 0;
    
    while (std::getline(file, line) && (max_points < 0 || count < max_points)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                double longitude = std::stod(lon_str);
                double latitude = std::stod(lat_str);
                points.emplace_back(longitude, latitude);
                count++;
            } catch (const std::exception& e) {
                // 跳过错误行
            }
        }
    }
    
    file.close();
    return points;
}

// 单一预测器压缩测试
class SinglePredictorCompressor {
public:
    enum PredictorType {
        PREDICTOR_LDR = 0,  // 线性预测
        PREDICTOR_CP = 1,   // 曲线预测
        PREDICTOR_ZP = 2    // 零预测（前值）
    };
    
    SinglePredictorCompressor(PredictorType type, double epsilon)
        : predictor_type_(type), epsilon_(epsilon) {
        output_stream_ = std::make_unique<OutputBitStream>(1000000);
    }
    
    void Compress(const std::vector<GpsPoint>& points) {
        if (points.empty()) return;
        
        // 写入第一个点
        compressed_bits_ += output_stream_->WriteLong(Double::DoubleToLongBits(points[0].longitude), 64);
        compressed_bits_ += output_stream_->WriteLong(Double::DoubleToLongBits(points[0].latitude), 64);
        
        // 初始化历史
        current_point_ = points[0];
        if (points.size() > 1) {
            prev_point_ = points[0];
        }
        if (points.size() > 2) {
            prev_prev_point_ = points[0];
        }
        
        // 压缩后续点
        for (size_t i = 1; i < points.size(); i++) {
            CompressPoint(points[i]);
        }
        
        total_points_ = points.size();
    }
    
    int GetCompressedBits() const { return compressed_bits_; }
    int GetQuantizationBits() const { return quantization_bits_; }
    int GetTotalPoints() const { return total_points_; }
    
    double GetAvgPredictionError() const {
        return total_points_ > 1 ? total_prediction_error_ / (total_points_ - 1) : 0;
    }
    
private:
    void CompressPoint(const GpsPoint& point) {
        // 预测
        GpsPoint predicted = Predict();
        
        // 计算预测误差
        double dx = point.longitude - predicted.longitude;
        double dy = point.latitude - predicted.latitude;
        double pred_error = std::sqrt(dx * dx + dy * dy);
        total_prediction_error_ += pred_error;
        
        // 计算残差
        GpsPoint delta(point.longitude - predicted.longitude,
                      point.latitude - predicted.latitude);
        
        // 量化
        int64_t quantized_lon = static_cast<int64_t>(std::round(delta.longitude / epsilon_));
        int64_t quantized_lat = static_cast<int64_t>(std::round(delta.latitude / epsilon_));
        
        // 编码（ZigZag + Elias Gamma）
        uint64_t zigzag_lon = ZigZagCodec::Encode(quantized_lon);
        uint64_t zigzag_lat = ZigZagCodec::Encode(quantized_lat);
        
        int bits_lon = EliasGammaCodec::Encode(zigzag_lon + 1, output_stream_.get());
        int bits_lat = EliasGammaCodec::Encode(zigzag_lat + 1, output_stream_.get());
        
        compressed_bits_ += bits_lon + bits_lat;
        quantization_bits_ += bits_lon + bits_lat;
        
        // 重构点（用于下一次预测）
        GpsPoint reconstructed(
            predicted.longitude + quantized_lon * epsilon_,
            predicted.latitude + quantized_lat * epsilon_
        );
        
        // 更新历史
        prev_prev_point_ = prev_point_;
        prev_point_ = current_point_;
        current_point_ = reconstructed;
    }
    
    GpsPoint Predict() const {
        switch (predictor_type_) {
            case PREDICTOR_LDR: {
                // 线性预测：P_i = P_{i-1} + (P_{i-1} - P_{i-2})
                GpsPoint velocity(
                    current_point_.longitude - prev_point_.longitude,
                    current_point_.latitude - prev_point_.latitude
                );
                return GpsPoint(
                    current_point_.longitude + velocity.longitude,
                    current_point_.latitude + velocity.latitude
                );
            }
            case PREDICTOR_CP: {
                // 曲线预测：P_i = P_{i-1} + velocity + acceleration
                GpsPoint velocity(
                    current_point_.longitude - prev_point_.longitude,
                    current_point_.latitude - prev_point_.latitude
                );
                GpsPoint prev_velocity(
                    prev_point_.longitude - prev_prev_point_.longitude,
                    prev_point_.latitude - prev_prev_point_.latitude
                );
                GpsPoint acceleration(
                    velocity.longitude - prev_velocity.longitude,
                    velocity.latitude - prev_velocity.latitude
                );
                return GpsPoint(
                    current_point_.longitude + velocity.longitude + acceleration.longitude,
                    current_point_.latitude + velocity.latitude + acceleration.latitude
                );
            }
            case PREDICTOR_ZP:
            default:
                // 零预测（前值）：P_i = P_{i-1}
                return current_point_;
        }
    }
    
    PredictorType predictor_type_;
    double epsilon_;
    std::unique_ptr<OutputBitStream> output_stream_;
    
    GpsPoint current_point_;
    GpsPoint prev_point_;
    GpsPoint prev_prev_point_;
    
    int compressed_bits_ = 0;
    int quantization_bits_ = 0;
    int total_points_ = 0;
    double total_prediction_error_ = 0;
};

int main() {
    std::string dataset_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int max_points = 100000;
    double epsilon = 1e-5 * std::sqrt(2);  // 与标准测试一致
    
    std::cout << "============================================================" << std::endl;
    std::cout << "预测器消融实验 - Ablation Study" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "数据集: Geolife 100k" << std::endl;
    std::cout << "测试点数: " << max_points << std::endl;
    std::cout << "误差阈值: " << std::scientific << epsilon << " 度" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    // 加载数据
    auto gps_data = LoadGpsDataFromCSV(dataset_path, max_points);
    if (gps_data.empty()) {
        std::cerr << "无法加载数据" << std::endl;
        return 1;
    }
    
    std::cout << "成功加载 " << gps_data.size() << " 个GPS点\n" << std::endl;
    
    // 测试单一预测器
    std::cout << "=== 单一预测器测试（无预测器标志位开销）===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    struct Result {
        std::string name;
        int quantization_bits;
        int total_bits;
        double avg_bits_per_point;
        double avg_prediction_error;
    };
    
    std::vector<Result> results;
    
    // 测试 LDR
    {
        SinglePredictorCompressor compressor(SinglePredictorCompressor::PREDICTOR_LDR, epsilon);
        compressor.Compress(gps_data);
        
        std::cout << "\n[调试] LDR 预测器统计：" << std::endl;
        std::cout << "  总压缩bits: " << compressor.GetCompressedBits() << std::endl;
        std::cout << "  量化bits: " << compressor.GetQuantizationBits() << std::endl;
        std::cout << "  第一个点开销(128 bits): " << (compressor.GetCompressedBits() - compressor.GetQuantizationBits()) << std::endl;
        std::cout << "  平均预测误差: " << std::scientific << compressor.GetAvgPredictionError() << std::endl;
        
        results.push_back({
            "LDR (线性预测)",
            compressor.GetQuantizationBits(),
            compressor.GetCompressedBits(),
            static_cast<double>(compressor.GetCompressedBits()) / compressor.GetTotalPoints(),
            compressor.GetAvgPredictionError()
        });
    }
    
    // 测试 CP
    {
        SinglePredictorCompressor compressor(SinglePredictorCompressor::PREDICTOR_CP, epsilon);
        compressor.Compress(gps_data);
        
        results.push_back({
            "CP  (曲线预测)",
            compressor.GetQuantizationBits(),
            compressor.GetCompressedBits(),
            static_cast<double>(compressor.GetCompressedBits()) / compressor.GetTotalPoints(),
            compressor.GetAvgPredictionError()
        });
    }
    
    // 测试 ZP
    {
        SinglePredictorCompressor compressor(SinglePredictorCompressor::PREDICTOR_ZP, epsilon);
        compressor.Compress(gps_data);
        
        std::cout << "\n[调试] ZP 预测器统计：" << std::endl;
        std::cout << "  总压缩bits: " << compressor.GetCompressedBits() << std::endl;
        std::cout << "  量化bits: " << compressor.GetQuantizationBits() << std::endl;
        std::cout << "  第一个点开销(128 bits): " << (compressor.GetCompressedBits() - compressor.GetQuantizationBits()) << std::endl;
        std::cout << "  平均预测误差: " << std::scientific << compressor.GetAvgPredictionError() << std::endl;
        
        results.push_back({
            "ZP  (零预测/前值)",
            compressor.GetQuantizationBits(),
            compressor.GetCompressedBits(),
            static_cast<double>(compressor.GetCompressedBits()) / compressor.GetTotalPoints(),
            compressor.GetAvgPredictionError()
        });
    }
    
    // 测试多预测器系统
    {
        TrajCompressSPCompressor compressor(gps_data.size(), epsilon);
        for (const auto& point : gps_data) {
            compressor.AddGpsPoint(point);
        }
        compressor.Close();
        
        auto stats = compressor.GetStats();
        results.push_back({
            "多预测器融合（含标志位）",
            stats.quantization_bits,
            stats.total_bits,
            static_cast<double>(stats.total_bits) / stats.total_points,
            stats.total_prediction_error / (stats.total_points - 1)
        });
        
        // 同时记录不含标志位的版本
        results.push_back({
            "多预测器融合（纯量化）",
            stats.quantization_bits,
            stats.quantization_bits + 128,  // 加上第一个点的开销
            static_cast<double>(stats.quantization_bits + 128) / stats.total_points,
            stats.total_prediction_error / (stats.total_points - 1)
        });
    }
    
    // 输出结果
    std::cout << std::left << std::setw(30) << "预测器方案"
              << std::right << std::setw(15) << "量化数据(bits)"
              << std::setw(15) << "平均(bits/点)"
              << std::setw(18) << "平均预测误差(度)" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& r : results) {
        std::cout << std::left << std::setw(30) << r.name
                  << std::right << std::setw(15) << r.quantization_bits
                  << std::setw(15) << std::fixed << std::setprecision(2) << r.avg_bits_per_point
                  << std::setw(18) << std::scientific << std::setprecision(2) << r.avg_prediction_error
                  << std::endl;
    }
    
    std::cout << std::string(80, '-') << std::endl;
    
    // 计算改进
    std::cout << "\n=== 性能对比分析 ===" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    // 找出最佳单一预测器
    int best_single_idx = 0;
    for (size_t i = 1; i < 3; i++) {
        if (results[i].quantization_bits < results[best_single_idx].quantization_bits) {
            best_single_idx = i;
        }
    }
    
    double best_single_bits = results[best_single_idx].avg_bits_per_point;
    double multi_pure_bits = results[4].avg_bits_per_point;  // 多预测器（纯量化）
    double multi_with_flag_bits = results[3].avg_bits_per_point;  // 多预测器（含标志位）
    
    std::cout << "最佳单一预测器: " << results[best_single_idx].name << std::endl;
    std::cout << "  量化数据: " << std::fixed << std::setprecision(2) << best_single_bits << " bits/点" << std::endl;
    
    std::cout << "\n多预测器融合（纯量化数据）: " << multi_pure_bits << " bits/点" << std::endl;
    std::cout << "  相比最佳单一预测器改进: " 
              << std::setprecision(2) << ((best_single_bits - multi_pure_bits) / best_single_bits * 100) 
              << "%" << std::endl;
    
    std::cout << "\n多预测器融合（含标志位）: " << multi_with_flag_bits << " bits/点" << std::endl;
    std::cout << "  预测器标志开销: " << std::setprecision(2) 
              << (multi_with_flag_bits - multi_pure_bits) << " bits/点" << std::endl;
    std::cout << "  相比最佳单一预测器: " 
              << std::setprecision(2) << ((best_single_bits - multi_with_flag_bits) / best_single_bits * 100) 
              << "%" << std::endl;
    
    // 计算预测器标志的ROI
    double flag_overhead = multi_with_flag_bits - multi_pure_bits;
    double quantization_improvement = best_single_bits - multi_pure_bits;
    
    std::cout << "\n=== 预测器标志ROI分析 ===" << std::endl;
    std::cout << "量化数据节省: " << std::setprecision(2) << quantization_improvement << " bits/点" << std::endl;
    std::cout << "标志位开销: " << flag_overhead << " bits/点" << std::endl;
    std::cout << "净收益: " << (quantization_improvement - flag_overhead) << " bits/点" << std::endl;
    std::cout << "ROI: " << std::setprecision(1) << (quantization_improvement / flag_overhead) << "x" << std::endl;
    
    if (quantization_improvement > flag_overhead) {
        std::cout << "\n✅ 多预测器融合有效！量化数据的节省超过了标志位开销" << std::endl;
    } else {
        std::cout << "\n⚠️  多预测器融合的收益被标志位开销抵消" << std::endl;
    }
    
    std::cout << "\n============================================================" << std::endl;
    std::cout << "消融实验完成！" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    return 0;
}

