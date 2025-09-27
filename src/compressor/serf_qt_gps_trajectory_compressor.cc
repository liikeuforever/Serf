#include "compressor/serf_qt_gps_trajectory_compressor.h"
#include "utils/double.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include <cmath>

SerfQtGpsTrajectoryCompressor::SerfQtGpsTrajectoryCompressor(int block_size, double e_max)
    : kBlockSize(block_size), kEMax(e_max * 0.999) {
    output_bit_stream_ = std::make_unique<OutputBitStream>(2 * block_size * 8);
    history_states_.reserve(kMaxHistorySize);
}

void SerfQtGpsTrajectoryCompressor::AddGpsPoint(const GpsPoint& point) {
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
        
        // 编码零校正策略
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

void SerfQtGpsTrajectoryCompressor::InitializeFirstTwoPoints(const GpsPoint& point) {
    if (first_point_) {
        // 处理点0：写入头部信息和原始坐标
        first_point_ = false;
        compressed_size_in_bits_ += output_bit_stream_->WriteInt(kBlockSize, 16);
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kEMax), 64);
        
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

void SerfQtGpsTrajectoryCompressor::AdvancedPrediction(GpsPoint& predicted_point,
                                                      MotionVector& predicted_motion) {
    if (history_states_.empty()) {
        // 如果没有历史状态，使用零运动预测
        predicted_motion = MotionVector(0, 0);
        predicted_point = current_reconstructed_point_;
        return;
    }
    
    // 使用一阶运动模型：基于最近的运动矢量进行预测
    // 这里可以根据需要实现更复杂的预测模型（如二阶、加权平均等）
    
    if (history_states_.size() == 1) {
        // 只有一个历史状态，使用静止预测
        predicted_motion = MotionVector(0, 0);
        predicted_point = current_reconstructed_point_;
    } else {
        // 使用最近的运动矢量作为预测
        predicted_motion = current_motion_vector_;
        
        // 基于预测运动矢量计算预测点
        predicted_point = CalculateDestinationPoint(current_reconstructed_point_, 
                                                   predicted_motion);
    }
}

bool SerfQtGpsTrajectoryCompressor::GeometricPruningAndFastDecision(
    const GpsPoint& current_point,
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    std::vector<CorrectionFlag>& viable_candidates) {
    
    // 快速路径：检查预测误差
    double prediction_error = CalculateDistance(current_point, predicted_point);
    
    if (prediction_error <= kEMax) {
        // 零校正策略可行，直接使用
        return true;
    }
    
    // 建立剪枝框架
    viable_candidates.clear();
    
    // 注意：不再无条件添加完全校正，而是通过验证后再添加
    
    // 剪枝测试B：纯速度校正
    // 计算当前点到预测线的垂直距离
    if (history_states_.size() >= 2) {
        double perpendicular_distance = CalculatePerpendicularDistance(
            current_point, current_reconstructed_point_, predicted_point);
        
        if (perpendicular_distance <= kEMax) {
            viable_candidates.push_back(FLAG_V_ONLY);
        }
    }
    
    // 剪枝测试C：纯角度校正
    // 简化的圆-圆相交测试
    // 这里使用启发式方法：如果角度调整能够显著减少误差，则认为可行
    double current_distance = CalculateDistance(current_reconstructed_point_, current_point);
    double predicted_distance = predicted_motion.velocity;
    
    if (std::abs(current_distance - predicted_distance) <= kEMax) {
        viable_candidates.push_back(FLAG_THETA_ONLY);
    }
    
    // 始终尝试完全校正策略
    viable_candidates.push_back(FLAG_BOTH);
    
    // 如果没有其他策略可行，也尝试其他单一校正策略
    if (viable_candidates.size() == 1) {  // 只有完全校正
        viable_candidates.push_back(FLAG_V_ONLY);
        viable_candidates.push_back(FLAG_THETA_ONLY);
    }
    
    return false;
}

void SerfQtGpsTrajectoryCompressor::EvaluateAndSelectBestStrategy(
    const GpsPoint& current_point,
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    const std::vector<CorrectionFlag>& candidates,
    CorrectionFlag& best_strategy,
    int64_t& best_qv,
    int64_t& best_qtheta) {
    
    int min_cost = INT_MAX;
    best_strategy = FLAG_BOTH; // 默认使用完全校正
    best_qv = 0;
    best_qtheta = 0;
    bool found_valid_strategy = false;
    
    for (CorrectionFlag strategy : candidates) {
        int64_t qv = 0, qtheta = 0;
        
        // 计算最优量化整数
        CalculateOptimalQuantization(current_point, predicted_point, predicted_motion, 
                                   strategy, qv, qtheta);
        
        // 验证重构误差是否在容忍范围内
        GpsPoint reconstructed_point = SimulateReconstruction(predicted_point, predicted_motion, 
                                                            strategy, qv, qtheta);
        double reconstruction_error = CalculateDistance(current_point, reconstructed_point);
        
        // 只考虑满足误差要求的策略
        if (reconstruction_error <= kEMax) {
            found_valid_strategy = true;
            
            // 估算编码成本
            int cost = EstimateEncodingCost(strategy, qv, qtheta);
            
            if (cost < min_cost) {
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
                                   FLAG_BOTH, best_qv, best_qtheta);
        
        // 如果完全校正仍然不满足误差要求，则需要调整量化参数
        GpsPoint reconstructed_point = SimulateReconstruction(predicted_point, predicted_motion, 
                                                            FLAG_BOTH, best_qv, best_qtheta);
        double reconstruction_error = CalculateDistance(current_point, reconstructed_point);
        
        if (reconstruction_error > kEMax) {
            // 使用更精确的量化来确保误差满足要求
            RefineQuantizationForErrorBound(current_point, predicted_point, predicted_motion, 
                                          best_qv, best_qtheta);
        }
    }
}

void SerfQtGpsTrajectoryCompressor::CalculateOptimalQuantization(
    const GpsPoint& current_point,
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    CorrectionFlag strategy,
    int64_t& qv,
    int64_t& qtheta) const {
    
    qv = 0;
    qtheta = 0;
    
    switch (strategy) {
        case FLAG_ZERO_CORR:
            // 零校正，不需要量化值
            break;
            
        case FLAG_V_ONLY: {
            // 纯速度校正：计算需要的速度调整
            double required_distance = CalculateDistance(current_reconstructed_point_, current_point);
            double velocity_diff = required_distance - predicted_motion.velocity;
            qv = static_cast<int64_t>(std::round(velocity_diff / kEpsilonV));
            break;
        }
        
        case FLAG_THETA_ONLY: {
            // 纯角度校正：计算需要的角度调整
            MotionVector true_motion = CalculateMotionVector(current_reconstructed_point_, 
                                                           current_point);
            double theta_diff = true_motion.theta - predicted_motion.theta;
            
            // 处理角度的周期性
            while (theta_diff > M_PI) theta_diff -= 2 * M_PI;
            while (theta_diff < -M_PI) theta_diff += 2 * M_PI;
            
            qtheta = static_cast<int64_t>(std::round(theta_diff / kEpsilonTheta));
            break;
        }
        
        case FLAG_BOTH: {
            // 完全校正：计算真实运动矢量与预测的差值
            MotionVector true_motion = CalculateMotionVector(current_reconstructed_point_, 
                                                           current_point);
            double velocity_diff = true_motion.velocity - predicted_motion.velocity;
            double theta_diff = true_motion.theta - predicted_motion.theta;
            
            // 处理角度的周期性
            while (theta_diff > M_PI) theta_diff -= 2 * M_PI;
            while (theta_diff < -M_PI) theta_diff += 2 * M_PI;
            
            qv = static_cast<int64_t>(std::round(velocity_diff / kEpsilonV));
            qtheta = static_cast<int64_t>(std::round(theta_diff / kEpsilonTheta));
            break;
        }
    }
}

void SerfQtGpsTrajectoryCompressor::EncodeStrategy(CorrectionFlag strategy, 
                                                  int64_t qv, int64_t qtheta) {
    // 编码标志位
    switch (strategy) {
        case FLAG_ZERO_CORR:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false); // 0
            break;
        case FLAG_V_ONLY:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);  // 1
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false); // 0
            break;
        case FLAG_THETA_ONLY:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);  // 1
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);  // 1
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false); // 0
            break;
        case FLAG_BOTH:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);  // 1
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);  // 1
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);  // 1
            break;
    }
    
    // 编码量化值
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        compressed_size_in_bits_ += EliasGammaCodec::Encode(
            ZigZagCodec::Encode(qv) + 1, output_bit_stream_.get());
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        compressed_size_in_bits_ += EliasGammaCodec::Encode(
            ZigZagCodec::Encode(qtheta) + 1, output_bit_stream_.get());
    }
}

