# Serf-XOR Optimizations for High Sampling Rate Edge Sensors

This document describes the optimizations implemented for the Serf-XOR algorithm, specifically designed for high sampling rate edge sensors with the following characteristics:
- High sampling rate
- Non-uniform data distribution
- Low precision requirements

## Files Created

### Compressor
- `src/compressor/serf_xor_compressor_optimized.h`
- `src/compressor/serf_xor_compressor_optimized.cc`

### Decompressor
- `src/decompressor/serf_xor_decompressor_optimized.h`
- `src/decompressor/serf_xor_decompressor_optimized.cc`

### Example
- `examples/optimized_serf_example.cc`

## Optimizations Implemented

### 1. Dynamic Adjust Digit (Shifter Optimization)

**Problem**: The original Serf-XOR uses a static `adjust_digit` (λ) parameter, which doesn't adapt to changing data characteristics over time.

**Solution**: Implemented exponential weighted moving average (EWMA) to dynamically update the adjust_digit parameter.

```cpp
// New average = α * new_value + (1-α) * old_average
ewma_min_ = kAlpha * value + (1.0 - kAlpha) * ewma_min_;
ewma_max_ = kAlpha * value + (1.0 - kAlpha) * ewma_max_;
adjust_digit_ = (ewma_min_ + ewma_max_) / 2.0;
```

**Benefits**:
- **Compression Time**: Reduces overhead of recalculating min/max values over windows
- **Compression Ratio**: Adapts to changing data characteristics, maintaining effective offset
- **Memory**: Only stores a few floating-point variables

### 2. Fast Approximator Search (Approximator Optimization)

**Problem**: The original approximator module searches the entire error range `[s_i - ε, s_i + ε]` to find the optimal value that maximizes trailing zeros when XORed with the previous value.

**Solution**: Instead of searching the entire range, test only high-probability candidates:

1. **Boundary values**: `s_i - ε` and `s_i + ε`
2. **Adjusted value**: `s_i + adjust_digit`
3. **Suffix candidate**: Value in range that shares maximum trailing bits with previous value

```cpp
uint64_t candidates[] = {candidate1, candidate2, candidate3, suffix_candidate};
// Test all candidates and choose the one with maximum trailing zeros
```

**Benefits**:
- **Compression Time**: Fixed computation instead of potentially long loops
- **Compression Ratio**: Slight risk of suboptimal values in edge cases, but generally maintains quality

### 3. Run-Length Encoding for Zero XOR Results (Compression Optimization)

**Problem**: At high sampling rates, consecutive sensor readings are often identical, resulting in XOR values of zero. Standard Serf-XOR uses 2 bits ('01') to encode each zero, which is inefficient for long runs.

**Solution**: Implement run-length encoding with Elias Gamma encoding for zero runs:

1. When XOR result is zero, increment a counter instead of writing '01'
2. When the run ends, write:
   - Special flag '11' to indicate zero run start
   - Run length using Elias Gamma encoding
   - Continue with standard encoding for the non-zero value

```cpp
if (xor_result == 0) {
    zero_run_count_++;  // Accumulate zeros
} else {
    // Flush accumulated run: '11' + Elias_Gamma(count) + encode_non_zero_value
    FlushZeroRun();
}
```

**Benefits**:
- **Compression Time**: Minimal overhead (incrementing counter vs. writing bits)
- **Compression Ratio**: Dramatic improvement for high sampling rates
  - Example: 100 identical values: 200 bits → ~9 bits (2 + ~7 for Elias Gamma encoding of 100)
- **Memory**: Only one additional counter variable

## Usage

### Basic Usage

```cpp
#include "src/compressor/serf_xor_compressor_optimized.h"
#include "src/decompressor/serf_xor_decompressor_optimized.h"

// Create optimized compressor
int window_size = 1000;
double max_diff = 0.1;
long initial_adjust_digit = 100;
double alpha = 0.1;  // EWMA smoothing factor

SerfXORCompressorOptimized compressor(window_size, max_diff, initial_adjust_digit, alpha);

// Compress data
for (double value : sensor_data) {
    compressor.AddValue(value);
}
compressor.Close();

// Get compressed data
Array<uint8_t> compressed_data = compressor.compressed_bytes_last_block();

// Decompress
SerfXORDecompressorOptimized decompressor(initial_adjust_digit, alpha);
std::vector<double> decompressed_data = decompressor.Decompress(compressed_data);
```

### Parameters

- `window_size`: Number of values per compression window
- `max_diff`: Maximum allowed error tolerance
- `initial_adjust_digit`: Initial value for the dynamic adjust digit
- `alpha`: Exponential smoothing factor (0 < alpha < 1, typically 0.1)
  - Smaller alpha: Slower adaptation, more stable
  - Larger alpha: Faster adaptation, more responsive to changes

## Performance Characteristics

### Ideal Use Cases
- High-frequency sensor data (IoT devices, environmental sensors)
- Time series with many consecutive identical or near-identical values
- Edge computing scenarios with limited computational resources
- Applications where slight precision loss is acceptable for better compression

### Expected Improvements
- **Compression Time**: 20-50% reduction for high-frequency data
- **Compression Ratio**: 2-10x improvement for data with many consecutive identical values
- **Memory Usage**: Minimal increase (few additional variables)
- **Adaptability**: Dynamic adjustment to changing data patterns

## Limitations

1. **Approximator Search**: May occasionally select suboptimal values, potentially reducing compression ratio in edge cases
2. **Zero Run Encoding**: Adds complexity to the bit stream format
3. **Parameter Tuning**: The alpha parameter may need tuning for different data characteristics
4. **Decompression Complexity**: Run-length decoding adds some complexity to decompression

## Compatibility

The optimized version maintains the same interface as the original Serf-XOR algorithm, with additional optional parameters for the optimizations. The compressed data format is different due to the run-length encoding optimization, so compressed data from the optimized version cannot be decompressed by the original decompressor and vice versa.
