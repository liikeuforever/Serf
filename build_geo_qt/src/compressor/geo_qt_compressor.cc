#include "compressor/geo_qt_compressor.h"

GeoQtCompressor::GeoQtCompressor(int geohash_precision) 
    : geohash_precision_(geohash_precision), 
      first_point_(true), 
      previous_geohash_int_(0),
      point_count_(0) {
  // 预留一些空间
  compressed_data_.reserve(1024);
}

void GeoQtCompressor::AddPoint(double latitude, double longitude) {
  // 将经纬度转换为Geohash整数
  uint64_t current_geohash_int = GeohashCodec::LatLonToInt(latitude, longitude, geohash_precision_);
  
  if (first_point_) {
    WriteFirstPoint(current_geohash_int);
    first_point_ = false;
  } else {
    WriteDeltaPoint(current_geohash_int);
  }
  
  previous_geohash_int_ = current_geohash_int;
  point_count_++;
}

std::vector<uint8_t> GeoQtCompressor::Finish() {
  return compressed_data_;
}

void GeoQtCompressor::Reset() {
  first_point_ = true;
  previous_geohash_int_ = 0;
  compressed_data_.clear();
  point_count_ = 0;
}

void GeoQtCompressor::WriteFirstPoint(uint64_t geohash_int) {
  // 第一个点：直接存储完整的55位Geohash整数
  // 使用7个字节存储（55位需要7字节，最高位设为0表示这是完整值）
  uint8_t header = 0x00;  // 最高位为0表示完整值
  compressed_data_.push_back(header);
  
  // 存储55位数据（7字节）
  for (int i = 0; i < 7; ++i) {
    compressed_data_.push_back(static_cast<uint8_t>((geohash_int >> (i * 8)) & 0xFF));
  }
}

void GeoQtCompressor::WriteDeltaPoint(uint64_t geohash_int) {
  // 计算残差
  int64_t delta = static_cast<int64_t>(geohash_int - previous_geohash_int_);
  
  // Zigzag编码
  uint64_t zigzag_delta = ZigZagCodec::Encode(delta);
  
  // LEB128编码
  std::vector<uint8_t> leb128_encoded = Leb128Codec::Encode(zigzag_delta);
  
  // 写入压缩数据
  compressed_data_.insert(compressed_data_.end(), leb128_encoded.begin(), leb128_encoded.end());
}
