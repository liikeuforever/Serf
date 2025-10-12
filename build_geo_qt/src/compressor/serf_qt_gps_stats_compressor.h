#pragma once

#include "compressor/serf_qt_gps_configurable_compressor.h"
#include <map>

/**
 * 带统计功能的GPS轨迹压缩器
 * 继承自可配置压缩器，添加策略使用统计
 */
class SerfQtGpsStatsCompressor : public SerfQtGpsConfigurableCompressor {
public:
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
     * 构造函数
     */
    SerfQtGpsStatsCompressor(int block_size, double e_max, 
                            double epsilon_v, double epsilon_theta);

    /**
     * 获取策略统计信息
     */
    const StrategyStats& GetStrategyStats() const { return stats_; }

protected:
    /**
     * 重写编码方法以收集统计信息
     */
    void EncodeStrategy(CorrectionFlag strategy, int64_t qv, int64_t qtheta) override;

private:
    StrategyStats stats_;
};
