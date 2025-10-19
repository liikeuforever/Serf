#include "compressor/serf_qt_curve_compressor.h"
#include <cmath>
#include "utils/double.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"

SerfQtCurveCompressor::SerfQtCurveCompressor(int block_size, double max_diff) 
    : kBlockSize(block_size), kMaxDiff(max_diff * 0.999) {
  output_bit_stream_ = std::make_unique<OutputBitStream>(2 * block_size * 8);
  history_states_.reserve(kMaxHistorySize);
}

double SerfQtCurveCompressor::CurvePredict() const {
  if (history_states_.empty()) {
    return 2.0;  // default value
  }
  
  size_t history_size = history_states_.size();
  
  if (history_size == 1) {
    // 只有一个历史点，使用零预测
    return history_states_[0].value;
  }
  
  if (history_size == 2) {
    // 两个历史点，使用线性预测
    double current = history_states_[1].value;
    double velocity = history_states_[1].velocity;
    return current + velocity;
  }
  
  // 完全对齐TrajCompress-SP的CP预测器逻辑
  double current = history_states_[history_size - 1].value;
  
  if (history_size >= 5) {
    // 三阶预测：使用5个点，包含速度平滑和jerk
    double v1 = history_states_[history_size - 1].velocity;
    double v2 = history_states_[history_size - 2].velocity;
    double v3 = history_states_[history_size - 3].velocity;
    double v4 = history_states_[history_size - 4].velocity;
    
    // 速度平滑：轻微平滑最近3个速度，权重偏向最近
    double smoothed_velocity = 0.45 * v1 + 0.35 * v2 + 0.20 * v3;
    
    // 加速度：最近2个
    double a1 = v1 - v2;
    double a2 = v2 - v3;
    
    // Jerk（加速度变化率）：捕捉加速度趋势
    double jerk = a1 - a2;
    
    // 三阶预测：速度 + 加速度 + 0.5*jerk（jerk用系数0.5以避免过拟合）
    return current + smoothed_velocity + a1 + 0.5 * jerk;
    
  } else if (history_size == 4) {
    // 二阶预测（平滑版）
    double v1 = history_states_[history_size - 1].velocity;
    double v2 = history_states_[history_size - 2].velocity;
    double v3 = history_states_[history_size - 3].velocity;
    
    double smoothed_velocity = 0.45 * v1 + 0.35 * v2 + 0.20 * v3;
    double acceleration = v1 - v2;
    
    return current + smoothed_velocity + acceleration;
    
  } else {  // history_size == 3
    // 基础二阶预测
    double velocity = history_states_[history_size - 1].velocity;
    double prev_velocity = history_states_[history_size - 2].velocity;
    double acceleration = velocity - prev_velocity;
    
    return current + velocity + acceleration;
  }
}

void SerfQtCurveCompressor::AddValue(double v) {
  if (first_) {
    first_ = false;
    // 写入头部信息
    compressed_size_in_bits_ += output_bit_stream_->WriteInt(kBlockSize, 16);
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kMaxDiff), 64);
    
    // 第一个点：基于默认值(2.0)的量化
    long q = static_cast<long>(std::round((v - 2.0) / (2 * kMaxDiff)));
    double recoverValue = 2.0 + 2 * kMaxDiff * static_cast<double>(q);
    compressed_size_in_bits_ += EliasGammaCodec::Encode(
        ZigZagCodec::Encode(static_cast<int64_t>(q)) + 1,
        output_bit_stream_.get());
    
    // 初始化历史状态（第一个点速度为0）
    history_states_.emplace_back(recoverValue, 0.0);
    return;
  }
  
  // 使用曲线预测
  double predicted = CurvePredict();
  
  // 量化
  long q = static_cast<long>(std::round((v - predicted) / (2 * kMaxDiff)));
  double recoverValue = predicted + 2 * kMaxDiff * static_cast<double>(q);
  compressed_size_in_bits_ += EliasGammaCodec::Encode(
      ZigZagCodec::Encode(static_cast<int64_t>(q)) + 1,
      output_bit_stream_.get());
  
  // 计算速度（当前重构值 - 上一个重构值）
  double current_reconstructed = history_states_.back().value;
  double velocity = recoverValue - current_reconstructed;
  
  // 更新历史状态
  history_states_.emplace_back(recoverValue, velocity);
  
  // 保持历史状态大小不超过限制
  if (history_states_.size() > kMaxHistorySize) {
    history_states_.erase(history_states_.begin());
  }
}

Array<uint8_t> SerfQtCurveCompressor::compressed_bytes() {
  return compressed_bytes_;
}

void SerfQtCurveCompressor::Close() {
  output_bit_stream_->Flush();
  compressed_bytes_ = output_bit_stream_->GetBuffer(std::ceil(compressed_size_in_bits_ / 8.0));
  output_bit_stream_->Refresh();
  first_ = true;
  history_states_.clear();
  stored_compressed_size_in_bits_ = compressed_size_in_bits_;
  compressed_size_in_bits_ = 0;
}

long SerfQtCurveCompressor::get_compressed_size_in_bits() const {
  return stored_compressed_size_in_bits_;
}
