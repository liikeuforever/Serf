#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include "src/compressor/space_filling_curve.h"

int main() {
    std::string filename = "test/data_set/Geolife_100k_longitude_latitude.csv";
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }
    
    std::vector<SpaceFillingCurve::GeoPoint> points;
    std::string line;
    
    // Read all points
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        double lon, lat;
        char comma;
        ss >> lon >> comma >> lat;
        points.push_back(SpaceFillingCurve::GeoPoint(lon, lat));
    }
    file.close();
    
    std::cout << "总点数: " << points.size() << std::endl;
    
    // Calculate bounding box
    SpaceFillingCurve::BoundingBox bbox;
    for (const auto& p : points) {
        bbox.Update(p);
    }
    
    std::cout << "\n边界框:" << std::endl;
    std::cout << "  经度: [" << bbox.min_lon << ", " << bbox.max_lon << "]" << std::endl;
    std::cout << "  纬度: [" << bbox.min_lat << ", " << bbox.max_lat << "]" << std::endl;
    std::cout << "  经度跨度: " << bbox.GetWidth() << " 度" << std::endl;
    std::cout << "  纬度跨度: " << bbox.GetHeight() << " 度" << std::endl;
    
    // Test different orders
    std::vector<int> orders = {8, 10, 12, 14, 16, 18, 19};
    
    std::cout << "\n=== 测试不同希尔伯特阶数 ===" << std::endl;
    std::cout << std::setw(6) << "阶数" 
              << std::setw(15) << "网格大小"
              << std::setw(20) << "总比特数"
              << std::setw(25) << "格子大小(经度,度)"
              << std::setw(25) << "格子大小(纬度,度)"
              << std::setw(20) << "格子对角线(度)"
              << std::setw(20) << "平均编码误差"
              << std::setw(20) << "最大编码误差"
              << std::endl;
    std::cout << std::string(145, '-') << std::endl;
    
    for (int order : orders) {
        int n = 1 << order;
        int total_bits = 2 * order;
        
        double cell_width = bbox.GetWidth() / n;
        double cell_height = bbox.GetHeight() / n;
        double diagonal = std::sqrt(cell_width * cell_width + cell_height * cell_height);
        
        // Test encoding error on a sample of points
        int sample_size = std::min(10000, static_cast<int>(points.size()));
        double sum_error = 0.0;
        double max_error = 0.0;
        
        for (int i = 0; i < sample_size; i++) {
            const auto& p = points[i * points.size() / sample_size];
            uint64_t h = SpaceFillingCurve::EncodeHilbert(p, order, bbox);
            auto decoded = SpaceFillingCurve::DecodeHilbert(h, order, bbox);
            
            double error = SpaceFillingCurve::CalculateDistance(p, decoded);
            sum_error += error;
            max_error = std::max(max_error, error);
        }
        
        double avg_error = sum_error / sample_size;
        
        std::cout << std::setw(6) << order
                  << std::setw(15) << (std::to_string(n) + "x" + std::to_string(n))
                  << std::setw(20) << total_bits
                  << std::setw(25) << std::scientific << std::setprecision(4) << cell_width
                  << std::setw(25) << cell_height
                  << std::setw(20) << diagonal
                  << std::setw(20) << avg_error
                  << std::setw(20) << max_error
                  << std::endl;
    }
    
    // Test spatial locality on consecutive points
    std::cout << "\n=== 测试相邻点的希尔伯特编码差异 ===" << std::endl;
    
    int test_order = 19;  // 使用高精度
    std::cout << "使用阶数: " << test_order << " (约 " << (2 * test_order) << " 比特)" << std::endl;
    
    std::vector<int64_t> hilbert_diffs;
    std::vector<double> spatial_diffs;
    
    for (size_t i = 1; i < points.size(); i++) {
        uint64_t h1 = SpaceFillingCurve::EncodeHilbert(points[i-1], test_order, bbox);
        uint64_t h2 = SpaceFillingCurve::EncodeHilbert(points[i], test_order, bbox);
        
        int64_t hilbert_diff = std::abs(static_cast<int64_t>(h1) - static_cast<int64_t>(h2));
        double spatial_diff = SpaceFillingCurve::CalculateDistance(points[i-1], points[i]);
        
        hilbert_diffs.push_back(hilbert_diff);
        spatial_diffs.push_back(spatial_diff);
    }
    
    // Sort for percentile calculation
    std::sort(hilbert_diffs.begin(), hilbert_diffs.end());
    std::sort(spatial_diffs.begin(), spatial_diffs.end());
    
    std::cout << "\n希尔伯特编码差异统计:" << std::endl;
    std::cout << "  中位数: " << hilbert_diffs[hilbert_diffs.size() / 2] << std::endl;
    std::cout << "  平均值: " << std::accumulate(hilbert_diffs.begin(), hilbert_diffs.end(), 0LL) / hilbert_diffs.size() << std::endl;
    std::cout << "  90%: " << hilbert_diffs[hilbert_diffs.size() * 9 / 10] << std::endl;
    std::cout << "  95%: " << hilbert_diffs[hilbert_diffs.size() * 95 / 100] << std::endl;
    std::cout << "  99%: " << hilbert_diffs[hilbert_diffs.size() * 99 / 100] << std::endl;
    std::cout << "  最大值: " << hilbert_diffs.back() << std::endl;
    
    std::cout << "\n空间距离统计:" << std::endl;
    std::cout << "  中位数: " << std::scientific << spatial_diffs[spatial_diffs.size() / 2] << " 度" << std::endl;
    std::cout << "  90%: " << spatial_diffs[spatial_diffs.size() * 9 / 10] << " 度" << std::endl;
    std::cout << "  95%: " << spatial_diffs[spatial_diffs.size() * 95 / 100] << " 度" << std::endl;
    std::cout << "  99%: " << spatial_diffs[spatial_diffs.size() * 99 / 100] << " 度" << std::endl;
    std::cout << "  最大值: " << spatial_diffs.back() << " 度" << std::endl;
    
    // Check how many consecutive points fall in the same cell
    int same_cell_count = 0;
    int adjacent_cell_count = 0;
    
    for (size_t i = 1; i < std::min(points.size(), size_t(10000)); i++) {
        uint64_t h1 = SpaceFillingCurve::EncodeHilbert(points[i-1], test_order, bbox);
        uint64_t h2 = SpaceFillingCurve::EncodeHilbert(points[i], test_order, bbox);
        
        int64_t diff = std::abs(static_cast<int64_t>(h1) - static_cast<int64_t>(h2));
        
        if (diff == 0) {
            same_cell_count++;
        } else if (diff <= 10) {  // 非常接近
            adjacent_cell_count++;
        }
    }
    
    std::cout << "\n相邻点在同一格子中的比例: " 
              << (100.0 * same_cell_count / 10000) << "%" << std::endl;
    std::cout << "相邻点在相邻格子中(diff<=10)的比例: " 
              << (100.0 * (same_cell_count + adjacent_cell_count) / 10000) << "%" << std::endl;
    
    // Test compression potential
    std::cout << "\n=== 压缩潜力分析 ===" << std::endl;
    
    // Count bit length of differences
    std::vector<int> bit_lengths;
    for (size_t i = 1; i < points.size(); i++) {
        uint64_t h1 = SpaceFillingCurve::EncodeHilbert(points[i-1], test_order, bbox);
        uint64_t h2 = SpaceFillingCurve::EncodeHilbert(points[i], test_order, bbox);
        
        int64_t diff = static_cast<int64_t>(h2) - static_cast<int64_t>(h1);  // 保持符号
        uint64_t abs_diff = std::abs(diff);
        
        int bits = 0;
        if (abs_diff == 0) {
            bits = 0;
        } else {
            bits = 64 - __builtin_clzll(abs_diff) + 1;  // +1 for sign
        }
        bit_lengths.push_back(bits);
    }
    
    std::sort(bit_lengths.begin(), bit_lengths.end());
    
    std::cout << "差分编码所需比特数:" << std::endl;
    std::cout << "  中位数: " << bit_lengths[bit_lengths.size() / 2] << " bits" << std::endl;
    std::cout << "  平均值: " << std::accumulate(bit_lengths.begin(), bit_lengths.end(), 0.0) / bit_lengths.size() << " bits" << std::endl;
    std::cout << "  90%: " << bit_lengths[bit_lengths.size() * 9 / 10] << " bits" << std::endl;
    std::cout << "  95%: " << bit_lengths[bit_lengths.size() * 95 / 100] << " bits" << std::endl;
    std::cout << "  99%: " << bit_lengths[bit_lengths.size() * 99 / 100] << " bits" << std::endl;
    std::cout << "  最大值: " << bit_lengths.back() << " bits" << std::endl;
    
    double avg_bits = std::accumulate(bit_lengths.begin(), bit_lengths.end(), 0.0) / bit_lengths.size();
    std::cout << "\n理论压缩率: " << std::fixed << std::setprecision(2) 
              << (100.0 * avg_bits / (2 * test_order)) << "% (原始 " 
              << (2 * test_order) << " bits -> 平均 " << avg_bits << " bits)" << std::endl;
    
    return 0;
}

