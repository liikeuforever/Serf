#include "serf_xor_decompressor_zero_opt.h"
#include "utils/elias_gamma_codec.h"
#include "utils/post_office_solver.h"

std::vector<double> SerfXORDecompressorZeroOpt::Decompress(const Array<uint8_t> &bs) {
  input_bit_stream_.SetBuffer(bs);
  UpdatePositionsIfNeeded();
  std::vector<double> values; values.reserve(1000);
  uint64_t value;
  while (SERF_LIKELY((value = ReadValue()) != Double::DoubleToLongBits(Double::kNan))) {
    values.emplace_back(Double::LongBitsToDouble(value) - static_cast<double>(adjust_digit_));
    stored_val_ = value;
  }
  return values;
}

uint64_t SerfXORDecompressorZeroOpt::ReadValue() {
  // If we have remaining zeros from a previous zero sequence, return the stored value
  if (remaining_zeros_ > 0) {
    remaining_zeros_--;
    return stored_val_;
  }
  
  uint64_t value = stored_val_;
  int center_bits;
  
  // Read first bit to determine the state
  int first_bit = input_bit_stream_.ReadInt(1);
  
  if (first_bit == 1) {
    // Read second bit to distinguish between '10' (case 1) and '11' (zero sequence)
    int second_bit = input_bit_stream_.ReadInt(1);
    
    if (second_bit == 0) {
      // case '10' - reuse previous leading/trailing zeros (original case 1)
      center_bits = 64 - stored_leading_zeros_ - stored_trailing_zeros_;
      value = input_bit_stream_.ReadLong(center_bits) << stored_trailing_zeros_;
      value = stored_val_ ^ value;
    } else {
      // case '11' - zero sequence
      int64_t zero_count = EliasGammaCodec::Decode(&input_bit_stream_);
      // Set remaining zeros count (minus 1 because we'll return one value now)
      remaining_zeros_ = static_cast<int>(zero_count - 1);
      value = stored_val_; // Return same value (zero XOR)
    }
  } else {
    // First bit is 0, read second bit
    int second_bit = input_bit_stream_.ReadInt(1);
    
    if (second_bit == 0) {
      // case '00' - new leading/trailing zeros
      int lead_and_trail =
          static_cast<int>(input_bit_stream_.ReadInt(leading_bits_per_value_ + trailing_bits_per_value_));
      int lead = lead_and_trail >> trailing_bits_per_value_;
      int trail = ~(0xffffffff << trailing_bits_per_value_) & lead_and_trail;
      stored_leading_zeros_ = leading_representation_[lead];
      stored_trailing_zeros_ = trailing_representation_[trail];
      center_bits = 64 - stored_leading_zeros_ - stored_trailing_zeros_;

      value = input_bit_stream_.ReadLong(center_bits) << stored_trailing_zeros_;
      value = stored_val_ ^ value;
    } else {
      // case '01' - original zero case (should not happen in optimized version)
      // This was the old way to encode zero XOR, now handled by '11' + count
      value = stored_val_; // Zero XOR result
    }
  }
  return value;
}

void SerfXORDecompressorZeroOpt::UpdatePositionsIfNeeded() {
  if (SERF_UNLIKELY(input_bit_stream_.ReadBit())) {
    UpdateLeadingRepresentation();
    UpdateTrailingRepresentation();
  }
}

void SerfXORDecompressorZeroOpt::UpdateLeadingRepresentation() {
  int num = static_cast<int>(input_bit_stream_.ReadInt(5));
  num = branch_less_table[num];
  leading_bits_per_value_ = PostOfficeSolver::kPositionLength2Bits[num];
  leading_representation_ = Array<int>(num);
  for (int i = 0; i < num; i++) {
    leading_representation_[i] = static_cast<int>(input_bit_stream_.ReadInt(6));
  }
}

void SerfXORDecompressorZeroOpt::UpdateTrailingRepresentation() {
  int num = static_cast<int>(input_bit_stream_.ReadInt(5));
  num = branch_less_table[num];
  trailing_bits_per_value_ = PostOfficeSolver::kPositionLength2Bits[num];
  trailing_representation_ = Array<int>(num);
  for (int i = 0; i < num; i++) {
    trailing_representation_[i] = static_cast<int>(input_bit_stream_.ReadInt(6));
  }
}
