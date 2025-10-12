#ifndef SERF_QT_GPS_TRAJECTORY_COMPRESSOR_H
#define SERF_QT_GPS_TRAJECTORY_COMPRESSOR_H

#include <vector>
#include <memory>
#include <cmath>

#include "utils/array.h"
#include "utils/output_bit_stream.h"

/**
 * 基于固定步长与几何剪枝的率-失真优化GPS轨迹压缩算法
 * 
 * 这是Serf-QT算法的GPS轨迹压缩变种，专门针对经纬度坐标对进行优化。
 * 主要特性：
 * - 无时间戳依赖，纯空间轨迹压缩
 * - 固定量化步长，避免动态计算开销
 * - 几何剪枝策略，快速筛选可行校正方案
 * - 极坐标运动表示（速度+角度）
 * - 严格的误差界限控制
 */
class SerfQtGpsTrajectoryCompressor {
public:
    /**
     * GPS轨迹点结构（经纬度坐标对）
     */
    struct GpsPoint {
        double longitude;  // 经度
        double latitude;   // 纬度
        
        GpsPoint() : longitude(0), latitude(0) {}
        GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
    };

    /**
     * 运动矢量结构（极坐标表示）
     */
    struct MotionVector {
        double velocity;  // 速度（度/步长）
        double theta;     // 角度（弧度）
        
        MotionVector() : velocity(0), theta(0) {}
        MotionVector(double v, double t) : velocity(v), theta(t) {}
    };

    /**
     * 历史状态结构
     */
    struct HistoryState {
        GpsPoint reconstructed_point;
        MotionVector motion_vector;
        
        HistoryState(const GpsPoint& point, const MotionVector& motion)
            : reconstructed_point(point), motion_vector(motion) {}
    };

    /**
     * 校正策略枚举
     */
    enum CorrectionFlag {
        FLAG_ZERO_CORR = 0,    // 零校正
        FLAG_V_ONLY = 1,       // 仅速度校正
        FLAG_THETA_ONLY = 2,   // 仅角度校正
        FLAG_BOTH = 3          // 完全校正（速度+角度）
    };

    /**
     * 构造函数
     * @param block_size 块大小
     * @param max_error 最大允许误差（度）
     */
    SerfQtGpsTrajectoryCompressor(int block_size, double max_error);

    /**
     * 析构函数
     */
    ~SerfQtGpsTrajectoryCompressor() = default;

    /**
     * 添加GPS轨迹点进行压缩
     * @param point GPS轨迹点
     */
    void AddGpsPoint(const GpsPoint& point);

    /**
     * 完成压缩过程
     */
    void Close();

    /**
     * 获取压缩后的字节数组
     */
    Array<uint8_t> compressed_bytes();

    /**
     * 获取压缩后的比特数
     */
    long get_compressed_size_in_bits() const;

private:
    // 固定参数（基于Geolife数据集优化）
    static constexpr double kEpsilonPos = 1e-5;           // 坐标精度基准 (度)
    static constexpr double kEpsilonV = 1e-6;            // 速度量化步长 (度/步长) - 更精细
    static constexpr double kEpsilonTheta = 0.0001;     // 角度量化步长 (弧度) ~0.0057度 - 更精细
    static constexpr int kMaxHistorySize = 8;            // 最大历史状态数量

    // 核心参数
    const double kEMax;                                   // 最大允许误差
    const int kBlockSize;                                 // 块大小

    // 状态变量
    bool first_point_ = true;
    int point_index_ = 0;
    std::unique_ptr<OutputBitStream> output_bit_stream_;
    Array<uint8_t> compressed_bytes_;
    long compressed_size_in_bits_ = 0;
    long stored_compressed_size_in_bits_ = 0;

    // 历史状态队列
    std::vector<HistoryState> history_states_;

    // 当前状态
    GpsPoint current_reconstructed_point_;
    MotionVector current_motion_vector_;

    /**
     * 初始化处理（处理前两个点）
     */
    void InitializeFirstTwoPoints(const GpsPoint& point);

