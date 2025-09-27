#include "compressor/serf_qt_gps_configurable_compressor.h"
#include "utils/double.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include <cmath>
#include <iostream>
#include <iomanip>

SerfQtGpsConfigurableCompressor::SerfQtGpsConfigurableCompressor(int block_size, double e_max, 
                                                                double epsilon_v, double epsilon_theta)
    : kBlockSize(block_size), kEMax(e_max * 0.999), kEpsilonV(epsilon_v), kEpsilonTheta(epsilon_theta) {
    output_bit_stream_ = std::make_unique<OutputBitStream>(2 * block_size * 8);
    history_states_.reserve(kMaxHistorySize);
}

void SerfQtGpsConfigurableCompressor::AddGpsPoint(const GpsPoint& point) {
    if (first_point_ || point_index_ < 2) {
        InitializeFirstTwoPoints(point);
        return;
    }

    // 主压缩循环：处理点 k >= 2
    GpsPoint predicted_point;
    MotionVector predicted_motion;
    
    // 步骤1: 高级预测
    AdvancedPrediction(predicted_point, predicted_motion);
    
    // 步骤2: 几何剪枝与快速决策
    std::vector<CorrectionFlag> viable_candidates;
    bool used_fast_path = GeometricPruningAndFastDecision(point, predicted_point, 
                                                         predicted_motion, viable_candidates);
    
    if (used_fast_path) {
        // 使用零校正策略，需要编码策略标志
        EncodeStrategy(FLAG_ZERO_CORR, 0, 0);
        UpdateState(point, predicted_point, predicted_motion, FLAG_ZERO_CORR, 0, 0);
        point_index_++;
        return;
    }
    
    // 步骤3: 评估可行候选与最终选择
    CorrectionFlag best_strategy;
    int64_t best_qv, best_qtheta;
    EvaluateAndSelectBestStrategy(point, predicted_point, predicted_motion, 
                                 viable_candidates, best_strategy, best_qv, best_qtheta);
    
    // 步骤4: 编码与状态更新
    EncodeStrategy(best_strategy, best_qv, best_qtheta);
    UpdateState(point, predicted_point, predicted_motion, best_strategy, best_qv, best_qtheta);
    
    point_index_++;
}

Array<uint8_t> SerfQtGpsConfigurableCompressor::GetCompressedData() {
    output_bit_stream_->Flush();
    int byte_length = (compressed_size_in_bits_ + 7) / 8;
    return output_bit_stream_->GetBuffer(byte_length);
}

void SerfQtGpsConfigurableCompressor::InitializeFirstTwoPoints(const GpsPoint& point) {
    if (first_point_) {
        // 处理点0：写入头部信息和原始坐标
        first_point_ = false;
        compressed_size_in_bits_ += output_bit_stream_->WriteInt(kBlockSize, 16);
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kEMax), 64);
        
        // 写入可配置参数
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kEpsilonV), 64);
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kEpsilonTheta), 64);
        
        // 写入第一个点的原始数据
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(point.longitude), 64);
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(point.latitude), 64);
        
        // 初始化状态
        current_reconstructed_point_ = point;
        current_motion_vector_ = MotionVector(0, 0);
        
        // 添加到历史状态
        history_states_.emplace_back(point, current_motion_vector_);
        
        point_index_ = 1;
    } else if (point_index_ == 1) {
        // 处理点1：使用简化的完全校正策略
        MotionVector true_motion = CalculateMotionVector(current_reconstructed_point_, point);
        
        // 量化运动矢量
        int64_t qv = static_cast<int64_t>(std::round(true_motion.velocity / kEpsilonV));
        int64_t qtheta = static_cast<int64_t>(std::round(true_motion.theta / kEpsilonTheta));
        
        // 编码
        EncodeStrategy(FLAG_BOTH, qv, qtheta);
        
        // 重构运动矢量和点
        MotionVector reconstructed_motion(qv * kEpsilonV, qtheta * kEpsilonTheta);
        GpsPoint reconstructed_point = CalculateDestinationPoint(current_reconstructed_point_, 
                                                               reconstructed_motion);
        
        // 更新状态
        current_reconstructed_point_ = reconstructed_point;
        current_motion_vector_ = reconstructed_motion;
        
        // 添加到历史状态
        history_states_.emplace_back(reconstructed_point, reconstructed_motion);
        if (history_states_.size() > kMaxHistorySize) {
            history_states_.erase(history_states_.begin());
        }
        
        point_index_ = 2;
    }
}

