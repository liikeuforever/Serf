#pragma once

#include "compressor/space_filling_curve.h"
#include "utils/output_bit_stream.h"
#include "utils/double.h"
#include <memory>
#include <vector>
#include <string>

/**
 * 基于空间填充曲线的经纬度压缩器
 * 使用差值 + ZigZag + Elias Gamma 编码
 */
class SFCCompressor {
public:
    // 压缩方法枚举
    enum CompressionMethod {
        STANDARD_GEOHASH,    // 标准GeoHash
        MBR_GEOHASH,         // MBR优化的GeoHash
        HILBERT_CURVE        // Hilbert曲线
    };
    
    /**
     * 构造函数（基于误差自动计算比特数，适用于全球范围）
     * @param method 压缩方法
     * @param max_error 最大允许误差（度），用于确定编码精度
     */
    SFCCompressor(CompressionMethod method, double max_error);
    
    /**
     * 构造函数（直接指定编码比特数，适用于已知MBR的场景）
     * @param method 压缩方法
     * @param encoding_bits 编码使用的比特数
     * @param bbox 边界框（MBR/Hilbert方法需要）
     */
    SFCCompressor(CompressionMethod method, int encoding_bits, const SpaceFillingCurve::BoundingBox& bbox);
    
    /**
     * 设置边界框（必须在添加点之前调用，用于MBR和Hilbert方法）
     * @param bbox 预先计算的边界框
     */
    void SetBoundingBox(const SpaceFillingCurve::BoundingBox& bbox);
    
    /**
     * 添加经纬度点进行压缩
     * @param point 经纬度点
     */
    void AddPoint(const SpaceFillingCurve::GeoPoint& point);
    
    /**
     * 获取压缩后的数据
     * @return 压缩数据
     */
    Array<uint8_t> GetCompressedData();
    
    /**
     * 获取压缩大小（比特）
     */
    int GetCompressedSizeInBits() const { return compressed_size_in_bits_; }
    
    /**
     * 获取数据集边界框
     */
    const SpaceFillingCurve::BoundingBox& GetBoundingBox() const { return bbox_; }
    
    /**
     * 获取编码比特数
     */
    int GetEncodingBits() const { return encoding_bits_; }
    
    /**
     * 获取压缩方法名称
     */
    std::string GetMethodName() const;
    
    /**
     * 获取总点数
     */
    int GetTotalPoints() const { return stats_.total_points; }
    
    /**
     * 获取统计信息
     */
    struct Statistics {
        int total_points = 0;
        int header_bits = 0;
        int encoding_bits_used = 0;
        int diff_encoding_bits = 0;
        double avg_bits_per_point = 0.0;
        double compression_ratio = 0.0;
        
        // 差值编码统计
        std::vector<int64_t> diff_values;
        std::vector<int> diff_bit_lengths;
        int64_t max_diff = 0;
        int64_t min_diff = 0;
        double avg_diff_magnitude = 0.0;
        
        void Calculate();
        void Print() const;
    };
    
    const Statistics& GetStatistics() const { return stats_; }
    
    /**
     * 重置压缩器（用于处理多个数据集）
     */
    void Reset();

private:
    CompressionMethod method_;
    double max_error_;
    int encoding_bits_;  // 编码使用的总比特数
    
    std::unique_ptr<OutputBitStream> output_bit_stream_;
    int compressed_size_in_bits_ = 0;
    
    bool first_point_ = true;
    uint64_t previous_code_ = 0;
    
    SpaceFillingCurve::BoundingBox bbox_;
    
    Statistics stats_;
    
    // 辅助函数
    void WriteHeader();
    uint64_t EncodePoint(const SpaceFillingCurve::GeoPoint& point);
    void EncodeFirstPoint(uint64_t code);
    void EncodeDifference(int64_t diff);
};

