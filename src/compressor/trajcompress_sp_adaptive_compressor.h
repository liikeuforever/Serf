#pragma once

#include "utils/output_bit_stream.h"
#include "utils/array.h"
#include "utils/double.h"
#include <vector>
#include <deque>
#include <memory>
#include <cmath>
#include <string>

/**
 * TrajCompress-SP-Adaptive: Trajectory Compression with Adaptive Switched Predictors
 * 自适应多预测器轨迹压缩算法
 * 
 * 核心创新：
 * 1. 自适应模式切换（Multi-Predictor模式 <-> LDR-Only模式）
 * 2. 基于成本的预测器选择（考虑标志位成本 + 误差编码成本）
 * 3. 逃逸码机制（使用111标记模式切换）
 * 4. 动态霍夫曼编码优化
 */
class TrajCompressSPAdaptiveCompressor {
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
     * 构造函数
     * @param block_size 块大小（用于预分配缓冲区）
     * @param epsilon 最大允许误差（度），内部会乘以0.999系数，与Serf-QT保持一致
     */
    TrajCompressSPAdaptiveCompressor(int block_size, double epsilon);
    
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
    
    // 自适应模式控制参数
    static constexpr int kModeEvaluationWindow = 32;     // 模式评估滑动窗口大小
    static constexpr double kLDROnlyThreshold = 0.95;    // 进入LDR-Only模式的阈值（95%）
    static constexpr int kModeExitCheckCount = 3;        // 退出LDR-Only模式需要连续N次
    static constexpr double kModeExitErrorRatio = 0.7;   // 退出时误差比例阈值
    
    // 状态
    std::unique_ptr<OutputBitStream> output_bit_stream_;
    int compressed_size_in_bits_ = 0;
    bool first_point_ = true;
    
    // 当前压缩模式
    CompressionMode current_mode_ = MODE_MULTI_PREDICTOR;
    
    // 模式切换控制
    std::vector<PredictorType> mode_evaluation_window_;  // 用于评估是否进入LDR-Only模式
    int consecutive_non_ldr_wins_ = 0;                   // 连续非LDR预测器更优的次数
    
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
    
    // 基于成本的模式切换系统（无魔法数字）
    static constexpr int kCostWindowSize = 64;        // 成本评估窗口大小
    static constexpr int kSwitchCost = 4;             // 模式切换成本（111 + 0/1）
    static constexpr int kStabilityMargin = 2;        // 防抖动边际
    static constexpr int kEvaluationInterval = 16;    // 评估间隔
    
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
    double GetRecentLDRRatio() const;
};

/**
 * TrajCompress-SP-Adaptive 解压缩器
 */
class TrajCompressSPAdaptiveDecompressor {
public:
    using GpsPoint = TrajCompressSPAdaptiveCompressor::GpsPoint;
    using PredictorType = TrajCompressSPAdaptiveCompressor::PredictorType;
    using CompressionMode = TrajCompressSPAdaptiveCompressor::CompressionMode;
    using HistoryState = TrajCompressSPAdaptiveCompressor::HistoryState;
    
    TrajCompressSPAdaptiveDecompressor(uint8_t* compressed_data, int data_size);
    
    bool ReadNextPoint(GpsPoint& point);
    std::vector<GpsPoint> ReadAllPoints();

private:
    std::unique_ptr<class InputBitStream> input_bit_stream_;
    
    // 参数
    int block_size_;
    double epsilon_;
    double quant_step_;
    
    // 状态
    bool first_point_ = true;
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
    PredictorType DecodeWithHuffman();
    void AddPredictorToWindow(PredictorType predictor);
    bool CheckForModeSwitch();  // 检查是否有模式切换标志
};

