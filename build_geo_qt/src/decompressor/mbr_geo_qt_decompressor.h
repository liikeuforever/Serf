#ifndef SERF_MBR_GEO_QT_DECOMPRESSOR_H
#define SERF_MBR_GEO_QT_DECOMPRESSOR_H

#include <cstdint>
#include <vector>
#include <utility>

#include "utils/mbr_geohash_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/leb128_codec.h"

/**
 * 基于 MBR 的优化版 Geo-Qt 解压器
 * 将压缩的字节数据解压为经纬度坐标序列
 */
class MbrGeoQtDecompressor {
 public:
  // 默认构造函数
  MbrGeoQtDecompressor() : point_count_(0) {}
  
  // 解压字节数据，返回经纬度坐标序列
  std::vector<std::pair<double, double>> Decompress(const std::vector<uint8_t>& compressed_data);
  
  // 获取解压统计信息
  size_t GetPointCount() const { return point_count_; }
  
  // 获取 MBR 信息
  double GetMinLat() const { return mbr_codec_.GetMinLat(); }
  double GetMaxLat() const { return mbr_codec_.GetMaxLat(); }
  double GetMinLon() const { return mbr_codec_.GetMinLon(); }
  double GetMaxLon() const { return mbr_codec_.GetMaxLon(); }

 private:
  MbrGeohashCodec mbr_codec_{0.0, 0.0, 0.0, 0.0};  // 默认初始化
  size_t point_count_;
  
  // 内部辅助函数
  void ReadMBRHeader(const std::vector<uint8_t>& data, size_t& offset);
  std::pair<double, double> ReadFirstPoint(const std::vector<uint8_t>& data, size_t& offset);
  std::pair<std::pair<double, double>, uint64_t> ReadDeltaPoint(const std::vector<uint8_t>& data, size_t& offset, uint64_t previous_geohash_int);
};

#endif  // SERF_MBR_GEO_QT_DECOMPRESSOR_H
