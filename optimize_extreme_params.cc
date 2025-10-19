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
    if (!f.is_open()) return points;
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
    int min_window, max_window, observe_window;
    std::string ToString() const {
        return "W[" + std::to_string(min_window) + "-" + std::to_string(max_window) + 
               "]_O" + std::to_string(observe_window);
    }
};

struct TestResult {
    ParamConfig config;
    double geolife_bpp, track_bpp;
    double geolife_diff, track_diff;
    
    bool BeatsBoth() const { return geolife_diff < 0 && track_diff < 0; }
    double WorstCase() const { return std::max(geolife_diff, track_diff); }
};

double TestLinear(const std::vector<GpsPoint>& data, double epsilon) {
    SerfQtLinearCompressor lon(data.size(), epsilon), lat(data.size(), epsilon);
    for (const auto& p : data) { lon.AddValue(p.longitude); lat.AddValue(p.latitude); }
    lon.Close(); lat.Close();
    return static_cast<double>(lon.get_compressed_size_in_bits() + lat.get_compressed_size_in_bits()) / data.size();
}

double TestAdaptive(const std::vector<GpsPoint>& data, double epsilon, const ParamConfig& config) {
    TrajCompressSPAdaptiveCompressor adaptive(data.size(), epsilon, true, 
        config.min_window, config.max_window, config.observe_window);
    for (const auto& p : data) {
        adaptive.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(p.longitude, p.latitude));
    }
    adaptive.Close();
    return static_cast<double>(adaptive.GetCompressedSizeInBits()) / data.size();
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n🚀 极限参数搜索 - 超大范围测试\n" << std::string(100, '=') << std::endl;
    
    auto geolife_data = ReadData("test/data_set/Geolife_100k_longitude_latitude.csv");
    auto track_data = ReadData("test/data_set/Track_63530k_longitude_latitude.csv");
    
    std::cout << "Geolife: " << geolife_data.size() << " 点, ";
    std::cout << "Track: " << track_data.size() << " 点\n" << std::endl;
    
    double geolife_linear = TestLinear(geolife_data, epsilon);
    double track_linear = TestLinear(track_data, epsilon);
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Geolife Linear: " << geolife_linear << " bits/点\n";
    std::cout << "Track Linear:   " << track_linear << " bits/点\n" << std::endl;
    
    std::vector<ParamConfig> configs;
    
    // 策略A: 超小窗口（极快响应）
    std::cout << "策略A: 超小窗口 (8-48)..." << std::endl;
    for (int min_w : {8, 12, 16, 20}) {
        for (int max_w : {32, 40, 48, 56, 64}) {
            if (max_w <= min_w + 16) continue;
            for (int obs : {96, 128, 160, 192}) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略B: 超大窗口（极慢但稳定）
    std::cout << "策略B: 超大窗口 (64-512)..." << std::endl;
    for (int min_w : {64, 80, 96, 112}) {
        for (int max_w : {256, 320, 384, 448, 512}) {
            if (max_w <= min_w * 2) continue;
            for (int obs : {384, 480, 576, 672}) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略C: 极端窄范围（精细控制）
    std::cout << "策略C: 极窄范围 (窗口跨度<40)..." << std::endl;
    for (int min_w = 10; min_w <= 50; min_w += 5) {
        for (int max_w = min_w + 20; max_w <= min_w + 40; max_w += 5) {
            for (int obs : {128, 192, 256}) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略D: 极端宽范围（大跨度）
    std::cout << "策略D: 极宽范围 (跨度>300)..." << std::endl;
    for (int min_w : {16, 24, 32, 40, 48}) {
        for (int max_w : {384, 448, 512, 576}) {
            if (max_w - min_w < 300) continue;
            for (int obs : {320, 416, 512}) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    // 策略E: 小min大max（最大灵活性）
    std::cout << "策略E: 小min大max (最大灵活度)..." << std::endl;
    for (int min_w : {8, 10, 12, 14, 16}) {
        for (int max_w : {256, 320, 384, 448, 512}) {
            for (int obs = 256; obs <= 512; obs += 64) {
                configs.push_back({min_w, max_w, obs});
            }
        }
    }
    
    std::cout << "\n总配置数: " << configs.size() << std::endl;
    std::cout << "预计时间: ~" << (configs.size() * 4 / 60) << " 分钟\n" << std::endl;
    
    std::cout << std::string(100, '=') << "\n开始测试...\n" << std::string(100, '=') << std::endl;
    
    std::vector<TestResult> results;
    std::vector<TestResult> beating_both;
    
    int count = 0;
    for (const auto& config : configs) {
        count++;
        TestResult result;
        result.config = config;
        result.geolife_bpp = TestAdaptive(geolife_data, epsilon, config);
        result.track_bpp = TestAdaptive(track_data, epsilon, config);
        result.geolife_diff = result.geolife_bpp - geolife_linear;
        result.track_diff = result.track_bpp - track_linear;
        
        results.push_back(result);
        if (result.BeatsBoth()) beating_both.push_back(result);
        
        if (count % 30 == 0 || count == configs.size()) {
            std::cout << "进度: " << count << "/" << configs.size() 
                      << " (" << std::fixed << std::setprecision(1) << (100.0*count/configs.size()) << "%)";
            std::cout << " - 同时超越: " << beating_both.size() << " 个";
            if (!beating_both.empty()) {
                auto& best = *std::min_element(beating_both.begin(), beating_both.end(),
                    [](const TestResult& a, const TestResult& b) { return a.WorstCase() < b.WorstCase(); });
                std::cout << " - 最佳: " << best.config.ToString();
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "\n" << std::string(100, '=') << "\n📊 结果分析\n" << std::string(100, '=') << std::endl;
    
    if (beating_both.empty()) {
        std::cout << "\n❌ 仍未找到同时超越的配置\n\n🔍 最接近的TOP 15配置：\n";
        std::sort(results.begin(), results.end(), 
            [](const TestResult& a, const TestResult& b) { return a.WorstCase() < b.WorstCase(); });
        
        std::cout << std::setw(25) << "配置"
                  << std::setw(18) << "Geo (diff)"
                  << std::setw(18) << "Track (diff)"
                  << std::setw(18) << "Worst" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        
        for (int i = 0; i < std::min(15, (int)results.size()); i++) {
            const auto& r = results[i];
            std::cout << std::setw(25) << r.config.ToString()
                      << std::setw(18) << std::fixed << std::setprecision(6) 
                      << (r.geolife_diff >= 0 ? "+" : "") << r.geolife_diff
                      << std::setw(18) << (r.track_diff >= 0 ? "+" : "") << r.track_diff
                      << std::setw(18) << r.WorstCase() << std::endl;
        }
    } else {
        std::cout << "\n✅ 找到 " << beating_both.size() << " 个同时超越的配置！\n\n🏆 TOP 10 配置：\n";
        std::sort(beating_both.begin(), beating_both.end(),
            [](const TestResult& a, const TestResult& b) { return a.WorstCase() < b.WorstCase(); });
        
        for (int i = 0; i < std::min(10, (int)beating_both.size()); i++) {
            const auto& r = beating_both[i];
            std::cout << (i+1) << ". " << r.config.ToString()
                      << " - Geo:" << std::fixed << std::setprecision(6) << r.geolife_diff
                      << ", Track:" << r.track_diff << " 🎉\n";
        }
        
        const auto& best = beating_both[0];
        std::cout << "\n💡 推荐配置：\n";
        std::cout << "  min_window = " << best.config.min_window << "\n";
        std::cout << "  max_window = " << best.config.max_window << "\n";
        std::cout << "  observe_window = " << best.config.observe_window << std::endl;
    }
    
    return 0;
}