void SerfQtGpsTrajectoryCompressor::UpdateState(const GpsPoint& current_point,
                                               const GpsPoint& predicted_point,
                                               const MotionVector& predicted_motion,
                                               CorrectionFlag strategy,
                                               int64_t qv,
                                               int64_t qtheta) {
    // 计算校正残差
    double delta_v = 0, delta_theta = 0;
    
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        delta_v = qv * kEpsilonV;
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        delta_theta = qtheta * kEpsilonTheta;
    }
    
    // 计算最终运动矢量
    MotionVector final_motion(predicted_motion.velocity + delta_v,
                             predicted_motion.theta + delta_theta);
    
    // 计算最终重构点
    GpsPoint final_point;
    if (strategy == FLAG_ZERO_CORR) {
        final_point = predicted_point;
    } else {
        final_point = CalculateDestinationPoint(current_reconstructed_point_, 
                                               final_motion);
    }
    
    // 更新当前状态
    current_reconstructed_point_ = final_point;
    current_motion_vector_ = final_motion;
    
    // 添加到历史状态队列
    history_states_.emplace_back(final_point, final_motion);
    if (history_states_.size() > kMaxHistorySize) {
        history_states_.erase(history_states_.begin());
    }
}

// 辅助函数实现
double SerfQtGpsTrajectoryCompressor::CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) const {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

SerfQtGpsTrajectoryCompressor::GpsPoint SerfQtGpsTrajectoryCompressor::CalculateDestinationPoint(
    const GpsPoint& start_point, const MotionVector& motion) const {
    
    double dx = motion.velocity * std::cos(motion.theta);
    double dy = motion.velocity * std::sin(motion.theta);
    
    return GpsPoint(start_point.longitude + dx,
                   start_point.latitude + dy);
}

SerfQtGpsTrajectoryCompressor::MotionVector SerfQtGpsTrajectoryCompressor::CalculateMotionVector(
    const GpsPoint& start_point, const GpsPoint& end_point) const {
    
    double dx = end_point.longitude - start_point.longitude;
    double dy = end_point.latitude - start_point.latitude;
    
    double velocity = std::sqrt(dx * dx + dy * dy);
    double theta = std::atan2(dy, dx);
    
    return MotionVector(velocity, theta);
}

double SerfQtGpsTrajectoryCompressor::CalculatePerpendicularDistance(
    const GpsPoint& point, const GpsPoint& line_start, const GpsPoint& line_end) const {
    
    double A = point.longitude - line_start.longitude;
    double B = point.latitude - line_start.latitude;
    double C = line_end.longitude - line_start.longitude;
    double D = line_end.latitude - line_start.latitude;
    
    double dot = A * C + B * D;
    double len_sq = C * C + D * D;
    
    if (len_sq == 0) {
        return CalculateDistance(point, line_start);
    }
    
    double param = dot / len_sq;
    
    GpsPoint closest_point;
    if (param < 0) {
        closest_point = line_start;
    } else if (param > 1) {
        closest_point = line_end;
    } else {
        closest_point = GpsPoint(line_start.longitude + param * C,
                               line_start.latitude + param * D);
    }
    
    return CalculateDistance(point, closest_point);
}

SerfQtGpsTrajectoryCompressor::GpsPoint SerfQtGpsTrajectoryCompressor::SimulateReconstruction(
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    CorrectionFlag strategy,
    int64_t qv,
    int64_t qtheta) const {
    
    // 计算校正残差
    double delta_v = 0, delta_theta = 0;
    
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        delta_v = qv * kEpsilonV;
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        delta_theta = qtheta * kEpsilonTheta;
    }
    
    // 计算最终运动矢量
    MotionVector final_motion(predicted_motion.velocity + delta_v,
                             predicted_motion.theta + delta_theta);
    
    // 计算最终重构点
    if (strategy == FLAG_ZERO_CORR) {
        return predicted_point;
    } else {
        return CalculateDestinationPoint(current_reconstructed_point_, final_motion);
    }
}

