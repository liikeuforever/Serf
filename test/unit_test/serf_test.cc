#include <gtest/gtest.h>
#include <iomanip>

#include "Perf_expr_config.hpp"
#include "Perf_file_utils.hpp"

#include "compressor/serf_xor_compressor.h"
#include "decompressor/serf_xor_decompressor.h"
#include "compressor/serf_qt_compressor.h"
#include "decompressor/serf_qt_decompressor.h"
#include "compressor_32/serf_xor_compressor_32.h"
#include "decompressor_32/serf_xor_decompressor_32.h"
#include "compressor/net_serf_xor_compressor.h"
#include "decompressor/net_serf_xor_decompressor.h"
#include "compressor/net_serf_qt_compressor.h"
#include "decompressor/net_serf_qt_decompressor.h"
#include "compressor_32/serf_qt_compressor_32.h"
#include "decompressor_32/serf_qt_decompressor_32.h"
#include "compressor/serf_xor_compressor_zero_opt.h"
#include "decompressor/serf_xor_decompressor_zero_opt.h"

TEST(Correctness, SerfXOR) {
  for (const auto &data_set : kDataSetList) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    int adjust_digit = kFileNameToAdjustDigit.find(data_set)->second;
    for (const auto &max_diff : kMaxDiffList) {
      SerfXORCompressor xor_compressor(1000, max_diff, adjust_digit);
      SerfXORDecompressor xor_decompressor(adjust_digit);

      std::vector<double> original_data;
      while ((original_data = ReadBlock(data_set_input_stream, kBlockSizeOverall)).size() == kBlockSizeOverall) {
        for (const auto &datum : original_data) {
          xor_compressor.AddValue(datum);
        }
        xor_compressor.Close();
        Array<uint8_t> result = xor_compressor.compressed_bytes_last_block();
        std::vector<double> decompressed = xor_decompressor.Decompress(result);
        ASSERT_EQ(original_data.size(), decompressed.size());
        for (int i = 0; i < kBlockSizeOverall; ++i) {
          ASSERT_NEAR(original_data[i], decompressed[i], max_diff);
        }
      }

      ResetFileStream(data_set_input_stream);
    }

    data_set_input_stream.close();
  }
}

TEST(Correctness, SerfQt) {
  for (const auto &data_set : kDataSetList) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    for (const auto &max_diff : kMaxDiffList) {
      std::vector<double> original_data;
      while ((original_data = ReadBlock(data_set_input_stream, kBlockSizeOverall)).size() == kBlockSizeOverall) {
        SerfQtCompressor qt_compressor(kBlockSizeOverall, max_diff);
        SerfQtDecompressor qt_decompressor;
        for (const auto &datum : original_data) {
          qt_compressor.AddValue(datum);
        }
        qt_compressor.Close();
        Array<uint8_t> result = qt_compressor.compressed_bytes();
        std::vector<double> decompressed = qt_decompressor.Decompress(result);
        ASSERT_EQ(original_data.size(), decompressed.size());
        for (int i = 0; i < kBlockSizeOverall; ++i) {
          ASSERT_NEAR(original_data[i], decompressed[i], max_diff) << data_set << i;
        }
      }

      ResetFileStream(data_set_input_stream);
    }

    data_set_input_stream.close();
  }
}

TEST(Correctness, NetSerfXOR) {
  for (const auto &data_set : kDataSetList) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    int adjust_digit = kFileNameToAdjustDigit.find(data_set)->second;
    for (const auto &max_diff : kMaxDiffList) {
      NetSerfXORCompressor net_serf_xor_compressor(kBlockSizeOverall, max_diff, adjust_digit);
      NetSerfXORDecompressor net_serf_xor_decompressor(kBlockSizeOverall, adjust_digit);

      double originalData;
      while (!data_set_input_stream.eof()) {
        data_set_input_stream >> originalData;
        Array<uint8_t> result = net_serf_xor_compressor.Compress(originalData);
        double decompressed = net_serf_xor_decompressor.Decompress(result);
        if (std::abs(originalData - decompressed) > max_diff) {
          GTEST_LOG_(INFO) << originalData << " " << decompressed << " " << max_diff;
        }
        ASSERT_TRUE(std::abs(originalData - decompressed) <= max_diff);
      }

      ResetFileStream(data_set_input_stream);
    }

    data_set_input_stream.close();
  }
}