void SerfQtGpsConfigurableCompressor::AdvancedPrediction(GpsPoint& predicted_point,
                                                        MotionVector& predicted_motion) {
    if (history_states_.empty()) {
        predicted_motion = MotionVector(0, 0);
        predicted_point = current_reconstructed_point_;
        return;
    }
    
    if (history_states_.size() == 1) {
        predicted_motion = MotionVector(0, 0);
        predicted_point = current_reconstructed_point_;
    } else {
        predicted_motion = current_motion_vector_;
        predicted_point = CalculateDestinationPoint(current_reconstructed_point_, predicted_motion);
    }
}

bool SerfQtGpsConfigurableCompressor::GeometricPruningAndFastDecision(
    const GpsPoint& current_point,
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    std::vector<CorrectionFlag>& viable_candidates) {
    
    // 快速决策：检查零校正是否满足误差要求
    double zero_correction_error = CalculateDistance(current_point, predicted_point);
    if (zero_correction_error <= kEMax) {
        return true; // 使用快速路径
    }
    
    // 几何剪枝：添加所有可能的校正策略
    viable_candidates.push_back(FLAG_V_ONLY);
    viable_candidates.push_back(FLAG_THETA_ONLY);
    viable_candidates.push_back(FLAG_BOTH);
    
    return false; // 需要进一步评估
}

void SerfQtGpsConfigurableCompressor::EvaluateAndSelectBestStrategy(
    const GpsPoint& current_point,
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    const std::vector<CorrectionFlag>& viable_candidates,
    CorrectionFlag& best_strategy,
    int64_t& best_qv,
    int64_t& best_qtheta) {
    
    double min_cost = std::numeric_limits<double>::max();
    bool found_valid_strategy = false;
    
    for (CorrectionFlag strategy : viable_candidates) {
        int64_t qv = 0, qtheta = 0;
        CalculateOptimalQuantization(current_point, predicted_point, predicted_motion, 
                                   strategy, qv, qtheta);
        
        // 验证误差
        GpsPoint reconstructed_point = SimulateReconstruction(predicted_point, predicted_motion, 
                                                            strategy, qv, qtheta);
        double reconstruction_error = CalculateDistance(current_point, reconstructed_point);
        
        if (reconstruction_error <= kEMax) {
            // 计算编码成本
            double cost = 2; // 策略标志基础成本
            if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
                cost += std::log2(2 * std::abs(qv) + 1);
            }
            if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
                cost += std::log2(2 * std::abs(qtheta) + 1);
            }
            
            if (cost < min_cost) {
                found_valid_strategy = true;
                min_cost = cost;
                best_strategy = strategy;
                best_qv = qv;
                best_qtheta = qtheta;
            }
        }
    }
    
    // 如果没有找到满足误差要求的策略，强制使用完全校正并重新计算
    if (!found_valid_strategy) {
        best_strategy = FLAG_BOTH;
        CalculateOptimalQuantization(current_point, predicted_point, predicted_motion, 
                                   best_strategy, best_qv, best_qtheta);
    }
}

