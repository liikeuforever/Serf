#ifndef SERF_MBR_GEOHASH_CODEC_H
#define SERF_MBR_GEOHASH_CODEC_H

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

/**
 * 基于 MBR 的相对位置 Geohash 编码器
 * 针对轨迹数据优化，使用相对位置而非全球坐标
 * 精度要求：1e-5 度，考虑对角线长度需要除以 sqrt(2)
 */
class MbrGeohashCodec {
 public:
  // 默认构造函数
  MbrGeohashCodec() : min_lat_(0.0), max_lat_(0.0), min_lon_(0.0), max_lon_(0.0),
                      lat_range_(0.0), lon_range_(0.0), lat_precision_(0.0), lon_precision_(0.0), required_bits_(1) {}
  
  // 构造函数：从数据点计算 MBR
  explicit MbrGeohashCodec(const std::vector<std::pair<double, double>>& points);
  
  // 构造函数：指定 MBR 边界
  MbrGeohashCodec(double min_lat, double max_lat, double min_lon, double max_lon);
  
  // 编码：将相对位置转换为 Geohash 整数
  uint64_t EncodeToInt(double latitude, double longitude) const;
  
  // 解码：将 Geohash 整数转换为相对位置
  std::pair<double, double> DecodeFromInt(uint64_t geohash_int) const;
  
  // 获取 MBR 信息
  double GetMinLat() const { return min_lat_; }
  double GetMaxLat() const { return max_lat_; }
  double GetMinLon() const { return min_lon_; }
  double GetMaxLon() const { return max_lon_; }
  
  // 获取精度信息
  double GetLatPrecision() const { return lat_precision_; }
  double GetLonPrecision() const { return lon_precision_; }
  
  // 获取所需的位数
  int GetRequiredBits() const { return required_bits_; }

 private:
  double min_lat_, max_lat_, min_lon_, max_lon_;
  double lat_range_, lon_range_;
  double lat_precision_, lon_precision_;
  int required_bits_;
  
  // 精度要求：1e-5 度，考虑对角线需要除以 sqrt(2)
  static constexpr double kTargetPrecision = 1e-5;  // 1e-5 度精度
  
  // 计算所需的位数
  int CalculateRequiredBits(double range, double target_precision) const;
  
  // 将相对坐标转换为整数
  uint64_t RelativeToInt(double lat_rel, double lon_rel) const;
  
  // 将整数转换为相对坐标
  std::pair<double, double> IntToRelative(uint64_t geohash_int) const;
};

#endif  // SERF_MBR_GEOHASH_CODEC_H
