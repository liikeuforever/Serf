#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cmath>

/**
 * 空间填充曲线编码工具类
 * 支持GeoHash、MBR优化GeoHash、Hilbert曲线
 */
class SpaceFillingCurve {
public:
    // 经纬度点结构
    struct GeoPoint {
        double longitude;
        double latitude;
        
        GeoPoint() : longitude(0), latitude(0) {}
        GeoPoint(double lon, double lat) : longitude(lon), latitude(lat) {}
    };
    
    // 边界框结构（用于MBR优化）
    struct BoundingBox {
        double min_lon;
        double max_lon;
        double min_lat;
        double max_lat;
        
        BoundingBox() : min_lon(180.0), max_lon(-180.0), 
                       min_lat(90.0), max_lat(-90.0) {}
        
        void Update(const GeoPoint& point) {
            min_lon = std::min(min_lon, point.longitude);
            max_lon = std::max(max_lon, point.longitude);
            min_lat = std::min(min_lat, point.latitude);
            max_lat = std::max(max_lat, point.latitude);
        }
        
        double GetWidth() const { return max_lon - min_lon; }
        double GetHeight() const { return max_lat - min_lat; }
    };
    
    /**
     * 计算满足精度要求的GeoHash位数
     * @param max_error 最大允许误差（度）
     * @return GeoHash的总比特数
     */
    static int CalculateGeoHashBits(double max_error);
    
    /**
     * 标准GeoHash编码（全球范围）
     * @param point 经纬度点
     * @param bits GeoHash总比特数（经纬度交错）
     * @return GeoHash编码值
     */
    static uint64_t EncodeStandardGeoHash(const GeoPoint& point, int bits);
    
    /**
     * 标准GeoHash解码
     * @param hash GeoHash编码值
     * @param bits GeoHash总比特数
     * @return 解码后的经纬度点（格子中心）
     */
    static GeoPoint DecodeStandardGeoHash(uint64_t hash, int bits);
    
    /**
     * MBR优化的GeoHash编码（基于数据集边界框）
     * @param point 经纬度点
     * @param bits GeoHash总比特数
     * @param bbox 数据集边界框
     * @return GeoHash编码值
     */
    static uint64_t EncodeMBRGeoHash(const GeoPoint& point, int bits, const BoundingBox& bbox);
    
    /**
     * MBR优化的GeoHash解码
     * @param hash GeoHash编码值
     * @param bits GeoHash总比特数
     * @param bbox 数据集边界框
     * @return 解码后的经纬度点（格子中心）
     */
    static GeoPoint DecodeMBRGeoHash(uint64_t hash, int bits, const BoundingBox& bbox);
    
    /**
     * Hilbert曲线编码（基于数据集边界框）
     * @param point 经纬度点
     * @param order Hilbert曲线阶数（总比特数 = 2 * order）
     * @param bbox 数据集边界框
     * @return Hilbert曲线编码值
     */
    static uint64_t EncodeHilbert(const GeoPoint& point, int order, const BoundingBox& bbox);
    
    /**
     * Hilbert曲线解码
     * @param hilbert_index Hilbert曲线编码值
     * @param order Hilbert曲线阶数
     * @param bbox 数据集边界框
     * @return 解码后的经纬度点（格子中心）
     */
    static GeoPoint DecodeHilbert(uint64_t hilbert_index, int order, const BoundingBox& bbox);
    
    /**
     * 计算两点之间的欧氏距离（度）
     */
    static double CalculateDistance(const GeoPoint& p1, const GeoPoint& p2);
    
    /**
     * 计算GeoHash格子的最大对角线距离
     * @param bits GeoHash总比特数
     * @param bbox 边界框（如果为nullptr则使用全球范围）
     * @return 最大对角线距离（度）
     */
    static double CalculateGeoHashMaxDiagonal(int bits, const BoundingBox* bbox = nullptr);

    // Hilbert曲线辅助函数（公开以便测试）
    static void Rot(int n, int* x, int* y, int rx, int ry);
    static uint64_t XYToHilbert(int n, int x, int y);
    static void HilbertToXY(int n, uint64_t d, int* x, int* y);

private:
};


