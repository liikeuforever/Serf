#include "space_filling_curve.h"
#include <cmath>
#include <algorithm>

int SpaceFillingCurve::CalculateGeoHashBits(double max_error) {
    // 计算满足对角线精度要求的GeoHash比特数
    // 对于全球范围：经度范围360度，纬度范围180度
    // 假设正方形格子（保守估计）
    
    double lon_range = 360.0;
    double lat_range = 180.0;
    
    int bits = 2; // 至少2位（经度1位+纬度1位）
    while (bits < 60) { // 限制最大60位（uint64_t范围）
        int lon_bits = (bits + 1) / 2; // 经度比特数
        int lat_bits = bits / 2;       // 纬度比特数
        
        double cell_width = lon_range / (1ULL << lon_bits);
        double cell_height = lat_range / (1ULL << lat_bits);
        
        // 计算对角线距离
        double diagonal = std::sqrt(cell_width * cell_width + cell_height * cell_height);
        
        if (diagonal <= max_error) {
            return bits;
        }
        bits += 2; // 每次增加2位（经度1位+纬度1位）
    }
    
    return 60; // 默认最大60位
}

uint64_t SpaceFillingCurve::EncodeStandardGeoHash(const GeoPoint& point, int bits) {
    double lon_min = -180.0, lon_max = 180.0;
    double lat_min = -90.0, lat_max = 90.0;
    
    uint64_t hash = 0;
    
    for (int i = 0; i < bits; i++) {
        if (i % 2 == 0) { // 偶数位编码经度
            double mid = (lon_min + lon_max) / 2.0;
            if (point.longitude >= mid) {
                hash |= (1ULL << (bits - 1 - i));
                lon_min = mid;
            } else {
                lon_max = mid;
            }
        } else { // 奇数位编码纬度
            double mid = (lat_min + lat_max) / 2.0;
            if (point.latitude >= mid) {
                hash |= (1ULL << (bits - 1 - i));
                lat_min = mid;
            } else {
                lat_max = mid;
            }
        }
    }
    
    return hash;
}

SpaceFillingCurve::GeoPoint SpaceFillingCurve::DecodeStandardGeoHash(uint64_t hash, int bits) {
    double lon_min = -180.0, lon_max = 180.0;
    double lat_min = -90.0, lat_max = 90.0;
    
    for (int i = 0; i < bits; i++) {
        if (i % 2 == 0) { // 偶数位是经度
            double mid = (lon_min + lon_max) / 2.0;
            if (hash & (1ULL << (bits - 1 - i))) {
                lon_min = mid;
            } else {
                lon_max = mid;
            }
        } else { // 奇数位是纬度
            double mid = (lat_min + lat_max) / 2.0;
            if (hash & (1ULL << (bits - 1 - i))) {
                lat_min = mid;
            } else {
                lat_max = mid;
            }
        }
    }
    
    // 返回格子中心点
    return GeoPoint((lon_min + lon_max) / 2.0, (lat_min + lat_max) / 2.0);
}

uint64_t SpaceFillingCurve::EncodeMBRGeoHash(const GeoPoint& point, int bits, const BoundingBox& bbox) {
    // 简化方案：使用标准的位交错方式（偶数位=经度，奇数位=纬度）
    // 虽然不是最优的，但更简单标准，且与解码匹配
    
    double lon_min = bbox.min_lon, lon_max = bbox.max_lon;
    double lat_min = bbox.min_lat, lat_max = bbox.max_lat;
    
    uint64_t hash = 0;
    
    for (int i = 0; i < bits; i++) {
        if (i % 2 == 0) { // 偶数位编码经度
            double mid = (lon_min + lon_max) / 2.0;
            if (point.longitude >= mid) {
                hash |= (1ULL << (bits - 1 - i));
                lon_min = mid;
            } else {
                lon_max = mid;
            }
        } else { // 奇数位编码纬度
            double mid = (lat_min + lat_max) / 2.0;
            if (point.latitude >= mid) {
                hash |= (1ULL << (bits - 1 - i));
                lat_min = mid;
            } else {
                lat_max = mid;
            }
        }
    }
    
    return hash;
}

SpaceFillingCurve::GeoPoint SpaceFillingCurve::DecodeMBRGeoHash(uint64_t hash, int bits, const BoundingBox& bbox) {
    // 简化方案：使用标准的位交错方式（偶数位=经度，奇数位=纬度）
    // 这样更简单且标准
    
    double lon_min = bbox.min_lon, lon_max = bbox.max_lon;
    double lat_min = bbox.min_lat, lat_max = bbox.max_lat;
    
    for (int i = 0; i < bits; i++) {
        if (i % 2 == 0) { // 偶数位是经度
            double mid = (lon_min + lon_max) / 2.0;
            if (hash & (1ULL << (bits - 1 - i))) {
                lon_min = mid;
            } else {
                lon_max = mid;
            }
        } else { // 奇数位是纬度
            double mid = (lat_min + lat_max) / 2.0;
            if (hash & (1ULL << (bits - 1 - i))) {
                lat_min = mid;
            } else {
                lat_max = mid;
            }
        }
    }
    
    // 返回格子中心点
    return GeoPoint((lon_min + lon_max) / 2.0, (lat_min + lat_max) / 2.0);
}

