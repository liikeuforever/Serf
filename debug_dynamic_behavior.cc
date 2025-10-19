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

// 自定义压缩器，添加调试输出
class DebugAdaptiveCompressor : public TrajCompressSPAdaptiveCompressor {
public:
    DebugAdaptiveCompressor(int block_size, double epsilon, bool enable_adaptive, 
                           int min_w, int max_w, int obs)
        : TrajCompressSPAdaptiveCompressor(block_size, epsilon, enable_adaptive, min_w, max_w, obs) {}
    
    // 暴露内部状态用于调试
    int GetCurrentWindowSize() const { return kCostWindowSize; }
    int GetCurrentMargin() const { return kStabilityMargin; }
};

int main() {
    double epsilon = 1e-5;
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "调试：观察动态参数在运行时的实际行为" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 只测试前10000点，加快速度
    auto track_data = ReadData("test/data_set/Track_63530k_longitude_latitude.csv", 10000);
    auto geolife_data = ReadData("test/data_set/Geolife_100k_longitude_latitude.csv", 10000);
    
    std::cout << "\nTrack: " << track_data.size() << " 点" << std::endl;
    std::cout << "Geolife: " << geolife_data.size() << " 点\n" << std::endl;
    
    // 测试配置
    struct TestConfig {
        int min_w, max_w, obs;
        std::string name;
    };
    
    std::vector<TestConfig> configs = {
        {16, 160, 256, "平衡配置"},
        {8, 512, 576, "超大范围"}
    };
    
    for (const auto& cfg : configs) {
        std::cout << "\n" << std::string(100, '-') << std::endl;
        std::cout << "测试配置: " << cfg.name 
                  << " (W[" << cfg.min_w << "-" << cfg.max_w << "]_O" << cfg.obs << ")" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        
        // Track
        std::cout << "\n【Track数据集】" << std::endl;
        {
            DebugAdaptiveCompressor comp(track_data.size(), epsilon, true, 
                                        cfg.min_w, cfg.max_w, cfg.obs);
            
            int sample_interval = track_data.size() / 10;
            for (size_t i = 0; i < track_data.size(); i++) {
                const auto& p = track_data[i];
                comp.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(p.longitude, p.latitude));
                
                // 每10%采样一次
                if (i > 0 && i % sample_interval == 0) {
                    std::cout << "  进度 " << std::setw(3) << (i * 100 / track_data.size()) << "%: "
                              << "Window=" << std::setw(3) << comp.GetCurrentWindowSize()
                              << ", Margin=" << comp.GetCurrentMargin() << std::endl;
                }
            }
            comp.Close();
            double bpp = static_cast<double>(comp.GetCompressedSizeInBits()) / track_data.size();
            std::cout << "  最终性能: " << std::fixed << std::setprecision(6) << bpp << " bits/点" << std::endl;
        }
        
        // Geolife
        std::cout << "\n【Geolife数据集】" << std::endl;
        {
            DebugAdaptiveCompressor comp(geolife_data.size(), epsilon, true, 
                                        cfg.min_w, cfg.max_w, cfg.obs);
            
            int sample_interval = geolife_data.size() / 10;
            for (size_t i = 0; i < geolife_data.size(); i++) {
                const auto& p = geolife_data[i];
                comp.AddGpsPoint(TrajCompressSPAdaptiveCompressor::GpsPoint(p.longitude, p.latitude));
                
                if (i > 0 && i % sample_interval == 0) {
                    std::cout << "  进度 " << std::setw(3) << (i * 100 / geolife_data.size()) << "%: "
                              << "Window=" << std::setw(3) << comp.GetCurrentWindowSize()
                              << ", Margin=" << comp.GetCurrentMargin() << std::endl;
                }
            }
            comp.Close();
            double bpp = static_cast<double>(comp.GetCompressedSizeInBits()) / geolife_data.size();
            std::cout << "  最终性能: " << std::fixed << std::setprecision(6) << bpp << " bits/点" << std::endl;
        }
    }
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "💡 观察要点：" << std::endl;
    std::cout << "1. Track（简单）的窗口是否快速调小？" << std::endl;
    std::cout << "2. Geolife（复杂）的窗口是否保持较大？" << std::endl;
    std::cout << "3. 窗口大小是否在整个过程中保持稳定（说明正确识别了轨迹特性）？" << std::endl;
    
    return 0;
}
