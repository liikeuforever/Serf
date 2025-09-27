#include "decompressor/serf_qt_gps_configurable_decompressor.h"
#include "utils/double.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include <cmath>

SerfQtGpsConfigurableDecompressor::SerfQtGpsConfigurableDecompressor(const Array<uint8_t>& compressed_data) {
    input_bit_stream_ = std::make_unique<InputBitStream>();
    input_bit_stream_->SetBuffer(compressed_data);
    history_states_.reserve(kMaxHistorySize);
}

SerfQtGpsConfigurableDecompressor::GpsPoint SerfQtGpsConfigurableDecompressor::GetNextGpsPoint() {
    if (!initialized_) {
        Initialize();
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

void SerfQtGpsConfigurableDecompressor::Initialize() {
    if (initialized_) return;
    
    // 读取头部信息
    kBlockSize = input_bit_stream_->ReadInt(16);
    kEMax = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    
    // 读取可配置参数
    kEpsilonV = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    kEpsilonTheta = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    
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

void SerfQtGpsConfigurableDecompressor::AdvancedPrediction(GpsPoint& predicted_point,
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

SerfQtGpsConfigurableDecompressor::CorrectionFlag SerfQtGpsConfigurableDecompressor::DecodeStrategy() {
    bool first_bit = input_bit_stream_->ReadBit();
    
    if (!first_bit) {
        return FLAG_ZERO_CORR;
    }
    
    bool second_bit = input_bit_stream_->ReadBit();
    
    if (!second_bit) {
        return FLAG_V_ONLY;
    }
    
    bool third_bit = input_bit_stream_->ReadBit();
    
    if (!third_bit) {
        return FLAG_THETA_ONLY;
    } else {
        return FLAG_BOTH;
    }
}

void SerfQtGpsConfigurableDecompressor::DecodeQuantizationValues(CorrectionFlag strategy, 
                                                                int64_t& qv, int64_t& qtheta) {
    qv = 0;
    qtheta = 0;
    
    if (strategy == FLAG_V_ONLY || strategy == FLAG_BOTH) {
        uint64_t encoded_qv = EliasGammaCodec::Decode(input_bit_stream_.get());
        qv = ZigZagCodec::Decode(encoded_qv - 1);
    }
    
    if (strategy == FLAG_THETA_ONLY || strategy == FLAG_BOTH) {
        uint64_t encoded_qtheta = EliasGammaCodec::Decode(input_bit_stream_.get());
        qtheta = ZigZagCodec::Decode(encoded_qtheta - 1);
    }
}

SerfQtGpsConfigurableDecompressor::GpsPoint SerfQtGpsConfigurableDecompressor::UpdateState(
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
    
    return final_point;
}

SerfQtGpsConfigurableDecompressor::GpsPoint SerfQtGpsConfigurableDecompressor::CalculateDestinationPoint(
    const GpsPoint& start_point, const MotionVector& motion) const {
    
    double dx = motion.velocity * std::cos(motion.theta);
    double dy = motion.velocity * std::sin(motion.theta);
    
    return GpsPoint(start_point.longitude + dx, start_point.latitude + dy);
}

SerfQtGpsConfigurableDecompressor::MotionVector SerfQtGpsConfigurableDecompressor::CalculateMotionVector(
    const GpsPoint& start_point, const GpsPoint& end_point) const {
    
    double dx = end_point.longitude - start_point.longitude;
    double dy = end_point.latitude - start_point.latitude;
    
    double velocity = std::sqrt(dx * dx + dy * dy);
    double theta = std::atan2(dy, dx);
    
    return MotionVector(velocity, theta);
}

void SerfQtGpsConfigurableDecompressor::Reset() {
    history_states_.clear();
    initialized_ = false;
    point_index_ = 0;
}