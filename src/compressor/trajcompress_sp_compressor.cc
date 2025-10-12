#include "compressor/trajcompress_sp_compressor.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/input_bit_stream.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// ==================== 压缩器实现 ====================

TrajCompressSPCompressor::TrajCompressSPCompressor(int block_size, double epsilon)
    : kBlockSize(block_size), kEpsilon(epsilon), kQuantStep(epsilon) {
    output_bit_stream_ = std::make_unique<OutputBitStream>(2 * block_size * 8);
    history_states_.reserve(kMaxHistorySize);
}

void TrajCompressSPCompressor::AddGpsPoint(const GpsPoint& point) {
    stats_.total_points++;
    
    if (first_point_) {
        ProcessFirstPoint(point);
        return;
    }
    
    // 并行预测
    GpsPoint pred_ldr, pred_cp, pred_zp;
    ParallelPredict(pred_ldr, pred_cp, pred_zp);
    
    // 选择最优预测器
    GpsPoint best_prediction;
    PredictorType best_predictor = SelectBestPredictor(point, pred_ldr, pred_cp, pred_zp, best_prediction);
    
    // 编码预测器标志和量化误差
    EncodePrediction(best_predictor, point, best_prediction);
    
    // 更新统计
    switch (best_predictor) {
        case PREDICTOR_LDR: stats_.ldr_count++; break;
        case PREDICTOR_CP: stats_.cp_count++; break;
        case PREDICTOR_ZP: stats_.zp_count++; break;
    }
}

void TrajCompressSPCompressor::Close() {
    output_bit_stream_->Flush();
    stats_.total_bits = compressed_size_in_bits_;
}

Array<uint8_t> TrajCompressSPCompressor::GetCompressedData() {
    int byte_length = (compressed_size_in_bits_ + 7) / 8;
    return output_bit_stream_->GetBuffer(byte_length);
}

void TrajCompressSPCompressor::ProcessFirstPoint(const GpsPoint& point) {
    first_point_ = false;
    
    // 写入头部信息
    compressed_size_in_bits_ += output_bit_stream_->WriteInt(kBlockSize, 16);
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kEpsilon), 64);
    
    // 写入第一个点的原始坐标
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(point.longitude), 64);
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(point.latitude), 64);
    
    // 初始化重构状态
    current_reconstructed_point_ = point;
    
    // 初始化历史状态（第一个点的速度为零）
    history_states_.emplace_back(point, GpsPoint(0, 0));
}

void TrajCompressSPCompressor::ParallelPredict(GpsPoint& pred_ldr, GpsPoint& pred_cp, GpsPoint& pred_zp) {
    // 零预测（ZP）：预测当前点等于上一个点
    pred_zp = current_reconstructed_point_;
    
    if (history_states_.size() < 2) {
        // 历史不足，所有预测器都退化为零预测
        pred_ldr = current_reconstructed_point_;
        pred_cp = current_reconstructed_point_;
        return;
    }
    
    // 线性航位推算（LDR）：假设匀速直线运动
    // velocity = P_{i-1} - P_{i-2}
    // predicted = P_{i-1} + velocity
    GpsPoint velocity = history_states_[history_states_.size() - 1].velocity;
    pred_ldr = current_reconstructed_point_ + velocity;
    
    // 曲线预测（CP）：假设匀加速运动
    // acceleration = velocity_{i-1} - velocity_{i-2}
    // predicted = P_{i-1} + velocity_{i-1} + acceleration
    if (history_states_.size() >= 3) {
        GpsPoint prev_velocity = history_states_[history_states_.size() - 2].velocity;
        GpsPoint acceleration = velocity - prev_velocity;
        pred_cp = current_reconstructed_point_ + velocity + acceleration;
    } else {
        // 历史不足，曲线预测退化为线性预测
        pred_cp = pred_ldr;
    }
}

TrajCompressSPCompressor::PredictorType TrajCompressSPCompressor::SelectBestPredictor(
    const GpsPoint& current_point,
    const GpsPoint& pred_ldr,
    const GpsPoint& pred_cp,
    const GpsPoint& pred_zp,
    GpsPoint& best_prediction) {
    
    // 计算每个预测器的预测误差
    double error_ldr = CalculateDistance(current_point, pred_ldr);
    double error_cp = CalculateDistance(current_point, pred_cp);
    double error_zp = CalculateDistance(current_point, pred_zp);
    
    // 选择误差最小的预测器
    PredictorType best_predictor;
    double best_error;
    
    if (error_ldr <= error_cp && error_ldr <= error_zp) {
        best_prediction = pred_ldr;
        best_predictor = PREDICTOR_LDR;
        best_error = error_ldr;
    } else if (error_cp <= error_zp) {
        best_prediction = pred_cp;
        best_predictor = PREDICTOR_CP;
        best_error = error_cp;
    } else {
        best_prediction = pred_zp;
        best_predictor = PREDICTOR_ZP;
        best_error = error_zp;
    }
    
    // 记录预测误差统计
    stats_.total_prediction_error += best_error;
    stats_.max_prediction_error = std::max(stats_.max_prediction_error, best_error);
    stats_.prediction_errors.push_back(best_error);
    
    return best_predictor;
}

