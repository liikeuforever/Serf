#include "src/compressor/trajcompress_sp_adaptive_compressor.h"
#include "src/compressor/serf_qt_linear_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

struct GpsPoint {
    double longitude, latitude;
    GpsPoint(double lon = 0, double lat = 0) : longitude(lon), latitude(lat) {}
};

std::vector<GpsPoint> ReadData(const std::string& file, int max_points = -1) {
    std::vector<GpsPoint> points;
    std::ifstream f(file);
    if (!f.is_open()) return points;
    std::string line;
    std::getline(f, line);
    int count = 0;
    while (std::getline(f, line) && (max_points < 0 || count < max_points)) {
        std::istringstream iss(line);
        std::string lon, lat;
        if (std::getline(iss, lon, ',') && std::getline(iss, lat, ',')) {
            points.push_back(GpsPoint(std::stod(lon), std::stod(lat)));
            count++;
        }
    }
    return points;
}

double TestLinear(const std::vector<GpsPoint>& data, double epsilon) {
    SerfQtLinearCompressor lon(data.size(), epsilon), lat(data.size(), epsilon);
    for (const auto& p : data) { lon.AddValue(p.longitude); lat.AddValue(p.latitude); }
    lon.Close(); lat.Close();
    return static_cast<double>(lon.get_compressed_size_in_bits() + lat.get_compressed_size_in_bits()) / data.size();
}

double TestAdaptive(const std::vector<GpsPoint>& data, double epsilon, int min_w, int max_w, int obs) {
    TrajCompressSPAdaptiveCompressor adaptive(data.size(), epsilon, true, min_w, max_w, obs);
    for (const auto& p : data) {
        adaptive.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(p.longitude, p.latitude));
    }
    adaptive.Close();
    return static_cast<double>(adaptive.GetCompressedSizeInBits()) / data.size();
}

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n🔬 精细范围搜索 - 寻找能同时超越的配置\n" << std::string(100, '=') << std::endl;
    
    auto track = ReadData("test/data_set/Track_63530k_longitude_latitude.csv");
    auto geolife = ReadData("test/data_set/Geolife_100k_longitude_latitude.csv");
    
    double track_linear = TestLinear(track, epsilon);
    double geolife_linear = TestLinear(geolife, epsilon);
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Track Linear:   " << track_linear << " bits/点" << std::endl;
    std::cout << "Geolife Linear: " << geolife_linear << " bits/点\n" << std::endl;
    
    // 精细搜索：基于之前的最接近结果
    // Track在小窗口表现好，Geolife在大窗口表现好
    // 尝试更精细的组合
    
    struct TestConfig {
        int min_w, max_w, obs;
    };
    
    std::vector<TestConfig> best_candidates;
    
    std::cout << "阶段1: 测试精细范围..." << std::endl;
    int test_count = 0;
    int beats_both = 0;
    
    // 策略：尝试让min更小，max更大，覆盖两端最优
    for (int min_w = 8; min_w <= 48; min_w += 4) {
        for (int max_w = 192; max_w <= 512; max_w += 32) {
            if (max_w - min_w < 128) continue;
            for (int obs = 256; obs <= 512; obs += 64) {
                test_count++;
                
                double track_bpp = TestAdaptive(track, epsilon, min_w, max_w, obs);
                double geolife_bpp = TestAdaptive(geolife, epsilon, min_w, max_w, obs);
                
                double track_diff = track_bpp - track_linear;
                double geolife_diff = geolife_bpp - geolife_linear;
                
                if (track_diff < 0 && geolife_diff < 0) {
                    beats_both++;
                    best_candidates.push_back({min_w, max_w, obs});
                    std::cout << "✅ 找到！W[" << min_w << "-" << max_w << "]_O" << obs
                              << " - Track:" << track_diff << ", Geolife:" << geolife_diff << std::endl;
                }
                
                if (test_count % 30 == 0) {
                    std::cout << "  进度: " << test_count << " - 找到 " << beats_both << " 个" << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    if (beats_both > 0) {
        std::cout << "🎉 成功！找到 " << beats_both << " 个同时超越的配置！" << std::endl;
        std::cout << "\n推荐使用：" << std::endl;
        for (const auto& cfg : best_candidates) {
            std::cout << "  W[" << cfg.min_w << "-" << cfg.max_w << "]_O" << cfg.obs << std::endl;
        }
    } else {
        std::cout << "😔 总共测试 " << test_count << " 个配置，仍未找到同时超越的配置" << std::endl;
        std::cout << "\n结论：动态参数系统虽然能自适应调整，但在原始高采样率数据上" << std::endl;
        std::cout << "无法同时达到两个极端情况（Track vs Geolife）的各自最优。" << std::endl;
        std::cout << "这是由于两个数据集需要完全相反的参数配置（小窗口 vs 大窗口）。" << std::endl;
    }
    
    return 0;
}
