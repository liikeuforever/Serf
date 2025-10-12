#include "compressor/mbr_geo_qt_compressor.h"
#include <cstring>

MbrGeoQtCompressor::MbrGeoQtCompressor(const std::vector<std::pair<double, double>>& points)
    : mbr_codec_(points), 
      first_point_(true), 
      previous_geohash_int_(0),
      point_count_(0) {
  compressed_data_.reserve(1024);
  WriteMBRHeader();
}

MbrGeoQtCompressor::MbrGeoQtCompressor(double min_lat, double max_lat, double min_lon, double max_lon)
    : mbr_codec_(min_lat, max_lat, min_lon, max_lon),
      first_point_(true), 
      previous_geohash_int_(0),
      point_count_(0) {
  compressed_data_.reserve(1024);
  WriteMBRHeader();
}

void MbrGeoQtCompressor::AddPoint(double latitude, double longitude) {
  // 将经纬度转换为相对位置 Geohash 整数
  uint64_t current_geohash_int = mbr_codec_.EncodeToInt(latitude, longitude);
  
  if (first_point_) {
    WriteFirstPoint(current_geohash_int);
    first_point_ = false;
  } else {
    WriteDeltaPoint(current_geohash_int);
  }
  
  previous_geohash_int_ = current_geohash_int;
  point_count_++;
}

std::vector<uint8_t> MbrGeoQtCompressor::Finish() {
  return compressed_data_;
}

void MbrGeoQtCompressor::Reset() {
  first_point_ = true;
  previous_geohash_int_ = 0;
  compressed_data_.clear();
  point_count_ = 0;
  WriteMBRHeader();
}

void MbrGeoQtCompressor::WriteMBRHeader() {
  // 写入 MBR 边界信息（8字节 × 4 = 32字节）
  double min_lat = mbr_codec_.GetMinLat();
  double max_lat = mbr_codec_.GetMaxLat();
  double min_lon = mbr_codec_.GetMinLon();
  double max_lon = mbr_codec_.GetMaxLon();
  
  uint64_t min_lat_bits = *reinterpret_cast<const uint64_t*>(&min_lat);
  uint64_t max_lat_bits = *reinterpret_cast<const uint64_t*>(&max_lat);
  uint64_t min_lon_bits = *reinterpret_cast<const uint64_t*>(&min_lon);
  uint64_t max_lon_bits = *reinterpret_cast<const uint64_t*>(&max_lon);
  
  for (int i = 0; i < 8; ++i) {
    compressed_data_.push_back(static_cast<uint8_t>((min_lat_bits >> (i * 8)) & 0xFF));
  }
  for (int i = 0; i < 8; ++i) {
    compressed_data_.push_back(static_cast<uint8_t>((max_lat_bits >> (i * 8)) & 0xFF));
  }
  for (int i = 0; i < 8; ++i) {
    compressed_data_.push_back(static_cast<uint8_t>((min_lon_bits >> (i * 8)) & 0xFF));
  }
  for (int i = 0; i < 8; ++i) {
    compressed_data_.push_back(static_cast<uint8_t>((max_lon_bits >> (i * 8)) & 0xFF));
  }
  
  // 写入所需位数信息（1字节）
  compressed_data_.push_back(static_cast<uint8_t>(mbr_codec_.GetRequiredBits()));
}

void MbrGeoQtCompressor::WriteFirstPoint(uint64_t geohash_int) {
  // 第一个点：直接存储完整的 Geohash 整数
  int required_bits = mbr_codec_.GetRequiredBits();
  int required_bytes = (required_bits + 7) / 8;
  
  // 写入数据长度（1字节）
  compressed_data_.push_back(static_cast<uint8_t>(required_bytes));
  
  // 存储 Geohash 整数
  for (int i = 0; i < required_bytes; ++i) {
    compressed_data_.push_back(static_cast<uint8_t>((geohash_int >> (i * 8)) & 0xFF));
  }
}

void MbrGeoQtCompressor::WriteDeltaPoint(uint64_t geohash_int) {
  // 计算残差
  int64_t delta = static_cast<int64_t>(geohash_int - previous_geohash_int_);
  
  // Zigzag编码
  uint64_t zigzag_delta = ZigZagCodec::Encode(delta);
  
  // LEB128编码
  std::vector<uint8_t> leb128_encoded = Leb128Codec::Encode(zigzag_delta);
  
  // 写入压缩数据
  compressed_data_.insert(compressed_data_.end(), leb128_encoded.begin(), leb128_encoded.end());
}
