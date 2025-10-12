#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>

struct Point {
    double longitude;
    double latitude;
};

int main() {
    std::string filename = "test/data_set/Geolife_100k_longitude_latitude.csv";
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }
    
    std::vector<Point> points;
    std::string line;
    
    // Read all points
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        Point p;
        char comma;
        ss >> p.longitude >> comma >> p.latitude;
        points.push_back(p);
    }
    file.close();
    
    std::cout << "Total points: " << points.size() << std::endl;
    
    if (points.size() < 2) {
        std::cerr << "Not enough points for analysis" << std::endl;
        return 1;
    }
    
    // Calculate differences between consecutive points
    double max_lon_diff = 0.0;
    double max_lat_diff = 0.0;
    double sum_lon_diff = 0.0;
    double sum_lat_diff = 0.0;
    double max_combined_diff = 0.0;
    double sum_combined_diff = 0.0;
    
    int max_lon_idx = 0;
    int max_lat_idx = 0;
    int max_combined_idx = 0;
    
    for (size_t i = 1; i < points.size(); ++i) {
        double lon_diff = std::abs(points[i].longitude - points[i-1].longitude);
        double lat_diff = std::abs(points[i].latitude - points[i-1].latitude);
        double combined_diff = std::sqrt(lon_diff * lon_diff + lat_diff * lat_diff);
        
        sum_lon_diff += lon_diff;
        sum_lat_diff += lat_diff;
        sum_combined_diff += combined_diff;
        
        if (lon_diff > max_lon_diff) {
            max_lon_diff = lon_diff;
            max_lon_idx = i;
        }
        
        if (lat_diff > max_lat_diff) {
            max_lat_diff = lat_diff;
            max_lat_idx = i;
        }
        
        if (combined_diff > max_combined_diff) {
            max_combined_diff = combined_diff;
            max_combined_idx = i;
        }
    }
    
    size_t num_diffs = points.size() - 1;
    double avg_lon_diff = sum_lon_diff / num_diffs;
    double avg_lat_diff = sum_lat_diff / num_diffs;
    double avg_combined_diff = sum_combined_diff / num_diffs;
    
    std::cout << std::fixed << std::setprecision(10);
    std::cout << "\n=== 相邻点经纬度跨度分析 ===" << std::endl;
    std::cout << "\n经度差异:" << std::endl;
    std::cout << "  平均经度跨度: " << avg_lon_diff << " 度" << std::endl;
    std::cout << "  最大经度跨度: " << max_lon_diff << " 度" << std::endl;
    std::cout << "  最大跨度位置: 点 " << (max_lon_idx-1) << " -> 点 " << max_lon_idx << std::endl;
    std::cout << "    点 " << (max_lon_idx-1) << ": (" << points[max_lon_idx-1].longitude 
              << ", " << points[max_lon_idx-1].latitude << ")" << std::endl;
    std::cout << "    点 " << max_lon_idx << ": (" << points[max_lon_idx].longitude 
              << ", " << points[max_lon_idx].latitude << ")" << std::endl;
    
    std::cout << "\n纬度差异:" << std::endl;
    std::cout << "  平均纬度跨度: " << avg_lat_diff << " 度" << std::endl;
    std::cout << "  最大纬度跨度: " << max_lat_diff << " 度" << std::endl;
    std::cout << "  最大跨度位置: 点 " << (max_lat_idx-1) << " -> 点 " << max_lat_idx << std::endl;
    std::cout << "    点 " << (max_lat_idx-1) << ": (" << points[max_lat_idx-1].longitude 
              << ", " << points[max_lat_idx-1].latitude << ")" << std::endl;
    std::cout << "    点 " << max_lat_idx << ": (" << points[max_lat_idx].longitude 
              << ", " << points[max_lat_idx].latitude << ")" << std::endl;
    
    std::cout << "\n组合距离 (欧氏距离):" << std::endl;
    std::cout << "  平均距离: " << avg_combined_diff << " 度" << std::endl;
    std::cout << "  最大距离: " << max_combined_diff << " 度" << std::endl;
    std::cout << "  最大距离位置: 点 " << (max_combined_idx-1) << " -> 点 " << max_combined_idx << std::endl;
    std::cout << "    点 " << (max_combined_idx-1) << ": (" << points[max_combined_idx-1].longitude 
              << ", " << points[max_combined_idx-1].latitude << ")" << std::endl;
    std::cout << "    点 " << max_combined_idx << ": (" << points[max_combined_idx].longitude 
              << ", " << points[max_combined_idx].latitude << ")" << std::endl;
    
    // Convert to meters (approximate, at latitude ~40 degrees)
    // 1 degree longitude ≈ 111320 * cos(40°) ≈ 85300 meters
    // 1 degree latitude ≈ 111320 meters
    double avg_lon_meters = avg_lon_diff * 85300;
    double avg_lat_meters = avg_lat_diff * 111320;
    double max_lon_meters = max_lon_diff * 85300;
    double max_lat_meters = max_lat_diff * 111320;
    
    std::cout << "\n=== 近似距离 (米) ===" << std::endl;
    std::cout << "平均经度跨度: " << avg_lon_meters << " 米" << std::endl;
    std::cout << "平均纬度跨度: " << avg_lat_meters << " 米" << std::endl;
    std::cout << "最大经度跨度: " << max_lon_meters << " 米" << std::endl;
    std::cout << "最大纬度跨度: " << max_lat_meters << " 米" << std::endl;
    
    // Statistics on distribution
    std::vector<double> lon_diffs, lat_diffs;
    for (size_t i = 1; i < points.size(); ++i) {
        lon_diffs.push_back(std::abs(points[i].longitude - points[i-1].longitude));
        lat_diffs.push_back(std::abs(points[i].latitude - points[i-1].latitude));
    }
    
    std::sort(lon_diffs.begin(), lon_diffs.end());
    std::sort(lat_diffs.begin(), lat_diffs.end());
    
    std::cout << "\n=== 分布统计 ===" << std::endl;
    std::cout << "经度差异:" << std::endl;
    std::cout << "  50th percentile (中位数): " << lon_diffs[lon_diffs.size() / 2] << " 度" << std::endl;
    std::cout << "  90th percentile: " << lon_diffs[lon_diffs.size() * 9 / 10] << " 度" << std::endl;
    std::cout << "  95th percentile: " << lon_diffs[lon_diffs.size() * 95 / 100] << " 度" << std::endl;
    std::cout << "  99th percentile: " << lon_diffs[lon_diffs.size() * 99 / 100] << " 度" << std::endl;
    
    std::cout << "\n纬度差异:" << std::endl;
    std::cout << "  50th percentile (中位数): " << lat_diffs[lat_diffs.size() / 2] << " 度" << std::endl;
    std::cout << "  90th percentile: " << lat_diffs[lat_diffs.size() * 9 / 10] << " 度" << std::endl;
    std::cout << "  95th percentile: " << lat_diffs[lat_diffs.size() * 95 / 100] << " 度" << std::endl;
    std::cout << "  99th percentile: " << lat_diffs[lat_diffs.size() * 99 / 100] << " 度" << std::endl;
    
    return 0;
}