void SerfQtGpsTrajectoryCompressor::RefineQuantizationForErrorBound(
    const GpsPoint& current_point,
    const GpsPoint& predicted_point,
    const MotionVector& predicted_motion,
    int64_t& qv,
    int64_t& qtheta) const {
    
    // 使用迭代方法精化量化参数以满足误差界限
    const int max_iterations = 10;
    const double tolerance = kEMax * 0.95; // 留一些余量
    
    for (int iter = 0; iter < max_iterations; ++iter) {
        GpsPoint reconstructed_point = SimulateReconstruction(predicted_point, predicted_motion, 
                                                            FLAG_BOTH, qv, qtheta);
        double error = CalculateDistance(current_point, reconstructed_point);
        
        if (error <= tolerance) {
            break; // 满足误差要求
        }
        
        // 计算误差向量
        double error_lon = current_point.longitude - reconstructed_point.longitude;
        double error_lat = current_point.latitude - reconstructed_point.latitude;
        
        // 调整量化参数以减少误差
        // 这是一个简化的调整策略
        if (std::abs(error_lon) > std::abs(error_lat)) {
            // 主要是经度误差，调整速度相关的量化
            if (error_lon > 0) {
                qv += 1;
            } else {
                qv -= 1;
            }
        } else {
            // 主要是纬度误差，调整角度相关的量化
            if (error_lat > 0) {
                qtheta += 1;
            } else {
                qtheta -= 1;
            }
        }
    }
}

