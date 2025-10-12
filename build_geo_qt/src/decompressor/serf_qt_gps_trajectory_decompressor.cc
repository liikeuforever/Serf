#include "decompressor/serf_qt_gps_trajectory_decompressor.h"
#include "utils/double.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include <cmath>

SerfQtGpsTrajectoryDecompressor::SerfQtGpsTrajectoryDecompressor(const Array<uint8_t>& compressed_data) {
    input_bit_stream_ = std::make_unique<InputBitStream>();
    input_bit_stream_->SetBuffer(compressed_data);
    history_states_.reserve(kMaxHistorySize);
}

SerfQtGpsTrajectoryDecompressor::GpsPoint SerfQtGpsTrajectoryDecompressor::DecompressNextPoint() {
    if (!initialized_) {
        Initialize();
    }
    
    if (!HasMoreData()) {
        return GpsPoint(); // 返回空点表示没有更多数据
    }
    
    if (point_index_ == 0) {
        // 第一个点已经在初始化时读取
        point_index_++;
        return current_reconstructed_point_;
    }
    
    if (point_index_ == 1) {
        // 第二个点使用完全校正策略
        CorrectionFlag strategy = DecodeStrategy();
        int64_t qv, qtheta;
        DecodeQuantizationValues(strategy, qv, qtheta);
        
        GpsPoint predicted_point = current_reconstructed_point_;
        MotionVector predicted_motion(0, 0);
        
        GpsPoint result = UpdateState(predicted_point, predicted_motion, strategy, 
                                     qv, qtheta);
        point_index_++;
        return result;
    }
    
    // 主解压循环：处理点 k >= 2
    
    // 步骤1: 高级预测（与压缩器完全一样）
    GpsPoint predicted_point;
    MotionVector predicted_motion;
    AdvancedPrediction(predicted_point, predicted_motion);
    
    // 步骤2: 解码标志位和校正数据
    CorrectionFlag strategy = DecodeStrategy();
    int64_t qv = 0, qtheta = 0;
    
    if (strategy != FLAG_ZERO_CORR) {
        DecodeQuantizationValues(strategy, qv, qtheta);
    }
    
    // 步骤3: 状态更新
    GpsPoint result = UpdateState(predicted_point, predicted_motion, strategy, 
                                 qv, qtheta);
    
    point_index_++;
    return result;
}

void SerfQtGpsTrajectoryDecompressor::Initialize() {
    if (initialized_) return;
    
    // 读取头部信息
    kBlockSize = input_bit_stream_->ReadInt(16);
    kEMax = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    
    // 读取第一个点的原始数据
    double longitude = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    double latitude = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    
    // 初始化状态
    current_reconstructed_point_ = GpsPoint(longitude, latitude);
    current_motion_vector_ = MotionVector(0, 0);
    
    // 添加到历史状态
    history_states_.emplace_back(current_reconstructed_point_, current_motion_vector_);
    
    initialized_ = true;
    point_index_ = 0;
}

void SerfQtGpsTrajectoryDecompressor::AdvancedPrediction(GpsPoint& predicted_point,
                                                        MotionVector& predicted_motion) {
    if (history_states_.empty()) {
        // 如果没有历史状态，使用零运动预测
        predicted_motion = MotionVector(0, 0);
        predicted_point = current_reconstructed_point_;
        return;
    }
    
    // 使用一阶运动模型：基于最近的运动矢量进行预测
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

SerfQtGpsTrajectoryDecompressor::CorrectionFlag SerfQtGpsTrajectoryDecompressor::DecodeStrategy() {
    // 读取第一个比特
    bool first_bit = input_bit_stream_->ReadBit();
    
    if (!first_bit) {
        // 0 -> FLAG_ZERO_CORR
        return FLAG_ZERO_CORR;
    }
    
    // 读取第二个比特
    bool second_bit = input_bit_stream_->ReadBit();
    
    if (!second_bit) {
        // 10 -> FLAG_V_ONLY
        return FLAG_V_ONLY;
    }
    
    // 读取第三个比特
    bool third_bit = input_bit_stream_->ReadBit();
    
    if (!third_bit) {
        // 110 -> FLAG_THETA_ONLY
        return FLAG_THETA_ONLY;
    } else {
        // 111 -> FLAG_BOTH
        return FLAG_BOTH;
    }
}

void SerfQtGpsTrajectoryDecompressor::DecodeQuantizationValues(CorrectionFlag strategy, 
                                                              int64_t& qv, int64_t& qtheta) {
    qv = 0;
    qtheta = 0;
    
    // 解码量化值
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        int64_t encoded_qv = EliasGammaCodec::Decode(input_bit_stream_.get());
        qv = ZigZagCodec::Decode(encoded_qv - 1);
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        int64_t encoded_qtheta = EliasGammaCodec::Decode(input_bit_stream_.get());
        qtheta = ZigZagCodec::Decode(encoded_qtheta - 1);
    }
}

SerfQtGpsTrajectoryDecompressor::GpsPoint SerfQtGpsTrajectoryDecompressor::UpdateState(
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
    
    return final_point;
}

SerfQtGpsTrajectoryDecompressor::GpsPoint SerfQtGpsTrajectoryDecompressor::CalculateDestinationPoint(
    const GpsPoint& start_point, const MotionVector& motion) const {
    
    double dx = motion.velocity * std::cos(motion.theta);
    double dy = motion.velocity * std::sin(motion.theta);
    
    return GpsPoint(start_point.longitude + dx,
                   start_point.latitude + dy);
}

bool SerfQtGpsTrajectoryDecompressor::HasMoreData() const {
    // 简化实现：基于点索引判断是否还有数据
    // 实际应用中可能需要更复杂的逻辑来检查流是否结束
    return true; // 暂时总是返回true，由调用者控制解压过程
}

void SerfQtGpsTrajectoryDecompressor::Reset() {
    initialized_ = false;
    point_index_ = 0;
    history_states_.clear();
    current_reconstructed_point_ = GpsPoint();
    current_motion_vector_ = MotionVector();
    
    // InputBitStream没有Reset方法，需要重新设置缓冲区
    // 这里简化处理，实际使用时可能需要保存原始数据
}
