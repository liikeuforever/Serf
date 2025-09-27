#pragma once

#include "utils/output_bit_stream.h"
#include "utils/double.h"
#include <vector>
#include <memory>
#include <cmath>

/**
 * 支持运行时参数配置的GPS轨迹压缩器
 * 用于参数优化测试
 */
class SerfQtGpsConfigurableCompressor {
public:
    // GPS点结构
    struct GpsPoint {
        double longitude;
        double latitude;
        
        GpsPoint() : longitude(0), latitude(0) {}
        GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
    };
    
    // 运动矢量结构
    struct MotionVector {
        double velocity;  // 速度 (度/步长)
        double theta;     // 角度 (弧度)
        
        MotionVector() : velocity(0), theta(0) {}
        MotionVector(double v, double t) : velocity(v), theta(t) {}
    };
    
    // 历史状态结构
    struct HistoryState {
        GpsPoint reconstructed_point;
        MotionVector motion_vector;
        
        HistoryState(const GpsPoint& point, const MotionVector& motion)
            : reconstructed_point(point), motion_vector(motion) {}
    };
    
    // 校正策略枚举
    enum CorrectionFlag {
        FLAG_ZERO_CORR = 0,    // 零校正
        FLAG_V_ONLY = 1,       // 仅速度校正
        FLAG_THETA_ONLY = 2,   // 仅角度校正
        FLAG_BOTH = 3          // 完全校正
    };

    /**
     * 构造函数
     * @param block_size 块大小
     * @param e_max 最大允许误差 (度)
     * @param epsilon_v 速度量化步长 (度/步长)
     * @param epsilon_theta 角度量化步长 (弧度)
     */
    SerfQtGpsConfigurableCompressor(int block_size, double e_max, 
                                   double epsilon_v, double epsilon_theta);

    /**
     * 添加GPS点进行压缩
     * @param point GPS点
     */
    void AddGpsPoint(const GpsPoint& point);

    /**
     * 获取压缩后的数据
     * @return 压缩数据
     */
    Array<uint8_t> GetCompressedData();

    /**
     * 获取压缩大小（比特）
     */
    int GetCompressedSizeInBits() const { return compressed_size_in_bits_; }
    
    /**
     * 策略统计结构
     */
    struct StrategyStats {
        int zero_corr_count = 0;
        int v_only_count = 0;
        int theta_only_count = 0;
        int both_count = 0;
        int total_strategy_bits = 0;
        int total_quantization_bits = 0;
        
        int GetTotalPoints() const {
            return zero_corr_count + v_only_count + theta_only_count + both_count;
        }
        
        void PrintStats() const;
    };
    
    /**
     * 获取策略统计信息
     */
    const StrategyStats& GetStrategyStats() const { return strategy_stats_; }

private:
    // 可配置参数
    const int kBlockSize;
    const double kEMax;
    const double kEpsilonV;
    const double kEpsilonTheta;
    
    // 固定参数
    static constexpr double kEpsilonPos = 1e-5;
    static constexpr int kMaxHistorySize = 8;

    // 内部状态
    std::unique_ptr<OutputBitStream> output_bit_stream_;
    int compressed_size_in_bits_ = 0;
    int point_index_ = 0;
    bool first_point_ = true;
    
    GpsPoint current_reconstructed_point_;
    MotionVector current_motion_vector_;
    std::vector<HistoryState> history_states_;
    
    // 策略统计
    StrategyStats strategy_stats_;

    // 核心算法函数
    void InitializeFirstTwoPoints(const GpsPoint& point);
    void AdvancedPrediction(GpsPoint& predicted_point, MotionVector& predicted_motion);
    bool GeometricPruningAndFastDecision(const GpsPoint& current_point,
                                        const GpsPoint& predicted_point,
                                        const MotionVector& predicted_motion,
                                        std::vector<CorrectionFlag>& viable_candidates);
    void EvaluateAndSelectBestStrategy(const GpsPoint& current_point,
                                      const GpsPoint& predicted_point,
                                      const MotionVector& predicted_motion,
                                      const std::vector<CorrectionFlag>& viable_candidates,
                                      CorrectionFlag& best_strategy,
                                      int64_t& best_qv,
                                      int64_t& best_qtheta);
    void CalculateOptimalQuantization(const GpsPoint& current_point,
                                     const GpsPoint& predicted_point,
                                     const MotionVector& predicted_motion,
                                     CorrectionFlag strategy,
                                     int64_t& qv,
                                     int64_t& qtheta);
    void EncodeStrategy(CorrectionFlag strategy, int64_t qv, int64_t qtheta);
    void UpdateState(const GpsPoint& current_point,
                    const GpsPoint& predicted_point,
                    const MotionVector& predicted_motion,
                    CorrectionFlag strategy,
                    int64_t qv,
                    int64_t qtheta);

    // 辅助函数
    GpsPoint CalculateDestinationPoint(const GpsPoint& start_point,
                                      const MotionVector& motion) const;
    MotionVector CalculateMotionVector(const GpsPoint& start_point,
                                      const GpsPoint& end_point) const;
    double CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) const;
    GpsPoint SimulateReconstruction(const GpsPoint& predicted_point,
                                   const MotionVector& predicted_motion,
                                   CorrectionFlag strategy,
                                   int64_t qv,
                                   int64_t qtheta) const;
};
