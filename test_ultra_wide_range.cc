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

std::vector<GpsPoint> ReadData(const std::string& file) {
    std::vector<GpsPoint> points;
    std::ifstream f(file);
    if (!f.is_open()) return points;
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string lon, lat;
        if (std::getline(iss, lon, ',') && std::getline(iss, lat, ',')) {
            points.push_back(GpsPoint(std::stod(lon), std::stod(lat)));
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
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "测试超大动态范围 - 验证理论上限" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    auto geolife = ReadData("test/data_set/Geolife_100k_longitude_latitude.csv");
    auto track = ReadData("test/data_set/Track_63530k_longitude_latitude.csv");
    
    std::cout << "\nGeolife: " << geolife.size() << " 点" << std::endl;
    std::cout << "Track: " << track.size() << " 点" << std::endl;
    
    double geo_linear = TestLinear(geolife, epsilon);
    double track_linear = TestLinear(track, epsilon);
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\n基准性能：" << std::endl;
    std::cout << "Geolife Linear: " << geo_linear << " bits/点" << std::endl;
    std::cout << "Track Linear:   " << track_linear << " bits/点" << std::endl;
    
    std::cout << "\n" << std::string(100, '-') << std::endl;
    std::cout << std::setw(50) << "配置" 
              << std::setw(18) << "Geolife (diff)"
              << std::setw(18) << "Track (diff)"
              << std::setw(14) << "同时超越?" << std::endl;
    std::cout << std::string(100, '-') << std::endl;
    
    struct TestConfig {
        int min_w, max_w, obs;
        std::string desc;
    };
    
    std::vector<TestConfig> configs = {
        // 当前默认
        {32, 128, 256, "当前默认 (32-128)"},
        
        // 扩大到Track最优
        {16, 160, 256, "扩展到160"},
        {16, 192, 256, "扩展到192"},
        
        // 扩大到Geolife最优
        {16, 256, 320, "扩展到256"},
        {16, 320, 384, "扩展到320 (Geolife最优)"},
        {16, 384, 448, "扩展到384"},
        {16, 448, 512, "扩展到448"},
        {16, 512, 576, "扩展到512"},
        
        // 极限范围
        {8, 384, 448, "极限范围 (8-384)"},
        {8, 512, 576, "超极限 (8-512)"},
        {8, 640, 640, "超超极限 (8-640)"},
        
        // 不同观察窗口
        {16, 320, 256, "Geolife最优窗+小观察"},
        {16, 320, 512, "Geolife最优窗+大观察"},
        {16, 320, 768, "Geolife最优窗+超大观察"},
    };
    
    int beat_both_count = 0;
    
    for (const auto& cfg : configs) {
        double geo_bpp = TestAdaptive(geolife, epsilon, cfg.min_w, cfg.max_w, cfg.obs);
        double track_bpp = TestAdaptive(track, epsilon, cfg.min_w, cfg.max_w, cfg.obs);
        
        double geo_diff = geo_bpp - geo_linear;
        double track_diff = track_bpp - track_linear;
        
        bool beats_both = (geo_diff < 0) && (track_diff < 0);
        if (beats_both) beat_both_count++;
        
        std::string config_str = "W[" + std::to_string(cfg.min_w) + "-" + 
                                std::to_string(cfg.max_w) + "]_O" + std::to_string(cfg.obs);
        
        std::cout << std::setw(50) << (config_str + " (" + cfg.desc + ")")
                  << std::setw(18) << (geo_diff >= 0 ? "+" : "") << geo_diff
                  << std::setw(18) << (track_diff >= 0 ? "+" : "") << track_diff
                  << std::setw(14) << (beats_both ? "✅ YES!" : "❌ No") << std::endl;
    }
    
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "\n📊 结果：" << std::endl;
    std::cout << "同时超越Linear的配置数: " << beat_both_count << "/" << configs.size() << std::endl;
    
    if (beat_both_count > 0) {
        std::cout << "\n🎉 成功！动态参数确实可以在两个数据集上都达到最优！" << std::endl;
        std::cout << "关键：需要足够大的窗口范围（至少要覆盖各个数据集的最优窗口）" << std::endl;
    } else {
        std::cout << "\n🤔 即使扩大范围也无法同时超越..." << std::endl;
        std::cout << "可能原因：" << std::endl;
        std::cout << "1. 动态调整的速度/幅度不够" << std::endl;
        std::cout << "2. 复杂度计算（churn rate）不够准确" << std::endl;
        std::cout << "3. 两个数据集的特性差异太大，需要不同的参数组合" << std::endl;
    }
    
    return 0;
}
