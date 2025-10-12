#ifndef SERF_GEO_QT_COMPRESSOR_H
#define SERF_GEO_QT_COMPRESSOR_H

#include <cstdint>
#include <vector>

#include "utils/geohash_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/leb128_codec.h"

/**
 * Geo-Qt 压缩器
 * 借鉴 Serf-Qt 思想，针对 Geohash 编码的轨迹数据进行流式压缩
 * 
 * 算法流程：
 * 1. 将经纬度转换为11位Geohash整数
 * 2. 计算与前一个点的残差
 * 3. 使用Zigzag编码处理负数
 * 4. 使用LEB128变长编码压缩残差
 */
class GeoQtCompressor {
 public:
  GeoQtCompressor(int geohash_precision = 11);
  
  // 添加一个经纬度点进行压缩
  void AddPoint(double latitude, double longitude);
  
  // 完成压缩，返回压缩后的字节数据
  std::vector<uint8_t> Finish();
  
  // 获取压缩统计信息
  size_t GetCompressedSize() const { return compressed_data_.size(); }
  size_t GetPointCount() const { return point_count_; }
  
  // 重置压缩器状态
  void Reset();

 private:
  const int geohash_precision_;
  bool first_point_;
  uint64_t previous_geohash_int_;
  std::vector<uint8_t> compressed_data_;
  size_t point_count_;
  
  // 内部辅助函数
  void WriteFirstPoint(uint64_t geohash_int);
  void WriteDeltaPoint(uint64_t geohash_int);
};

#endif  // SERF_GEO_QT_COMPRESSOR_H