void SerfQtGpsConfigurableCompressor::CalculateOptimalQuantization(
    const GpsPoint& current_point,
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    CorrectionFlag strategy,
    int64_t& qv,
    int64_t& qtheta) {
    
    qv = 0;
    qtheta = 0;
    
    switch (strategy) {
        case FLAG_ZERO_CORR:
            break;
            
        case FLAG_V_ONLY: {
            MotionVector true_motion = CalculateMotionVector(current_reconstructed_point_, current_point);
            double velocity_diff = true_motion.velocity - predicted_motion.velocity;
            qv = static_cast<int64_t>(std::round(velocity_diff / kEpsilonV));
            break;
        }
        
        case FLAG_THETA_ONLY: {
            MotionVector true_motion = CalculateMotionVector(current_reconstructed_point_, current_point);
            double theta_diff = true_motion.theta - predicted_motion.theta;
            while (theta_diff > M_PI) theta_diff -= 2 * M_PI;
            while (theta_diff < -M_PI) theta_diff += 2 * M_PI;
            qtheta = static_cast<int64_t>(std::round(theta_diff / kEpsilonTheta));
            break;
        }
        
        case FLAG_BOTH: {
            MotionVector true_motion = CalculateMotionVector(current_reconstructed_point_, current_point);
            double velocity_diff = true_motion.velocity - predicted_motion.velocity;
            double theta_diff = true_motion.theta - predicted_motion.theta;
            
            while (theta_diff > M_PI) theta_diff -= 2 * M_PI;
            while (theta_diff < -M_PI) theta_diff += 2 * M_PI;
            
            qv = static_cast<int64_t>(std::round(velocity_diff / kEpsilonV));
            qtheta = static_cast<int64_t>(std::round(theta_diff / kEpsilonTheta));
            break;
        }
    }
}

void SerfQtGpsConfigurableCompressor::EncodeStrategy(CorrectionFlag strategy, 
                                                    int64_t qv, int64_t qtheta) {
    // 记录编码前的比特数
    int bits_before = compressed_size_in_bits_;
    
    switch (strategy) {
        case FLAG_ZERO_CORR:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false);
            break;
        case FLAG_V_ONLY:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false);
            break;
        case FLAG_THETA_ONLY:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false);
            break;
        case FLAG_BOTH:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            break;
    }
    
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        compressed_size_in_bits_ += EliasGammaCodec::Encode(
            ZigZagCodec::Encode(qv) + 1, output_bit_stream_.get());
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        compressed_size_in_bits_ += EliasGammaCodec::Encode(
            ZigZagCodec::Encode(qtheta) + 1, output_bit_stream_.get());
    }
    
    // 更新统计信息
    int bits_after = compressed_size_in_bits_;
    int bits_used = bits_after - bits_before;
    
    switch (strategy) {
        case FLAG_ZERO_CORR:
            strategy_stats_.zero_corr_count++;
            strategy_stats_.total_strategy_bits += 1;
            break;
        case FLAG_V_ONLY:
            strategy_stats_.v_only_count++;
            strategy_stats_.total_strategy_bits += 2;
            strategy_stats_.total_quantization_bits += (bits_used - 2);
            break;
        case FLAG_THETA_ONLY:
            strategy_stats_.theta_only_count++;
            strategy_stats_.total_strategy_bits += 3;
            strategy_stats_.total_quantization_bits += (bits_used - 3);
            break;
        case FLAG_BOTH:
            strategy_stats_.both_count++;
            strategy_stats_.total_strategy_bits += 3;
            strategy_stats_.total_quantization_bits += (bits_used - 3);
            break;
    }
}

void SerfQtGpsConfigurableCompressor::UpdateState(const GpsPoint& current_point,
                                                 const GpsPoint& predicted_point,
                                                 const MotionVector& predicted_motion,
                                                 CorrectionFlag strategy,
                                                 int64_t qv,
                                                 int64_t qtheta) {
    double delta_v = 0, delta_theta = 0;
    
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        delta_v = qv * kEpsilonV;
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        delta_theta = qtheta * kEpsilonTheta;
    }
    
    MotionVector final_motion(predicted_motion.velocity + delta_v,
                             predicted_motion.theta + delta_theta);
    
    GpsPoint final_point;
    if (strategy == FLAG_ZERO_CORR) {
        final_point = predicted_point;
    } else {
        final_point = CalculateDestinationPoint(current_reconstructed_point_, final_motion);
    }
    
    current_reconstructed_point_ = final_point;
    current_motion_vector_ = final_motion;
    
    history_states_.emplace_back(final_point, final_motion);
    if (history_states_.size() > kMaxHistorySize) {
        history_states_.erase(history_states_.begin());
    }
}

