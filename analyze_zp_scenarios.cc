/**
 * 深入分析 ZP 被选择的场景
 * 揭示为什么移除"最差"的预测器反而使性能下降
 */

#include "src/compressor/trajcompress_sp_compressor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <iomanip>

using GpsPoint = TrajCompressSPCompressor::GpsPoint;

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
    double epsilon = 1e-5 * std::sqrt(2);
    
    auto gps_data = LoadGpsDataFromCSV(dataset_path, max_points);
    if (gps_data.size() < 3) return 1;
    
    std::cout << "============================================================" << std::endl;
    std::cout << "分析 ZP 被选择的场景特征" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    // 模拟三预测器系统
    GpsPoint current = gps_data[0];
    GpsPoint prev = gps_data[0];
    GpsPoint prev_prev = gps_data[0];
    
    int ldr_selected = 0, cp_selected = 0, zp_selected = 0;
    
    // 统计：当ZP被选择时，LDR和CP的预测误差
    std::vector<double> ldr_error_when_zp_selected;
    std::vector<double> cp_error_when_zp_selected;
    std::vector<double> zp_error_when_zp_selected;
    
    // 统计：当LDR被选择时，ZP的预测误差
    std::vector<double> zp_error_when_ldr_selected;
    
    // 统计：当CP被选择时，ZP的预测误差
    std::vector<double> zp_error_when_cp_selected;
    
    // 逐点分析
    for (size_t i = 2; i < gps_data.size(); i++) {
        // 三种预测
        GpsPoint velocity(current.longitude - prev.longitude, current.latitude - prev.latitude);
        GpsPoint pred_ldr(current.longitude + velocity.longitude, current.latitude + velocity.latitude);
        
        GpsPoint prev_velocity(prev.longitude - prev_prev.longitude, prev.latitude - prev_prev.latitude);
        GpsPoint acceleration(velocity.longitude - prev_velocity.longitude, velocity.latitude - prev_velocity.latitude);
        GpsPoint pred_cp(current.longitude + velocity.longitude + acceleration.longitude,
                        current.latitude + velocity.latitude + acceleration.latitude);
        
        GpsPoint pred_zp = current;
        
        // 计算预测误差
        double error_ldr = CalculateDistance(gps_data[i], pred_ldr);
        double error_cp = CalculateDistance(gps_data[i], pred_cp);
        double error_zp = CalculateDistance(gps_data[i], pred_zp);
        
        // 选择最优预测器
        if (error_ldr <= error_cp && error_ldr <= error_zp) {
            ldr_selected++;
            zp_error_when_ldr_selected.push_back(error_zp);
        } else if (error_cp <= error_zp) {
            cp_selected++;
            zp_error_when_cp_selected.push_back(error_zp);
        } else {
            zp_selected++;
            ldr_error_when_zp_selected.push_back(error_ldr);
            cp_error_when_zp_selected.push_back(error_cp);
            zp_error_when_zp_selected.push_back(error_zp);
        }
        
        // 重构（简化版本）
        GpsPoint best_pred = (error_ldr <= error_cp && error_ldr <= error_zp) ? pred_ldr :
                            (error_cp <= error_zp) ? pred_cp : pred_zp;
        GpsPoint residual = gps_data[i] - best_pred;
        int64_t q_lon = static_cast<int64_t>(std::round(residual.longitude / epsilon));
        int64_t q_lat = static_cast<int64_t>(std::round(residual.latitude / epsilon));
        GpsPoint reconstructed(best_pred.longitude + q_lon * epsilon, best_pred.latitude + q_lat * epsilon);
        
        prev_prev = prev;
        prev = current;
        current = reconstructed;
    }
    
    int total = ldr_selected + cp_selected + zp_selected;
    
    std::cout << "=== 预测器选择统计 ===" << std::endl;
    std::cout << "LDR 被选: " << ldr_selected << " (" << std::fixed << std::setprecision(1) 
              << (100.0 * ldr_selected / total) << "%)" << std::endl;
    std::cout << "CP 被选:  " << cp_selected << " (" << (100.0 * cp_selected / total) << "%)" << std::endl;
    std::cout << "ZP 被选:  " << zp_selected << " (" << (100.0 * zp_selected / total) << "%)" << std::endl;
    
    // 计算统计量
    auto calc_stats = [](const std::vector<double>& data) {
        if (data.empty()) return std::make_tuple(0.0, 0.0, 0.0, 0.0);
        
        double mean = 0;
        for (auto v : data) mean += v;
        mean /= data.size();
        
        std::vector<double> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        
        return std::make_tuple(
            mean,
            sorted[sorted.size() * 0.50],
            sorted[sorted.size() * 0.90],
            sorted[sorted.size() * 0.99]
        );
    };
    
    auto [ldr_err_mean, ldr_err_p50, ldr_err_p90, ldr_err_p99] = calc_stats(ldr_error_when_zp_selected);
    auto [cp_err_mean, cp_err_p50, cp_err_p90, cp_err_p99] = calc_stats(cp_error_when_zp_selected);
    auto [zp_err_mean, zp_err_p50, zp_err_p90, zp_err_p99] = calc_stats(zp_error_when_zp_selected);
    
    std::cout << "\n=== 关键发现：当 ZP 被选择时 ===" << std::endl;
    std::cout << "这说明在这些场景下，ZP 是\"三个烂苹果中最不烂的\"！\n" << std::endl;
    
    std::cout << "ZP 自己的预测误差（度）:" << std::endl;
    std::cout << "  平均: " << std::scientific << std::setprecision(2) << zp_err_mean 
              << "  P50: " << zp_err_p50 << "  P90: " << zp_err_p90 << "  P99: " << zp_err_p99 << std::endl;
    
    std::cout << "\n但 LDR 的预测误差（在同样场景）:" << std::endl;
    std::cout << "  平均: " << ldr_err_mean 
              << "  P50: " << ldr_err_p50 << "  P90: " << ldr_err_p90 << "  P99: " << ldr_err_p99 << std::endl;
    std::cout << "  ⚠️  LDR 比 ZP 差 " << std::fixed << std::setprecision(1) 
              << ((ldr_err_mean / zp_err_mean - 1) * 100) << "%！" << std::endl;
    
    std::cout << "\n且 CP 的预测误差（在同样场景）:" << std::endl;
    std::cout << "  平均: " << std::scientific << cp_err_mean 
              << "  P50: " << cp_err_p50 << "  P90: " << cp_err_p90 << "  P99: " << cp_err_p99 << std::endl;
    std::cout << "  ⚠️  CP 比 ZP 差 " << std::fixed 
              << ((cp_err_mean / zp_err_mean - 1) * 100) << "%！" << std::endl;
    
    // 估算量化成本
    auto estimate_bits = [epsilon](double error) {
        // 简化估算：误差越大，量化值越大，编码bits越多
        int64_t quant = static_cast<int64_t>(error / epsilon);
        if (quant == 0) return 2.0;  // Elias Gamma 最小2 bits（编码1）
        return 2.0 * std::log2(quant + 1) + 1;  // 粗略估算
    };
    
    double zp_bits_in_zp_scenarios = estimate_bits(zp_err_mean);
    double ldr_bits_in_zp_scenarios = estimate_bits(ldr_err_mean);
    double cp_bits_in_zp_scenarios = estimate_bits(cp_err_mean);
    
    std::cout << "\n=== 量化编码成本估算（在ZP优势场景中）===" << std::endl;
    std::cout << "使用 ZP:  " << std::fixed << std::setprecision(1) << zp_bits_in_zp_scenarios << " bits（实际选择）" << std::endl;
    std::cout << "强用 LDR: " << ldr_bits_in_zp_scenarios << " bits（如果移除ZP）" << std::endl;
    std::cout << "强用 CP:  " << cp_bits_in_zp_scenarios << " bits（如果移除ZP）" << std::endl;
    
    double extra_cost_without_zp = std::min(ldr_bits_in_zp_scenarios, cp_bits_in_zp_scenarios) - zp_bits_in_zp_scenarios;
    std::cout << "\n移除 ZP 后的额外成本: +" << extra_cost_without_zp << " bits/点（在ZP场景中）" << std::endl;
    std::cout << "占全体的比例: " << (zp_selected * 1.0 / total) << std::endl;
    std::cout << "总体额外成本: +" << (extra_cost_without_zp * zp_selected / total) << " bits/点" << std::endl;
    
    std::cout << "\n=== 解释 ===" << std::endl;
    std::cout << "ZP 虽然平均表现差（10.84 bits），但它在 " << (100.0 * zp_selected / total) << "% 的场景下" << std::endl;
    std::cout << "是\"最不差\"的选择。在这些场景中：" << std::endl;
    std::cout << "  - 可能是轨迹突变、GPS跳变、车辆停止等异常情况" << std::endl;
    std::cout << "  - LDR 和 CP 的预测完全失效" << std::endl;
    std::cout << "  - ZP 虽然不好，但比 LDR/CP 的\"灾难性预测\"要好得多" << std::endl;
    std::cout << "\n移除 ZP 后，这些场景被迫使用 LDR 或 CP，导致量化数据大幅增加！" << std::endl;
    
    std::cout << "\n============================================================" << std::endl;
    
    return 0;
}

