#include "decompressor/geo_qt_decompressor.h"
#include <stdexcept>

GeoQtDecompressor::GeoQtDecompressor(int geohash_precision) 
    : geohash_precision_(geohash_precision), point_count_(0) {
}

std::vector<std::pair<double, double>> GeoQtDecompressor::Decompress(const std::vector<uint8_t>& compressed_data) {
  std::vector<std::pair<double, double>> result;
  size_t offset = 0;
  bool first_point = true;
  uint64_t previous_geohash_int = 0;
  point_count_ = 0;
  
  while (offset < compressed_data.size()) {
    std::pair<double, double> point;
    
    if (first_point) {
      point = ReadFirstPoint(compressed_data, offset);
      // 从第一个点重建geohash整数
      previous_geohash_int = GeohashCodec::LatLonToInt(point.first, point.second, geohash_precision_);
      first_point = false;
    } else {
      point = ReadDeltaPoint(compressed_data, offset, previous_geohash_int);
      // 更新geohash整数
      previous_geohash_int = GeohashCodec::LatLonToInt(point.first, point.second, geohash_precision_);
    }
    
    result.push_back(point);
    point_count_++;
  }
  
  return result;
}

std::pair<double, double> GeoQtDecompressor::ReadFirstPoint(const std::vector<uint8_t>& data, size_t& offset) {
  if (offset >= data.size()) {
    throw std::runtime_error("Unexpected end of data while reading first point");
  }
  
  // 检查头部字节
  uint8_t header = data[offset++];
  if ((header & 0x80) != 0x00) {
    throw std::runtime_error("Invalid header for first point");
  }
  
  // 读取55位Geohash整数（7字节）
  if (offset + 7 > data.size()) {
    throw std::runtime_error("Insufficient data for first point");
  }
  
  uint64_t geohash_int = 0;
  for (int i = 0; i < 7; ++i) {
    geohash_int |= (static_cast<uint64_t>(data[offset++]) << (i * 8));
  }
  
  // 转换为经纬度
  return GeohashCodec::IntToLatLon(geohash_int, geohash_precision_);
}

std::pair<double, double> GeoQtDecompressor::ReadDeltaPoint(const std::vector<uint8_t>& data, size_t& offset, uint64_t previous_geohash_int) {
  if (offset >= data.size()) {
    throw std::runtime_error("Unexpected end of data while reading delta point");
  }
  
  // 解码LEB128
  size_t new_offset;
  uint64_t zigzag_delta = Leb128Codec::DecodeAt(data, offset, new_offset);
  offset = new_offset;
  
  // Zigzag解码
  int64_t delta = ZigZagCodec::Decode(zigzag_delta);
  
  // 重建Geohash整数
  uint64_t current_geohash_int = previous_geohash_int + delta;
  
  // 转换为经纬度
  return GeohashCodec::IntToLatLon(current_geohash_int, geohash_precision_);
}