void TrajCompressSPCompressor::EncodePrediction(PredictorType predictor,
                                               const GpsPoint& current_point,
                                               const GpsPoint& predicted_point) {
    // 1. 编码预测器标志（2 bits）
    // 00 = LDR, 01 = CP, 10 = ZP, 11 = reserved
    switch (predictor) {
        case PREDICTOR_LDR:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false);
            stats_.predictor_flag_bits += 2;
            break;
        case PREDICTOR_CP:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            stats_.predictor_flag_bits += 2;
            break;
        case PREDICTOR_ZP:
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(true);
            compressed_size_in_bits_ += output_bit_stream_->WriteBit(false);
            stats_.predictor_flag_bits += 2;
            break;
    }
    
    // 2. 计算预测误差（二维向量）
    GpsPoint delta = current_point - predicted_point;
    
    // 3. 量化误差（使用epsilon作为量化步长，确保重构误差≤epsilon）
    int64_t quantized_delta_lon = static_cast<int64_t>(std::round(delta.longitude / kEpsilon));
    int64_t quantized_delta_lat = static_cast<int64_t>(std::round(delta.latitude / kEpsilon));
    
    // 4. ZigZag + Elias Gamma 编码量化误差
    int bits_lon = EliasGammaCodec::Encode(
        ZigZagCodec::Encode(quantized_delta_lon) + 1,
        output_bit_stream_.get()
    );
    int bits_lat = EliasGammaCodec::Encode(
        ZigZagCodec::Encode(quantized_delta_lat) + 1,
        output_bit_stream_.get()
    );
    
    compressed_size_in_bits_ += bits_lon + bits_lat;
    stats_.quantization_bits += bits_lon + bits_lat;
    
    // 5. 重构点（与解压器保持同步）
    GpsPoint reconstructed_delta(
        quantized_delta_lon * kEpsilon,
        quantized_delta_lat * kEpsilon
    );
    GpsPoint reconstructed_point = predicted_point + reconstructed_delta;
    
    // 6. 更新历史状态
    UpdateHistory(reconstructed_point);
}

void TrajCompressSPCompressor::UpdateHistory(const GpsPoint& reconstructed_point) {
    // 计算速度向量
    GpsPoint velocity = reconstructed_point - current_reconstructed_point_;
    
    // 更新当前重构点
    current_reconstructed_point_ = reconstructed_point;
    
    // 添加到历史状态
    history_states_.emplace_back(reconstructed_point, velocity);
    
    // 保持历史状态大小不超过限制
    if (history_states_.size() > kMaxHistorySize) {
        history_states_.erase(history_states_.begin());
    }
}

