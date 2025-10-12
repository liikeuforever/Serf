#include "leb128_codec.h"

std::vector<uint8_t> Leb128Codec::Encode(uint64_t value) {
  std::vector<uint8_t> result;
  EncodeToVector(value, result);
  return result;
}

int Leb128Codec::EncodeToVector(uint64_t value, std::vector<uint8_t>& output) {
  int bytes_written = 0;
  
  do {
    uint8_t byte = value & kDataBits;
    value >>= 7;
    
    if (value != 0) {
      byte |= kContinuationBit;
    }
    
    output.push_back(byte);
    bytes_written++;
  } while (value != 0);
  
  return bytes_written;
}

uint64_t Leb128Codec::Decode(const std::vector<uint8_t>& data, size_t& offset) {
  return DecodeAt(data, offset, offset);
}

uint64_t Leb128Codec::DecodeAt(const std::vector<uint8_t>& data, size_t offset, size_t& new_offset) {
  uint64_t result = 0;
  int shift = 0;
  size_t current_offset = offset;
  
  while (current_offset < data.size()) {
    uint8_t byte = data[current_offset++];
    result |= (static_cast<uint64_t>(byte & kDataBits) << shift);
    
    if ((byte & kContinuationBit) == 0) {
      break;
    }
    
    shift += 7;
    
    // 防止溢出
    if (shift >= 64) {
      throw std::overflow_error("LEB128 decode overflow");
    }
  }
  
  new_offset = current_offset;
  return result;
}
