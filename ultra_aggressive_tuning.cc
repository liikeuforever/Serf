/**
 * 超激进参数搜索 - 寻找能同时超越Geolife和Track的通用配置
 * 策略：在96-320窗口之间进行超密集采样，测试所有可能的参数组合
 */

#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <map>
#include <algorithm>

struct GpsPoint {
    double longitude;
    double latitude;
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
};

struct ParameterConfig {
    int cost_window_size;
    int stability_margin;
    bool clear_after_switch;
    int evaluation_interval;
    
    std::string GetName() const {
        std::stringstream ss;
        ss << "W" << cost_window_size 
           << "_M" << stability_margin 
           << "_" << (clear_after_switch ? "C" : "NC")
           << "_E" << evaluation_interval;
        return ss.str();
    }
};

struct TestResult {
    ParameterConfig config;
    double geolife_perf;
    double track_perf;
    bool beats_linear_geolife;
    bool beats_linear_track;
    
    bool BeatsBothLinear() const {
        return beats_linear_geolife && beats_linear_track;
    }
    
    double GetWorstCase() const {
        return std::max(geolife_perf, track_perf);
    }
};

std::vector<GpsPoint> ReadGpsData(const std::string& filename, int max_points = -1) {
    std::vector<GpsPoint> data;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return data;
    }
    
    std::string line;
    std::getline(file, line);
    
    while (std::getline(file, line) && (max_points < 0 || data.size() < (size_t)max_points)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                double lon = std::stod(lon_str);
                double lat = std::stod(lat_str);
                data.push_back(GpsPoint(lon, lat));
            } catch (...) {
                continue;
            }
        }
    }
    
    return data;
}

double TestAdaptive(const std::vector<GpsPoint>& gps_data, 
                   const ParameterConfig& config,
                   double epsilon) {
    TrajCompressSPAdaptiveCompressor compressor(
        gps_data.size(), 
        epsilon,
        config.cost_window_size,
        config.stability_margin,
        config.clear_after_switch,
        config.evaluation_interval
    );
    
    using AdaptiveGpsPoint = TrajCompressSPAdaptiveCompressor::GpsPoint;
    for (const auto& point : gps_data) {
        compressor.AddGpsPoint(AdaptiveGpsPoint(point.longitude, point.latitude));
    }
    
    compressor.Close();
    auto stats = compressor.GetStats();
    
    return static_cast<double>(stats.total_bits) / stats.total_points;
}

