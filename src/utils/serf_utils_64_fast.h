#ifndef SERF_64_UTILS_FAST_H
#define SERF_64_UTILS_FAST_H

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
#include <vector>

#include "double.h"

/**
 * Fast Approximator Search Utilities for Serf-XOR
 * 
 * This optimized version reduces compression time by testing only 
 * high-probability candidate values instead of exhaustive search:
 * 
 * 1. Boundary values: s_i - ε and s_i + ε
 * 2. Suffix sharing: Values that share longest suffix with previous approximation
 * 3. Original value: As fallback
 */
class SerfUtils64Fast {
 public:
  static uint64_t FindAppLongFast(double min, double max, double v, uint64_t last_long, 
                                  double max_diff, double adjust_digit);

 private:
  // Internal implementation for fast search
  static uint64_t FindAppLongFastInternal(double min_double, double max_double, uint64_t sign,
                                          double original, uint64_t last_long, 
                                          double max_diff, double adjust_digit);
  
  // Generate high-probability candidate values
  static std::vector<uint64_t> GenerateCandidates(double min, double max, double v, 
                                                  uint64_t last_long, double adjust_digit);
  
  // Find value that shares longest suffix (trailing bits) with last_long
  static uint64_t FindBestSuffixMatch(double min, double max, uint64_t last_long, double adjust_digit);
  
  // Check if a candidate value is valid (within error bounds and range)
  static bool IsValidCandidate(uint64_t candidate, double min, double max, double original,
                              double max_diff, double adjust_digit);
  
  // Count trailing zeros in XOR result (for compression efficiency)
  static int CountTrailingZeros(uint64_t xor_result);
  
  // Select best candidate based on trailing zeros count
  static uint64_t SelectBestCandidate(const std::vector<uint64_t>& candidates, uint64_t last_long);
};

#endif  // SERF_64_UTILS_FAST_H
