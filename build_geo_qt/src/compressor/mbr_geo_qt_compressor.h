#ifndef SERF_MBR_GEO_QT_COMPRESSOR_H
#define SERF_MBR_GEO_QT_COMPRESSOR_H

#include <cstdint>
#include <vector>
#include <memory>

#include "utils/mbr_geohash_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/leb128_codec.h"

/**
 * 基于 MBR 的优化版 Geo-Qt 压缩器
 * 使用相对位置编码，显著提高压缩效率
 */
class MbrGeoQtCompressor {
 public:
  // 构造函数：从数据点自动计算 MBR
  explicit MbrGeoQtCompressor(const std::vector<std::pair<double, double>>& points);
  
  // 构造函数：指定 MBR 边界
  MbrGeoQtCompressor(double min_lat, double max_lat, double min_lon, double max_lon);
  
  // 添加一个经纬度点进行压缩
  void AddPoint(double latitude, double longitude);
  
  // 完成压缩，返回压缩后的字节数据
  std::vector<uint8_t> Finish();
  
  // 获取压缩统计信息
  size_t GetCompressedSize() const { return compressed_data_.size(); }
  size_t GetPointCount() const { return point_count_; }
  
  // 获取 MBR 信息
  double GetMinLat() const { return mbr_codec_.GetMinLat(); }
  double GetMaxLat() const { return mbr_codec_.GetMaxLat(); }
  double GetMinLon() const { return mbr_codec_.GetMinLon(); }
  double GetMaxLon() const { return mbr_codec_.GetMaxLon(); }
  int GetRequiredBits() const { return mbr_codec_.GetRequiredBits(); }
  
  // 重置压缩器状态
  void Reset();

 private:
  MbrGeohashCodec mbr_codec_;
  bool first_point_;
  uint64_t previous_geohash_int_;
  std::vector<uint8_t> compressed_data_;
  size_t point_count_;
  
  // 内部辅助函数
  void WriteMBRHeader();
  void WriteFirstPoint(uint64_t geohash_int);
  void WriteDeltaPoint(uint64_t geohash_int);
};

#endif  // SERF_MBR_GEO_QT_COMPRESSOR_H
