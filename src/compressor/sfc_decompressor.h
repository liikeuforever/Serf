#pragma once

#include "compressor/space_filling_curve.h"
#include "compressor/sfc_compressor.h"
#include "utils/input_bit_stream.h"
#include <memory>
#include <vector>

/**
 * 空间填充曲线解压缩器
 */
class SFCDecompressor {
public:
    /**
     * 构造函数
     * @param compressed_data 压缩数据（Array）
     */
    SFCDecompressor(const Array<uint8_t>& compressed_data);
    
    /**
     * 解压缩所有点
     * @param expected_points 期望的点数（用于停止解压）
     * @return 解压后的点集合
     */
    std::vector<SpaceFillingCurve::GeoPoint> DecompressAll(int expected_points);
    
    /**
     * 获取压缩方法
     */
    SFCCompressor::CompressionMethod GetMethod() const { return method_; }
    
    /**
     * 获取最大误差
     */
    double GetMaxError() const { return max_error_; }
    
    /**
     * 获取解压的点数
     */
    int GetPointCount() const { return point_count_; }

private:
    std::unique_ptr<InputBitStream> input_bit_stream_;
    
    SFCCompressor::CompressionMethod method_;
    double max_error_;
    int encoding_bits_;
    SpaceFillingCurve::BoundingBox bbox_;
    int point_count_ = 0;
    
    void ReadHeader();
    SpaceFillingCurve::GeoPoint DecodePoint(uint64_t code);
};

