#include "decompressor/mbr_geo_qt_decompressor.h"
#include <stdexcept>

std::vector<std::pair<double, double>> MbrGeoQtDecompressor::Decompress(const std::vector<uint8_t>& compressed_data) {
  std::vector<std::pair<double, double>> result;
  size_t offset = 0;
  bool first_point = true;
  uint64_t previous_geohash_int = 0;
  point_count_ = 0;
  
  // 读取 MBR 头部信息
  ReadMBRHeader(compressed_data, offset);
  
  while (offset < compressed_data.size()) {
    std::pair<double, double> point;
    
    if (first_point) {
      point = ReadFirstPoint(compressed_data, offset);
      // 从第一个点重建geohash整数
      previous_geohash_int = mbr_codec_.EncodeToInt(point.first, point.second);
      first_point = false;
    } else {
      auto [point_result, current_geohash_int] = ReadDeltaPoint(compressed_data, offset, previous_geohash_int);
      point = point_result;
      previous_geohash_int = current_geohash_int;
    }
    
    result.push_back(point);
    point_count_++;
  }
  
  return result;
}

void MbrGeoQtDecompressor::ReadMBRHeader(const std::vector<uint8_t>& data, size_t& offset) {
  if (offset + 33 > data.size()) {  // 32字节MBR + 1字节位数
    throw std::runtime_error("Insufficient data for MBR header");
  }
  
  // 读取 MBR 边界
  uint64_t min_lat_bits = 0, max_lat_bits = 0, min_lon_bits = 0, max_lon_bits = 0;
  
  for (int i = 0; i < 8; ++i) {
    min_lat_bits |= (static_cast<uint64_t>(data[offset++]) << (i * 8));
  }
  for (int i = 0; i < 8; ++i) {
    max_lat_bits |= (static_cast<uint64_t>(data[offset++]) << (i * 8));
  }
  for (int i = 0; i < 8; ++i) {
    min_lon_bits |= (static_cast<uint64_t>(data[offset++]) << (i * 8));
  }
  for (int i = 0; i < 8; ++i) {
    max_lon_bits |= (static_cast<uint64_t>(data[offset++]) << (i * 8));
  }
  
  // 读取所需位数
  int required_bits = data[offset++];
  
  // 重建 MBR 边界
  double min_lat = *reinterpret_cast<const double*>(&min_lat_bits);
  double max_lat = *reinterpret_cast<const double*>(&max_lat_bits);
  double min_lon = *reinterpret_cast<const double*>(&min_lon_bits);
  double max_lon = *reinterpret_cast<const double*>(&max_lon_bits);
  
  // 创建 MBR 编码器
  mbr_codec_ = MbrGeohashCodec(min_lat, max_lat, min_lon, max_lon);
}

std::pair<double, double> MbrGeoQtDecompressor::ReadFirstPoint(const std::vector<uint8_t>& data, size_t& offset) {
  if (offset >= data.size()) {
    throw std::runtime_error("Unexpected end of data while reading first point");
  }
  
  // 读取数据长度
  int data_bytes = data[offset++];
  
  if (offset + data_bytes > data.size()) {
    throw std::runtime_error("Insufficient data for first point");
  }
  
  // 读取 Geohash 整数
  uint64_t geohash_int = 0;
  for (int i = 0; i < data_bytes; ++i) {
    geohash_int |= (static_cast<uint64_t>(data[offset++]) << (i * 8));
  }
  
  // 转换为经纬度
  return mbr_codec_.DecodeFromInt(geohash_int);
}

std::pair<std::pair<double, double>, uint64_t> MbrGeoQtDecompressor::ReadDeltaPoint(const std::vector<uint8_t>& data, size_t& offset, uint64_t previous_geohash_int) {
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
  auto point = mbr_codec_.DecodeFromInt(current_geohash_int);
  
  return {point, current_geohash_int};
}
