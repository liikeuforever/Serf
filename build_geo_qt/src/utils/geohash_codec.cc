#include "geohash_codec.h"
#include <algorithm>

const std::string GeohashCodec::kBase32Chars = "0123456789bcdefghjkmnpqrstuvwxyz";
const int GeohashCodec::kMaxPrecision = 12;

std::string GeohashCodec::Encode(double latitude, double longitude, int precision) {
  uint64_t bits = LatLonToBits(latitude, longitude, precision);
  return EncodeBits(bits, precision);
}

std::pair<double, double> GeohashCodec::Decode(const std::string& geohash) {
  uint64_t bits = DecodeBits(geohash);
  return BitsToLatLon(bits, geohash.length());
}

uint64_t GeohashCodec::GeohashToInt(const std::string& geohash) {
  return DecodeBits(geohash);
}

std::string GeohashCodec::IntToGeohash(uint64_t geohash_int, int precision) {
  return EncodeBits(geohash_int, precision);
}

uint64_t GeohashCodec::LatLonToInt(double latitude, double longitude, int precision) {
  return LatLonToBits(latitude, longitude, precision);
}

std::pair<double, double> GeohashCodec::IntToLatLon(uint64_t geohash_int, int precision) {
  return BitsToLatLon(geohash_int, precision);
}

std::string GeohashCodec::EncodeBits(uint64_t bits, int precision) {
  std::string result;
  result.reserve(precision);
  
  for (int i = 0; i < precision; ++i) {
    // 提取5位
    int char_index = (bits >> (i * 5)) & 0x1F;
    result += kBase32Chars[char_index];
  }
  
  return result;
}

uint64_t GeohashCodec::DecodeBits(const std::string& geohash) {
  uint64_t bits = 0;
  
  for (size_t i = 0; i < geohash.length(); ++i) {
    char c = geohash[i];
    size_t char_index = kBase32Chars.find(c);
    if (char_index == std::string::npos) {
      throw std::invalid_argument("Invalid geohash character: " + std::string(1, c));
    }
    
    bits |= (char_index << (i * 5));
  }
  
  return bits;
}

std::pair<double, double> GeohashCodec::BitsToLatLon(uint64_t bits, int precision) {
  // 分离经纬度位
  uint64_t lat_bits = 0;
  uint64_t lon_bits = 0;
  
  for (int i = 0; i < precision * 5; ++i) {
    if (i % 2 == 0) {
      // 偶数位是经度
      lon_bits |= ((bits >> i) & 1) << (i / 2);
    } else {
      // 奇数位是纬度
      lat_bits |= ((bits >> i) & 1) << (i / 2);
    }
  }
  
  // 转换为实际坐标
  double lat_min = -90.0, lat_max = 90.0;
  double lon_min = -180.0, lon_max = 180.0;
  
  int lat_bits_count = (precision * 5 + 1) / 2;
  int lon_bits_count = (precision * 5) / 2;
  
  for (int i = 0; i < lat_bits_count; ++i) {
    double lat_mid = (lat_min + lat_max) / 2.0;
    if (lat_bits & (1ULL << i)) {
      lat_min = lat_mid;
    } else {
      lat_max = lat_mid;
    }
  }
  
  for (int i = 0; i < lon_bits_count; ++i) {
    double lon_mid = (lon_min + lon_max) / 2.0;
    if (lon_bits & (1ULL << i)) {
      lon_min = lon_mid;
    } else {
      lon_max = lon_mid;
    }
  }
  
  // 返回单元格中心点
  return {(lat_min + lat_max) / 2.0, (lon_min + lon_max) / 2.0};
}

uint64_t GeohashCodec::LatLonToBits(double latitude, double longitude, int precision) {
  // 确保坐标在有效范围内
  latitude = std::max(-90.0, std::min(90.0, latitude));
  longitude = std::max(-180.0, std::min(180.0, longitude));
  
  uint64_t lat_bits = 0;
  uint64_t lon_bits = 0;
  
  double lat_min = -90.0, lat_max = 90.0;
  double lon_min = -180.0, lon_max = 180.0;
  
  int lat_bits_count = (precision * 5 + 1) / 2;
  int lon_bits_count = (precision * 5) / 2;
  
  // 编码纬度
  for (int i = 0; i < lat_bits_count; ++i) {
    double lat_mid = (lat_min + lat_max) / 2.0;
    if (latitude >= lat_mid) {
      lat_bits |= (1ULL << i);
      lat_min = lat_mid;
    } else {
      lat_max = lat_mid;
    }
  }
  
  // 编码经度
  for (int i = 0; i < lon_bits_count; ++i) {
    double lon_mid = (lon_min + lon_max) / 2.0;
    if (longitude >= lon_mid) {
      lon_bits |= (1ULL << i);
      lon_min = lon_mid;
    } else {
      lon_max = lon_mid;
    }
  }
  
  // 交错合并经纬度位
  uint64_t result = 0;
  for (int i = 0; i < precision * 5; ++i) {
    if (i % 2 == 0) {
      // 偶数位是经度
      result |= ((lon_bits >> (i / 2)) & 1) << i;
    } else {
      // 奇数位是纬度
      result |= ((lat_bits >> (i / 2)) & 1) << i;
    }
  }
  
  return result;
}
