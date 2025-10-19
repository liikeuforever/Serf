#include "compressor/trajcompress_sp_adaptive_compressor.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/input_bit_stream.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// ==================== 压缩器实现 ====================

TrajCompressSPAdaptiveCompressor::TrajCompressSPAdaptiveCompressor(
    int block_size, double epsilon,
    int cost_window_size,
    int stability_margin,
    bool clear_after_switch,
    int evaluation_interval)
    : kBlockSize(block_size), 
      kEpsilon(epsilon * 0.999), 
      kQuantStep(2 * epsilon * 0.999),
      kCostWindowSize(cost_window_size),
      kSwitchCost(4),  // 固定值：'111' + '0'/'1'
      kStabilityMargin(stability_margin),
      kEvaluationInterval(evaluation_interval),
      kClearWindowAfterSwitch(clear_after_switch) {
    output_bit_stream_ = std::make_unique<OutputBitStream>(2 * block_size * 8);
    history_states_.reserve(kMaxHistorySize);
    predictor_window_.reserve(kSlidingWindowSize);
    mode_evaluation_window_.reserve(kModeEvaluationWindow);
    
    // 初始化 Huffman 编码表（假设初始频率分布）
    predictor_frequency_[PREDICTOR_LDR] = 60;  // LDR最常用
    predictor_frequency_[PREDICTOR_CP] = 10;   // CP较少
    predictor_frequency_[PREDICTOR_ZP] = 30;   // ZP中等
    UpdateHuffmanCodes();
}

void TrajCompressSPAdaptiveCompressor::AddGpsPoint(const GpsPoint& point) {
    stats_.total_points++;
    
    if (first_point_) {
        ProcessFirstPoint(point);
        return;
    }
    
    // === 1. 并行计算所有预测器的成本（双模型并行追踪） ===
    GpsPoint pred_ldr, pred_cp, pred_zp;
    ParallelPredict(pred_ldr, pred_cp, pred_zp);
    
    // 计算每个预测器的误差编码成本
    GpsPoint error_ldr = point - pred_ldr;
    GpsPoint error_cp = point - pred_cp;
    GpsPoint error_zp = point - pred_zp;
    
    int cost_ldr_error = EstimateErrorEncodingCost(error_ldr);
    int cost_cp_error = EstimateErrorEncodingCost(error_cp);
    int cost_zp_error = EstimateErrorEncodingCost(error_zp);
    
    // 找出最优预测器（基于总成本：Huffman标志位 + 误差编码）
    int cost_ldr_total = GetHuffmanBitCost(PREDICTOR_LDR) + cost_ldr_error;
    int cost_cp_total = GetHuffmanBitCost(PREDICTOR_CP) + cost_cp_error;
    int cost_zp_total = GetHuffmanBitCost(PREDICTOR_ZP) + cost_zp_error;
    
    PredictorType best_predictor;
    GpsPoint best_prediction;
    int best_cost;
    
    if (cost_ldr_total <= cost_cp_total && cost_ldr_total <= cost_zp_total) {
        best_predictor = PREDICTOR_LDR;
        best_prediction = pred_ldr;
        best_cost = cost_ldr_total;
    } else if (cost_cp_total <= cost_zp_total) {
        best_predictor = PREDICTOR_CP;
        best_prediction = pred_cp;
        best_cost = cost_cp_total;
    } else {
        best_predictor = PREDICTOR_ZP;
        best_prediction = pred_zp;
        best_cost = cost_zp_total;
    }
    
    // === 2. 计算两种模型的成本（用于智能切换决策） ===
    int multi_model_cost = best_cost;                    // 多预测器模型：最优预测器的成本
    int ldr_only_model_cost = cost_ldr_error;            // LDR-Only模型：LDR误差成本（无标志位）
    
    // 更新成本窗口
    UpdateCostWindows(multi_model_cost, ldr_only_model_cost);
    
    // === 3. 根据当前模式进行实际编码 ===
    if (current_mode_ == MODE_LDR_ONLY) {
        EncodeLDROnly(point);
        stats_.ldr_only_mode_points++;
    } else {  // MODE_MULTI_PREDICTOR
        EncodeMultiPredictor(point);
        stats_.multi_predictor_mode_points++;
    }
    
    // === 4. 周期性地基于成本进行模式切换决策（无魔法数字） ===
    if (stats_.total_points % kEvaluationInterval == 0 && stats_.total_points > kCostWindowSize) {
        EvaluateAndSwitchModeBasedOnCost();
    }
}