SerfQtGpsConfigurableCompressor::GpsPoint SerfQtGpsConfigurableCompressor::CalculateDestinationPoint(
    const GpsPoint& start_point, const MotionVector& motion) const {
    
    double dx = motion.velocity * std::cos(motion.theta);
    double dy = motion.velocity * std::sin(motion.theta);
    
    return GpsPoint(start_point.longitude + dx, start_point.latitude + dy);
}

SerfQtGpsConfigurableCompressor::MotionVector SerfQtGpsConfigurableCompressor::CalculateMotionVector(
    const GpsPoint& start_point, const GpsPoint& end_point) const {
    
    double dx = end_point.longitude - start_point.longitude;
    double dy = end_point.latitude - start_point.latitude;
    
    double velocity = std::sqrt(dx * dx + dy * dy);
    double theta = std::atan2(dy, dx);
    
    return MotionVector(velocity, theta);
}

double SerfQtGpsConfigurableCompressor::CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) const {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

SerfQtGpsConfigurableCompressor::GpsPoint SerfQtGpsConfigurableCompressor::SimulateReconstruction(
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    CorrectionFlag strategy,
    int64_t qv,
    int64_t qtheta) const {
    
    double delta_v = 0, delta_theta = 0;
    
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        delta_v = qv * kEpsilonV;
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        delta_theta = qtheta * kEpsilonTheta;
    }
    
    MotionVector final_motion(predicted_motion.velocity + delta_v,
                             predicted_motion.theta + delta_theta);
    
    if (strategy == FLAG_ZERO_CORR) {
        return predicted_point;
    } else {
        return CalculateDestinationPoint(current_reconstructed_point_, final_motion);
    }
}