TEST(Correctness, TestNetSerfQt) {
  for (const auto &data_set : kDataSetList) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    for (const auto &max_diff : kMaxDiffList) {
      NetSerfQtCompressor net_serf_qt_compressor(max_diff);
      NetSerfQtDecompressor net_serf_qt_decompressor(max_diff);

      double originalData;
      while (!data_set_input_stream.eof()) {
        data_set_input_stream >> originalData;
        Array<uint8_t> result = net_serf_qt_compressor.Compress(originalData);
        double decompressed = net_serf_qt_decompressor.Decompress(result);
        if (std::abs(originalData - decompressed) > max_diff) {
          GTEST_LOG_(INFO) << originalData << " " << decompressed << " " << max_diff;
        }
        ASSERT_TRUE(std::abs(originalData - decompressed) <= max_diff);
      }

      ResetFileStream(data_set_input_stream);
    }

    data_set_input_stream.close();
  }
}

TEST(Correctness, SerfXOR32) {
  for (const auto &data_set : kDataSetList32) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    SerfXORCompressor32 xor_compressor_32(1000, kMaxDiff32);
    SerfXORDecompressor32 xor_decompressor_32;

    std::vector<float> original_data;
    while ((original_data = ReadBlock32(data_set_input_stream, kBlockSize32)).size() == kBlockSize32) {
      for (const auto &datum : original_data) {
        xor_compressor_32.AddValue(datum);
      }
      xor_compressor_32.Close();
      Array<uint8_t> result = xor_compressor_32.compressed_bytes_last_block();
      std::vector<float> decompressed = xor_decompressor_32.Decompress(result);
      EXPECT_EQ(original_data.size(), decompressed.size());
      for (int i = 0; i < kBlockSize32; ++i) {
        if (std::abs(original_data[i] - decompressed[i]) > kMaxDiff32) {
          GTEST_LOG_(INFO) << original_data[i] << " " << decompressed[i] << " " << kMaxDiff32;
        }
        ASSERT_TRUE(std::abs(original_data[i] - decompressed[i]) <= kMaxDiff32);
      }
    }

    ResetFileStream(data_set_input_stream);

    data_set_input_stream.close();
  }
}

TEST(Correctness, SerfQt32) {
  for (const auto &data_set : kDataSetList32) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    SerfQtCompressor32 qt_compressor_32(kBlockSize32, kMaxDiff32);
    SerfQtDecompressor32 qt_decompressor_32;

    std::vector<float> original_data;
    while ((original_data = ReadBlock32(data_set_input_stream, kBlockSize32)).size() == kBlockSize32) {
      for (const auto &datum : original_data) {
        qt_compressor_32.AddValue(datum);
      }
      qt_compressor_32.Close();
      Array<uint8_t> result = qt_compressor_32.compressed_bytes();
      std::vector<float> decompressed = qt_decompressor_32.Decompress(result);
      EXPECT_EQ(original_data.size(), decompressed.size());
      for (int i = 0; i < kBlockSize32; ++i) {
        if (std::abs(original_data[i] - decompressed[i]) > kMaxDiff32) {
          GTEST_LOG_(INFO) << original_data[i] << " " << decompressed[i] << " " << kMaxDiff32;
        }
        ASSERT_TRUE(std::abs(original_data[i] - decompressed[i]) <= kMaxDiff32);
      }
    }

    ResetFileStream(data_set_input_stream);

    data_set_input_stream.close();
  }
}
TEST(Correctness, SerfXORZeroOpt) {
  for (const auto &data_set : kDataSetList) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    int adjust_digit = kFileNameToAdjustDigit.find(data_set)->second;
    for (const auto &max_diff : kMaxDiffList) {
      SerfXORCompressorZeroOpt xor_compressor_opt(1000, max_diff, adjust_digit);
      SerfXORDecompressorZeroOpt xor_decompressor_opt(adjust_digit);

      std::vector<double> original_data;
      while ((original_data = ReadBlock(data_set_input_stream, kBlockSizeOverall)).size() == kBlockSizeOverall) {
        for (const auto &datum : original_data) {
          xor_compressor_opt.AddValue(datum);
        }
        xor_compressor_opt.Close();
        Array<uint8_t> result = xor_compressor_opt.compressed_bytes_last_block();
        std::vector<double> decompressed = xor_decompressor_opt.Decompress(result);
        ASSERT_EQ(original_data.size(), decompressed.size());
        for (int i = 0; i < kBlockSizeOverall; ++i) {
          ASSERT_NEAR(original_data[i], decompressed[i], max_diff);
        }
      }

      ResetFileStream(data_set_input_stream);
    }

    data_set_input_stream.close();
  }
}

