#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>

struct GpsPoint {
    double longitude, latitude;
    GpsPoint(double lon = 0, double lat = 0) : longitude(lon), latitude(lat) {}
};

std::vector<GpsPoint> ReadData(const std::string& file) {
    std::vector<GpsPoint> points;
    std::ifstream f(file);
    if (!f.is_open()) {
        std::cerr << "⚠️  无法打开文件: " << file << std::endl;
        return points;
    }
    
    std::string line;
    std::getline(f, line); // skip header
    
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string lon, lat;
        if (std::getline(iss, lon, ',') && std::getline(iss, lat, ',')) {
            points.push_back(GpsPoint(std::stod(lon), std::stod(lat)));
        }
    }
    return points;
}

struct ParamConfig {
    int min_window;
    int max_window;
    int observe_window;
    
    std::string ToString() const {
        return "W[" + std::to_string(min_window) + "-" + std::to_string(max_window) + 
               "]_O" + std::to_string(observe_window);
    }
};

struct TestResult {
    ParamConfig config;
    double geolife_bpp;
    double track_bpp;
    double geolife_diff_vs_linear;  // 负数=更好
    double track_diff_vs_linear;    // 负数=更好
    
    bool BeatsBothLinear() const {
        return geolife_diff_vs_linear < 0 && track_diff_vs_linear < 0;
    }
    
    double WorstCase() const {
        return std::max(geolife_diff_vs_linear, track_diff_vs_linear);
    }
};

double TestLinear(const std::vector<GpsPoint>& data, double epsilon) {
    SerfQtLinearCompressor lon(data.size(), epsilon), lat(data.size(), epsilon);
    for (const auto& p : data) {
        lon.AddValue(p.longitude);
        lat.AddValue(p.latitude);
    }
    lon.Close();
    lat.Close();
    return static_cast<double>(lon.get_compressed_size_in_bits() + 
                               lat.get_compressed_size_in_bits()) / data.size();
}

