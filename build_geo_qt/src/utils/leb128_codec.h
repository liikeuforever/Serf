#ifndef SERF_LEB128_CODEC_H
#define SERF_LEB128_CODEC_H

#include <cstdint>
#include <vector>

/**
 * LEB128 (Little Endian Base 128) 编码器
 * 用于高效编码小整数，特别适合 Geo-Qt 算法中的残差编码
 */
class Leb128Codec {
 public:
  // 编码一个64位无符号整数为LEB128格式
  static std::vector<uint8_t> Encode(uint64_t value);
  
  // 编码一个64位无符号整数到字节向量中
  static int EncodeToVector(uint64_t value, std::vector<uint8_t>& output);
  
  // 从字节向量中解码一个LEB128编码的整数
  static uint64_t Decode(const std::vector<uint8_t>& data, size_t& offset);
  
  // 从字节向量中解码一个LEB128编码的整数（不修改偏移量）
  static uint64_t DecodeAt(const std::vector<uint8_t>& data, size_t offset, size_t& new_offset);

 private:
  static const uint8_t kContinuationBit = 0x80;  // 10000000
  static const uint8_t kDataBits = 0x7F;        // 01111111
};

#endif  // SERF_LEB128_CODEC_H
