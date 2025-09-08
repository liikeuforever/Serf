#ifndef SERF_XOR_COMPRESSOR_ZERO_OPT_H_
#define SERF_XOR_COMPRESSOR_ZERO_OPT_H_

/*
 * Give hints to the compiler for branch prediction optimization.
 */
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 2))
#define SERF_LIKELY(c) (__builtin_expect(!!(c), 1))
#define SERF_UNLIKELY(c) (__builtin_expect(!!(c), 0))
#else
#define SERF_LIKELY(c) (c)
#define SERF_UNLIKELY(c) (c)
#endif

#include <cstdint>
#include <cmath>
#include <memory>

#include "utils/double.h"
#include "utils/output_bit_stream.h"
#include "utils/serf_utils_64.h"
#include "utils/array.h"

/**
 * Optimized Serf XOR Compressor with Zero Sequence Optimization
 * 
 * This version introduces a fourth state '11' to handle sequences of identical values
 * more efficiently using Elias Gamma encoding for the count.
 * 
 * Four states:
 * - '11' + Elias Gamma count: Zero sequence (new optimization)
 * - '10': Use previous leading/trailing zeros (original case 1)
 * - '01': Single zero XOR (legacy, now rarely used)
 * - '00': New leading/trailing zeros
 */
class SerfXORCompressorZeroOpt {
 public:
  SerfXORCompressorZeroOpt(int windows_size, double max_diff, long adjust_digit);

  void AddValue(double v);

  long compressed_size_last_block() const;

  Array<uint8_t> compressed_bytes_last_block();

  Array<uint8_t> &compressed_bytes();

  void Close();

 private:
  const double kMaxDiff;
  const long kAdjustDigit;
  const int kWindowSize;
  uint64_t stored_val_ = Double::DoubleToLongBits(2);

  std::unique_ptr<OutputBitStream> output_buffer_;
  Array<uint8_t> compressed_bytes_last_block_ = Array<uint8_t>();

  long compressed_size_this_block_;
  long compressed_size_last_block_ = 0;
  long compressed_size_this_window_ = 0;
  int number_of_values_this_window_ = 0;
  double compression_ratio_last_window_ = 0;

  Array<int> leading_representation_ = {
      0, 0, 0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 2, 2, 2, 2,
      3, 3, 4, 4, 5, 5, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7
  };
  Array<int> leading_round_ = {
      0, 0, 0, 0, 0, 0, 0, 0,
      8, 8, 8, 8, 12, 12, 12, 12,
      16, 16, 18, 18, 20, 20, 22, 22,
      24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 24, 24, 24
  };
  Array<int> trailing_representation_ = {
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 1, 1,
      1, 1, 1, 1, 2, 2, 2, 2,
      3, 3, 3, 3, 4, 4, 4, 4,
      5, 5, 6, 6, 6, 6, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7,
  };
  Array<int> trailing_round_ = {
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 22, 22,
      22, 22, 22, 22, 28, 28, 28, 28,
      32, 32, 32, 32, 36, 36, 36, 36,
      40, 40, 42, 42, 42, 42, 46, 46,
      46, 46, 46, 46, 46, 46, 46, 46,
      46, 46, 46, 46, 46, 46, 46, 46,
  };

  int leading_bits_per_value_ = 3;
  int trailing_bits_per_value_ = 3;
  Array<int> lead_distribution_ = Array<int>(64);
  Array<int> trail_distribution_ = Array<int>(64);
  int stored_leading_zeros_ = std::numeric_limits<int>::max();
  int stored_trailing_zeros_ = std::numeric_limits<int>::max();
  
  // Zero sequence optimization
  int zero_sequence_count_ = 0;
  bool in_zero_sequence_ = false;

  int CompressValue(uint64_t value);
  int UpdatePositionsIfNeeded();
  int FlushZeroSequence();
};

#endif // SERF_XOR_COMPRESSOR_ZERO_OPT_H_
