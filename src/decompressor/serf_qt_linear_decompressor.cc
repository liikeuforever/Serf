#include "decompressor/serf_qt_linear_decompressor.h"
#include "utils/double.h"
#include "utils/zig_zag_codec.h"
#include "utils/elias_gamma_codec.h"

std::vector<double> SerfQtLinearDecompressor::Decompress(const Array<uint8_t> &bs) {
  input_bit_stream_->SetBuffer(bs);
  block_size_ = input_bit_stream_->ReadInt(16);
  max_diff_ = Double::LongBitsToDouble(input_bit_stream_->ReadLong(64));
  
  // Reset state
  prev_value1_ = 2;
  prev_value2_ = 2;
  first_ = true;
  second_ = true;
  
  std::vector<double> decompressed_value_list;
  decompressed_value_list.reserve(block_size_);
  
  while (block_size_--) {
    decompressed_value_list.emplace_back(NextValue());
  }
  
  return decompressed_value_list;
}

double SerfQtLinearDecompressor::LinearPredict() const {
  if (first_) {
    return 2.0;  // default value for first point
  } else if (second_) {
    return prev_value1_;  // use previous value for second point
  } else {
    // Linear prediction: predicted = 2 * prev1 - prev2
    return 2.0 * prev_value1_ - prev_value2_;
  }
}

double SerfQtLinearDecompressor::NextValue() {
  int64_t decodeValue = ZigZagCodec::Decode(EliasGammaCodec::Decode(input_bit_stream_.get()) - 1);
  
  double predicted;
  if (first_) {
    first_ = false;
    predicted = 2.0;  // default prediction for first value
    double recoverValue = predicted + 2 * max_diff_ * static_cast<double>(decodeValue);
    prev_value1_ = recoverValue;
    return recoverValue;
  } else if (second_) {
    second_ = false;
    predicted = prev_value1_;  // use previous value as prediction for second value
    double recoverValue = predicted + 2 * max_diff_ * static_cast<double>(decodeValue);
    prev_value2_ = prev_value1_;
    prev_value1_ = recoverValue;
    return recoverValue;
  } else {
    // Linear prediction for third value onwards
    predicted = LinearPredict();
    double recoverValue = predicted + 2 * max_diff_ * static_cast<double>(decodeValue);
    
    // Update history
    prev_value2_ = prev_value1_;
    prev_value1_ = recoverValue;
    return recoverValue;
  }
}