void TrajCompressSPAdaptiveCompressor::EncodeMultiPredictor(const GpsPoint& point) {
    // 并行预测
    GpsPoint pred_ldr, pred_cp, pred_zp;
    ParallelPredict(pred_ldr, pred_cp, pred_zp);
    
    // 基于成本选择最优预测器（策略二）
    GpsPoint best_prediction;
    int best_cost;
    PredictorType best_predictor = SelectBestPredictorByCost(
        point, pred_ldr, pred_cp, pred_zp, best_prediction, best_cost);
    
    // 编码预测器标志和量化误差
    EncodePrediction(best_predictor, point, best_prediction);
    
    // 更新最后使用的预测器
    last_used_predictor_ = best_predictor;
    
    // 添加到模式评估窗口
    mode_evaluation_window_.push_back(best_predictor);
    if (mode_evaluation_window_.size() > kModeEvaluationWindow) {
        mode_evaluation_window_.erase(mode_evaluation_window_.begin());
    }
    
    // 更新统计
    switch (best_predictor) {
        case PREDICTOR_LDR: stats_.ldr_count++; break;
        case PREDICTOR_CP: stats_.cp_count++; break;
        case PREDICTOR_ZP: stats_.zp_count++; break;
    }
}

void TrajCompressSPAdaptiveCompressor::EncodeLDROnly(const GpsPoint& point) {
    // 强制使用LDR预测
    GpsPoint pred_ldr, pred_cp, pred_zp;
    ParallelPredict(pred_ldr, pred_cp, pred_zp);
    
    // 直接编码LDR的量化误差，不写预测器标志
    GpsPoint delta = point - pred_ldr;
    
    // 量化误差
    int64_t quantized_delta_lon = static_cast<int64_t>(std::round(delta.longitude / kQuantStep));
    int64_t quantized_delta_lat = static_cast<int64_t>(std::round(delta.latitude / kQuantStep));
    
    // 编码量化误差（使用ZigZag + Elias Gamma）
    int bits_before = compressed_size_in_bits_;
    compressed_size_in_bits_ += EliasGammaCodec::Encode(
        ZigZagCodec::Encode(quantized_delta_lon) + 1, output_bit_stream_.get());
    compressed_size_in_bits_ += EliasGammaCodec::Encode(
        ZigZagCodec::Encode(quantized_delta_lat) + 1, output_bit_stream_.get());
    stats_.quantized_data_bits += (compressed_size_in_bits_ - bits_before);
    
    // 重构点
    GpsPoint reconstructed_delta(
        quantized_delta_lon * kQuantStep,
        quantized_delta_lat * kQuantStep
    );
    GpsPoint reconstructed_point = pred_ldr + reconstructed_delta;
    
    // 更新重构状态
    UpdateReconstructedState(reconstructed_point);
    
    // 更新统计
    stats_.ldr_count++;
    double error = CalculateDistance(point, reconstructed_point);
    stats_.total_prediction_error += error;
    stats_.max_prediction_error = std::max(stats_.max_prediction_error, error);
    stats_.prediction_errors.push_back(error);
}

// 旧的基于魔法数字的模式切换方法已被基于成本的智能切换替代

void TrajCompressSPAdaptiveCompressor::EncodeModeSwitch(CompressionMode new_mode) {
    // 编码逃逸码：111 (3 bits)
    output_bit_stream_->WriteBit(true);
    output_bit_stream_->WriteBit(true);
    output_bit_stream_->WriteBit(true);
    
    // 编码模式切换标志：0 = LDR-Only, 1 = Multi-Predictor (1 bit)
    output_bit_stream_->WriteBit(new_mode == MODE_MULTI_PREDICTOR);
    
    compressed_size_in_bits_ += 4;
    stats_.mode_switch_bits += 4;
    stats_.mode_switch_count++;
}