TEST(Performance, SerfXORvsZeroOpt) {
  std::cout << "\n=== Serf-XOR vs Zero-Optimized Compression Comparison ===" << std::endl;
  std::cout << std::setw(20) << "Dataset" 
            << std::setw(12) << "MaxDiff"
            << std::setw(15) << "Original(bits)"
            << std::setw(15) << "Optimized(bits)"
            << std::setw(12) << "Improvement"
            << std::setw(10) << "Ratio" << std::endl;
  std::cout << std::string(84, '-') << std::endl;

  for (const auto &data_set : kDataSetList) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
      continue;
    }

    int adjust_digit = kFileNameToAdjustDigit.find(data_set)->second;
    for (const auto &max_diff : kMaxDiffList) {
      long total_original_bits = 0;
      long total_optimized_bits = 0;
      int block_count = 0;

      std::vector<double> original_data;
      while ((original_data = ReadBlock(data_set_input_stream, kBlockSizeOverall)).size() == kBlockSizeOverall) {
        // Test original compressor
        SerfXORCompressor xor_compressor(1000, max_diff, adjust_digit);
        SerfXORDecompressor xor_decompressor(adjust_digit);
        
        for (const auto &datum : original_data) {
          xor_compressor.AddValue(datum);
        }
        xor_compressor.Close();
        Array<uint8_t> original_result = xor_compressor.compressed_bytes_last_block();
        long original_bits = xor_compressor.compressed_size_last_block();
        
        // Verify original correctness
        std::vector<double> original_decompressed = xor_decompressor.Decompress(original_result);
        ASSERT_EQ(original_data.size(), original_decompressed.size());
        for (int i = 0; i < kBlockSizeOverall; ++i) {
          ASSERT_NEAR(original_data[i], original_decompressed[i], max_diff);
        }

        // Test optimized compressor
        SerfXORCompressorZeroOpt xor_compressor_opt(1000, max_diff, adjust_digit);
        SerfXORDecompressorZeroOpt xor_decompressor_opt(adjust_digit);
        
        for (const auto &datum : original_data) {
          xor_compressor_opt.AddValue(datum);
        }
        xor_compressor_opt.Close();
        Array<uint8_t> optimized_result = xor_compressor_opt.compressed_bytes_last_block();
        long optimized_bits = xor_compressor_opt.compressed_size_last_block();
        
        // Verify optimized correctness
        std::vector<double> optimized_decompressed = xor_decompressor_opt.Decompress(optimized_result);
        ASSERT_EQ(original_data.size(), optimized_decompressed.size());
        for (int i = 0; i < kBlockSizeOverall; ++i) {
          ASSERT_NEAR(original_data[i], optimized_decompressed[i], max_diff);
        }

        total_original_bits += original_bits;
        total_optimized_bits += optimized_bits;
        block_count++;
      }

      if (block_count > 0) {
        double avg_original = (double)total_original_bits / block_count;
        double avg_optimized = (double)total_optimized_bits / block_count;
        double improvement = avg_original - avg_optimized;
        double ratio = avg_optimized / avg_original;
        
        std::cout << std::setw(20) << data_set.substr(0, 18)
                  << std::setw(12) << max_diff
                  << std::setw(15) << std::fixed << std::setprecision(1) << avg_original
                  << std::setw(15) << std::fixed << std::setprecision(1) << avg_optimized
                  << std::setw(12) << std::fixed << std::setprecision(1) << improvement
                  << std::setw(10) << std::fixed << std::setprecision(3) << ratio
                  << std::endl;
      }

      ResetFileStream(data_set_input_stream);
    }

    data_set_input_stream.close();
  }
  
  std::cout << std::string(84, '-') << std::endl;
  std::cout << "Note: Improvement = Original - Optimized (positive means optimization saves bits)" << std::endl;
  std::cout << "      Ratio = Optimized / Original (< 1.0 means optimization is better)" << std::endl;
}