double TestAdaptive(const std::vector<GpsPoint>& data, double epsilon, const ParamConfig& config) {
    TrajCompressSPAdaptiveCompressor adaptive(
        data.size(), epsilon, true, 
        config.min_window, config.max_window, config.observe_window
    );
    for (const auto& p : data) {
        adaptive.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(p.longitude, p.latitude));
    }
    adaptive.Close();
    return static_cast<double>(adaptive.GetCompressedSizeInBits()) / data.size();
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n" << std::string(120, '=') << std::endl;
    std::cout << "动态参数优化 - 目标：在Geolife和Track原始数据上超越Linear" << std::endl;
    std::cout << std::string(120, '=') << std::endl;
    
    // 加载数据集
    std::cout << "\n📊 加载数据集..." << std::endl;
    auto geolife_data = ReadData("test/data_set/Geolife_100k_longitude_latitude.csv");
    auto track_data = ReadData("test/data_set/Track_63530k_longitude_latitude.csv");
    
    if (geolife_data.empty() || track_data.empty()) {
        std::cerr << "❌ 数据加载失败" << std::endl;
        return 1;
    }
    
    std::cout << "Geolife: " << geolife_data.size() << " 点" << std::endl;
    std::cout << "Track: " << track_data.size() << " 点" << std::endl;
    
    // 计算Linear基准
    std::cout << "\n📏 计算Linear基准..." << std::endl;
    double geolife_linear = TestLinear(geolife_data, epsilon);
    double track_linear = TestLinear(track_data, epsilon);
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Geolife Linear: " << geolife_linear << " bits/点" << std::endl;
    std::cout << "Track Linear:   " << track_linear << " bits/点" << std::endl;
    
    // 定义参数搜索空间
    std::vector<ParamConfig> configs;
    
    // 策略1: 小窗口（快速响应）- 适合Track这样的简单轨迹
    std::cout << "\n🔍 策略1: 小窗口（16-64）..." << std::endl;
    for (int min_w = 16; min_w <= 32; min_w += 8) {
        for (int max_w = 48; max_w <= 96; max_w += 16) {
            if (max_w <= min_w) continue;
            for (int obs = 128; obs <= 256; obs += 64) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略2: 中等窗口（平衡）
    std::cout << "🔍 策略2: 中等窗口（24-128）..." << std::endl;
    for (int min_w = 24; min_w <= 48; min_w += 8) {
        for (int max_w = 96; max_w <= 160; max_w += 16) {
            if (max_w <= min_w) continue;
            for (int obs = 192; obs <= 320; obs += 64) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略3: 大窗口（长期稳定）- 适合Geolife这样的复杂轨迹
    std::cout << "🔍 策略3: 大窗口（32-192）..." << std::endl;
    for (int min_w = 32; min_w <= 64; min_w += 16) {
        for (int max_w = 128; max_w <= 256; max_w += 32) {
            if (max_w <= min_w) continue;
            for (int obs = 256; obs <= 384; obs += 64) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略4: 超大窗口（极端稳定）
    std::cout << "🔍 策略4: 超大窗口（48-320）..." << std::endl;
    for (int min_w = 48; min_w <= 96; min_w += 24) {
        for (int max_w = 192; max_w <= 384; max_w += 64) {
            if (max_w <= min_w) continue;
            for (int obs = 320; obs <= 512; obs += 96) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略5: 窄范围窗口（精细调节）
    std::cout << "🔍 策略5: 窄范围窗口（20-80，精细控制）..." << std::endl;
    for (int min_w = 20; min_w <= 40; min_w += 4) {
        for (int max_w = 64; max_w <= 96; max_w += 8) {
            if (max_w - min_w < 32) continue;
            for (int obs = 160; obs <= 256; obs += 32) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    std::cout << "\n总配置数: " << configs.size() << std::endl;
    std::cout << "预计测试时间: ~" << (configs.size() * 4 / 60) << " 分钟" << std::endl;
    
    std::cout << "\n" << std::string(120, '=') << std::endl;
    std::cout << "开始测试..." << std::endl;
    std::cout << std::string(120, '=') << std::endl;
    
    std::vector<TestResult> results;
    std::vector<TestResult> beating_both;
    
    int count = 0;
    for (const auto& config : configs) {
        count++;
        
        double geolife_bpp = TestAdaptive(geolife_data, epsilon, config);
        double track_bpp = TestAdaptive(track_data, epsilon, config);
        
        TestResult result;
        result.config = config;
        result.geolife_bpp = geolife_bpp;
        result.track_bpp = track_bpp;
        result.geolife_diff_vs_linear = geolife_bpp - geolife_linear;
        result.track_diff_vs_linear = track_bpp - track_linear;
        
        results.push_back(result);
        
        if (result.BeatsBothLinear()) {
            beating_both.push_back(result);
        }
        
        if (count % 20 == 0 || count == configs.size()) {
            std::cout << "进度: " << count << "/" << configs.size() 
                      << " (" << std::fixed << std::setprecision(1) 
                      << (100.0 * count / configs.size()) << "%)";
            std::cout << " - 同时超越: " << beating_both.size() << " 个配置";
            
            if (!beating_both.empty()) {
                auto& best = *std::min_element(beating_both.begin(), beating_both.end(),
                    [](const TestResult& a, const TestResult& b) {
                        return a.WorstCase() < b.WorstCase();
                    });
                std::cout << " - 最佳: " << best.config.ToString();
            }
            std::cout << std::endl;
        }
    }
    
    // 结果分析
    std::cout << "\n" << std::string(120, '=') << std::endl;
    std::cout << "📊 结果分析" << std::endl;
    std::cout << std::string(120, '=') << std::endl;
    
    if (beating_both.empty()) {
        std::cout << "\n❌ 没有找到同时超越Linear的配置" << std::endl;
        
        // 找出最接近的配置
        std::cout << "\n🔍 最接近的TOP 10配置：" << std::endl;
        std::sort(results.begin(), results.end(), 
            [](const TestResult& a, const TestResult& b) {
                return a.WorstCase() < b.WorstCase();
            });
        
        std::cout << std::setw(30) << "配置"
                  << std::setw(18) << "Geolife (diff)"
                  << std::setw(18) << "Track (diff)"
                  << std::setw(18) << "Worst Case" << std::endl;
        std::cout << std::string(120, '-') << std::endl;
        
        for (int i = 0; i < std::min(10, (int)results.size()); i++) {
            const auto& r = results[i];
            std::cout << std::setw(30) << r.config.ToString()
                      << std::setw(18) << std::fixed << std::setprecision(6) 
                      << (r.geolife_diff_vs_linear >= 0 ? "+" : "") << r.geolife_diff_vs_linear
                      << std::setw(18) << (r.track_diff_vs_linear >= 0 ? "+" : "") << r.track_diff_vs_linear
                      << std::setw(18) << r.WorstCase() << std::endl;
        }
    } else {
        std::cout << "\n✅ 找到 " << beating_both.size() << " 个同时超越Linear的配置！" << std::endl;
        
        // 按"最差情况"排序
        std::sort(beating_both.begin(), beating_both.end(),
            [](const TestResult& a, const TestResult& b) {
                return a.WorstCase() < b.WorstCase();
            });
        
        std::cout << "\n🏆 TOP 10 最佳配置：" << std::endl;
        std::cout << std::setw(30) << "配置"
                  << std::setw(18) << "Geolife (diff)"
                  << std::setw(18) << "Track (diff)"
                  << std::setw(18) << "Worst Case" << std::endl;
        std::cout << std::string(120, '-') << std::endl;
        
        for (int i = 0; i < std::min(10, (int)beating_both.size()); i++) {
            const auto& r = beating_both[i];
            std::cout << std::setw(30) << r.config.ToString()
                      << std::setw(18) << std::fixed << std::setprecision(6) 
                      << r.geolife_diff_vs_linear
                      << std::setw(18) << r.track_diff_vs_linear
                      << std::setw(18) << r.WorstCase() << " 🎉" << std::endl;
        }
        
        // 推荐配置
        const auto& best = beating_both[0];
        std::cout << "\n💡 推荐配置：" << std::endl;
        std::cout << "  min_window = " << best.config.min_window << std::endl;
        std::cout << "  max_window = " << best.config.max_window << std::endl;
        std::cout << "  observe_window = " << best.config.observe_window << std::endl;
        std::cout << "\n性能：" << std::endl;
        std::cout << "  Geolife: " << std::fixed << std::setprecision(6) 
                  << best.geolife_bpp << " (vs Linear " 
                  << geolife_linear << ", " << best.geolife_diff_vs_linear << ")" << std::endl;
        std::cout << "  Track:   " << best.track_bpp << " (vs Linear " 
                  << track_linear << ", " << best.track_diff_vs_linear << ")" << std::endl;
    }
    
    return 0;
}

