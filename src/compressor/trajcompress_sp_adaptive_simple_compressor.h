#pragma once

#include "utils/output_bit_stream.h"
#include "utils/input_bit_stream.h"
#include "utils/array.h"
#include <vector>
#include <deque>
#include <memory>

/**
 * TrajCompress-SP-Adaptive-Simple: 极简版自适应压缩器
 * 
 * 简化策略：
 * 1. 移除所有动态参数调整逻辑
 * 2. 移除 kStabilityMargin 防抖逻辑
 * 3. 固定所有内部参数为最优值
 * 4. 只需2个用户参数：block_size 和 epsilon
 */
class TrajCompressSPAdaptiveSimpleCompressor {
public:
    // GPS点结构
    struct GpsPoint {
        double longitude;
        double latitude;
        
        GpsPoint() : longitude(0), latitude(0) {}
        GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
        
        GpsPoint operator+(const GpsPoint& other) const {
            return GpsPoint(longitude + other.longitude, latitude + other.latitude);
        }
        
        GpsPoint operator-(const GpsPoint& other) const {
            return GpsPoint(longitude - other.longitude, latitude - other.latitude);
        }
        
        GpsPoint operator*(double scale) const {
            return GpsPoint(longitude * scale, latitude * scale);
        }
    };
    
    // 预测器类型枚举
    enum PredictorType {
        PREDICTOR_LDR = 0,    // Linear Dead Reckoning - 线性航位推算
        PREDICTOR_CP = 1,     // Curve Predictor - 曲线预测（二阶）
        PREDICTOR_ZP = 2      // Zero Predictor - 零预测（Serf-QT风格）
    };
    
    // 压缩模式枚举
    enum CompressionMode {
        MODE_MULTI_PREDICTOR = 0,  // 多预测器模式（默认）
        MODE_LDR_ONLY = 1          // 纯线性预测模式
    };
    
    // 历史状态结构
    struct HistoryState {
        GpsPoint reconstructed_point;
        GpsPoint velocity;  // 速度向量（经纬度差）
        
        HistoryState() {}
        HistoryState(const GpsPoint& point, const GpsPoint& vel)
            : reconstructed_point(point), velocity(vel) {}
    };
    
    // 统计信息结构
    struct CompressionStats {
        int total_points = 0;
        
        // 预测器使用统计
        int ldr_count = 0;
        int cp_count = 0;
        int zp_count = 0;
        
        // 模式切换统计
        int mode_switch_count = 0;
        int ldr_only_mode_points = 0;
        int multi_predictor_mode_points = 0;
        
        // 编码成本统计
        int total_bits = 0;
        int predictor_flag_bits = 0;
        int mode_switch_bits = 0;
        int quantized_data_bits = 0;
        
        // 预测误差统计
        double total_prediction_error = 0;
        double max_prediction_error = 0;
        std::vector<double> prediction_errors;
        
        void PrintStats() const;
        void PrintDetailedStats() const;
    };

    /**
     * 极简构造函数
     * @param block_size 块大小（用于预分配缓冲区）
     * @param epsilon 最大允许误差（度），内部会乘以0.999系数，与Serf-QT保持一致
     * @param evaluation_window 评估窗口大小（可选，默认96）
     */
    TrajCompressSPAdaptiveSimpleCompressor(int block_size, double epsilon, int evaluation_window = 96);
    
    /**
     * 添加GPS点进行压缩
     * @param point GPS点
     */
    void AddGpsPoint(const GpsPoint& point);
    
    /**
     * 结束压缩，刷新缓冲区
     */
    void Close();
    
    /**
     * 获取压缩后的数据
     */
    Array<uint8_t> GetCompressedData();
    
    /**
     * 获取压缩大小（比特）
     */
    int GetCompressedSizeInBits() const { return compressed_size_in_bits_; }
    
    /**
     * 获取统计信息
     */
    const CompressionStats& GetStats() const { return stats_; }

private:
    // 参数
    const int kBlockSize;
    const double kEpsilon;          // 误差阈值（经过0.999系数处理）
    const double kQuantStep;        // 量化步长 = 2 * epsilon * 0.999
    
    // 极简版：固定所有参数为最优值（参数已优化简化）
    const int kEvaluationWindow;                        // 评估窗口（唯一时间尺度参数，可配置）
    static constexpr bool kClearWindowAfterSwitch = false;  // 固定不清空
    
    // 状态
    std::unique_ptr<OutputBitStream> output_bit_stream_;
    int compressed_size_in_bits_ = 0;
    bool first_point_ = true;
    
    // 当前压缩模式
    CompressionMode current_mode_ = MODE_MULTI_PREDICTOR;
    
    // 预测器选择
    PredictorType last_used_predictor_ = PREDICTOR_ZP;
    
    // 重构状态（与解压器保持同步）
    GpsPoint current_reconstructed_point_;
    std::vector<HistoryState> history_states_;
    static constexpr int kMaxHistorySize = 3;
    
    // 统计信息
    CompressionStats stats_;
    