TrajCompressSPAdaptiveCompressor::PredictorType TrajCompressSPAdaptiveCompressor::SelectBestPredictorByCost(
    const GpsPoint& current_point,
    const GpsPoint& pred_ldr,
    const GpsPoint& pred_cp,
    const GpsPoint& pred_zp,
    GpsPoint& best_prediction,
    int& best_cost) {
    
    // 计算每个预测器的误差
    GpsPoint error_ldr = current_point - pred_ldr;
    GpsPoint error_cp = current_point - pred_cp;
    GpsPoint error_zp = current_point - pred_zp;
    
    // 估算每个预测器的总成本（Huffman标志位 + 误差编码）
    int cost_ldr = GetHuffmanBitCost(PREDICTOR_LDR) + EstimateErrorEncodingCost(error_ldr);
    int cost_cp = GetHuffmanBitCost(PREDICTOR_CP) + EstimateErrorEncodingCost(error_cp);
    int cost_zp = GetHuffmanBitCost(PREDICTOR_ZP) + EstimateErrorEncodingCost(error_zp);
    
    // 选择成本最低的预测器
    if (cost_ldr <= cost_cp && cost_ldr <= cost_zp) {
        best_prediction = pred_ldr;
        best_cost = cost_ldr;
        return PREDICTOR_LDR;
    } else if (cost_cp <= cost_zp) {
        best_prediction = pred_cp;
        best_cost = cost_cp;
        return PREDICTOR_CP;
    } else {
        best_prediction = pred_zp;
        best_cost = cost_zp;
        return PREDICTOR_ZP;
    }
}

int TrajCompressSPAdaptiveCompressor::EstimateEliasGammaBits(int64_t value) const {
    if (value <= 0) return 1;  // 最小编码长度
    int log2_val = static_cast<int>(std::floor(std::log2(value)));
    return 2 * log2_val + 1;
}

int TrajCompressSPAdaptiveCompressor::EstimateErrorEncodingCost(const GpsPoint& error) const {
    // 量化误差
    int64_t quantized_lon = static_cast<int64_t>(std::round(error.longitude / kQuantStep));
    int64_t quantized_lat = static_cast<int64_t>(std::round(error.latitude / kQuantStep));
    
    // ZigZag编码
    uint64_t zigzag_lon = ZigZagCodec::Encode(quantized_lon);
    uint64_t zigzag_lat = ZigZagCodec::Encode(quantized_lat);
    
    // 估算Elias Gamma编码的比特数
    int bits_lon = EstimateEliasGammaBits(zigzag_lon + 1);
    int bits_lat = EstimateEliasGammaBits(zigzag_lat + 1);
    
    return bits_lon + bits_lat;
}

// ========== 成本窗口管理 ==========

void TrajCompressSPAdaptiveCompressor::UpdateCostWindows(int multi_cost, int ldr_only_cost) {
    // 入队新成本
    point_costs_multi_.push_back(multi_cost);
    window_total_cost_multi_ += multi_cost;
    
    point_costs_ldr_only_.push_back(ldr_only_cost);
    window_total_cost_ldr_only_ += ldr_only_cost;
    
    // 如果窗口满了，出队旧成本
    if (point_costs_multi_.size() > kCostWindowSize) {
        window_total_cost_multi_ -= point_costs_multi_.front();
        point_costs_multi_.pop_front();
        
        window_total_cost_ldr_only_ -= point_costs_ldr_only_.front();
        point_costs_ldr_only_.pop_front();
    }
}

// ========== 基于成本的智能模式切换（无魔法数字） ==========

