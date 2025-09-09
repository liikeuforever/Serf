#include "serf_utils_64_fast.h"
#include <algorithm>
#include <cmath>

uint64_t SerfUtils64Fast::FindAppLongFast(double min, double max, double v, uint64_t last_long, 
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

uint64_t SerfUtils64Fast::FindAppLongFastInternal(double min_double, double max_double, uint64_t sign,
                                                  double original, uint64_t last_long, 
                                                  double max_diff, double adjust_digit) {
  // Generate high-probability candidate values
  std::vector<uint64_t> candidates = GenerateCandidates(min_double, max_double, original, 
                                                        last_long, adjust_digit);
  
  // Filter valid candidates
  std::vector<uint64_t> valid_candidates;
  for (uint64_t candidate : candidates) {
    uint64_t signed_candidate = candidate ^ sign;
    if (IsValidCandidate(signed_candidate, min_double, max_double, original, max_diff, adjust_digit)) {
      valid_candidates.push_back(signed_candidate);
    }
  }
  
  if (valid_candidates.empty()) {
    // No valid candidates found, return original value
    return Double::DoubleToLongBits(original + adjust_digit);
  }
  
  // Select the best candidate (most trailing zeros in XOR)
  return SelectBestCandidate(valid_candidates, last_long);
}

std::vector<uint64_t> SerfUtils64Fast::GenerateCandidates(double min, double max, double v, 
                                                          uint64_t last_long, double adjust_digit) {
  std::vector<uint64_t> candidates;
  
  uint64_t min_bits = Double::DoubleToLongBits(min) & 0x7fffffffffffffffULL;
  uint64_t max_bits = Double::DoubleToLongBits(max) & 0x7fffffffffffffffULL;
  uint64_t last_bits = last_long & 0x7fffffffffffffffULL;
  
  // Strategy 1: Boundary values (essential for maintaining range)
  candidates.push_back(min_bits);
  candidates.push_back(max_bits);
  
  // Strategy 2: Original value (fallback option)
  candidates.push_back(Double::DoubleToLongBits(v + adjust_digit) & 0x7fffffffffffffffULL);
  
  // Strategy 3: Simple suffix preservation (focus on trailing zeros)
  // Try to preserve trailing bits from last_long
  for (int preserve_bits = 1; preserve_bits <= 8; preserve_bits++) {
    uint64_t suffix_mask = (1ULL << preserve_bits) - 1;
    uint64_t desired_suffix = last_bits & suffix_mask;
    uint64_t prefix_mask = ~suffix_mask;
    
    // Try min with preserved suffix
    uint64_t candidate1 = (min_bits & prefix_mask) | desired_suffix;
    if (candidate1 >= min_bits && candidate1 <= max_bits) {
      candidates.push_back(candidate1);
    }
    
    // Try max with preserved suffix
    uint64_t candidate2 = (max_bits & prefix_mask) | desired_suffix;
    if (candidate2 >= min_bits && candidate2 <= max_bits) {
      candidates.push_back(candidate2);
    }
    
    // Try middle value with preserved suffix
    uint64_t mid_bits = (min_bits + max_bits) / 2;
    uint64_t candidate3 = (mid_bits & prefix_mask) | desired_suffix;
    if (candidate3 >= min_bits && candidate3 <= max_bits) {
      candidates.push_back(candidate3);
    }
  }
  
  // Remove duplicates
  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
  
  return candidates;
}

uint64_t SerfUtils64Fast::FindBestSuffixMatch(double min, double max, uint64_t last_long, 
                                              double adjust_digit) {
  uint64_t min_bits = Double::DoubleToLongBits(min) & 0x7fffffffffffffffULL;
  uint64_t max_bits = Double::DoubleToLongBits(max) & 0x7fffffffffffffffULL;
  uint64_t last_bits = last_long & 0x7fffffffffffffffULL;
  
  // Find the longest suffix we can preserve
  uint64_t best_match = 0;
  int best_suffix_length = 0;
  
  for (int suffix_bits = 1; suffix_bits <= 16; suffix_bits++) {
    uint64_t suffix_mask = (1ULL << suffix_bits) - 1;
    uint64_t desired_suffix = last_bits & suffix_mask;
    
    // Try to construct a value in range with this suffix
    uint64_t prefix_mask = ~suffix_mask;
    uint64_t min_prefix = min_bits & prefix_mask;
    uint64_t max_prefix = max_bits & prefix_mask;
    
    for (uint64_t prefix = min_prefix; prefix <= max_prefix; prefix += (1ULL << suffix_bits)) {
      uint64_t candidate = prefix | desired_suffix;
      if (candidate >= min_bits && candidate <= max_bits) {
        if (suffix_bits > best_suffix_length) {
          best_suffix_length = suffix_bits;
          best_match = candidate;
        }
        break;  // Found a match for this suffix length
      }
    }
  }
  
  return best_match;
}

bool SerfUtils64Fast::IsValidCandidate(uint64_t candidate, double min, double max, double original,
                                       double max_diff, double adjust_digit) {
  // Check range bounds
  double candidate_double = Double::LongBitsToDouble(candidate);
  if (candidate_double < min || candidate_double > max) {
    return false;
  }
  
  // Check error bounds
  double diff = candidate_double - adjust_digit - original;
  return (diff >= -max_diff && diff <= max_diff);
}

int SerfUtils64Fast::CountTrailingZeros(uint64_t xor_result) {
  if (xor_result == 0) return 64;
  return __builtin_ctzll(xor_result);
}

uint64_t SerfUtils64Fast::SelectBestCandidate(const std::vector<uint64_t>& candidates, uint64_t last_long) {
  if (candidates.empty()) {
    return 0;
  }
  
  uint64_t best_candidate = candidates[0];
  int best_trailing_zeros = CountTrailingZeros(best_candidate ^ last_long);
  
  for (size_t i = 1; i < candidates.size(); i++) {
    int trailing_zeros = CountTrailingZeros(candidates[i] ^ last_long);
    if (trailing_zeros > best_trailing_zeros) {
      best_trailing_zeros = trailing_zeros;
      best_candidate = candidates[i];
    }
  }
  
  return best_candidate;
}
