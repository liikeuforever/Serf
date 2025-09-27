#include "compressor/serf_qt_linear_compressor.h"
#include <cmath>
#include "utils/double.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"

SerfQtLinearCompressor::SerfQtLinearCompressor(int block_size, double max_diff) 
    : kBlockSize(block_size), kMaxDiff(max_diff * 0.999) {
  output_bit_stream_ = std::make_unique<OutputBitStream>(2 * block_size * 8);
}

double SerfQtLinearCompressor::LinearPredict() const {
  if (first_) {
    return 2.0;  // default value for first point
  } else if (second_) {
    return prev_value1_;  // use previous value for second point
  } else {
    // Linear prediction: predicted = 2 * prev1 - prev2
    return 2.0 * prev_value1_ - prev_value2_;
  }
}

void SerfQtLinearCompressor::AddValue(double v) {
  if (first_) {
    first_ = false;
    compressed_size_in_bits_ += output_bit_stream_->WriteInt(kBlockSize, 16);
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(kMaxDiff), 64);
    // Store first value directly
    prev_value1_ = v;
    // For first value, quantization is based on difference from default (2.0)
    long q = static_cast<long>(std::round((v - 2.0) / (2 * kMaxDiff)));
    double recoverValue = 2.0 + 2 * kMaxDiff * static_cast<double>(q);
    compressed_size_in_bits_ += EliasGammaCodec::Encode(ZigZagCodec::Encode(static_cast<int64_t>(q)) + 1,
                                                        output_bit_stream_.get());
    prev_value1_ = recoverValue;
    return;
  }
  
  if (second_) {
    second_ = false;
    // For second value, use previous value as prediction
    long q = static_cast<long>(std::round((v - prev_value1_) / (2 * kMaxDiff)));
    double recoverValue = prev_value1_ + 2 * kMaxDiff * static_cast<double>(q);
    compressed_size_in_bits_ += EliasGammaCodec::Encode(ZigZagCodec::Encode(static_cast<int64_t>(q)) + 1,
                                                        output_bit_stream_.get());
    prev_value2_ = prev_value1_;
    prev_value1_ = recoverValue;
    return;
  }
  
  // For third value onwards, use linear prediction
  double predicted = LinearPredict();
  long q = static_cast<long>(std::round((v - predicted) / (2 * kMaxDiff)));
  double recoverValue = predicted + 2 * kMaxDiff * static_cast<double>(q);
  compressed_size_in_bits_ += EliasGammaCodec::Encode(ZigZagCodec::Encode(static_cast<int64_t>(q)) + 1,
                                                      output_bit_stream_.get());
  
  // Update history
  prev_value2_ = prev_value1_;
  prev_value1_ = recoverValue;
}

Array<uint8_t> SerfQtLinearCompressor::compressed_bytes() {
  return compressed_bytes_;
}

void SerfQtLinearCompressor::Close() {
  output_bit_stream_->Flush();
  compressed_bytes_ = output_bit_stream_->GetBuffer(std::ceil(compressed_size_in_bits_ / 8.0));
  output_bit_stream_->Refresh();
  first_ = true;
  second_ = true;
  prev_value1_ = 2;
  prev_value2_ = 2;
  stored_compressed_size_in_bits_ = compressed_size_in_bits_;
  compressed_size_in_bits_ = 0;
}

long SerfQtLinearCompressor::get_compressed_size_in_bits() const {
  return stored_compressed_size_in_bits_;
}