void TrajCompressSPAdaptiveCompressor::EvaluateAndSwitchModeBasedOnCost() {
    // 确保有足够的数据进行决策
    if (point_costs_multi_.size() < static_cast<size_t>(kCostWindowSize)) return;
    
    if (current_mode_ == MODE_MULTI_PREDICTOR) {
        // 检查是否切换到 LDR-Only 更划算
        // 条件：LDR-Only成本 < Multi成本 - 切换成本 - 稳定边际
        if (window_total_cost_ldr_only_ < window_total_cost_multi_ - kSwitchCost - kStabilityMargin) {
            EncodeModeSwitch(MODE_LDR_ONLY);
            current_mode_ = MODE_LDR_ONLY;
            
            // 根据配置决定是否清空成本窗口
            if (kClearWindowAfterSwitch) {
                point_costs_multi_.clear();
                point_costs_ldr_only_.clear();
                window_total_cost_multi_ = 0;
                window_total_cost_ldr_only_ = 0;
            }
            // 否则保持窗口连续滑动，以便更快响应模式变化
        }
    } else {  // current_mode_ == MODE_LDR_ONLY
        // 检查是否切换回 Multi-Predictor 更划算
        // 条件：Multi成本 < LDR-Only成本 - 切换成本 - 稳定边际
        if (window_total_cost_multi_ < window_total_cost_ldr_only_ - kSwitchCost - kStabilityMargin) {
            EncodeModeSwitch(MODE_MULTI_PREDICTOR);
            current_mode_ = MODE_MULTI_PREDICTOR;
            
            // 根据配置决定是否清空成本窗口
            if (kClearWindowAfterSwitch) {
                point_costs_multi_.clear();
                point_costs_ldr_only_.clear();
                window_total_cost_multi_ = 0;
                window_total_cost_ldr_only_ = 0;
            }
            // 否则保持窗口连续滑动，以便更快响应模式变化
        }
    }
}

// ========== Huffman 编码相关 ==========

int TrajCompressSPAdaptiveCompressor::GetHuffmanBitCost(PredictorType predictor) const {
    return huffman_codes_[predictor].length;
}

void TrajCompressSPAdaptiveCompressor::UpdateHuffmanCodes() {
    // 对预测器按频率排序
    struct PredictorFreq {
        PredictorType type;
        int frequency;
    };
    
    std::vector<PredictorFreq> freq_list = {
        {PREDICTOR_LDR, predictor_frequency_[PREDICTOR_LDR]},
        {PREDICTOR_CP, predictor_frequency_[PREDICTOR_CP]},
        {PREDICTOR_ZP, predictor_frequency_[PREDICTOR_ZP]}
    };
    
    std::sort(freq_list.begin(), freq_list.end(), 
              [](const PredictorFreq& a, const PredictorFreq& b) {
                  return a.frequency > b.frequency;
              });
    
    // 分配Huffman编码：最高频率用0 (1 bit)，次高用10 (2 bits)，最低用11 (2 bits)
    huffman_codes_[freq_list[0].type] = HuffmanCode({false});               // 0
    huffman_codes_[freq_list[1].type] = HuffmanCode({true, false});         // 10
    huffman_codes_[freq_list[2].type] = HuffmanCode({true, true});          // 11
}

void TrajCompressSPAdaptiveCompressor::EncodeWithHuffman(PredictorType predictor) {
    const HuffmanCode& code = huffman_codes_[predictor];
    for (bool bit : code.bits) {
        output_bit_stream_->WriteBit(bit);
    }
    compressed_size_in_bits_ += code.length;
    
    // 添加到滑动窗口并更新频率
    AddPredictorToWindow(predictor);
}

void TrajCompressSPAdaptiveCompressor::AddPredictorToWindow(PredictorType predictor) {
    predictor_window_.push_back(predictor);
    predictor_frequency_[predictor]++;
    
    if (predictor_window_.size() > kSlidingWindowSize) {
        PredictorType old_predictor = predictor_window_.front();
        predictor_window_.erase(predictor_window_.begin());
        predictor_frequency_[old_predictor]--;
    }
    
    // 每隔一定数量的点更新Huffman编码
    if (predictor_window_.size() % 100 == 0) {
        UpdateHuffmanCodes();
    }
}

