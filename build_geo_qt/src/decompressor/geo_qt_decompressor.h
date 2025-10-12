#ifndef SERF_GEO_QT_DECOMPRESSOR_H
#define SERF_GEO_QT_DECOMPRESSOR_H

#include <cstdint>
#include <vector>
#include <utility>

#include "utils/geohash_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/leb128_codec.h"

/**
 * Geo-Qt 解压器
 * 将压缩的字节数据解压为经纬度坐标序列
 */
class GeoQtDecompressor {
 public:
  GeoQtDecompressor(int geohash_precision = 11);
  
  // 解压字节数据，返回经纬度坐标序列
  std::vector<std::pair<double, double>> Decompress(const std::vector<uint8_t>& compressed_data);
  
  // 获取解压统计信息
  size_t GetPointCount() const { return point_count_; }

 private:
  const int geohash_precision_;
  size_t point_count_;
  
  // 内部辅助函数
  std::pair<double, double> ReadFirstPoint(const std::vector<uint8_t>& data, size_t& offset);
  std::pair<double, double> ReadDeltaPoint(const std::vector<uint8_t>& data, size_t& offset, uint64_t previous_geohash_int);
};

#endif  // SERF_GEO_QT_DECOMPRESSOR_H