int SerfQtGpsTrajectoryCompressor::EstimateEncodingCost(CorrectionFlag strategy, 
                                                       int64_t qv, int64_t qtheta) const {
    int cost = 0;
    
    // 标志位成本
    switch (strategy) {
        case FLAG_ZERO_CORR: cost += 1; break;
        case FLAG_V_ONLY: cost += 2; break;
        case FLAG_THETA_ONLY: cost += 3; break;
        case FLAG_BOTH: cost += 3; break;
    }
    
    // 量化值编码成本（简化估算）
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        cost += static_cast<int>(std::log2(std::abs(qv) + 1)) + 2;
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        cost += static_cast<int>(std::log2(std::abs(qtheta) + 1)) + 2;
    }
    
    return cost;
}

Array<uint8_t> SerfQtGpsTrajectoryCompressor::compressed_bytes() {
    return compressed_bytes_;
}

void SerfQtGpsTrajectoryCompressor::Close() {
    output_bit_stream_->Flush();
    compressed_bytes_ = output_bit_stream_->GetBuffer(std::ceil(compressed_size_in_bits_ / 8.0));
    output_bit_stream_->Refresh();
    
    // 重置状态
    first_point_ = true;
    point_index_ = 0;
    history_states_.clear();
    current_reconstructed_point_ = GpsPoint();
    current_motion_vector_ = MotionVector();
    stored_compressed_size_in_bits_ = compressed_size_in_bits_;
    compressed_size_in_bits_ = 0;
}

long SerfQtGpsTrajectoryCompressor::get_compressed_size_in_bits() const {
    return stored_compressed_size_in_bits_;
}
