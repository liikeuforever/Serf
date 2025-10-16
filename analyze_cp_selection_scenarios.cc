/**
 * 分析原始CP预测器被选中的场景特征
 * 找出CP相对于LDR的优势场景
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <algorithm>

struct GpsPoint {
    double longitude;
    double latitude;
    
    GpsPoint() : longitude(0), latitude(0) {}
    GpsPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
    
    GpsPoint operator-(const GpsPoint& other) const {
        return GpsPoint(longitude - other.longitude, latitude - other.latitude);
    }
    
    GpsPoint operator+(const GpsPoint& other) const {
        return GpsPoint(longitude + other.longitude, latitude + other.latitude);
    }
};

struct HistoryState {
    GpsPoint point;
    GpsPoint velocity;
    HistoryState(const GpsPoint& p, const GpsPoint& v) : point(p), velocity(v) {}
};

std::vector<GpsPoint> LoadGpsDataFromCSV(const std::string& filename, int max_points) {
    std::vector<GpsPoint> points;
    std::ifstream file(filename);
    if (!file.is_open()) return points;
    
    std::string line;
    int count = 0;
    while (std::getline(file, line) && count < max_points) {
        std::stringstream ss(line);
        std::string lon_str, lat_str;
        if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str, ',')) {
            try {
                points.emplace_back(std::stod(lon_str), std::stod(lat_str));
                count++;
            } catch (...) {}
        }
    }
    return points;
}

double CalculateDistance(const GpsPoint& p1, const GpsPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    std::string dataset_path = "/Users/xuzihang/GitProject/GG/Serf/test/data_set/Geolife_100k_longitude_latitude.csv";
    int max_points = 100000;
    
    auto gps_data = LoadGpsDataFromCSV(dataset_path, max_points);
    if (gps_data.size() < 10) return 1;
    
    std::cout << "============================================================" << std::endl;
    std::cout << "CP预测器被选中场景分析" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    // 模拟原始算法的预测器选择
    std::vector<HistoryState> history;
    GpsPoint current_point = gps_data[0];
    
    int cp_selected = 0, ldr_selected = 0, zp_selected = 0;
    
    // CP被选中时的场景特征
    std::vector<double> cp_acceleration_magnitude;  // CP选中时的加速度大小
    std::vector<double> cp_advantage;  // CP相对于LDR的误差优势
    std::vector<double> ldr_advantage;  // LDR相对于CP的误差优势（LDR选中时）
    
    for (size_t i = 0; i < gps_data.size(); i++) {
        // 更新历史
        if (i > 0) {
            GpsPoint velocity = gps_data[i] - gps_data[i-1];
            history.emplace_back(gps_data[i], velocity);
            if (history.size() > 10) history.erase(history.begin());
        } else {
            history.emplace_back(gps_data[i], GpsPoint(0, 0));
        }
        current_point = gps_data[i];
        
        if (i + 1 >= gps_data.size()) break;
        
        // 预测下一个点
        GpsPoint pred_ldr, pred_cp, pred_zp;
        
        // ZP
        pred_zp = current_point;
        
        if (history.size() >= 2) {
            // LDR
            GpsPoint velocity = history[history.size() - 1].velocity;
            pred_ldr = current_point + velocity;
            
            // CP (原始3点二阶)
            if (history.size() >= 3) {
                GpsPoint prev_velocity = history[history.size() - 2].velocity;
                GpsPoint acceleration = velocity - prev_velocity;
                pred_cp = current_point + velocity + acceleration;
                
                // 计算误差
                double error_ldr = CalculateDistance(gps_data[i+1], pred_ldr);
                double error_cp = CalculateDistance(gps_data[i+1], pred_cp);
                double error_zp = CalculateDistance(gps_data[i+1], pred_zp);
                
                // 选择最优预测器
                if (error_ldr <= error_cp && error_ldr <= error_zp) {
                    ldr_selected++;
                    ldr_advantage.push_back(error_cp - error_ldr);  // CP比LDR差多少
                } else if (error_cp <= error_zp) {
                    cp_selected++;
                    cp_advantage.push_back(error_ldr - error_cp);  // CP比LDR好多少
                    
                    // 记录加速度大小
                    double acc_mag = std::sqrt(
                        acceleration.longitude * acceleration.longitude +
                        acceleration.latitude * acceleration.latitude
                    );
                    cp_acceleration_magnitude.push_back(acc_mag);
                } else {
                    zp_selected++;
                }
            }
        }
    }
    
    int total = cp_selected + ldr_selected + zp_selected;
    
    std::cout << "=== 预测器选择统计 ===" << std::endl;
    std::cout << "LDR: " << ldr_selected << " (" << (100.0 * ldr_selected / total) << "%)" << std::endl;
    std::cout << "CP:  " << cp_selected << " (" << (100.0 * cp_selected / total) << "%)" << std::endl;
    std::cout << "ZP:  " << zp_selected << " (" << (100.0 * zp_selected / total) << "%)" << std::endl;
    
    // 分析CP的优势
    if (!cp_advantage.empty()) {
        std::sort(cp_advantage.begin(), cp_advantage.end());
        std::sort(cp_acceleration_magnitude.begin(), cp_acceleration_magnitude.end());
        
        double avg_advantage = 0;
        for (auto v : cp_advantage) avg_advantage += v;
        avg_advantage /= cp_advantage.size();
        
        double avg_acc = 0;
        for (auto v : cp_acceleration_magnitude) avg_acc += v;
        avg_acc /= cp_acceleration_magnitude.size();
        
        std::cout << "\n=== CP被选中时的特征 ===" << std::endl;
        std::cout << std::scientific << std::setprecision(2);
        std::cout << "CP相对于LDR的误差优势（平均）: " << avg_advantage << " 度" << std::endl;
        std::cout << "  P50: " << cp_advantage[cp_advantage.size() * 0.50] << std::endl;
        std::cout << "  P90: " << cp_advantage[cp_advantage.size() * 0.90] << std::endl;
        std::cout << "  P99: " << cp_advantage[cp_advantage.size() * 0.99] << std::endl;
        
        std::cout << "\n加速度大小（平均）: " << avg_acc << " 度" << std::endl;
        std::cout << "  P50: " << cp_acceleration_magnitude[cp_acceleration_magnitude.size() * 0.50] << std::endl;
        std::cout << "  P90: " << cp_acceleration_magnitude[cp_acceleration_magnitude.size() * 0.90] << std::endl;
        std::cout << "  P99: " << cp_acceleration_magnitude[cp_acceleration_magnitude.size() * 0.99] << std::endl;
    }
    
    // 分析LDR的优势
    if (!ldr_advantage.empty()) {
        std::sort(ldr_advantage.begin(), ldr_advantage.end());
        
        double avg_ldr = 0;
        for (auto v : ldr_advantage) avg_ldr += v;
        avg_ldr /= ldr_advantage.size();
        
        std::cout << "\n=== LDR被选中时CP的劣势 ===" << std::endl;
        std::cout << "CP相对于LDR差多少（平均）: " << avg_ldr << " 度" << std::endl;
        std::cout << "  P50: " << ldr_advantage[ldr_advantage.size() * 0.50] << std::endl;
        std::cout << "  P90: " << ldr_advantage[ldr_advantage.size() * 0.90] << std::endl;
        std::cout << "  P99: " << ldr_advantage[ldr_advantage.size() * 0.99] << std::endl;
    }
    
    std::cout << "\n=== 关键发现 ===" << std::endl;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "1. CP在 " << (100.0 * cp_selected / total) << "% 的场景下是最优的" << std::endl;
    std::cout << "2. 在这些场景下，CP的优势是因为能够捕捉加速度变化" << std::endl;
    std::cout << "3. 但在 " << (100.0 * ldr_selected / total) << "% 的场景下，LDR更优" << std::endl;
    std::cout << "4. CP的改进方向：" << std::endl;
    std::cout << "   - 保留对加速度的敏感性（不能过度平滑）" << std::endl;
    std::cout << "   - 可能需要在噪声抑制和响应性之间找平衡" << std::endl;
    
    std::cout << "\n============================================================" << std::endl;
    
    return 0;
}