// ============================================================================
// Hilbert曲线标准实现
// 基于: "Programming the Hilbert curve" by John Skilling (2004)
// 以及 Wikipedia Hilbert curve article
// 
// 算法说明:
// - 将2D坐标 (x, y) 映射到1D Hilbert距离 d
// - Rot函数处理坐标系的旋转变换
// - 当 ry=0 时需要旋转，ry=1 时保持不变（这是正确的！）
// 
// 已验证：4x4网格输出正确的0-15序列
// ============================================================================

void SpaceFillingCurve::Rot(int n, int* x, int* y, int rx, int ry) {
    // 根据当前子象限(rx, ry)旋转坐标系
    // 这是Hilbert曲线的核心：通过递归旋转保证曲线的连续性
    
    if (ry == 0) {
        // 在 ry=0 的象限中需要进行旋转和翻转
        if (rx == 1) {
            // rx=1, ry=0: 翻转两个坐标
            *x = n - 1 - *x;
            *y = n - 1 - *y;
        }
        // 交换 x 和 y（顺时针90度旋转）
        int t = *x;
        *x = *y;
        *y = t;
    }
    // ry=1 的情况：不需要旋转（保持原坐标系）
    // 这不是bug，而是Hilbert曲线算法的正确行为！
}

uint64_t SpaceFillingCurve::XYToHilbert(int n, int x, int y) {
    // 将 (x, y) 坐标转换为 Hilbert 曲线上的距离 d
    uint64_t d = 0;
    
    // 从最高位到最低位逐层处理
    for (int s = n / 2; s > 0; s /= 2) {
        // 提取当前位的 x, y 值
        int rx = (x & s) > 0;
        int ry = (y & s) > 0;
        
        // 计算当前位对总距离的贡献
        // 公式: d += s * s * ((3 * rx) ^ ry)
        // 这将 (rx, ry) 的4种组合映射到 0,1,2,3
        d += static_cast<uint64_t>(s) * s * ((3 * rx) ^ ry);
        
        // 旋转坐标系以处理下一层
        Rot(s, &x, &y, rx, ry);
    }
    
    return d;
}

void SpaceFillingCurve::HilbertToXY(int n, uint64_t d, int* x, int* y) {
    // 将 Hilbert 距离 d 转换回 (x, y) 坐标
    *x = 0;
    *y = 0;
    
    // 从最低位到最高位逐层处理（与编码相反）
    for (int s = 1; s < n; s *= 2) {
        // 从距离中提取 rx, ry
        int rx = 1 & (d / 2);
        int ry = 1 & (d ^ rx);
        
        // 应用旋转（恢复原始坐标系）
        Rot(s, x, y, rx, ry);
        
        // 累加当前层的坐标贡献
        *x += s * rx;
        *y += s * ry;
        
        // 移到下一层
        d /= 4;
    }
}

uint64_t SpaceFillingCurve::EncodeHilbert(const GeoPoint& point, int order, const BoundingBox& bbox) {
    int n = 1 << order; // 2^order
    
    // 将经纬度归一化到 [0, 1) 的范围
    double norm_lon = (point.longitude - bbox.min_lon) / bbox.GetWidth();
    double norm_lat = (point.latitude - bbox.min_lat) / bbox.GetHeight();
    
    // 确保在范围内
    norm_lon = std::max(0.0, std::min(1.0, norm_lon));
    norm_lat = std::max(0.0, std::min(1.0, norm_lat));
    
    // 【修正】使用 * n 而不是 * (n - 1)，避免量化偏差
    int x = static_cast<int>(norm_lon * n);
    int y = static_cast<int>(norm_lat * n);
    
    // 边界处理：防止 normalized_coord 恰好为 1.0 导致越界
    x = std::min(n - 1, x);
    y = std::min(n - 1, y);
    
    return XYToHilbert(n, x, y);
}

SpaceFillingCurve::GeoPoint SpaceFillingCurve::DecodeHilbert(uint64_t hilbert_index, int order, const BoundingBox& bbox) {
    int n = 1 << order;
    
    int x, y;
    HilbertToXY(n, hilbert_index, &x, &y);
    
    // 将整数坐标反归一化到经纬度
    // 返回格子中心点：(integer_coord + 0.5) / n
    // 这与编码时的 * n 逻辑匹配
    double norm_lon = (x + 0.5) / n;
    double norm_lat = (y + 0.5) / n;
    
    double lon = bbox.min_lon + norm_lon * bbox.GetWidth();
    double lat = bbox.min_lat + norm_lat * bbox.GetHeight();
    
    return GeoPoint(lon, lat);
}

double SpaceFillingCurve::CalculateDistance(const GeoPoint& p1, const GeoPoint& p2) {
    double dx = p1.longitude - p2.longitude;
    double dy = p1.latitude - p2.latitude;
    return std::sqrt(dx * dx + dy * dy);
}

double SpaceFillingCurve::CalculateGeoHashMaxDiagonal(int bits, const BoundingBox* bbox) {
    double lon_range = bbox ? bbox->GetWidth() : 360.0;
    double lat_range = bbox ? bbox->GetHeight() : 180.0;
    
    int lon_bits = (bits + 1) / 2;
    int lat_bits = bits / 2;
    
    double cell_width = lon_range / (1ULL << lon_bits);
    double cell_height = lat_range / (1ULL << lat_bits);
    
    return std::sqrt(cell_width * cell_width + cell_height * cell_height);
}