    // Huffman 编码相关（动态自适应）
    static constexpr int kSlidingWindowSize = 1000;
    std::vector<PredictorType> predictor_window_;
    int predictor_frequency_[3] = {0, 0, 0};  // [LDR, CP, ZP]
    
    struct HuffmanCode {
        std::vector<bool> bits;
        int length;
        HuffmanCode() : length(0) {}
        HuffmanCode(const std::vector<bool>& b) : bits(b), length(b.size()) {}
    };
    HuffmanCode huffman_codes_[3];
    
    // 极简版：移除所有动态参数调整系统
    static constexpr int kSwitchCost = 4;   // 模式切换成本（固定）
    
    std::deque<int> point_costs_multi_;          // 多预测器模型窗口成本
    std::deque<int> point_costs_ldr_only_;       // LDR-Only模型窗口成本
    long long window_total_cost_multi_ = 0;      // 多预测器模型总成本
    long long window_total_cost_ldr_only_ = 0;   // LDR-Only模型总成本
    
    // 核心算法函数
    void ProcessFirstPoint(const GpsPoint& point);
    
    // 并行预测
    void ParallelPredict(GpsPoint& pred_ldr, GpsPoint& pred_cp, GpsPoint& pred_zp);
    
    // 基于成本的预测器选择（策略二）
    PredictorType SelectBestPredictorByCost(const GpsPoint& current_point,
                                            const GpsPoint& pred_ldr,
                                            const GpsPoint& pred_cp,
                                            const GpsPoint& pred_zp,
                                            GpsPoint& best_prediction,
                                            int& best_cost);
    
    // 多预测器模式编码
    void EncodeMultiPredictor(const GpsPoint& point);
    
    // LDR-Only模式编码
    void EncodeLDROnly(const GpsPoint& point);
    
    // 编码预测结果
    void EncodePrediction(PredictorType predictor,
                         const GpsPoint& current_point,
                         const GpsPoint& predicted_point);
    
    // 模式切换控制（基于成本的智能决策，无魔法数字）
    void EvaluateAndSwitchModeBasedOnCost();
    void EncodeModeSwitch(CompressionMode new_mode);
    
    // 估算编码成本
    int EstimateEliasGammaBits(int64_t value) const;
    int EstimateErrorEncodingCost(const GpsPoint& error) const;
    
    // 成本窗口管理
    void UpdateCostWindows(int multi_cost, int ldr_only_cost);
    
    // Huffman 编码相关
    void UpdateHuffmanCodes();
    void EncodeWithHuffman(PredictorType predictor);
    void AddPredictorToWindow(PredictorType predictor);
    int GetHuffmanBitCost(PredictorType predictor) const;
    
    // 历史管理
    void UpdateHistory(const GpsPoint& reconstructed_point);
    void UpdateReconstructedState(const GpsPoint& reconstructed_point);
    
    // 辅助函数
    double CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) const;
};

/**
 * TrajCompress-SP-Adaptive-Simple 解压缩器
 */
class TrajCompressSPAdaptiveSimpleDecompressor {
public:
    using GpsPoint = TrajCompressSPAdaptiveSimpleCompressor::GpsPoint;
    using PredictorType = TrajCompressSPAdaptiveSimpleCompressor::PredictorType;
    using CompressionMode = TrajCompressSPAdaptiveSimpleCompressor::CompressionMode;
    using HistoryState = TrajCompressSPAdaptiveSimpleCompressor::HistoryState;
    
    TrajCompressSPAdaptiveSimpleDecompressor(uint8_t* compressed_data, int data_size);
    
    bool ReadNextPoint(GpsPoint& point);
    std::vector<GpsPoint> ReadAllPoints();

private:
    std::unique_ptr<InputBitStream> input_bit_stream_;
    
    // 参数
    int block_size_;
    double epsilon_;
    double quant_step_;
    int evaluation_window_;  // 评估窗口大小
    
    // 状态
    bool first_point_ = true;
    int points_read_ = 0;  // 已读取的点数（用于同步评估窗口）
    CompressionMode current_mode_ = CompressionMode::MODE_MULTI_PREDICTOR;
    PredictorType last_used_predictor_ = PredictorType::PREDICTOR_ZP;
    
    // 重构状态
    GpsPoint current_reconstructed_point_;
    std::vector<HistoryState> history_states_;
    static constexpr int kMaxHistorySize = 3;
    
    // Huffman 解码相关
    static constexpr int kSlidingWindowSize = 1000;
    std::vector<PredictorType> predictor_window_;
    int predictor_frequency_[3] = {0, 0, 0};
    PredictorType huffman_decoder_map_[3];
    
    // 辅助函数
    void ReadHeader();
    void ParallelPredict(GpsPoint& pred_ldr, GpsPoint& pred_cp, GpsPoint& pred_zp);
    void UpdateHistory(const GpsPoint& reconstructed_point);
    void UpdateHuffmanDecoder();
    PredictorType DecodeWithHuffman();  // 解码Huffman（简化版）
    void AddPredictorToWindow(PredictorType predictor);
};

