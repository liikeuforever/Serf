/**
 * 穷举式参数搜索 - 全范围超细粒度扫描
 * 目标：找到能同时超越Geolife和Track的通用配置
 * 策略：在32-640窗口范围内，步长2-5进行极细扫描
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
    
    double GetTotalGap() const {
        return geolife_perf + track_perf;
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
    std::cout << "穷举式参数搜索 - 全范围超细粒度扫描" << std::endl;
    std::cout << "目标: 找到能同时超越Geolife和Track的通用配置" << std::endl;
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
    
    // 生成超大范围超细粒度参数组合
    std::vector<ParameterConfig> configs;
    
    // 窗口大小策略：全范围覆盖，不同区域不同粒度
    std::vector<int> window_sizes;
    
    // 区域1: 32-50，步长1（超小窗口，极细）
    for (int w = 32; w <= 50; w += 1) {
        window_sizes.push_back(w);
    }
    
    // 区域2: 52-100，步长2（小窗口，很细）
    for (int w = 52; w <= 100; w += 2) {
        window_sizes.push_back(w);
    }
    
    // 区域3: 102-200，步长2（中窗口，很细 - 关键区域）
    for (int w = 102; w <= 200; w += 2) {
        window_sizes.push_back(w);
    }
    
    // 区域4: 205-350，步长5（大窗口，中等细度 - 关键区域）
    for (int w = 205; w <= 350; w += 5) {
        window_sizes.push_back(w);
    }
    
    // 区域5: 360-450，步长10（超大窗口）
    for (int w = 360; w <= 450; w += 10) {
        window_sizes.push_back(w);
    }
    
    // 区域6: 480-640，步长20（极大窗口，探索边界）
    for (int w = 480; w <= 640; w += 20) {
        window_sizes.push_back(w);
    }
    
    // 边际值：0, 1, 2（全覆盖）
    std::vector<int> margins = {0, 1, 2};
    
    // 清空策略：false优先（已知不清空更好）
    std::vector<bool> clear_flags = {false, true};
    
    // 评估间隔：4-24，步长2（超细粒度）
    std::vector<int> eval_intervals = {4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24};
    
    std::cout << "\n窗口尺寸候选数: " << window_sizes.size() << std::endl;
    std::cout << "评估间隔候选数: " << eval_intervals.size() << std::endl;
    
    // 生成配置：聚焦于最有希望的组合
    // 策略1: 零边际 + 不清空 + 全部窗口 + 所有评估间隔
    std::cout << "\n策略1: 零边际 + 不清空（最激进）..." << std::endl;
    for (int w : window_sizes) {
        for (int e : eval_intervals) {
            configs.push_back({w, 0, false, e});
        }
    }
    
    // 策略2: 低边际 + 不清空 + 全部窗口 + 关键评估间隔
    std::cout << "策略2: 低边际 + 不清空..." << std::endl;
    for (int w : window_sizes) {
        for (int e : {6, 8, 10, 12, 14, 16}) {
            configs.push_back({w, 1, false, e});
        }
    }
    
    // 策略3: 零边际 + 清空 + 选择窗口（作为对比）
    std::cout << "策略3: 零边际 + 清空（对比）..." << std::endl;
    for (int w : window_sizes) {
        if (w % 10 == 0 || w % 5 == 0) {  // 只测试部分窗口
            for (int e : {8, 12, 16}) {
                configs.push_back({w, 0, true, e});
            }
        }
    }
    
    // 策略4: 标准边际 + 不清空 + 选择窗口
    std::cout << "策略4: 标准边际 + 不清空..." << std::endl;
    for (int w : window_sizes) {
        if (w % 10 == 0) {  // 每10个测试一次
            configs.push_back({w, 2, false, 12});
            configs.push_back({w, 2, false, 16});
        }
    }
    
    std::cout << "\n总配置数: " << configs.size() << std::endl;
    std::cout << "预计测试时间: ~" << (configs.size() * 2 / 60) << " 分钟" << std::endl;
    std::cout << "\n开始穷举式搜索...\n" << std::endl;
    
    std::vector<TestResult> results;
    int count = 0;
    int beats_both_count = 0;
    TestResult* best_both = nullptr;
    
    for (const auto& config : configs) {
        count++;
        
        if (count % 50 == 0 || count == 1) {
            std::cout << "进度: " << count << "/" << configs.size() 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (count * 100.0 / configs.size()) << "%) - 已找到" << beats_both_count << "个同时超越的配置" << std::endl;
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
            double geo_improvement = ((geolife_linear - result.geolife_perf) / geolife_linear) * 100;
            double track_improvement = ((track_linear - result.track_perf) / track_linear) * 100;
            
            std::cout << "  🎉🎉🎉 [" << count << "] " << config.GetName() 
                      << " - 同时超越！" << std::endl;
            std::cout << "     Geolife: " << std::fixed << std::setprecision(4) << result.geolife_perf 
                      << " (超越" << std::fixed << std::setprecision(3) << geo_improvement << "%)" << std::endl;
            std::cout << "     Track: " << std::fixed << std::setprecision(4) << result.track_perf 
                      << " (超越" << std::fixed << std::setprecision(3) << track_improvement << "%)" << std::endl;
            
            if (best_both == nullptr || result.GetTotalGap() < best_both->GetTotalGap()) {
                best_both = &results.back();
                std::cout << "     ⭐ 新的最优配置！" << std::endl;
            }
        }
    }
    
    std::cout << "\n" << std::string(140, '=') << std::endl;
    std::cout << "穷举式搜索完成！" << std::endl;
    std::cout << std::string(140, '=') << std::endl;
    
    std::cout << "\n总测试配置数: " << configs.size() << std::endl;
    std::cout << "同时超越Linear的配置数: " << beats_both_count << std::endl;
    
    if (beats_both_count > 0) {
        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "🏆🏆🏆 成功！找到了" << beats_both_count << "个能同时超越两个数据集的配置！" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        // 收集所有成功配置
        std::vector<TestResult*> winners;
        for (auto& r : results) {
            if (r.BeatsBothLinear()) {
                winners.push_back(&r);
            }
        }
        
        // 按总性能排序
        std::sort(winners.begin(), winners.end(),
                  [](const TestResult* a, const TestResult* b) {
                      return a->GetTotalGap() < b->GetTotalGap();
                  });
        
        std::cout << "\n所有同时超越Linear的配置（按总性能排序，越小越好）:\n" << std::endl;
        std::cout << std::left
                  << std::setw(25) << "配置"
                  << std::setw(15) << "Geolife"
                  << std::setw(15) << "超越%"
                  << std::setw(15) << "Track"
                  << std::setw(15) << "超越%"
                  << std::setw(15) << "总和"
                  << std::endl;
        std::cout << std::string(140, '-') << std::endl;
        
        for (size_t i = 0; i < winners.size(); i++) {
            const auto& r = *winners[i];
            double geo_improvement = ((geolife_linear - r.geolife_perf) / geolife_linear) * 100;
            double track_improvement = ((track_linear - r.track_perf) / track_linear) * 100;
            
            if (i == 0) std::cout << "🥇 ";
            else if (i == 1) std::cout << "🥈 ";
            else if (i == 2) std::cout << "🥉 ";
            else std::cout << "   ";
            
            std::cout << std::left
                      << std::setw(25) << r.config.GetName()
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.geolife_perf
                      << std::setw(15) << std::fixed << std::setprecision(4) << geo_improvement
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.track_perf
                      << std::setw(15) << std::fixed << std::setprecision(4) << track_improvement
                      << std::setw(15) << std::fixed << std::setprecision(4) << r.GetTotalGap()
                      << std::endl;
        }
        
        // 打印最优配置详情
        if (!winners.empty()) {
            const auto& best = *winners[0];
            std::cout << "\n" << std::string(140, '=') << std::endl;
            std::cout << "🏆 最优通用配置" << std::endl;
            std::cout << std::string(140, '=') << std::endl;
            std::cout << "配置名称: " << best.config.GetName() << std::endl;
            std::cout << "  窗口大小: " << best.config.cost_window_size << std::endl;
            std::cout << "  稳定边际: " << best.config.stability_margin << std::endl;
            std::cout << "  切换后清空: " << (best.config.clear_after_switch ? "是" : "否") << std::endl;
            std::cout << "  评估间隔: " << best.config.evaluation_interval << std::endl;
            std::cout << "\n性能表现:" << std::endl;
            
            double geo_imp = ((geolife_linear - best.geolife_perf) / geolife_linear) * 100;
            double track_imp = ((track_linear - best.track_perf) / track_linear) * 100;
            
            std::cout << "  Geolife: " << std::fixed << std::setprecision(4) << best.geolife_perf 
                      << " bits/点 (vs " << geolife_linear << ", 🏆 超越" 
                      << std::fixed << std::setprecision(4) << geo_imp << "%)" << std::endl;
            std::cout << "  Track: " << std::fixed << std::setprecision(4) << best.track_perf 
                      << " bits/点 (vs " << track_linear << ", 🏆 超越" 
                      << std::fixed << std::setprecision(4) << track_imp << "%)" << std::endl;
            std::cout << "  平均性能: " << std::fixed << std::setprecision(4) 
                      << ((best.geolife_perf + best.track_perf) / 2.0) << " bits/点" << std::endl;
            std::cout << "  总和: " << std::fixed << std::setprecision(4) << best.GetTotalGap() << " bits/点" << std::endl;
        }
        
    } else {
        std::cout << "\n⚠️ 未找到能同时超越两个数据集的配置" << std::endl;
        std::cout << "\n最接近的前20个配置:\n" << std::endl;
        
        // 按距离Linear的总差距排序
        std::sort(results.begin(), results.end(),
                  [&](const TestResult& a, const TestResult& b) {
                      double gap_a = std::max(a.geolife_perf - geolife_linear, 0.0) + 
                                    std::max(a.track_perf - track_linear, 0.0);
                      double gap_b = std::max(b.geolife_perf - geolife_linear, 0.0) + 
                                    std::max(b.track_perf - track_linear, 0.0);
                      return gap_a < gap_b;
                  });
        
        std::cout << std::left
                  << std::setw(25) << "配置"
                  << std::setw(15) << "Geolife"
                  << std::setw(15) << "差距Geo"
                  << std::setw(15) << "Track"
                  << std::setw(15) << "差距Track"
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
    std::cout << "✅ 穷举式参数搜索完成！" << std::endl;
    std::cout << std::string(140, '=') << std::endl;
    
    return 0;
}

