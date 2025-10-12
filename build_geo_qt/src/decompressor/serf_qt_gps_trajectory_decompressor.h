#ifndef SERF_QT_GPS_TRAJECTORY_DECOMPRESSOR_H
#define SERF_QT_GPS_TRAJECTORY_DECOMPRESSOR_H

#include <cstdint>
#include <memory>
#include <vector>
#include <cmath>

#include "utils/input_bit_stream.h"
#include "utils/array.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"

/**
 * 基于固定步长与几何剪枝的率-失真优化GPS轨迹解压缩器
 * 
 * 解压器是压缩器状态更新逻辑的精确镜像
 */
class SerfQtGpsTrajectoryDecompressor {
public:
    // 校正策略标志位（与压缩器保持一致）
    enum CorrectionFlag {
        FLAG_ZERO_CORR = 0,      // 零校正: 0
        FLAG_V_ONLY = 2,         // 纯速度校正: 10
        FLAG_THETA_ONLY = 6,     // 纯角度校正: 110
        FLAG_BOTH = 7            // 完全校正: 111
    };

    // GPS轨迹点结构（简化版本，无时间戳）
    struct GpsPoint {
        double longitude;
        double latitude;
        
        GpsPoint() : longitude(0), latitude(0) {}
        GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
    };

    // 运动矢量结构（极坐标形式）
    struct MotionVector {
        double velocity;    // 速度 (度/步长)
        double theta;       // 角度 (弧度)
        
        MotionVector() : velocity(0), theta(0) {}
        MotionVector(double v, double t) : velocity(v), theta(t) {}
    };

    // 历史状态结构
    struct HistoryState {
        GpsPoint reconstructed_point;
        MotionVector motion_vector;
        
        HistoryState() {}
        HistoryState(const GpsPoint& p, const MotionVector& v) 
            : reconstructed_point(p), motion_vector(v) {}
    };

    /**
     * 构造函数
     * @param compressed_data 压缩后的数据
     */
    SerfQtGpsTrajectoryDecompressor(const Array<uint8_t>& compressed_data);

    /**
     * 解压下一个GPS轨迹点
     * @return 解压后的GPS轨迹点，如果没有更多数据则返回空点
     */
    GpsPoint DecompressNextPoint();

    /**
     * 检查是否还有更多数据可以解压
     */
    bool HasMoreData() const;

    /**
     * 重置解压器状态
     */
    void Reset();

private:
    // 固定参数（基于Geolife数据集优化）
    static constexpr double kEpsilonPos = 1e-5;           // 坐标精度基准 (度)
    static constexpr double kEpsilonV = 1e-6;            // 速度量化步长 (度/步长) - 更精细
    static constexpr double kEpsilonTheta = 0.0001;     // 角度量化步长 (弧度) ~0.0057度 - 更精细
    static constexpr int kMaxHistorySize = 8;            // 最大历史状态数量

    // 核心参数
    double kEMax;                 // 最大允许误差
    int kBlockSize;               // 块大小

    // 状态变量
    bool initialized_ = false;
    int point_index_ = 0;
    std::unique_ptr<InputBitStream> input_bit_stream_;

    // 历史状态队列
    std::vector<HistoryState> history_states_;

    // 当前状态
    GpsPoint current_reconstructed_point_;
    MotionVector current_motion_vector_;

    /**
     * 初始化解压器（读取头部信息）
     */
    void Initialize();

    /**
     * 高级预测：基于历史状态预测当前点的运动矢量和位置
     * @param predicted_point 输出预测点
     * @param predicted_motion 输出预测运动矢量
     */
    void AdvancedPrediction(GpsPoint& predicted_point, 
                           MotionVector& predicted_motion);

    /**
     * 解码校正策略标志位
     * @return 解码的校正策略
     */
    CorrectionFlag DecodeStrategy();

    /**
     * 解码量化值
     * @param strategy 校正策略
     * @param qv 输出速度量化值
     * @param qtheta 输出角度量化值
     */
    void DecodeQuantizationValues(CorrectionFlag strategy, int64_t& qv, int64_t& qtheta);

    /**
     * 更新状态（与压缩器保持同步）
     * @param predicted_point 预测点
     * @param predicted_motion 预测运动矢量
     * @param strategy 校正策略
     * @param qv 速度量化值
     * @param qtheta 角度量化值
     * @return 重构的GPS点
     */
    GpsPoint UpdateState(const GpsPoint& predicted_point,
                        const MotionVector& predicted_motion,
                        CorrectionFlag strategy,
                        int64_t qv,
                        int64_t qtheta);

    // 辅助函数
    /**
     * 计算从起点按给定运动矢量移动一步后的目标点
     */
    GpsPoint CalculateDestinationPoint(const GpsPoint& start_point,
                                      const MotionVector& motion) const;
};

#endif  // SERF_QT_GPS_TRAJECTORY_DECOMPRESSOR_H
