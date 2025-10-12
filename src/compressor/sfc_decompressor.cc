#include "compressor/sfc_decompressor.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/double.h"
#include <iostream>

SFCDecompressor::SFCDecompressor(const Array<uint8_t>& compressed_data) {
    input_bit_stream_ = std::make_unique<InputBitStream>();
    input_bit_stream_->SetBuffer(compressed_data);
    ReadHeader();
}

void SFCDecompressor::ReadHeader() {
    // 读取压缩方法 (2 bits)
    int method_value = input_bit_stream_->ReadInt(2);
    method_ = static_cast<SFCCompressor::CompressionMethod>(method_value);
    
    // 读取最大误差 (64 bits)
    uint64_t max_error_bits = input_bit_stream_->ReadLong(64);
    max_error_ = Double::LongBitsToDouble(max_error_bits);
    
    // 读取编码比特数 (8 bits)
    encoding_bits_ = input_bit_stream_->ReadInt(8);
    
    // 如果是MBR或Hilbert，读取边界框 (4 * 64 = 256 bits)
    if (method_ == SFCCompressor::MBR_GEOHASH || method_ == SFCCompressor::HILBERT_CURVE) {
        uint64_t min_lon_bits = input_bit_stream_->ReadLong(64);
        bbox_.min_lon = Double::LongBitsToDouble(min_lon_bits);
        
        uint64_t max_lon_bits = input_bit_stream_->ReadLong(64);
        bbox_.max_lon = Double::LongBitsToDouble(max_lon_bits);
        
        uint64_t min_lat_bits = input_bit_stream_->ReadLong(64);
        bbox_.min_lat = Double::LongBitsToDouble(min_lat_bits);
        
        uint64_t max_lat_bits = input_bit_stream_->ReadLong(64);
        bbox_.max_lat = Double::LongBitsToDouble(max_lat_bits);
    }
}

std::vector<SpaceFillingCurve::GeoPoint> SFCDecompressor::DecompressAll(int expected_points) {
    std::vector<SpaceFillingCurve::GeoPoint> points;
    points.reserve(expected_points);
    
    // 读取第一个点的完整编码
    uint64_t previous_code = input_bit_stream_->ReadLong(encoding_bits_);
    points.push_back(DecodePoint(previous_code));
    point_count_++;
    
    // 读取后续点的差值
    for (int i = 1; i < expected_points; ++i) {
        // 解码差值
        uint64_t zigzag = EliasGammaCodec::Decode(input_bit_stream_.get());
        int64_t diff = ZigZagCodec::Decode(zigzag - 1);
        
        // 计算当前点的编码
        uint64_t current_code = static_cast<uint64_t>(static_cast<int64_t>(previous_code) + diff);
        
        // 解码点
        points.push_back(DecodePoint(current_code));
        
        previous_code = current_code;
        point_count_++;
    }
    
    return points;
}

SpaceFillingCurve::GeoPoint SFCDecompressor::DecodePoint(uint64_t code) {
    switch (method_) {
        case SFCCompressor::STANDARD_GEOHASH:
            return SpaceFillingCurve::DecodeStandardGeoHash(code, encoding_bits_);
        
        case SFCCompressor::MBR_GEOHASH:
            return SpaceFillingCurve::DecodeMBRGeoHash(code, encoding_bits_, bbox_);
        
        case SFCCompressor::HILBERT_CURVE: {
            int order = encoding_bits_ / 2;
            return SpaceFillingCurve::DecodeHilbert(code, order, bbox_);
        }
        
        default:
            return SpaceFillingCurve::GeoPoint(0, 0);
    }
}

