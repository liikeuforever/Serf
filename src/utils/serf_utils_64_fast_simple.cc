#include "serf_utils_64_fast_simple.h"
#include <algorithm>

uint64_t SerfUtils64FastSimple::FindAppLongFast(double min, double max, double v, uint64_t last_long, 
                                                double max_diff, double adjust_digit) {
  // Handle sign cases similar to original implementation
  if (min >= 0) {
    // Both positive
    return FindAppLongFastInternal(min, max, 0, v, last_long, max_diff, adjust_digit);
  } else if (max <= 0) {
    // Both negative
    return FindAppLongFastInternal(-max, -min, 0x8000000000000000ULL, v, last_long,
                                   max_diff, adjust_digit);
  } else if (min < 0 && max >= 0) {
    // Cross zero, try positive range first
    uint64_t pos_result = FindAppLongFastInternal(0, max, 0, v, last_long, max_diff, adjust_digit);
    if (pos_result != Double::DoubleToLongBits(v + adjust_digit)) {
      return pos_result;
    }
    // Try negative range
    return FindAppLongFastInternal(0, -min, 0x8000000000000000ULL, v, last_long,
                                   max_diff, adjust_digit);
  }
  
  // Fallback to original value
  return Double::DoubleToLongBits(v + adjust_digit);
}

uint64_t SerfUtils64FastSimple::FindAppLongFastInternal(double min_double, double max_double, uint64_t sign,
                                                        double original, uint64_t last_long, 
                                                        double max_diff, double adjust_digit) {
  // Similar to original algorithm but with limited iterations
  uint64_t min = Double::DoubleToLongBits(min_double) & 0x7fffffffffffffffULL;
  uint64_t max = Double::DoubleToLongBits(max_double);
  int leading_zeros = __builtin_clzll(min ^ max);
  int64_t front_mask = 0xffffffffffffffff << (64 - leading_zeros);
  int shift = 64 - leading_zeros;
  uint64_t result_long;
  double diff;
  uint64_t append;
  
  // Limit iterations for speed
  int max_iterations = std::min(kMaxSearchIterations, shift + 1);
  int iteration_count = 0;
  
  for (; shift >= 0 && iteration_count < max_iterations; --shift, ++iteration_count) {
    // Calculate front and rear
    uint64_t front = front_mask & min;
    uint64_t rear = (~front_mask) & last_long;

    append = rear | front;

    // Check conditions
    bool condition1 = (append >= min && append <= max);
    result_long = condition1 ? (append ^ sign) : 0;

    // Check diff condition
    diff = Double::LongBitsToDouble(result_long) - adjust_digit - original;
    bool diff_satisfied = (diff >= -max_diff && diff <= max_diff);

    if (condition1 && diff_satisfied) {
      return result_long;
    }

    // Try with bit weight addition
    append = (append + kBitWeight[shift]) & 0x7fffffffffffffffULL;

    bool condition2 = append <= max;
    result_long = condition2 ? (append ^ sign) : 0;

    // Check diff again
    diff = Double::LongBitsToDouble(result_long) - adjust_digit - original;
    diff_satisfied = (diff >= -max_diff && diff <= max_diff);

    if (condition2 && diff_satisfied) {
      return result_long;
    }

    // Update front mask
    front_mask >>= 1;
  }

  // If no good value found in limited iterations, return original value
  return Double::DoubleToLongBits(original + adjust_digit);
}