void TrajCompressSPAdaptiveCompressor::Close() {
    output_bit_stream_->Flush();
    stats_.total_bits = compressed_size_in_bits_;
}

Array<uint8_t> TrajCompressSPAdaptiveCompressor::GetCompressedData() {
    int byte_length = (compressed_size_in_bits_ + 7) / 8;
    return output_bit_stream_->GetBuffer(byte_length);
}

void TrajCompressSPAdaptiveCompressor::ProcessFirstPoint(const GpsPoint& point) {
    first_point_ = false;
    
    // 写入头部信息
    compressed_size_in_bits_ += output_bit_stream_->WriteInt(kBlockSize, 16);
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kEpsilon), 64);
    
    // 写入第一个点的原始坐标
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(point.longitude), 64);
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(point.latitude), 64);
    
    // 初始化重构状态
    current_reconstructed_point_ = point;
    history_states_.emplace_back(point, GpsPoint(0, 0));
}

void TrajCompressSPAdaptiveCompressor::ParallelPredict(GpsPoint& pred_ldr, GpsPoint& pred_cp, GpsPoint& pred_zp) {
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
    if (history_states_.size() >= 5) {
        GpsPoint v1 = history_states_[history_states_.size() - 1].velocity;
        GpsPoint v2 = history_states_[history_states_.size() - 2].velocity;
        GpsPoint v3 = history_states_[history_states_.size() - 3].velocity;
        
        GpsPoint smoothed_velocity(
            0.45 * v1.longitude + 0.35 * v2.longitude + 0.20 * v3.longitude,
            0.45 * v1.latitude + 0.35 * v2.latitude + 0.20 * v3.latitude
        );
        
        GpsPoint a1 = v1 - v2;
        GpsPoint a2 = v2 - v3;
        GpsPoint jerk = a1 - a2;
        
        pred_cp = current_reconstructed_point_ + smoothed_velocity + a1 + jerk * 0.5;
    } else if (history_states_.size() >= 4) {
        GpsPoint v1 = history_states_[history_states_.size() - 1].velocity;
        GpsPoint v2 = history_states_[history_states_.size() - 2].velocity;
        GpsPoint v3 = history_states_[history_states_.size() - 3].velocity;
        
        GpsPoint smoothed_velocity(
            0.45 * v1.longitude + 0.35 * v2.longitude + 0.20 * v3.longitude,
            0.45 * v1.latitude + 0.35 * v2.latitude + 0.20 * v3.latitude
        );
        
        GpsPoint acceleration = v1 - v2;
        pred_cp = current_reconstructed_point_ + smoothed_velocity + acceleration;
    } else if (history_states_.size() >= 3) {
        GpsPoint velocity = history_states_[history_states_.size() - 1].velocity;
        GpsPoint prev_velocity = history_states_[history_states_.size() - 2].velocity;
        GpsPoint acceleration = velocity - prev_velocity;
        pred_cp = current_reconstructed_point_ + velocity + acceleration;
    } else {
        pred_cp = pred_ldr;
    }
}

void TrajCompressSPAdaptiveCompressor::EncodePrediction(PredictorType predictor,
                                                       const GpsPoint& current_point,
                                                       const GpsPoint& predicted_point) {
    // 编码预测器标志（使用Huffman编码）
    int bits_before_flag = compressed_size_in_bits_;
    EncodeWithHuffman(predictor);
    stats_.predictor_flag_bits += (compressed_size_in_bits_ - bits_before_flag);
    
    // 计算预测误差
    GpsPoint delta = current_point - predicted_point;
    
    // 量化误差
    int64_t quantized_delta_lon = static_cast<int64_t>(std::round(delta.longitude / kQuantStep));
    int64_t quantized_delta_lat = static_cast<int64_t>(std::round(delta.latitude / kQuantStep));
    
    // 编码量化误差
    int bits_before_data = compressed_size_in_bits_;
    compressed_size_in_bits_ += EliasGammaCodec::Encode(
        ZigZagCodec::Encode(quantized_delta_lon) + 1, output_bit_stream_.get());
    compressed_size_in_bits_ += EliasGammaCodec::Encode(
        ZigZagCodec::Encode(quantized_delta_lat) + 1, output_bit_stream_.get());
    stats_.quantized_data_bits += (compressed_size_in_bits_ - bits_before_data);
    
    // 重构点
    GpsPoint reconstructed_delta(
        quantized_delta_lon * kQuantStep,
        quantized_delta_lat * kQuantStep
    );
    GpsPoint reconstructed_point = predicted_point + reconstructed_delta;
    
    // 更新重构状态
    UpdateReconstructedState(reconstructed_point);
    
    // 更新统计
    double error = CalculateDistance(current_point, reconstructed_point);
    stats_.total_prediction_error += error;
    stats_.max_prediction_error = std::max(stats_.max_prediction_error, error);
    stats_.prediction_errors.push_back(error);
}

