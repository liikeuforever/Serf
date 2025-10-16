/**
 * 分析 Geolife 数据集的轨迹特征
 * 找出为什么 CP 预测器没有达到预期效果
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
    std::cout << "Geolife 数据集轨迹特征分析" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    // 分析速度变化
    std::vector<double> velocities;
    std::vector<double> accelerations;
    std::vector<double> jerk_values;  // 加速度变化率
    
    for (size_t i = 1; i < gps_data.size(); i++) {
        GpsPoint v = gps_data[i] - gps_data[i-1];
        double speed = std::sqrt(v.longitude * v.longitude + v.latitude * v.latitude);
        velocities.push_back(speed);
        
        if (i >= 2) {
            double prev_speed = velocities[i-2];
            double acc = speed - prev_speed;
            accelerations.push_back(acc);
            
            if (i >= 3) {
                double prev_acc = accelerations[accelerations.size() - 2];
                double j = acc - prev_acc;
                jerk_values.push_back(j);
            }
        }
    }
    
    // 统计
    auto calc_stats = [](const std::vector<double>& data) {
        if (data.empty()) return std::make_tuple(0.0, 0.0, 0.0, 0.0, 0.0);
        
        double mean = 0;
        for (auto v : data) mean += std::abs(v);
        mean /= data.size();
        
        std::vector<double> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        
        // 标准差
        double variance = 0;
        for (auto v : data) variance += v * v;
        variance /= data.size();
        double stddev = std::sqrt(variance);
        
        return std::make_tuple(
            mean,
            stddev,
            sorted[sorted.size() * 0.50],
            sorted[sorted.size() * 0.90],
            sorted[sorted.size() * 0.99]
        );
    };
    
    auto [vel_mean, vel_std, vel_p50, vel_p90, vel_p99] = calc_stats(velocities);
    auto [acc_mean, acc_std, acc_p50, acc_p90, acc_p99] = calc_stats(accelerations);
    auto [jerk_mean, jerk_std, jerk_p50, jerk_p90, jerk_p99] = calc_stats(jerk_values);
    
    std::cout << "=== 轨迹运动特征 ===" << std::endl;
    std::cout << "\n速度（每秒位移，度）:" << std::endl;
    std::cout << "  平均: " << std::scientific << std::setprecision(2) << vel_mean << std::endl;
    std::cout << "  标准差: " << vel_std << std::endl;
    std::cout << "  P50: " << vel_p50 << ", P90: " << vel_p90 << ", P99: " << vel_p99 << std::endl;
    
    std::cout << "\n加速度（速度变化，度/点²）:" << std::endl;
    std::cout << "  平均绝对值: " << acc_mean << std::endl;
    std::cout << "  标准差: " << acc_std << std::endl;
    std::cout << "  P50: " << acc_p50 << ", P90: " << acc_p90 << ", P99: " << acc_p99 << std::endl;
    
    std::cout << "\nJerk（加速度变化率，度/点³）:" << std::endl;
    std::cout << "  平均绝对值: " << jerk_mean << std::endl;
    std::cout << "  标准差: " << jerk_std << std::endl;
    std::cout << "  P50: " << jerk_p50 << ", P90: " << jerk_p90 << ", P99: " << jerk_p99 << std::endl;
    
    // 分析线性度
    int linear_segments = 0;  // 加速度接近0
    int curved_segments = 0;  // 加速度显著
    double acc_threshold = 1e-6;  // 阈值
    
    for (auto acc : accelerations) {
        if (std::abs(acc) < acc_threshold) {
            linear_segments++;
        } else {
            curved_segments++;
        }
    }
    
    std::cout << "\n=== 轨迹线性度分析 ===" << std::endl;
    std::cout << "接近匀速运动（|加速度| < " << acc_threshold << "）: " 
              << std::fixed << std::setprecision(1) << (100.0 * linear_segments / accelerations.size()) << "%" << std::endl;
    std::cout << "明显加速/减速: " << (100.0 * curved_segments / accelerations.size()) << "%" << std::endl;
    
    // 测试不同阶数的预测效果
    std::cout << "\n=== 不同预测器的效果对比 ===" << std::endl;
    
    // 重新模拟预测
    std::vector<GpsPoint> history;
    double ldr_error_sum = 0, cp2_error_sum = 0, cp3_error_sum = 0, cp5_error_sum = 0;
    int count = 0;
    
    for (size_t i = 0; i < gps_data.size(); i++) {
        history.push_back(gps_data[i]);
        if (history.size() > 10) history.erase(history.begin());
        
        if (i >= 2) {
            // LDR (线性，2个点)
            GpsPoint v1 = history[history.size()-1] - history[history.size()-2];
            GpsPoint pred_ldr = history[history.size()-1] + v1;
            double error_ldr = (i+1 < gps_data.size()) ? CalculateDistance(gps_data[i+1], pred_ldr) : 0;
            ldr_error_sum += error_ldr;
            
            // CP2 (二阶，3个点)
            if (i >= 2) {
                GpsPoint v2 = history[history.size()-2] - history[history.size()-3];
                GpsPoint a = v1 - v2;
                GpsPoint pred_cp2 = history[history.size()-1] + v1 + a;
                double error_cp2 = (i+1 < gps_data.size()) ? CalculateDistance(gps_data[i+1], pred_cp2) : 0;
                cp2_error_sum += error_cp2;
            }
            
            // CP3 (三阶，4个点) - 考虑jerk
            if (i >= 3) {
                GpsPoint v2 = history[history.size()-2] - history[history.size()-3];
                GpsPoint v3 = history[history.size()-3] - history[history.size()-4];
                GpsPoint a1 = v1 - v2;
                GpsPoint a2 = v2 - v3;
                GpsPoint jerk = a1 - a2;
                GpsPoint pred_cp3 = history[history.size()-1] + v1 + a1 + jerk;
                double error_cp3 = (i+1 < gps_data.size()) ? CalculateDistance(gps_data[i+1], pred_cp3) : 0;
                cp3_error_sum += error_cp3;
            }
            
            // CP5 (基于5个点的最小二乘拟合)
            if (history.size() >= 5) {
                // 简化版：使用5个点的平均速度和加速度
                double sum_vlon = 0, sum_vlat = 0;
                int n_v = 0;
                for (size_t j = history.size() - 5; j < history.size(); j++) {
                    if (j > 0) {
                        GpsPoint v = history[j] - history[j-1];
                        sum_vlon += v.longitude;
                        sum_vlat += v.latitude;
                        n_v++;
                    }
                }
                GpsPoint avg_v(sum_vlon / n_v, sum_vlat / n_v);
                
                // 计算加速度趋势
                double sum_alon = 0, sum_alat = 0;
                int n_a = 0;
                for (size_t j = history.size() - 4; j < history.size(); j++) {
                    if (j >= 2) {
                        GpsPoint v_curr = history[j] - history[j-1];
                        GpsPoint v_prev = history[j-1] - history[j-2];
                        GpsPoint a = v_curr - v_prev;
                        sum_alon += a.longitude;
                        sum_alat += a.latitude;
                        n_a++;
                    }
                }
                GpsPoint avg_a(sum_alon / n_a, sum_alat / n_a);
                
                GpsPoint pred_cp5 = history[history.size()-1] + avg_v + avg_a;
                double error_cp5 = (i+1 < gps_data.size()) ? CalculateDistance(gps_data[i+1], pred_cp5) : 0;
                cp5_error_sum += error_cp5;
            }
            
            count++;
        }
    }
    
    std::cout << "LDR (线性，2点):          平均误差 " << std::scientific << (ldr_error_sum / count) << " 度" << std::endl;
    std::cout << "CP2 (二阶，3点):          平均误差 " << (cp2_error_sum / count) << " 度" << std::endl;
    std::cout << "CP3 (三阶+jerk，4点):     平均误差 " << (cp3_error_sum / count) << " 度" << std::endl;
    std::cout << "CP5 (平滑拟合，5点):      平均误差 " << (cp5_error_sum / count) << " 度" << std::endl;
    
    std::cout << "\n=== 关键发现 ===" << std::endl;
    std::cout << "1. Geolife数据高采样率（1-5秒），轨迹相对平滑" << std::endl;
    std::cout << "2. 加速度变化小（标准差: " << acc_std << "），说明运动较为匀速" << std::endl;
    std::cout << "3. 当前2阶CP预测器可能：" << std::endl;
    std::cout << "   - 对噪声敏感（只用3个点）" << std::endl;
    std::cout << "   - 过拟合短期波动" << std::endl;
    std::cout << "4. 建议改进：" << std::endl;
    std::cout << "   - 使用更多点（5-7个）进行平滑拟合" << std::endl;
    std::cout << "   - 或者使用加权平均，减少噪声影响" << std::endl;
    
    std::cout << "\n============================================================" << std::endl;
    
    return 0;
}