    /**
     * 高级预测：基于历史状态预测当前点的运动矢量和位置
     * @param predicted_point 输出预测点
     * @param predicted_motion 输出预测运动矢量
     */
    void AdvancedPrediction(GpsPoint& predicted_point, MotionVector& predicted_motion);

    /**
     * 几何剪枝与快速决策
     * @param current_point 当前真实点
     * @param predicted_point 预测点
     * @param predicted_motion 预测运动矢量
     * @param viable_candidates 输出可行候选策略列表
     * @return 如果零校正可行则返回true
     */
    bool GeometricPruningAndFastDecision(const GpsPoint& current_point,
                                        const GpsPoint& predicted_point,
                                        const MotionVector& predicted_motion,
                                        std::vector<CorrectionFlag>& viable_candidates);

    /**
     * 评估并选择最佳策略
     * @param current_point 当前真实点
     * @param predicted_point 预测点
     * @param predicted_motion 预测运动矢量
     * @param candidates 候选策略列表
     * @param best_strategy 输出最佳策略
     * @param best_qv 输出最佳速度量化值
     * @param best_qtheta 输出最佳角度量化值
     */
    void EvaluateAndSelectBestStrategy(const GpsPoint& current_point,
                                      const GpsPoint& predicted_point,
                                      const MotionVector& predicted_motion,
                                      const std::vector<CorrectionFlag>& candidates,
                                      CorrectionFlag& best_strategy,
                                      int64_t& best_qv,
                                      int64_t& best_qtheta);

    /**
     * 编码策略和量化值
     * @param strategy 校正策略
     * @param qv 速度量化值
     * @param qtheta 角度量化值
     */
    void EncodeStrategy(CorrectionFlag strategy, int64_t qv, int64_t qtheta);

    /**
     * 更新内部状态
     * @param current_point 当前真实点
     * @param predicted_point 预测点
     * @param predicted_motion 预测运动矢量
     * @param strategy 选择的策略
     * @param qv 速度量化值
     * @param qtheta 角度量化值
     */
    void UpdateState(const GpsPoint& current_point,
                    const GpsPoint& predicted_point,
                    const MotionVector& predicted_motion,
                    CorrectionFlag strategy,
                    int64_t qv,
                    int64_t qtheta);

    // 辅助函数
    /**
     * 计算两点间的欧几里得距离
     */
    double CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) const;

    /**
     * 基于起始点和运动矢量计算目标点
     */
    GpsPoint CalculateDestinationPoint(const GpsPoint& start_point, 
                                      const MotionVector& motion) const;

    /**
     * 计算两点间的运动矢量
     */
    MotionVector CalculateMotionVector(const GpsPoint& start_point, 
                                      const GpsPoint& end_point) const;

    /**
     * 计算点到直线的垂直距离
     */
    double CalculatePerpendicularDistance(const GpsPoint& point,
                                         const GpsPoint& line_start,
                                         const GpsPoint& line_end) const;

    /**
     * 估算编码成本（比特数）
     */
    int EstimateEncodingCost(CorrectionFlag strategy, int64_t qv, int64_t qtheta) const;

    /**
     * 计算最优量化值
     */
    void CalculateOptimalQuantization(const GpsPoint& current_point,
                                     const GpsPoint& predicted_point,
                                     const MotionVector& predicted_motion,
                                     CorrectionFlag strategy,
                                     int64_t& qv,
                                     int64_t& qtheta) const;

    /**
     * 模拟重构过程，计算给定策略和量化值的重构点
     */
    GpsPoint SimulateReconstruction(const GpsPoint& predicted_point,
                                   const MotionVector& predicted_motion,
                                   CorrectionFlag strategy,
                                   int64_t qv,
                                   int64_t qtheta) const;

    /**
     * 精化量化参数以满足误差界限
     */
    void RefineQuantizationForErrorBound(const GpsPoint& current_point,
                                        const GpsPoint& predicted_point,
                                        const MotionVector& predicted_motion,
                                        int64_t& qv,
                                        int64_t& qtheta) const;
};

#endif  // SERF_QT_GPS_TRAJECTORY_COMPRESSOR_H