void TrajCompressSPAdaptiveCompressor::UpdateHistory(const GpsPoint& reconstructed_point) {
    GpsPoint velocity(0, 0);
    if (!history_states_.empty()) {
        velocity = reconstructed_point - history_states_.back().reconstructed_point;
    }
    
    history_states_.emplace_back(reconstructed_point, velocity);
    
    if (history_states_.size() > kMaxHistorySize) {
        history_states_.erase(history_states_.begin());
    }
}

void TrajCompressSPAdaptiveCompressor::UpdateReconstructedState(const GpsPoint& reconstructed_point) {
    UpdateHistory(reconstructed_point);
    current_reconstructed_point_ = reconstructed_point;
}

double TrajCompressSPAdaptiveCompressor::CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) const {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

// 统计信息输出
void TrajCompressSPAdaptiveCompressor::CompressionStats::PrintStats() const {
    std::cout << "\n=== TrajCompress-SP-Adaptive 压缩统计 ===" << std::endl;
    std::cout << "总点数: " << total_points << std::endl;
    std::cout << "预测器使用: LDR=" << ldr_count << ", CP=" << cp_count << ", ZP=" << zp_count << std::endl;
    std::cout << "模式切换次数: " << mode_switch_count << std::endl;
    std::cout << "LDR-Only模式点数: " << ldr_only_mode_points << " (" 
              << (100.0 * ldr_only_mode_points / total_points) << "%)" << std::endl;
    std::cout << "Multi-Predictor模式点数: " << multi_predictor_mode_points << " (" 
              << (100.0 * multi_predictor_mode_points / total_points) << "%)" << std::endl;
    std::cout << "总比特数: " << total_bits << std::endl;
    std::cout << "  预测器标志: " << predictor_flag_bits << " bits" << std::endl;
    std::cout << "  模式切换: " << mode_switch_bits << " bits" << std::endl;
    std::cout << "  量化数据: " << quantized_data_bits << " bits" << std::endl;
}

void TrajCompressSPAdaptiveCompressor::CompressionStats::PrintDetailedStats() const {
    PrintStats();
    
    if (!prediction_errors.empty()) {
        std::vector<double> sorted_errors = prediction_errors;
        std::sort(sorted_errors.begin(), sorted_errors.end());
        
        size_t p50 = sorted_errors.size() * 0.50;
        size_t p90 = sorted_errors.size() * 0.90;
        size_t p99 = sorted_errors.size() * 0.99;
        
        std::cout << "\n预测误差分布:" << std::endl;
        std::cout << "  平均误差: " << (total_prediction_error / prediction_errors.size()) << std::endl;
        std::cout << "  最大误差: " << max_prediction_error << std::endl;
        std::cout << "  P50: " << sorted_errors[p50] << std::endl;
        std::cout << "  P90: " << sorted_errors[p90] << std::endl;
        std::cout << "  P99: " << sorted_errors[p99] << std::endl;
    }
}

// ==================== 解压缩器实现 ====================

TrajCompressSPAdaptiveDecompressor::TrajCompressSPAdaptiveDecompressor(uint8_t* compressed_data, int data_size) {
    input_bit_stream_ = std::make_unique<InputBitStream>(compressed_data, data_size);
    predictor_window_.reserve(kSlidingWindowSize);
    ReadHeader();
}

void TrajCompressSPAdaptiveDecompressor::ReadHeader() {
    block_size_ = input_bit_stream_->ReadInt(16);
    epsilon_ = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
    quant_step_ = 2 * epsilon_;
    
    // 初始化Huffman解码器
    predictor_frequency_[PredictorType::PREDICTOR_LDR] = 60;
    predictor_frequency_[PredictorType::PREDICTOR_CP] = 10;
    predictor_frequency_[PredictorType::PREDICTOR_ZP] = 30;
    UpdateHuffmanDecoder();
}

bool TrajCompressSPAdaptiveDecompressor::ReadNextPoint(GpsPoint& point) {
    if (first_point_) {
        first_point_ = false;
        
        // 读取第一个点的原始坐标
        double lon = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
        double lat = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
        point = GpsPoint(lon, lat);
        
        current_reconstructed_point_ = point;
        history_states_.emplace_back(point, GpsPoint(0, 0));
        return true;
    }
    
    // 检查是否有模式切换标志
    if (CheckForModeSwitch()) {
        // 模式已切换，继续读取下一个点
    }
    
    GpsPoint pred_ldr, pred_cp, pred_zp;
    ParallelPredict(pred_ldr, pred_cp, pred_zp);
    
    GpsPoint predicted_point;
    
    if (current_mode_ == CompressionMode::MODE_LDR_ONLY) {
        // LDR-Only模式：直接使用LDR预测
        predicted_point = pred_ldr;
    } else {
        // Multi-Predictor模式：解码预测器标志（使用Huffman解码）
        PredictorType predictor = DecodeWithHuffman();
        
        switch (predictor) {
            case PredictorType::PREDICTOR_LDR: predicted_point = pred_ldr; break;
            case PredictorType::PREDICTOR_CP: predicted_point = pred_cp; break;
            case PredictorType::PREDICTOR_ZP: predicted_point = pred_zp; break;
        }
        
        last_used_predictor_ = predictor;
    }
    
    // 解码量化误差
    uint64_t encoded_lon = EliasGammaCodec::Decode(input_bit_stream_.get());
    uint64_t encoded_lat = EliasGammaCodec::Decode(input_bit_stream_.get());
    
    int64_t quantized_delta_lon = ZigZagCodec::Decode(encoded_lon - 1);
    int64_t quantized_delta_lat = ZigZagCodec::Decode(encoded_lat - 1);
    
    // 重构点
    GpsPoint reconstructed_delta(
        quantized_delta_lon * quant_step_,
        quantized_delta_lat * quant_step_
    );
    GpsPoint reconstructed_point = predicted_point + reconstructed_delta;
    
    // 更新历史状态
    UpdateHistory(reconstructed_point);
    current_reconstructed_point_ = reconstructed_point;
    
    point = reconstructed_point;
    return true;
}

bool TrajCompressSPAdaptiveDecompressor::CheckForModeSwitch() {
    // 尝试读取3个比特，查看是否是逃逸码 111
    bool bit1 = input_bit_stream_->ReadBit();
    if (!bit1) {
        // 第一位是0，不是逃逸码，回退
        // 注意：InputBitStream可能需要支持回退功能，这里简化处理
        // 实际实现中可以使用peek功能
        return false;
    }
    
    bool bit2 = input_bit_stream_->ReadBit();
    if (!bit2) {
        // 10...，不是逃逸码
        return false;
    }
    
    bool bit3 = input_bit_stream_->ReadBit();
    if (!bit3) {
        // 110，不是逃逸码
        return false;
    }
    
    // 111，是逃逸码，读取模式切换标志
    bool mode_flag = input_bit_stream_->ReadBit();
    current_mode_ = mode_flag ? CompressionMode::MODE_MULTI_PREDICTOR : CompressionMode::MODE_LDR_ONLY;
    
    return true;
}

std::vector<TrajCompressSPAdaptiveDecompressor::GpsPoint> 
TrajCompressSPAdaptiveDecompressor::ReadAllPoints() {
    std::vector<GpsPoint> points;
    GpsPoint point;
    
    for (int i = 0; i < block_size_; ++i) {
        if (ReadNextPoint(point)) {
            points.push_back(point);
        } else {
            break;
        }
    }
    
    return points;
}

void TrajCompressSPAdaptiveDecompressor::ParallelPredict(GpsPoint& pred_ldr, GpsPoint& pred_cp, GpsPoint& pred_zp) {
    pred_zp = current_reconstructed_point_;
    
    if (history_states_.size() < 2) {
        pred_ldr = current_reconstructed_point_;
        pred_cp = current_reconstructed_point_;
        return;
    }
    
    GpsPoint velocity = history_states_[history_states_.size() - 1].velocity;
    pred_ldr = current_reconstructed_point_ + velocity;
    
    if (history_states_.size() >= 3) {
        GpsPoint v1 = history_states_[history_states_.size() - 1].velocity;
        GpsPoint v2 = history_states_[history_states_.size() - 2].velocity;
        GpsPoint acceleration = v1 - v2;
        pred_cp = current_reconstructed_point_ + v1 + acceleration;
    } else {
        pred_cp = pred_ldr;
    }
}

void TrajCompressSPAdaptiveDecompressor::UpdateHistory(const GpsPoint& reconstructed_point) {
    GpsPoint velocity(0, 0);
    if (!history_states_.empty()) {
        velocity = reconstructed_point - history_states_.back().reconstructed_point;
    }
    
    history_states_.emplace_back(reconstructed_point, velocity);
    
    if (history_states_.size() > kMaxHistorySize) {
        history_states_.erase(history_states_.begin());
    }
}

void TrajCompressSPAdaptiveDecompressor::UpdateHuffmanDecoder() {
    struct PredictorFreq {
        PredictorType type;
        int frequency;
    };
    
    std::vector<PredictorFreq> freq_list = {
        {PredictorType::PREDICTOR_LDR, predictor_frequency_[PredictorType::PREDICTOR_LDR]},
        {PredictorType::PREDICTOR_CP, predictor_frequency_[PredictorType::PREDICTOR_CP]},
        {PredictorType::PREDICTOR_ZP, predictor_frequency_[PredictorType::PREDICTOR_ZP]}
    };
    
    std::sort(freq_list.begin(), freq_list.end(), 
              [](const PredictorFreq& a, const PredictorFreq& b) {
                  return a.frequency > b.frequency;
              });
    
    huffman_decoder_map_[0] = freq_list[0].type;  // 0 -> 最高频率
    huffman_decoder_map_[1] = freq_list[1].type;  // 10 -> 次高频率
    huffman_decoder_map_[2] = freq_list[2].type;  // 11 -> 最低频率
}

TrajCompressSPAdaptiveDecompressor::PredictorType TrajCompressSPAdaptiveDecompressor::DecodeWithHuffman() {
    bool first_bit = input_bit_stream_->ReadBit();
    
    if (!first_bit) {
        // 0 -> 最高频率预测器
        AddPredictorToWindow(huffman_decoder_map_[0]);
        return huffman_decoder_map_[0];
    }
    
    bool second_bit = input_bit_stream_->ReadBit();
    
    if (!second_bit) {
        // 10 -> 次高频率预测器
        AddPredictorToWindow(huffman_decoder_map_[1]);
        return huffman_decoder_map_[1];
    }
    
    // 11 -> 最低频率预测器
    AddPredictorToWindow(huffman_decoder_map_[2]);
    return huffman_decoder_map_[2];
}

void TrajCompressSPAdaptiveDecompressor::AddPredictorToWindow(PredictorType predictor) {
    predictor_window_.push_back(predictor);
    predictor_frequency_[predictor]++;
    
    if (predictor_window_.size() > kSlidingWindowSize) {
        PredictorType old_predictor = predictor_window_.front();
        predictor_window_.erase(predictor_window_.begin());
        predictor_frequency_[old_predictor]--;
    }
    
    if (predictor_window_.size() % 100 == 0) {
        UpdateHuffmanDecoder();
    }
}

