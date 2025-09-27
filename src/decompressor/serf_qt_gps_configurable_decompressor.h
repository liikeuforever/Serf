#pragma once

#include "utils/input_bit_stream.h"
#include "utils/double.h"
#include <vector>
#include <memory>
#include <cmath>

/**
 * 支持运行时参数配置的GPS轨迹解压器
 * 用于参数优化测试
 */
class SerfQtGpsConfigurableDecompressor {
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
     * @param compressed_data 压缩数据
     */
    explicit SerfQtGpsConfigurableDecompressor(const Array<uint8_t>& compressed_data);

    /**
     * 获取下一个GPS点
     * @return 解压后的GPS点
     */
    GpsPoint GetNextGpsPoint();

    /**
     * 检查是否还有更多数据
     */
    bool HasMoreData() const { return true; }

    /**
     * 重置解压器状态
     */
    void Reset();

private:
    // 从压缩数据中读取的参数
    double kEpsilonV;             // 速度量化步长 (度/步长)
    double kEpsilonTheta;         // 角度量化步长 (弧度)
    
    // 固定参数
    static constexpr double kEpsilonPos = 1e-5;
    static constexpr int kMaxHistorySize = 8;

    // 核心参数
    double kEMax;                 // 最大允许误差
    int kBlockSize;               // 块大小

    // 内部状态
    std::unique_ptr<InputBitStream> input_bit_stream_;
    bool initialized_;
    int point_index_;
    
    GpsPoint current_reconstructed_point_;
    MotionVector current_motion_vector_;
    std::vector<HistoryState> history_states_;

    // 核心算法函数
    void Initialize();
    void AdvancedPrediction(GpsPoint& predicted_point, MotionVector& predicted_motion);
    CorrectionFlag DecodeStrategy();
    void DecodeQuantizationValues(CorrectionFlag strategy, int64_t& qv, int64_t& qtheta);
    GpsPoint UpdateState(const GpsPoint& predicted_point,
                        const MotionVector& predicted_motion,
                        CorrectionFlag strategy,
                        int64_t qv,
                        int64_t qtheta);

    // 辅助函数
    GpsPoint CalculateDestinationPoint(const GpsPoint& start_point,
                                      const MotionVector& motion) const;
    MotionVector CalculateMotionVector(const GpsPoint& start_point,
                                      const GpsPoint& end_point) const;
};
