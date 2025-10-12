#include "mbr_geohash_codec.h"
#include <algorithm>
#include <cmath>
#include <iostream>

MbrGeohashCodec::MbrGeohashCodec(const std::vector<std::pair<double, double>>& points) {
  if (points.empty()) {
    throw std::invalid_argument("Empty point set for MBR calculation");
  }
  
  // 计算 MBR
  min_lat_ = max_lat_ = points[0].first;
  min_lon_ = max_lon_ = points[0].second;
  
  for (const auto& point : points) {
    min_lat_ = std::min(min_lat_, point.first);
    max_lat_ = std::max(max_lat_, point.first);
    min_lon_ = std::min(min_lon_, point.second);
    max_lon_ = std::max(max_lon_, point.second);
  }
  
  // 计算范围
  lat_range_ = max_lat_ - min_lat_;
  lon_range_ = max_lon_ - min_lon_;
  
  // 计算精度：考虑对角线长度，需要除以 sqrt(2)
  // 使用更严格的精度来确保满足 1e-5 度要求
  lat_precision_ = kTargetPrecision;  // 直接使用 1e-5 度精度
  lon_precision_ = kTargetPrecision;
  
  // 计算所需的位数
  int lat_bits = CalculateRequiredBits(lat_range_, lat_precision_);
  int lon_bits = CalculateRequiredBits(lon_range_, lon_precision_);
  
  required_bits_ = lat_bits + lon_bits;
  
  std::cout << "MBR: [" << min_lat_ << ", " << max_lat_ << "] x [" 
            << min_lon_ << ", " << max_lon_ << "]" << std::endl;
  std::cout << "范围: " << lat_range_ << " x " << lon_range_ << " 度" << std::endl;
  std::cout << "精度: " << lat_precision_ << " x " << lon_precision_ << " 度" << std::endl;
  std::cout << "所需位数: " << required_bits_ << " (纬度: " << lat_bits 
            << ", 经度: " << lon_bits << ")" << std::endl;
}

MbrGeohashCodec::MbrGeohashCodec(double min_lat, double max_lat, double min_lon, double max_lon)
    : min_lat_(min_lat), max_lat_(max_lat), min_lon_(min_lon), max_lon_(max_lon) {
  
  lat_range_ = max_lat_ - min_lat_;
  lon_range_ = max_lon_ - min_lon_;
  
  lat_precision_ = kTargetPrecision;  // 直接使用 1e-5 度精度
  lon_precision_ = kTargetPrecision;
  
  int lat_bits = CalculateRequiredBits(lat_range_, lat_precision_);
  int lon_bits = CalculateRequiredBits(lon_range_, lon_precision_);
  
  required_bits_ = lat_bits + lon_bits;
}

uint64_t MbrGeohashCodec::EncodeToInt(double latitude, double longitude) const {
  // 转换为相对坐标 [0, 1]
  double lat_rel = (latitude - min_lat_) / lat_range_;
  double lon_rel = (longitude - min_lon_) / lon_range_;
  
  // 确保在 [0, 1] 范围内
  lat_rel = std::max(0.0, std::min(1.0, lat_rel));
  lon_rel = std::max(0.0, std::min(1.0, lon_rel));
  
  return RelativeToInt(lat_rel, lon_rel);
}

std::pair<double, double> MbrGeohashCodec::DecodeFromInt(uint64_t geohash_int) const {
  auto [lat_rel, lon_rel] = IntToRelative(geohash_int);
  
  // 转换回绝对坐标
  double latitude = min_lat_ + lat_rel * lat_range_;
  double longitude = min_lon_ + lon_rel * lon_range_;
  
  return {latitude, longitude};
}

int MbrGeohashCodec::CalculateRequiredBits(double range, double target_precision) const {
  if (range <= 0 || target_precision <= 0) {
    return 1;
  }
  
  // 计算需要多少个精度单位来覆盖整个范围
  double units_needed = range / target_precision;
  
  // 计算需要的位数：2^bits >= units_needed
  int bits = static_cast<int>(std::ceil(std::log2(units_needed)));
  
  return std::max(1, bits);
}

uint64_t MbrGeohashCodec::RelativeToInt(double lat_rel, double lon_rel) const {
  int lat_bits = CalculateRequiredBits(lat_range_, lat_precision_);
  int lon_bits = CalculateRequiredBits(lon_range_, lon_precision_);
  
  // 将相对坐标转换为整数
  uint64_t lat_int = static_cast<uint64_t>(lat_rel * ((1ULL << lat_bits) - 1));
  uint64_t lon_int = static_cast<uint64_t>(lon_rel * ((1ULL << lon_bits) - 1));
  
  // 交错合并：经度在低位，纬度在高位
  uint64_t result = 0;
  int lat_pos = 0, lon_pos = 0;
  
  for (int i = 0; i < lat_bits + lon_bits; ++i) {
    if (i % 2 == 0) {
      // 偶数位存储经度
      if (lon_pos < lon_bits) {
        result |= ((lon_int >> lon_pos) & 1) << i;
        lon_pos++;
      }
    } else {
      // 奇数位存储纬度
      if (lat_pos < lat_bits) {
        result |= ((lat_int >> lat_pos) & 1) << i;
        lat_pos++;
      }
    }
  }
  
  return result;
}

std::pair<double, double> MbrGeohashCodec::IntToRelative(uint64_t geohash_int) const {
  int lat_bits = CalculateRequiredBits(lat_range_, lat_precision_);
  int lon_bits = CalculateRequiredBits(lon_range_, lon_precision_);
  
  uint64_t lat_int = 0, lon_int = 0;
  int lat_pos = 0, lon_pos = 0;
  
  // 分离经纬度位
  for (int i = 0; i < lat_bits + lon_bits; ++i) {
    if (i % 2 == 0) {
      // 偶数位是经度
      if (lon_pos < lon_bits) {
        lon_int |= ((geohash_int >> i) & 1) << lon_pos;
        lon_pos++;
      }
    } else {
      // 奇数位是纬度
      if (lat_pos < lat_bits) {
        lat_int |= ((geohash_int >> i) & 1) << lat_pos;
        lat_pos++;
      }
    }
  }
  
  // 转换回相对坐标
  double lat_rel = static_cast<double>(lat_int) / ((1ULL << lat_bits) - 1);
  double lon_rel = static_cast<double>(lon_int) / ((1ULL << lon_bits) - 1);
  
  return {lat_rel, lon_rel};
}
