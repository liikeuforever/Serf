#pragma once

#include "compressor/serf_qt_gps_trajectory_compressor.h"
#include "decompressor/serf_qt_gps_trajectory_decompressor.h"

/**
 * 可配置参数的GPS轨迹压缩器
 * 用于参数优化测试
 */
class ConfigurableGpsCompressor {
public:
    using GpsPoint = SerfQtGpsTrajectoryCompressor::GpsPoint;
    
    ConfigurableGpsCompressor(int block_size, double e_max, double epsilon_v, double epsilon_theta)
        : block_size_(block_size), e_max_(e_max), epsilon_v_(epsilon_v), epsilon_theta_(epsilon_theta) {
        // 初始化内部状态
        Reset();
    }
    
    void AddGpsPoint(const GpsPoint& point);
    Array<uint8_t> GetCompressedData();
    void Reset();
    
private:
    // 可配置参数
    int block_size_;
    double e_max_;
    double epsilon_v_;
    double epsilon_theta_;
    
    // 固定参数
    static constexpr double kEpsilonPos = 1e-5;
    static constexpr int kMaxHistorySize = 8;
    
    // 内部状态（简化版本，专门用于测试）
    std::vector<GpsPoint> points_;
    bool initialized_;
};

/**
 * 可配置参数的GPS轨迹解压器
 */
class ConfigurableGpsDecompressor {
public:
    using GpsPoint = SerfQtGpsTrajectoryDecompressor::GpsPoint;
    
    ConfigurableGpsDecompressor(const Array<uint8_t>& compressed_data, 
                               double epsilon_v, double epsilon_theta)
        : epsilon_v_(epsilon_v), epsilon_theta_(epsilon_theta) {
        // 使用标准解压器但忽略参数差异
        decompressor_ = std::make_unique<SerfQtGpsTrajectoryDecompressor>(compressed_data);
    }
    
    GpsPoint GetNextGpsPoint() {
        return decompressor_->GetNextGpsPoint();
    }
    
private:
    double epsilon_v_;
    double epsilon_theta_;
    std::unique_ptr<SerfQtGpsTrajectoryDecompressor> decompressor_;
};
