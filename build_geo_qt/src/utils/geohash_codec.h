#ifndef SERF_GEOHASH_CODEC_H
#define SERF_GEOHASH_CODEC_H

#include <cstdint>
#include <string>

/**
 * Geohash 编码/解码器
 * 支持11位精度的Geohash编码，满足1e-5度的精度要求
 */
class GeohashCodec {
 public:
  // Geohash 编码：将经纬度转换为11位Geohash字符串
  static std::string Encode(double latitude, double longitude, int precision = 11);
  
  // Geohash 解码：将11位Geohash字符串转换为经纬度（返回单元格中心点）
  static std::pair<double, double> Decode(const std::string& geohash);
  
  // 将Geohash字符串转换为55位整数（11位 * 5位/字符）
  static uint64_t GeohashToInt(const std::string& geohash);
  
  // 将55位整数转换为Geohash字符串
  static std::string IntToGeohash(uint64_t geohash_int, int precision = 11);
  
  // 直接经纬度到整数的转换
  static uint64_t LatLonToInt(double latitude, double longitude, int precision = 11);
  
  // 直接整数到经纬度的转换
  static std::pair<double, double> IntToLatLon(uint64_t geohash_int, int precision = 11);

 private:
  static const std::string kBase32Chars;
  static const int kMaxPrecision;
  
  // 内部辅助函数
  static std::string EncodeBits(uint64_t bits, int precision);
  static uint64_t DecodeBits(const std::string& geohash);
  static std::pair<double, double> BitsToLatLon(uint64_t bits, int precision);
  static uint64_t LatLonToBits(double latitude, double longitude, int precision);
};

#endif  // SERF_GEOHASH_CODEC_H