void SerfQtGpsConfigurableCompressor::StrategyStats::PrintStats() const {
    int total_points = GetTotalPoints();
    if (total_points == 0) {
        std::cout << "没有策略统计数据" << std::endl;
        return;
    }
    
    std::cout << "\n=== 策略使用分布统计 ===" << std::endl;
    std::cout << "总处理点数: " << total_points << std::endl;
    
    std::cout << "\n各策略使用次数和比例:" << std::endl;
    std::cout << "  FLAG_ZERO_CORR (零校正):   " << std::setw(8) << zero_corr_count 
              << " (" << std::fixed << std::setprecision(2) 
              << (100.0 * zero_corr_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_V_ONLY (速度校正):    " << std::setw(8) << v_only_count 
              << " (" << (100.0 * v_only_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_THETA_ONLY (角度校正): " << std::setw(8) << theta_only_count 
              << " (" << (100.0 * theta_only_count / total_points) << "%)" << std::endl;
    std::cout << "  FLAG_BOTH (完全校正):      " << std::setw(8) << both_count 
              << " (" << (100.0 * both_count / total_points) << "%)" << std::endl;
    
    // 编码成本分析
    int total_bits = total_strategy_bits + total_quantization_bits;
    std::cout << "\n编码成本分析:" << std::endl;
    std::cout << "  策略标志总比特: " << total_strategy_bits << " bits (" 
              << std::setprecision(1) << (100.0 * total_strategy_bits / total_bits) << "%)" << std::endl;
    std::cout << "  量化数据总比特: " << total_quantization_bits << " bits (" 
              << (100.0 * total_quantization_bits / total_bits) << "%)" << std::endl;
    std::cout << "  总编码比特数: " << total_bits << " bits" << std::endl;
    
    // 平均成本分析
    std::cout << "\n平均编码成本:" << std::endl;
    std::cout << "  平均每点总成本: " << std::setprecision(2) 
              << (static_cast<double>(total_bits) / total_points) << " bits/点" << std::endl;
    std::cout << "  平均策略成本: " 
              << (static_cast<double>(total_strategy_bits) / total_points) << " bits/点" << std::endl;
    std::cout << "  平均量化成本: " 
              << (static_cast<double>(total_quantization_bits) / total_points) << " bits/点" << std::endl;
    
    // 各策略的平均量化成本
    int correction_count = v_only_count + theta_only_count + both_count;
    if (correction_count > 0) {
        std::cout << "\n各策略平均量化成本:" << std::endl;
        
        if (v_only_count > 0) {
            double avg_v_cost = static_cast<double>(total_quantization_bits) * v_only_count / 
                               correction_count / v_only_count;
            std::cout << "  速度校正平均量化成本: " << std::setprecision(1) << avg_v_cost << " bits" << std::endl;
        }
        
        if (theta_only_count > 0) {
            double avg_theta_cost = static_cast<double>(total_quantization_bits) * theta_only_count / 
                                   correction_count / theta_only_count;
            std::cout << "  角度校正平均量化成本: " << avg_theta_cost << " bits" << std::endl;
        }
        
        if (both_count > 0) {
            double avg_both_cost = static_cast<double>(total_quantization_bits) * both_count / 
                                  correction_count / both_count;
            std::cout << "  完全校正平均量化成本: " << avg_both_cost << " bits" << std::endl;
        }
    }
    
    // 压缩效率分析
    double zero_ratio = static_cast<double>(zero_corr_count) / total_points;
    double correction_ratio = 1.0 - zero_ratio;
    
    std::cout << "\n=== 压缩效率分析 ===" << std::endl;
    std::cout << "零校正比例: " << std::setprecision(2) << (zero_ratio * 100) << "%" << std::endl;
    std::cout << "需要校正比例: " << (correction_ratio * 100) << "%" << std::endl;
    
    // 优化建议
    if (correction_ratio > 0.8) {
        std::cout << "\n🔧 压缩比优化建议 (校正比例过高 " << (correction_ratio * 100) << "%):" << std::endl;
        std::cout << "  1. 🎯 改进预测算法 - 当前预测准确性较低" << std::endl;
        std::cout << "  2. 📏 增大量化步长 - 减少校正需求" << std::endl;
        std::cout << "  3. 🔄 考虑自适应误差界限 - 动态调整精度要求" << std::endl;
        std::cout << "  4. 📊 分析数据特征 - 针对性优化预测模型" << std::endl;
    } else if (correction_ratio > 0.6) {
        std::cout << "\n💡 压缩比可进一步优化 (校正比例较高 " << (correction_ratio * 100) << "%):" << std::endl;
        std::cout << "  1. 🎯 微调预测参数" << std::endl;
        std::cout << "  2. 📏 优化量化策略" << std::endl;
        std::cout << "  3. 🔄 考虑更复杂的预测模型" << std::endl;
    } else if (correction_ratio > 0.4) {
        std::cout << "\n✅ 预测效果良好 (零校正比例 " << (zero_ratio * 100) << "%)，可微调:" << std::endl;
        std::cout << "  1. 📏 精细调整量化参数" << std::endl;
        std::cout << "  2. 🎯 优化边界情况处理" << std::endl;
    } else {
        std::cout << "\n🏆 预测效果优秀！零校正比例达到 " << (zero_ratio * 100) << "%" << std::endl;
    }
    
    // 理论压缩比分析
    std::cout << "\n=== 理论压缩比分析 ===" << std::endl;
    double theoretical_best = 128.0; // 如果全部零校正
    double current_avg_bits = static_cast<double>(total_bits) / total_points;
    double theoretical_current = 128.0 / current_avg_bits;
    
    std::cout << "当前平均编码: " << std::setprecision(2) << current_avg_bits << " bits/点" << std::endl;
    std::cout << "理论压缩比: " << theoretical_current << ":1" << std::endl;
    std::cout << "理论最优 (100%零校正): " << theoretical_best << ":1" << std::endl;
    std::cout << "效率百分比: " << (1.0 / current_avg_bits * 100) << "%" << std::endl;
}