double TestLinear(const std::vector<GpsPoint>& gps_data, double epsilon) {
    SerfQtLinearCompressor lon_compressor(gps_data.size(), epsilon);
    SerfQtLinearCompressor lat_compressor(gps_data.size(), epsilon);
    
    for (const auto& point : gps_data) {
        lon_compressor.AddValue(point.longitude);
        lat_compressor.AddValue(point.latitude);
    }
    
    lon_compressor.Close();
    lat_compressor.Close();
    
    long total_bits = lon_compressor.get_compressed_size_in_bits() + 
                      lat_compressor.get_compressed_size_in_bits();
    
    return static_cast<double>(total_bits) / gps_data.size();
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n" << std::string(140, '=') << std::endl;
    std::cout << "超激进参数搜索 - 寻找能同时超越Geolife和Track的通用配置" << std::endl;
    std::cout << std::string(140, '=') << std::endl;
    
    // 读取数据
    std::cout << "\n正在读取数据集..." << std::endl;
    auto geolife_data = ReadGpsData("test/data_set/Geolife_100k_longitude_latitude.csv", 100000);
    auto track_data = ReadGpsData("test/data_set/Track_63530k_longitude_latitude.csv", 63530);
    
    if (geolife_data.empty() || track_data.empty()) {
        std::cerr << "数据读取失败！" << std::endl;
        return 1;
    }
    
    std::cout << "Geolife点数: " << geolife_data.size() << std::endl;
    std::cout << "Track点数: " << track_data.size() << std::endl;
    
    // 测试Linear基准
    std::cout << "\n计算Linear基准..." << std::endl;
    double geolife_linear = TestLinear(geolife_data, epsilon);
    double track_linear = TestLinear(track_data, epsilon);
    
    std::cout << "Geolife Linear: " << std::fixed << std::setprecision(4) << geolife_linear << " bits/点" << std::endl;
    std::cout << "Track Linear: " << std::fixed << std::setprecision(4) << track_linear << " bits/点" << std::endl;
    
    // 生成超密集参数组合
    std::vector<ParameterConfig> configs;
    
    // 窗口大小：超密集采样，特别是在96-320之间
    std::vector<int> window_sizes;
    
    // 小窗口：64-96，步长8
    for (int w = 64; w <= 96; w += 8) {
        window_sizes.push_back(w);
    }
    
    // 中窗口：100-200，步长10（重点区域）
    for (int w = 100; w <= 200; w += 10) {
        window_sizes.push_back(w);
    }
    
    // 大窗口：210-320，步长10
    for (int w = 210; w <= 320; w += 10) {
        window_sizes.push_back(w);
    }
    
    // 超大窗口：340-400，步长20
    for (int w = 340; w <= 400; w += 20) {
        window_sizes.push_back(w);
    }
    
    // 边际值：0, 1, 2（全覆盖）
    std::vector<int> margins = {0, 1, 2};
    
    // 清空策略：true, false
    std::vector<bool> clear_flags = {true, false};
    
    // 评估间隔：6-20，步长2（超密集）
    std::vector<int> eval_intervals = {6, 8, 10, 12, 14, 16, 18, 20};
    
    // 生成所有组合（但采用智能策略）
    // 策略1: 零边际 + 不清空 + 所有窗口和评估间隔的组合（最有希望）
    for (int w : window_sizes) {
        for (int e : eval_intervals) {
            configs.push_back({w, 0, false, e});
        }
    }
    
    // 策略2: 低边际 + 不清空 + 关键窗口
    for (int w : {96, 110, 120, 130, 140, 150, 160, 180, 200, 220, 240, 260, 280, 300, 320}) {
        for (int e : {8, 10, 12, 14, 16}) {
            configs.push_back({w, 1, false, e});
        }
    }
    
    // 策略3: 零边际 + 清空 + 部分窗口（作为对比）
    for (int w : {96, 120, 150, 180, 210, 240, 270, 300}) {
        for (int e : {10, 12, 14, 16}) {
            configs.push_back({w, 0, true, e});
        }
    }
    
    // 策略4: 标准边际 + 不清空 + 关键窗口（保守对比）
    for (int w : {100, 120, 140, 160, 180, 200, 220, 240}) {
        configs.push_back({w, 2, false, 12});
    }
    
    std::cout << "\n生成配置总数: " << configs.size() << std::endl;
    std::cout << "预计测试时间: ~" << (configs.size() * 2 / 60) << " 分钟" << std::endl;
    std::cout << "\n开始测试...\n" << std::endl;
    
    std::vector<TestResult> results;
    int count = 0;
    int beats_both_count = 0;
    
    for (const auto& config : configs) {
        count++;
        
        if (count % 20 == 0) {
            std::cout << "进度: " << count << "/" << configs.size() 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (count * 100.0 / configs.size()) << "%)" << std::endl;
        }
        
        TestResult result;
        result.config = config;
        
        // 测试Geolife
        result.geolife_perf = TestAdaptive(geolife_data, config, epsilon);
        result.beats_linear_geolife = (result.geolife_perf < geolife_linear);
        
        // 测试Track
        result.track_perf = TestAdaptive(track_data, config, epsilon);
        result.beats_linear_track = (result.track_perf < track_linear);
        
        results.push_back(result);
        
        if (result.BeatsBothLinear()) {
            beats_both_count++;
            std::cout << "  🎉🎉 [" << count << "] " << config.GetName() 
                      << " - 同时超越！ Geolife=" << std::fixed << std::setprecision(4) 
                      << result.geolife_perf << " Track=" << result.track_perf << std::endl;
        }
    }
    
    std::cout << "\n" << std::string(140, '=') << std::endl;
    std::cout << "测试完成！" << std::endl;
    std::cout << std::string(140, '=') << std::endl;
    
    std::cout << "\n总测试配置数: " << configs.size() << std::endl;
    std::cout << "同时超越Linear的配置数: " << beats_both_count << std::endl;
    
    if (beats_both_count > 0) {
        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "🏆 成功找到能同时超越两个数据集的配置！" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        // 按最差情况性能排序（min-max策略）
        std::vector<TestResult*> winners;
        for (auto& r : results) {
            if (r.BeatsBothLinear()) {
                winners.push_back(&r);
            }
        }
        
        std::sort(winners.begin(), winners.end(),
                  [](const TestResult* a, const TestResult* b) {
                      // 首先按最差情况排序
                      if (std::abs(a->GetWorstCase() - b->GetWorstCase()) > 1e-6) {
                          return a->GetWorstCase() < b->GetWorstCase();
                      }
                      // 如果最差情况相同，按平均性能排序
                      double avg_a = (a->geolife_perf + a->track_perf) / 2.0;
                      double avg_b = (b->geolife_perf + b->track_perf) / 2.0;
                      return avg_a < avg_b;
                  });
        
        std::cout << "\n所有同时超越Linear的配置（按最差情况性能排序）:\n" << std::endl;
        std::cout << std::left
                  << std::setw(25) << "配置"
                  << std::setw(15) << "Geolife"
                  << std::setw(15) << "vs Linear"
                  << std::setw(15) << "Track"
                  << std::setw(15) << "vs Linear"
                  << std::setw(15) << "最差情况"
                  << std::setw(15) << "平均性能"
                  << std::endl;
        std::cout << std::string(140, '-') << std::endl;
        
        for (size_t i = 0; i < winners.size(); i++) {
            const auto& r = *winners[i];
            double geo_gap = ((r.geolife_perf - geolife_linear) / geolife_linear) * 100;
            double track_gap = ((r.track_perf - track_linear) / track_linear) * 100;
            double avg_perf = (r.geolife_perf + r.track_perf) / 2.0;
            
            if (i == 0) std::cout << "🏆 ";
            else std::cout << "   ";
            
            std::cout << std::left
                      << std::setw(25) << r.config.GetName()
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.geolife_perf
                      << std::setw(15) << std::fixed << std::setprecision(3) << geo_gap << "%"
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.track_perf
                      << std::setw(15) << std::fixed << std::setprecision(3) << track_gap << "%"
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.GetWorstCase()
                      << std::setw(15) << std::fixed << std::setprecision(4) << avg_perf;
            
            if (i == 0) std::cout << " <- 推荐";
            std::cout << std::endl;
        }
        
        // 打印最优配置详情
        if (!winners.empty()) {
            const auto& best = *winners[0];
            std::cout << "\n" << std::string(140, '=') << std::endl;
            std::cout << "🏆 推荐配置（Min-Max最优）" << std::endl;
            std::cout << std::string(140, '=') << std::endl;
            std::cout << "配置名称: " << best.config.GetName() << std::endl;
            std::cout << "  窗口大小: " << best.config.cost_window_size << std::endl;
            std::cout << "  稳定边际: " << best.config.stability_margin << std::endl;
            std::cout << "  切换后清空: " << (best.config.clear_after_switch ? "是" : "否") << std::endl;
            std::cout << "  评估间隔: " << best.config.evaluation_interval << std::endl;
            std::cout << "\n性能表现:" << std::endl;
            std::cout << "  Geolife: " << std::fixed << std::setprecision(4) << best.geolife_perf 
                      << " bits/点 (vs " << geolife_linear << ", 超越" 
                      << std::fixed << std::setprecision(3) 
                      << ((geolife_linear - best.geolife_perf) / geolife_linear * 100) << "%)" << std::endl;
            std::cout << "  Track: " << std::fixed << std::setprecision(4) << best.track_perf 
                      << " bits/点 (vs " << track_linear << ", 超越" 
                      << std::fixed << std::setprecision(3) 
                      << ((track_linear - best.track_perf) / track_linear * 100) << "%)" << std::endl;
            std::cout << "  最差情况: " << std::fixed << std::setprecision(4) << best.GetWorstCase() 
                      << " bits/点" << std::endl;
            std::cout << "  平均性能: " << std::fixed << std::setprecision(4) 
                      << ((best.geolife_perf + best.track_perf) / 2.0) << " bits/点" << std::endl;
        }
        
    } else {
        std::cout << "\n⚠️ 未找到能同时超越两个数据集的配置" << std::endl;
        std::cout << "\n最接近的配置（按综合性能排序）:\n" << std::endl;
        
        // 找出接近的配置
        std::sort(results.begin(), results.end(),
                  [](const TestResult& a, const TestResult& b) {
                      double score_a = std::max(a.geolife_perf - 7.1803, 0.0) + 
                                      std::max(a.track_perf - 6.2748, 0.0);
                      double score_b = std::max(b.geolife_perf - 7.1803, 0.0) + 
                                      std::max(b.track_perf - 6.2748, 0.0);
                      return score_a < score_b;
                  });
        
        std::cout << std::left
                  << std::setw(25) << "配置"
                  << std::setw(15) << "Geolife"
                  << std::setw(15) << "差距"
                  << std::setw(15) << "Track"
                  << std::setw(15) << "差距"
                  << std::setw(15) << "总差距"
                  << std::endl;
        std::cout << std::string(140, '-') << std::endl;
        
        for (size_t i = 0; i < std::min((size_t)20, results.size()); i++) {
            const auto& r = results[i];
            double geo_gap = r.geolife_perf - geolife_linear;
            double track_gap = r.track_perf - track_linear;
            double total_gap = std::max(geo_gap, 0.0) + std::max(track_gap, 0.0);
            
            std::cout << std::left
                      << std::setw(25) << r.config.GetName()
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.geolife_perf
                      << std::setw(15) << std::fixed << std::setprecision(4) << geo_gap
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.track_perf
                      << std::setw(15) << std::fixed << std::setprecision(4) << track_gap
                      << std::setw(15) << std::fixed << std::setprecision(4) << total_gap
                      << std::endl;
        }
    }
    
    std::cout << "\n" << std::string(140, '=') << std::endl;
    std::cout << "✅ 超激进参数搜索完成！" << std::endl;
    std::cout << std::string(140, '=') << std::endl;
    
    return 0;
}