double TrajCompressSPCompressor::CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) const {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

void TrajCompressSPCompressor::CompressionStats::PrintStats() const {
    if (total_points == 0) {
        std::cout << "没有数据" << std::endl;
        return;
    }
    
    std::cout << "\n=== TrajCompress-SP 压缩统计 ===" << std::endl;
    std::cout << "总点数: " << total_points << std::endl;
    
    std::cout << "\n=== 预测器使用分布 ===" << std::endl;
    int moving_points = total_points - 1;  // 除去第一个点
    if (moving_points > 0) {
        std::cout << "LDR (线性预测): " << ldr_count << " (" 
                  << std::fixed << std::setprecision(2)
                  << (100.0 * ldr_count / moving_points) << "%)" << std::endl;
        std::cout << "CP (曲线预测):  " << cp_count << " (" 
                  << (100.0 * cp_count / moving_points) << "%)" << std::endl;
        std::cout << "ZP (零预测):    " << zp_count << " (" 
                  << (100.0 * zp_count / moving_points) << "%)" << std::endl;
    }
    
    std::cout << "\n=== 预测精度统计 ===" << std::endl;
    if (moving_points > 0) {
        double avg_error = total_prediction_error / moving_points;
        std::cout << "平均预测误差: " << std::scientific << std::setprecision(6) << avg_error 
                  << " 度 (" << std::fixed << std::setprecision(2) << (avg_error * 111000) << " 米)" << std::endl;
        std::cout << "最大预测误差: " << std::scientific << max_prediction_error 
                  << " 度 (" << std::fixed << (max_prediction_error * 111000) << " 米)" << std::endl;
    }
    
    std::cout << "\n=== 编码成本分析 ===" << std::endl;
    std::cout << "预测器标志: " << std::setw(8) << predictor_flag_bits << " bits (" 
              << std::setprecision(1) << (100.0 * predictor_flag_bits / total_bits) << "%)" << std::endl;
    std::cout << "量化数据:   " << std::setw(8) << quantization_bits << " bits (" 
              << (100.0 * quantization_bits / total_bits) << "%)" << std::endl;
    std::cout << "总计:       " << std::setw(8) << total_bits << " bits" << std::endl;
    
    std::cout << "\n=== 压缩效率 ===" << std::endl;
    double avg_bits_per_point = static_cast<double>(total_bits) / total_points;
    std::cout << "平均每点: " << std::setprecision(2) << avg_bits_per_point << " bits/点" << std::endl;
    
    // 原始数据：每个点2个double = 128 bits
    double compression_ratio = 128.0 / avg_bits_per_point;
    std::cout << "压缩比: " << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    std::cout << "压缩率: " << std::setprecision(1) << (100.0 - 100.0 / compression_ratio) << "%" << std::endl;
}

void TrajCompressSPCompressor::CompressionStats::PrintDetailedStats() const {
    PrintStats();
    
    std::cout << "\n=== 详细分析 ===" << std::endl;
    
    int moving_points = total_points - 1;
    
    // 预测器效果分析
    if (moving_points > 0) {
        std::cout << "\n预测器选择分析:" << std::endl;
        std::cout << "  线性预测占比: " << std::setprecision(1) 
                  << (100.0 * ldr_count / moving_points) << "% - ";
        if (ldr_count > moving_points * 0.6) {
            std::cout << "主导预测器，轨迹线性度高，适合高采样率" << std::endl;
        } else {
            std::cout << "使用正常" << std::endl;
        }
        
        std::cout << "  曲线预测占比: " << (100.0 * cp_count / moving_points) << "% - ";
        if (cp_count > moving_points * 0.3) {
            std::cout << "高转弯率，轨迹曲线性明显" << std::endl;
        } else {
            std::cout << "使用正常" << std::endl;
        }
        
        std::cout << "  零预测占比: " << (100.0 * zp_count / moving_points) << "% - ";
        if (zp_count > moving_points * 0.3) {
            std::cout << "预测失效率较高，可能存在突变或静止点" << std::endl;
        } else {
            std::cout << "预测效果良好" << std::endl;
        }
    }
    
    // 预测误差分布分析
    if (!prediction_errors.empty()) {
        std::cout << "\n预测误差分布:" << std::endl;
        
        // 计算百分位数
        std::vector<double> sorted_errors = prediction_errors;
        std::sort(sorted_errors.begin(), sorted_errors.end());
        
        size_t p50_idx = sorted_errors.size() * 0.50;
        size_t p90_idx = sorted_errors.size() * 0.90;
        size_t p95_idx = sorted_errors.size() * 0.95;
        size_t p99_idx = sorted_errors.size() * 0.99;
        
        std::cout << "  P50 (中位数): " << std::scientific << sorted_errors[p50_idx] 
                  << " 度 (" << std::fixed << std::setprecision(2) 
                  << (sorted_errors[p50_idx] * 111000) << " 米)" << std::endl;
        std::cout << "  P90: " << std::scientific << sorted_errors[p90_idx] 
                  << " 度 (" << std::fixed << (sorted_errors[p90_idx] * 111000) << " 米)" << std::endl;
        std::cout << "  P95: " << std::scientific << sorted_errors[p95_idx] 
                  << " 度 (" << std::fixed << (sorted_errors[p95_idx] * 111000) << " 米)" << std::endl;
        std::cout << "  P99: " << std::scientific << sorted_errors[p99_idx] 
                  << " 度 (" << std::fixed << (sorted_errors[p99_idx] * 111000) << " 米)" << std::endl;
    }
    
    // 编码效率分析
    std::cout << "\n编码效率分析:" << std::endl;
    if (moving_points > 0) {
        double avg_pred_bits = static_cast<double>(predictor_flag_bits) / moving_points;
        double avg_quant_bits = static_cast<double>(quantization_bits) / moving_points;
        
        std::cout << "  平均预测器标志: " << std::setprecision(2) << avg_pred_bits << " bits/点" << std::endl;
        std::cout << "  平均量化数据: " << avg_quant_bits << " bits/点" << std::endl;
        std::cout << "  平均总编码: " << (avg_pred_bits + avg_quant_bits) << " bits/点" << std::endl;
    }
}

// ==================== 解压缩器实现 ====================

TrajCompressSPDecompressor::TrajCompressSPDecompressor(uint8_t* compressed_data, int data_size) {
    input_bit_stream_ = std::make_unique<InputBitStream>(compressed_data, data_size);
    ReadHeader();
}

void TrajCompressSPDecompressor::ReadHeader() {
    // 读取头部信息
    block_size_ = input_bit_stream_->ReadInt(16);
    epsilon_ = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    quant_step_ = epsilon_;
    
    // 读取第一个点的原始坐标
    double first_lon = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    double first_lat = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    
    // 初始化重构状态
    current_reconstructed_point_ = GpsPoint(first_lon, first_lat);
    history_states_.emplace_back(current_reconstructed_point_, GpsPoint(0, 0));
}

bool TrajCompressSPDecompressor::ReadNextPoint(GpsPoint& point) {
    if (first_point_) {
        // 返回第一个点
        first_point_ = false;
        point = current_reconstructed_point_;
        return true;
    }
    
    // 尝试读取预测器标志（2 bits）
    // 如果读取失败，说明已经到达数据流末尾
    try {
        bool bit1 = input_bit_stream_->ReadBit();
        bool bit2 = input_bit_stream_->ReadBit();
        
        PredictorType predictor;
        if (!bit1 && !bit2) {
            predictor = TrajCompressSPCompressor::PREDICTOR_LDR;
        } else if (!bit1 && bit2) {
            predictor = TrajCompressSPCompressor::PREDICTOR_CP;
        } else {  // bit1 && !bit2
            predictor = TrajCompressSPCompressor::PREDICTOR_ZP;
        }
        
        // 并行预测
        GpsPoint pred_ldr, pred_cp, pred_zp;
        ParallelPredict(pred_ldr, pred_cp, pred_zp);
        
        // 选择对应的预测结果
        GpsPoint predicted_point;
        switch (predictor) {
            case TrajCompressSPCompressor::PREDICTOR_LDR: predicted_point = pred_ldr; break;
            case TrajCompressSPCompressor::PREDICTOR_CP: predicted_point = pred_cp; break;
            case TrajCompressSPCompressor::PREDICTOR_ZP: predicted_point = pred_zp; break;
        }
        
        // 读取量化误差
        int64_t quantized_delta_lon = ZigZagCodec::Decode(
            EliasGammaCodec::Decode(input_bit_stream_.get()) - 1
        );
        int64_t quantized_delta_lat = ZigZagCodec::Decode(
            EliasGammaCodec::Decode(input_bit_stream_.get()) - 1
        );
        
        // 重构点
        GpsPoint reconstructed_delta(
            quantized_delta_lon * epsilon_,
            quantized_delta_lat * epsilon_
        );
        GpsPoint reconstructed_point = predicted_point + reconstructed_delta;
    
        // 更新历史状态
        UpdateHistory(reconstructed_point);
        
        point = reconstructed_point;
        return true;
    } catch (...) {
        // 读取失败，到达数据流末尾
        return false;
    }
}

std::vector<TrajCompressSPDecompressor::GpsPoint> TrajCompressSPDecompressor::ReadAllPoints() {
    std::vector<GpsPoint> points;
    GpsPoint point;
    
    while (ReadNextPoint(point)) {
        points.push_back(point);
    }
    
    return points;
}

void TrajCompressSPDecompressor::ParallelPredict(GpsPoint& pred_ldr, GpsPoint& pred_cp, GpsPoint& pred_zp) {
    // 零预测（ZP）
    pred_zp = current_reconstructed_point_;
    
    if (history_states_.size() < 2) {
        pred_ldr = current_reconstructed_point_;
        pred_cp = current_reconstructed_point_;
        return;
    }
    
    // 线性航位推算（LDR）
    GpsPoint velocity = history_states_[history_states_.size() - 1].velocity;
    pred_ldr = current_reconstructed_point_ + velocity;
    
    // 曲线预测（CP）
    if (history_states_.size() >= 3) {
        GpsPoint prev_velocity = history_states_[history_states_.size() - 2].velocity;
        GpsPoint acceleration = velocity - prev_velocity;
        pred_cp = current_reconstructed_point_ + velocity + acceleration;
    } else {
        pred_cp = pred_ldr;
    }
}

void TrajCompressSPDecompressor::UpdateHistory(const GpsPoint& reconstructed_point) {
    // 计算速度向量
    GpsPoint velocity = reconstructed_point - current_reconstructed_point_;
    
    // 更新当前重构点
    current_reconstructed_point_ = reconstructed_point;
    
    // 添加到历史状态
    history_states_.emplace_back(reconstructed_point, velocity);
    
    // 保持历史状态大小不超过限制
    if (history_states_.size() > kMaxHistorySize) {
        history_states_.erase(history_states_.begin());
    }
}
