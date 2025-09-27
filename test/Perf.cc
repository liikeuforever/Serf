#include <gtest/gtest.h>
#include <functional>

#include "Perf_baseline_inc.hpp"
#include "Perf_expr_config.hpp"
#include "Perf_file_utils.hpp"
#include "Perf_expr_data_struct.hpp"

void ExportTotalExprTable(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "total" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  // Write header
  expr_table_output_stream
      << "Method,DataSet,BlockSize,MaxDiff,CompressionRatio,CompressionTime(AvgPerBlock),DecompressionTime(AvgPerBlock)"
      << std::endl;
  // Write record
  for (const auto &conf_record : expr_table) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    expr_table_output_stream << conf.method() << "," << conf.data_set() << "," << conf.block_size() << ","
                             << conf.max_diff() << "," << record.CalCompressionRatio(conf) << ","
                             << record.AvgCompressionTimePerBlock() << ","
                             << record.AvgDecompressionTimePerBlock() << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

// Auto-Gen for the Overall Experiment

void GenOverallTableCR(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "overall_cr" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListOverall) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeOverall, kMaxDiffOverall);
      expr_table_output_stream << expr_table.find(this_conf)->second.CalCompressionRatio(this_conf) << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenOverallTableCT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "overall_ct" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListOverall) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeOverall, kMaxDiffOverall);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgCompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenOverallTableDT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "overall_dt" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListOverall) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeOverall, kMaxDiffOverall);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgDecompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

// Auto-Gen for the SinglePrecision Experiment

void GenSinglePrecisionTableCR(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "single_precision_cr" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodList32) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList32) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSize32, kMaxDiff32, true);
      expr_table_output_stream << expr_table.find(this_conf)->second.CalCompressionRatio(this_conf) << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenSinglePrecisionTableCT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "single_precision_ct" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodList32) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList32) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSize32, kMaxDiff32, true);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgCompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenSinglePrecisionTableDT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "single_precision_dt" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodList32) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList32) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSize32, kMaxDiff32, true);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgDecompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

// Auto-Gen for the TSBS Experiment

void GenTSBSTableCR(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "tsbs_cr" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListTSBS) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetListTSBS) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeTSBS, kMaxDiffTSBS);
      expr_table_output_stream << expr_table.find(this_conf)->second.CalCompressionRatio(this_conf) << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenTSBSTableCT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "tsbs_ct" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListTSBS) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetListTSBS) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeTSBS, kMaxDiffTSBS);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgCompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenTSBSTableDT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "tsbs_dt" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListTSBS) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetListTSBS) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeTSBS, kMaxDiffTSBS);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgDecompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

// Auto-Gen for the Param(Abs MaxDiff) Experiment

void GenParamAbsDiffTable(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "param_abs_diff_results" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &max_diff : kMaxDiffList) {
    expr_table_output_stream << max_diff << std::endl;
    expr_table_output_stream << "Compression Ratio" << std::endl;
    for (const auto &method : kMethodListParamAbsMaxDiff) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, kBlockSizeParamAbsMaxDiff, max_diff);
        auto result = expr_table.find(this_conf);
        if (result != expr_table.end()) {
          expr_table_output_stream << result->second.CalCompressionRatio(this_conf) << ",";
        }
      }
      expr_table_output_stream << std::endl;
    }
    expr_table_output_stream << "Compression Time" << std::endl;
    for (const auto &method : kMethodListParamAbsMaxDiff) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, kBlockSizeParamAbsMaxDiff, max_diff);
        auto result = expr_table.find(this_conf);
        if (result != expr_table.end()) {
          expr_table_output_stream << result->second.AvgCompressionTimePerBlock() << ",";
        }
      }
      expr_table_output_stream << std::endl;
    }
    expr_table_output_stream << "Decompression Time" << std::endl;
    for (const auto &method : kMethodListParamAbsMaxDiff) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, kBlockSizeParamAbsMaxDiff, max_diff);
        auto result = expr_table.find(this_conf);
        if (result != expr_table.end()) {
          expr_table_output_stream << result->second.AvgDecompressionTimePerBlock() << ",";
        }
      }
      expr_table_output_stream << std::endl;
    }
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

// Auto-Gen for the Param(Block Size) Experiment

void GenParamBlockSizeTable(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "param_block_size_results" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &block_size : kBlockSizeList) {
    expr_table_output_stream << block_size << std::endl;
    expr_table_output_stream << "Compression Ratio" << std::endl;
    for (const auto &method : kMethodListParamBlockSize) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, block_size, kAbsMaxDiffParamBlockSize);
        auto result = expr_table.find(this_conf);
        if (result != expr_table.end()) {
          expr_table_output_stream << result->second.CalCompressionRatio(this_conf) << ",";
        }
      }
      expr_table_output_stream << std::endl;
    }
    expr_table_output_stream << "Compression Time" << std::endl;
    for (const auto &method : kMethodListParamBlockSize) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, block_size, kAbsMaxDiffParamBlockSize);
        auto result = expr_table.find(this_conf);
        if (result != expr_table.end()) {
          expr_table_output_stream << result->second.AvgCompressionTimePerBlock() << ",";
        }
      }
      expr_table_output_stream << std::endl;
    }
    expr_table_output_stream << "Decompression Time" << std::endl;
    for (const auto &method : kMethodListParamBlockSize) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, block_size, kAbsMaxDiffParamBlockSize);
        auto result = expr_table.find(this_conf);
        if (result != expr_table.end()) {
          expr_table_output_stream << result->second.AvgDecompressionTimePerBlock() << ",";
        }
      }
      expr_table_output_stream << std::endl;
    }
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

// Auto-Gen for the Param(Rel MaxDiff) Experiment

void GenParamRelDiffTableCR(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "param_rel_diff_cr" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &max_diff : kMaxDiffRel) {
    for (const auto &method : kMethodListRel) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, kBlockSizeRel, max_diff);
        expr_table_output_stream << expr_table.find(this_conf)->second.CalCompressionRatio(this_conf) << ",";
      }
      expr_table_output_stream << std::endl;
    }
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenParamRelDiffTableCT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "param_rel_diff_ct" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &max_diff : kMaxDiffRel) {
    for (const auto &method : kMethodListRel) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, kBlockSizeRel, max_diff);
        expr_table_output_stream << expr_table.find(this_conf)->second.AvgCompressionTimePerBlock() << ",";
      }
      expr_table_output_stream << std::endl;
    }
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenParamRelDiffTableDT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "param_rel_diff_dt" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &max_diff : kMaxDiffRel) {
    for (const auto &method : kMethodListRel) {
      expr_table_output_stream << method << ",";
      for (const auto &data_set : kDataSetList) {
        ExprConf this_conf = ExprConf(method, data_set, kBlockSizeRel, max_diff);
        expr_table_output_stream << expr_table.find(this_conf)->second.AvgDecompressionTimePerBlock() << ",";
      }
      expr_table_output_stream << std::endl;
    }
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

// Auto-Gen for the Ablation Experiment

void GenAblationTableCR(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "ablation_cr" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListAblation) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeAblation, kMaxDiffAblation);
      expr_table_output_stream << expr_table.find(this_conf)->second.CalCompressionRatio(this_conf) << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenAblationTableCT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "ablation_ct" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListAblation) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeAblation, kMaxDiffAblation);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgCompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void GenAblationTableDT(ExprTable &expr_table) {
  std::ofstream expr_table_output_stream(kExportExprTablePrefix + "ablation_dt" + kExportExprTableSuffix);
  if (!expr_table_output_stream.is_open()) {
    std::cerr << "Failed to export performance data." << std::endl;
    exit(-1);
  }

  expr_table_output_stream << std::setiosflags(std::ios::fixed) << std::setprecision(6);

  for (const auto &method : kMethodListAblation) {
    expr_table_output_stream << method << ",";
    for (const auto &data_set : kDataSetList) {
      ExprConf this_conf = ExprConf(method, data_set, kBlockSizeAblation, kMaxDiffAblation);
      expr_table_output_stream << expr_table.find(this_conf)->second.AvgDecompressionTimePerBlock() << ",";
    }
    expr_table_output_stream << std::endl;
  }

  expr_table_output_stream.flush();
  expr_table_output_stream.close();
}

void PerfSerfXOR(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressor serf_xor_compressor(1000, max_diff, kFileNameToAdjustDigit.find(data_set)->second);
  SerfXORDecompressor serf_xor_decompressor(kFileNameToAdjustDigit.find(data_set)->second);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfQt(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfQtCompressor serf_qt_compressor(block_size, max_diff);
  SerfQtDecompressor serf_qt_decompressor;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_qt_compressor.AddValue(value);
    serf_qt_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_qt_compressor.get_compressed_size_in_bits());
    Array<uint8_t> compression_output = serf_qt_compressor.compressed_bytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_qt_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfQt", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfQtLinear(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                      const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfQtLinearCompressor serf_qt_linear_compressor(block_size, max_diff);
  SerfQtLinearDecompressor serf_qt_linear_decompressor;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_qt_linear_compressor.AddValue(value);
    serf_qt_linear_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_qt_linear_compressor.get_compressed_size_in_bits());
    Array<uint8_t> compression_output = serf_qt_linear_compressor.compressed_bytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_qt_linear_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfQtLinear", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfDeflate(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    DeflateCompressor deflate_compressor(block_size);
    DeflateDecompressor deflate_decompressor;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) deflate_compressor.addValue(value);
    deflate_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(deflate_compressor.getCompressedSizeInBits());
    Array<uint8_t> compression_output = deflate_compressor.getBytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = deflate_decompressor.decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Deflate", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfLZ4(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
             const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    LZ4Compressor lz_4_compressor(block_size);
    LZ4Decompressor lz_4_decompressor;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) lz_4_compressor.addValue(value);
    lz_4_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(lz_4_compressor.getCompressedSizeInBits());
    Array<char> compression_output = lz_4_compressor.getBytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = lz_4_decompressor.decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("LZ4", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfFPC(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
             const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    FpcCompressor fpc_compressor(5, block_size);
    FpcDecompressor fpc_decompressor(5, block_size);

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) fpc_compressor.addValue(value);
    fpc_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(fpc_compressor.getCompressedSizeInBits());
    std::vector<char> compression_output = fpc_compressor.getBytes();
    fpc_decompressor.setBytes(compression_output.data(), compression_output.size());

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = fpc_decompressor.decompress();
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("FPC", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfZstd(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
              const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    char compression_output[block_size * 10];
    double decompression_output[block_size];

    auto compression_start_time = std::chrono::steady_clock::now();
    size_t compression_output_len = ZSTD_compress(compression_output, block_size * 10, original_data.data(),
                                                  original_data.size() * sizeof(double), ZSTD_defaultCLevel());
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    ZSTD_decompress(decompression_output, block_size * sizeof(double), compression_output, compression_output_len);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    for (int i = 0; i < block_size; ++i) {
      EXPECT_FLOAT_EQ(original_data[i], decompression_output[i]);
    }
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Zstd", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSnappy(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    std::string compression_output;
    std::string decompression_output;
    auto compression_start_time = std::chrono::steady_clock::now();
    size_t compression_output_len = snappy::Compress(reinterpret_cast<const char *>(original_data.data()),
                                                     original_data.size() * sizeof(double), &compression_output);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    snappy::Uncompress(compression_output.data(), compression_output.size(), &decompression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Snappy", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfElf(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
             const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    uint8_t *compression_output_buffer;
    double *decompression_output = new double[block_size];
    ssize_t compression_output_len_in_bytes;
    ssize_t decompression_len;

    auto compression_start_time = std::chrono::steady_clock::now();
    compression_output_len_in_bytes = elf_encode(original_data.data(), original_data.size(),
                                                 &compression_output_buffer, 0);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len_in_bytes * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    decompression_len = elf_decode(compression_output_buffer, compression_output_len_in_bytes, decompression_output,
                                   0);
    delete[] decompression_output;
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Elf", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfChimp128(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                  const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    ChimpCompressor chimp_compressor(128);
    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) {
      chimp_compressor.addValue(value);
    }
    chimp_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();
    perf_record.AddCompressedSize(chimp_compressor.get_size());
    Array<uint8_t> compression_output = chimp_compressor.get_compress_pack();
    auto decompression_start_time = std::chrono::steady_clock::now();
    ChimpDecompressor chimp_decompressor(compression_output, 128);
    std::vector<double> decompression_output = chimp_decompressor.decompress();
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Chimp128", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfGorilla(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;
  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    GorillaCompressor gorilla_compressor(block_size);
    GorillaDecompressor gorilla_decompressor;
    ++block_count;
    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) {
      gorilla_compressor.addValue(value);
    }
    gorilla_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();
    perf_record.AddCompressedSize(gorilla_compressor.get_compress_size_in_bits());
    Array<uint8_t> compression_output = gorilla_compressor.get_compress_pack();
    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompression_output = gorilla_decompressor.decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Gorilla", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfLZ77(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
              const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;
  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    auto *compression_output = new uint8_t[10000];
    auto *decompression_output = new double[1000];

    auto compression_start_time = std::chrono::steady_clock::now();
    int compression_output_len = fastlz_compress_level(2, original_data.data(), block_size * sizeof(double),
                                                       compression_output);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    fastlz_decompress(compression_output, compression_output_len, decompression_output,
                      block_size * sizeof(double));
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    delete[] compression_output;
    delete[] decompression_output;
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("LZ77", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfMachete(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    auto *compression_buffer = new uint8_t[100000];
    auto *decompression_buffer = new double[block_size];

    auto compression_start_time = std::chrono::steady_clock::now();
    ssize_t compression_output_len = machete_compress<lorenzo1, hybrid>(original_data.data(), original_data.size(),
                                                                        &compression_buffer, max_diff);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    ssize_t decompression_output_len = machete_decompress<lorenzo1, hybrid>(compression_buffer,
                                                                            compression_output_len,
                                                                            decompression_buffer);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    delete[] compression_buffer;
    delete[] decompression_buffer;
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Machete", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSZ2(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
             const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    size_t compression_output_len;
    auto decompression_output = new double[block_size];

    auto compression_start_time = std::chrono::steady_clock::now();
    auto compression_output = SZ_compress_args(SZ_DOUBLE, original_data.data(), &compression_output_len,
                                               ABS, max_diff * 0.99, 0, 0, 0, 0, 0, 0, original_data.size());
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    size_t decompression_output_len = SZ_decompress_args(SZ_DOUBLE, compression_output,
                                                         compression_output_len, decompression_output, 0, 0,
                                                         0, 0, block_size);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    delete[] decompression_output;
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SZ2", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSimPiece(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                  const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    std::vector<Point> input_points;
    for (int i = 0; i < original_data.size(); ++i) {
      input_points.emplace_back(i, original_data[i]);
    }

    char *compression_output = new char[original_data.size() * 8];
    int compression_output_len = 0;
    int timestamp_store_size;
    auto compression_start_time = std::chrono::steady_clock::now();
    SimPiece sim_piece_compress(input_points, max_diff);
    compression_output_len = sim_piece_compress.toByteArray(compression_output, true, &timestamp_store_size);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize((compression_output_len - timestamp_store_size) * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    SimPiece sim_piece_decompress(compression_output, compression_output_len, true);
    sim_piece_decompress.decompress();
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SimPiece", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

#ifdef USE_X86_INTRINSICS
void PerfSprintz(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
             const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    DoubleSprintzCompressor sprintz_compressor(max_diff);
    auto *compression_output = new int16_t [original_data.size() * 8];
    DoubleSprintzDecompressor sprintz_decompressor;

    auto compression_start_time = std::chrono::steady_clock::now();
    int compression_size = sprintz_compressor.compress(original_data, compression_output);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_size * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    sprintz_decompressor.decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    delete[] compression_output;
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Sprintz", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}
#endif

void PerfALP(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
             const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    uint8_t compress_output_buffer[(block_size * sizeof(double)) + 8096];
    auto decompress_buffer_size = alp::AlpApiUtils::align_value<size_t, alp::config::VECTOR_SIZE>(block_size);
    double decompress_output_buffer[decompress_buffer_size];
    alp::AlpCompressor compressor;
    alp::AlpDecompressor decompressor;
    auto compression_start_time = std::chrono::steady_clock::now();
    compressor.compress(original_data.data(), original_data.size(), compress_output_buffer);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compressor.get_size() * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    decompressor.decompress(compress_output_buffer, block_size, decompress_output_buffer);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("ALP", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

// Single Precision

void PerfSerfXOR_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                    const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressor32 serf_xor_compressor(1000, max_diff);
  SerfXORDecompressor32 serf_xor_decompressor;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<float> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfQt_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                   const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfQtCompressor32 serf_qt_compressor(block_size, max_diff * 0.97f);
  SerfQtDecompressor32 serf_qt_decompressor;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_qt_compressor.AddValue(value);
    serf_qt_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_qt_compressor.stored_compressed_size_in_bits());
    Array<uint8_t> compression_output = serf_qt_compressor.compressed_bytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<float> decompressed_data = serf_qt_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfQt", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfDeflate_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                    const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    DeflateCompressor deflate_compressor(block_size);
    DeflateDecompressor deflate_decompressor;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) deflate_compressor.addValue32(value);
    deflate_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(deflate_compressor.getCompressedSizeInBits());
    Array<uint8_t> compression_output = deflate_compressor.getBytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<float> decompressed_data = deflate_decompressor.decompress32(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Deflate", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfLZ4_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    LZ4Compressor lz_4_compressor(block_size);
    LZ4Decompressor lz_4_decompressor;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) lz_4_compressor.addValue32(value);
    lz_4_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(lz_4_compressor.getCompressedSizeInBits());
    Array<char> compression_output = lz_4_compressor.getBytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<float> decompressed_data = lz_4_decompressor.decompress32(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("LZ4", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfLZ77_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;
  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    auto *compression_output = new uint8_t[10000];
    auto *decompression_output = new float[1000];

    auto compression_start_time = std::chrono::steady_clock::now();
    int compression_output_len = fastlz_compress_level(2, original_data.data(), block_size * sizeof(float),
                                                       compression_output);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    fastlz_decompress(compression_output, compression_output_len, decompression_output,
                      block_size * sizeof(float));
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    delete[] compression_output;
    delete[] decompression_output;
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("LZ77", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSnappy_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                   const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    std::string compression_output;
    std::string decompression_output;
    auto compression_start_time = std::chrono::steady_clock::now();
    size_t compression_output_len = snappy::Compress(reinterpret_cast<const char *>(original_data.data()),
                                                     original_data.size() * sizeof(float), &compression_output);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    snappy::Uncompress(compression_output.data(), compression_output.size(), &decompression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Snappy", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfZstd_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    char compression_output[block_size * 10];
    float decompression_output[block_size];

    auto compression_start_time = std::chrono::steady_clock::now();
    size_t compression_output_len = ZSTD_compress(compression_output, block_size * 10, original_data.data(),
                                                  original_data.size() * sizeof(float), 3);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    ZSTD_decompress(decompression_output, block_size * sizeof(float), compression_output, compression_output_len);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Zstd", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSZ2_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    size_t compression_output_len;
    auto decompression_output = new float[block_size];

    auto compression_start_time = std::chrono::steady_clock::now();
    auto compression_output = SZ_compress_args(SZ_FLOAT, original_data.data(), &compression_output_len,
                                               ABS, max_diff * 0.97, 0, 0, 0, 0, 0, 0, original_data.size());
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    size_t decompression_output_len = SZ_decompress_args(SZ_FLOAT, compression_output,
                                                         compression_output_len, decompression_output, 0, 0,
                                                         0, 0, block_size);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    delete[] decompression_output;
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SZ2", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfElf_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    uint8_t *compression_output_buffer;
    auto *decompression_output = new float[block_size];
    ssize_t compression_output_len_in_bytes;
    ssize_t decompression_len;

    auto compression_start_time = std::chrono::steady_clock::now();
    compression_output_len_in_bytes = elf_encode_32(original_data.data(), original_data.size(),
                                                    &compression_output_buffer, 0);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len_in_bytes * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    decompression_len = elf_decode_32(compression_output_buffer, compression_output_len_in_bytes,
                                      decompression_output, 0);
    delete[] decompression_output;
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Elf", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfChimp128_32(std::ifstream &data_set_input_stream_ref, float max_diff, int block_size,
                     const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<float> original_data;

  while ((original_data = ReadBlock32(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    ChimpCompressor32 chimp_compressor(128);

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) {
      chimp_compressor.addValue(value);
    }
    chimp_compressor.close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(chimp_compressor.get_size());

    Array<uint8_t> compression_output = chimp_compressor.get_compress_pack();

    auto decompression_start_time = std::chrono::steady_clock::now();
    ChimpDecompressor32 chimp_decompressor(compression_output, 128);
    std::vector<float> decompression_output = chimp_decompressor.decompress();
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Chimp128", data_set, block_size, max_diff, true), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

// Ablation

void PerfSerfXOR_Without_Shifter(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressor serf_xor_compressor(1000, max_diff, 0);
  SerfXORDecompressor serf_xor_decompressor(0);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR_w/o_Shifter", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfXOR_Without_OptAppr(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                                 const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressorNoAppr serf_xor_compressor(1000, max_diff, kFileNameToAdjustDigit.find(data_set)->second);
  SerfXORDecompressor serf_xor_decompressor(kFileNameToAdjustDigit.find(data_set)->second);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR_w/o_OptAppr", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfXOR_Without_FastSearch(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                                    const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressorNoFastSearch serf_xor_compressor(1000, max_diff, kFileNameToAdjustDigit.find(data_set)->second);
  SerfXORDecompressor serf_xor_decompressor(kFileNameToAdjustDigit.find(data_set)->second);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR_w/o_FastSearch", data_set, block_size, max_diff),
                                        perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

// Relational error-bound

void PerfSZ2Rel(std::ifstream &data_set_input_stream_ref, double rel_diff, int block_size,
                const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;
    size_t compression_output_len;
    auto decompression_output = new double[block_size];

    auto compression_start_time = std::chrono::steady_clock::now();
    auto compression_output = SZ_compress_args(SZ_DOUBLE, original_data.data(), &compression_output_len,
                                               REL, 0, rel_diff, 0, 0, 0, 0, 0, original_data.size());
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    size_t decompression_output_len = SZ_decompress_args(SZ_DOUBLE, compression_output,
                                                         compression_output_len, decompression_output, 0, 0,
                                                         0, 0, block_size);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);

    delete[] decompression_output;
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SZ2_Rel", data_set, block_size, rel_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfXORRel(std::ifstream &data_set_input_stream_ref, double rel_diff, int block_size,
                    const std::string &data_set, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressorRel serf_xor_compressor_rel(1000, rel_diff, kFileNameToAdjustDigit.find(data_set)->second);
  SerfXORDecompressor serf_xor_decompressor(kFileNameToAdjustDigit.find(data_set)->second);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor_rel.AddValue(value);
    serf_xor_compressor_rel.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor_rel.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor_rel.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR_Rel", data_set, block_size, rel_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

// Lambda Expr

void PerfSerfXORLambda(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                       const std::string &data_set, int lambda, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressor serf_xor_compressor(1000, max_diff, lambda);
  SerfXORDecompressor serf_xor_decompressor(lambda);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfXORLambdaRel(std::ifstream &data_set_input_stream_ref, double max_diff, int block_size,
                       const std::string &data_set, int lambda, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressorRel serf_xor_compressor(1000, max_diff, lambda);
  SerfXORDecompressor serf_xor_decompressor(lambda);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlock(data_set_input_stream_ref, block_size)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR_Rel", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

// Beta experiment
void PerfSerfXORBeta(std::ifstream &data_set_input_stream_ref, const std::string &data_set, double max_diff,
                     int block_size, int beta, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfXORCompressor serf_xor_compressor(1000, max_diff, kFileNameToAdjustDigit.find(data_set)->second);
  SerfXORDecompressor serf_xor_decompressor(kFileNameToAdjustDigit.find(data_set)->second);

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlockUsingBeta(data_set_input_stream_ref, block_size, beta)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_xor_compressor.AddValue(value);
    serf_xor_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_xor_compressor.compressed_size_last_block());
    Array<uint8_t> compression_output = serf_xor_compressor.compressed_bytes_last_block();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_xor_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfXOR", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfSerfQtBeta(std::ifstream &data_set_input_stream_ref, const std::string &data_set, double max_diff,
                    int block_size, int beta, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  SerfQtCompressor serf_qt_compressor(block_size, max_diff);
  SerfQtDecompressor serf_qt_decompressor;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlockUsingBeta(data_set_input_stream_ref, block_size, beta)).size() == block_size) {
    ++block_count;

    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_qt_compressor.AddValue(value);
    serf_qt_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(serf_qt_compressor.get_compressed_size_in_bits());
    Array<uint8_t> compression_output = serf_qt_compressor.compressed_bytes();

    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_qt_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("SerfQt", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

void PerfElfBeta(std::ifstream &data_set_input_stream_ref, const std::string &data_set, double max_diff,
                 int block_size, int beta, ExprTable &table_to_insert) {
  PerfRecord perf_record;

  int block_count = 0;
  std::vector<double> original_data;

  while ((original_data = ReadBlockUsingBeta(data_set_input_stream_ref, block_size, beta)).size() == block_size) {
    ++block_count;

    uint8_t *compression_output_buffer;
    double *decompression_output = new double[block_size];
    ssize_t compression_output_len_in_bytes;
    ssize_t decompression_len;

    auto compression_start_time = std::chrono::steady_clock::now();
    compression_output_len_in_bytes = elf_encode(original_data.data(), original_data.size(),
                                                 &compression_output_buffer, 0);
    auto compression_end_time = std::chrono::steady_clock::now();

    perf_record.AddCompressedSize(compression_output_len_in_bytes * 8);

    auto decompression_start_time = std::chrono::steady_clock::now();
    decompression_len = elf_decode(compression_output_buffer, compression_output_len_in_bytes, decompression_output,
                                   0);
    delete[] decompression_output;
    auto decompression_end_time = std::chrono::steady_clock::now();

    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);

    perf_record.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record.IncreaseDecompressionTime(decompression_time_in_a_block);
  }

  perf_record.set_block_count(block_count);
  table_to_insert.insert(std::make_pair(ExprConf("Elf", data_set, block_size, max_diff), perf_record));
  ResetFileStream(data_set_input_stream_ref);
}

TEST(Perf, Overall) {
  ExprTable expr_table_overall;

  for (const auto &data_set : kDataSetList) {
    std::ifstream data_input_stream(kDataSetDirPrefix + data_set);
    if (!data_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    // Lossy Compression
    PerfSerfXOR(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfSerfQt(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfMachete(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfSZ2(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfSimPiece(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
#ifdef USE_X86_INTRINSICS
    PerfSprintz(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
#endif

    // Lossless Compression
    PerfGorilla(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfChimp128(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfDeflate(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfElf(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfFPC(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfLZ4(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfSerfXOR(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfLZ77(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfZstd(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);
    PerfSnappy(data_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, expr_table_overall);

    data_input_stream.close();
  }

  GenOverallTableCR(expr_table_overall);
  GenOverallTableCT(expr_table_overall);
  GenOverallTableDT(expr_table_overall);
}

TEST(Perf, ParamAbsMaxDiff) {
  ExprTable expr_table_abs_diff;

  for (const auto &data_set : kDataSetList) {
    std::ifstream data_input_stream(kDataSetDirPrefix + data_set);
    if (!data_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    for (const auto &max_diff : kMaxDiffList) {
      PerfSerfXOR(data_input_stream, max_diff, kBlockSizeParamAbsMaxDiff, data_set, expr_table_abs_diff);
      PerfSerfQt(data_input_stream, max_diff, kBlockSizeParamAbsMaxDiff, data_set, expr_table_abs_diff);
      PerfSimPiece(data_input_stream, max_diff, kBlockSizeParamAbsMaxDiff, data_set, expr_table_abs_diff);
      PerfSZ2(data_input_stream, max_diff, kBlockSizeParamAbsMaxDiff, data_set, expr_table_abs_diff);
      PerfMachete(data_input_stream, max_diff, kBlockSizeParamAbsMaxDiff, data_set, expr_table_abs_diff);
#ifdef USE_X86_INTRINSICS
      PerfSprintz(data_input_stream, max_diff, kBlockSizeParamAbsMaxDiff, data_set, expr_table_abs_diff);
#endif
    }
  }

  GenParamAbsDiffTable(expr_table_abs_diff);
}

TEST(Perf, ParamBlockSize) {
  ExprTable expr_table_block_size;

  for (const auto &data_set : kDataSetList) {
    std::ifstream data_input_stream(kDataSetDirPrefix + data_set);
    if (!data_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    for (const auto & block_size : kBlockSizeList) {
      PerfSerfXOR(data_input_stream, kAbsMaxDiffParamBlockSize, block_size, data_set, expr_table_block_size);
      PerfSerfQt(data_input_stream, kAbsMaxDiffParamBlockSize, block_size, data_set, expr_table_block_size);
      PerfSimPiece(data_input_stream, kAbsMaxDiffParamBlockSize, block_size, data_set, expr_table_block_size);
      PerfSZ2(data_input_stream, kAbsMaxDiffParamBlockSize, block_size, data_set, expr_table_block_size);
      PerfMachete(data_input_stream, kAbsMaxDiffParamBlockSize, block_size, data_set, expr_table_block_size);
#ifdef USE_X86_INTRINSICS
      PerfSprintz(data_input_stream, kAbsMaxDiffParamBlockSize, block_size, data_set, expr_table_block_size);
#endif
      if (block_size >= 600) {
        PerfALP(data_input_stream, kAbsMaxDiffParamBlockSize, block_size, data_set, expr_table_block_size);
      }
    }
  }

  GenParamBlockSizeTable(expr_table_block_size);
}

TEST(Perf, Rel) {
  ExprTable expr_table_rel;

  for (const auto &data_set : kDataSetList) {
    std::ifstream data_input_stream(kDataSetDirPrefix + data_set);
    if (!data_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    for (const auto &rel_diff : kMaxDiffRel) {
      PerfSerfXORRel(data_input_stream, rel_diff, kBlockSizeOverall, data_set, expr_table_rel);
      PerfSZ2Rel(data_input_stream, rel_diff, kBlockSizeOverall, data_set, expr_table_rel);
    }

    data_input_stream.close();
  }

  GenParamRelDiffTableCR(expr_table_rel);
  GenParamRelDiffTableCT(expr_table_rel);
  GenParamRelDiffTableDT(expr_table_rel);
}

TEST(Perf, SinglePrecision) {
  ExprTable expr_table_32;

  for (const auto &data_set : kDataSetList32) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    // Lossy
    PerfSerfXOR_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfSerfQt_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfSZ2_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);

    // Lossless
    PerfChimp128_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfDeflate_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfElf_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfLZ4_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfLZ77_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfZstd_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);
    PerfSnappy_32(data_set_input_stream, kMaxDiff32, kBlockSize32, data_set, expr_table_32);

    data_set_input_stream.close();
  }

  GenSinglePrecisionTableCR(expr_table_32);
  GenSinglePrecisionTableCT(expr_table_32);
  GenSinglePrecisionTableDT(expr_table_32);
}

TEST(Perf, Serf_Ablation) {
  ExprTable expr_table_ablation;

  for (const auto &data_set : kDataSetList) {
    std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
    if (!data_set_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    PerfSerfXOR(data_set_input_stream, kMaxDiffAblation, kBlockSizeAblation, data_set, expr_table_ablation);
    PerfSerfXOR_Without_Shifter(data_set_input_stream,
                                kMaxDiffAblation,
                                kBlockSizeAblation,
                                data_set,
                                expr_table_ablation);
    PerfSerfXOR_Without_OptAppr(data_set_input_stream,
                                kMaxDiffAblation,
                                kBlockSizeAblation,
                                data_set,
                                expr_table_ablation);
    PerfSerfXOR_Without_FastSearch(data_set_input_stream,
                                   kMaxDiffAblation,
                                   kBlockSizeAblation,
                                   data_set,
                                   expr_table_ablation);

    data_set_input_stream.close();
  }

  GenAblationTableCR(expr_table_ablation);
  GenAblationTableCT(expr_table_ablation);
  GenAblationTableDT(expr_table_ablation);
}

TEST(Perf, Lambda) {
  std::ofstream result_output(kExportExprTablePrefix + "lambda_ct" + kExportExprTableSuffix);
  if (!result_output.is_open()) std::cout << "Failed to creat perf result file." << std::endl;

  for (const auto &factor : kLambdaFactorList) {
    result_output << factor << ",";
    for (const auto &data_set : kDataSetList) {
      std::ifstream data_set_input_stream(kDataSetDirPrefix + data_set);
      if (!data_set_input_stream.is_open()) {
        std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
      }
      int lambda_for_this_data_set = kFileNameToAdjustDigit.find(data_set)->second;
      ExprTable expr_table_lambda;
      ExprConf this_conf = ExprConf("SerfXOR", data_set, kBlockSizeOverall, kMaxDiffOverall);
      int test_lambda = static_cast<int>((factor * lambda_for_this_data_set));
      PerfSerfXORLambda(data_set_input_stream, kMaxDiffOverall, kBlockSizeOverall, data_set, test_lambda,
                           expr_table_lambda);
      result_output << expr_table_lambda.find(this_conf)->second.AvgCompressionTimePerBlock() << ",";
      data_set_input_stream.close();
    }
    result_output << std::endl;
  }
}

TEST(Perf, Beta) {
  const static std::string chosen_data_set = "Motor-temp.csv";
  const static int min_beta = 1;
  const static int max_beta = 15;

  std::ofstream result_output(kExportExprTablePrefix + "beta_cr" + kExportExprTableSuffix);
  if (!result_output.is_open()) std::cout << "Failed to creat perf result file." << std::endl;

  std::ifstream data_set_input_stream(kDataSetDirPrefix + chosen_data_set);
  if (!data_set_input_stream.is_open()) {
    std::cerr << "Failed to open the file [" << chosen_data_set << "]" << std::endl;
  }

  for (int beta = min_beta; beta <= max_beta; beta++) {
    ExprTable expr_table_beta;
    PerfSerfXORBeta(data_set_input_stream, chosen_data_set, kMaxDiffOverall, kBlockSizeOverall, beta, expr_table_beta);
    PerfSerfQtBeta(data_set_input_stream, chosen_data_set, kMaxDiffOverall, kBlockSizeOverall, beta, expr_table_beta);
    PerfElfBeta(data_set_input_stream, chosen_data_set, kMaxDiffOverall, kBlockSizeOverall, beta, expr_table_beta);
    ExprConf serf_xor_conf = ExprConf("SerfXOR", chosen_data_set, kBlockSizeOverall, kMaxDiffOverall);
    ExprConf serf_qt_conf = ExprConf("SerfQt", chosen_data_set, kBlockSizeOverall, kMaxDiffOverall);
    ExprConf elf_conf = ExprConf("Elf", chosen_data_set, kBlockSizeOverall, kMaxDiffOverall);
    result_output << beta << "," << "SerfXOR,"
                  << expr_table_beta.find(serf_xor_conf)->second.CalCompressionRatio(serf_xor_conf)
                  << std::endl;
    result_output << beta << "," << "SerfQt,"
                  << expr_table_beta.find(serf_qt_conf)->second.CalCompressionRatio(serf_qt_conf)
                  << std::endl;
    result_output << beta << "," << "Elf,"
                  << expr_table_beta.find(elf_conf)->second.CalCompressionRatio(elf_conf)
                  << std::endl;
  }
}

TEST(Perf, TSBS) {
  ExprTable expr_table_tsbs;

  for (const auto &data_set : kDataSetListTSBS) {
    std::ifstream data_input_stream(kDataSetDirPrefix + data_set);
    if (!data_input_stream.is_open()) {
      std::cerr << "Failed to open the file [" << data_set << "]" << std::endl;
    }

    // Lossy Compression
    PerfSerfXOR(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfSerfQt(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfMachete(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfSZ2(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfSimPiece(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
#ifdef USE_X86_INTRINSICS
    PerfSprintz(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
#endif

    // Lossless Compression
    PerfGorilla(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfChimp128(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfDeflate(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfElf(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfFPC(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfLZ4(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfSerfXOR(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfLZ77(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfZstd(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);
    PerfSnappy(data_input_stream, kMaxDiffTSBS, kBlockSizeTSBS, data_set, expr_table_tsbs);

    data_input_stream.close();
  }

  GenTSBSTableCR(expr_table_tsbs);
  GenTSBSTableCT(expr_table_tsbs);
  GenTSBSTableDT(expr_table_tsbs);
}

std::vector<double> ReadLongitudeOnly(std::ifstream &file_input_stream_ref, int block_size) {
  std::vector<double> ret;
  ret.reserve(block_size);
  int entry_count = 0;
  std::string line;
  
  while (std::getline(file_input_stream_ref, line) && entry_count < block_size) {
    size_t comma_pos = line.find(',');
    if (comma_pos != std::string::npos) {
      try {
        double longitude = std::stod(line.substr(0, comma_pos));
        ret.emplace_back(longitude);
        ++entry_count;
      } catch (...) {
        // Skip invalid lines
      }
    }
  }
  return ret;
}

std::vector<double> ReadLatitudeOnly(std::ifstream &file_input_stream_ref, int block_size) {
  std::vector<double> ret;
  ret.reserve(block_size);
  int entry_count = 0;
  std::string line;
  
  while (std::getline(file_input_stream_ref, line) && entry_count < block_size) {
    size_t comma_pos = line.find(',');
    if (comma_pos != std::string::npos) {
      try {
        double latitude = std::stod(line.substr(comma_pos + 1));
        ret.emplace_back(latitude);
        ++entry_count;
      } catch (...) {
        // Skip invalid lines
      }
    }
  }
  return ret;
}

void TestDataType(const std::string& dataset_name, const std::string& file_path, 
                  std::function<std::vector<double>(std::ifstream&, int)> read_func,
                  const std::string& data_type, ExprTable& expr_table_comparison,
                  int max_blocks = 100, double max_diff = 1.0E-3) {
  const int test_block_size = 100;
  
  // Test original Serf-QT
  std::ifstream data_input_stream1(file_path);
  if (!data_input_stream1.is_open()) {
    std::cerr << "Failed to open " << file_path << std::endl;
    return;
  }
  
  PerfRecord perf_record_qt;
  int block_count = 0;
  std::vector<double> original_data;
  
  while ((original_data = read_func(data_input_stream1, test_block_size)).size() == test_block_size && block_count < max_blocks) {
    ++block_count;
    
    SerfQtCompressor serf_qt_compressor(test_block_size, max_diff);
    SerfQtDecompressor serf_qt_decompressor;
    
    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_qt_compressor.AddValue(value);
    serf_qt_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();
    
    perf_record_qt.AddCompressedSize(serf_qt_compressor.get_compressed_size_in_bits());
    Array<uint8_t> compression_output = serf_qt_compressor.compressed_bytes();
    
    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_qt_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();
    
    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);
    
    perf_record_qt.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record_qt.IncreaseDecompressionTime(decompression_time_in_a_block);
  }
  
  perf_record_qt.set_block_count(block_count);
  std::string qt_name = dataset_name + "_" + data_type;
  expr_table_comparison.insert(std::make_pair(ExprConf("SerfQt", qt_name, test_block_size, max_diff), perf_record_qt));
  data_input_stream1.close();
  
  // Test linear prediction Serf-QT
  std::ifstream data_input_stream2(file_path);
  PerfRecord perf_record_linear;
  block_count = 0;
  
  while ((original_data = read_func(data_input_stream2, test_block_size)).size() == test_block_size && block_count < max_blocks) {
    ++block_count;
    
    SerfQtLinearCompressor serf_qt_linear_compressor(test_block_size, max_diff);
    SerfQtLinearDecompressor serf_qt_linear_decompressor;
    
    auto compression_start_time = std::chrono::steady_clock::now();
    for (const auto &value : original_data) serf_qt_linear_compressor.AddValue(value);
    serf_qt_linear_compressor.Close();
    auto compression_end_time = std::chrono::steady_clock::now();
    
    perf_record_linear.AddCompressedSize(serf_qt_linear_compressor.get_compressed_size_in_bits());
    Array<uint8_t> compression_output = serf_qt_linear_compressor.compressed_bytes();
    
    auto decompression_start_time = std::chrono::steady_clock::now();
    std::vector<double> decompressed_data = serf_qt_linear_decompressor.Decompress(compression_output);
    auto decompression_end_time = std::chrono::steady_clock::now();
    
    auto compression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        compression_end_time - compression_start_time);
    auto decompression_time_in_a_block = std::chrono::duration_cast<std::chrono::microseconds>(
        decompression_end_time - decompression_start_time);
    
    perf_record_linear.IncreaseCompressionTime(compression_time_in_a_block);
    perf_record_linear.IncreaseDecompressionTime(decompression_time_in_a_block);
  }
  
  perf_record_linear.set_block_count(block_count);
  std::string linear_name = dataset_name + "_" + data_type;
  expr_table_comparison.insert(std::make_pair(ExprConf("SerfQtLinear", linear_name, test_block_size, max_diff), perf_record_linear));
  data_input_stream2.close();
}

TEST(Perf, SerfQtLinearComparison) {
  ExprTable expr_table_comparison;
  
  // Test datasets
  const std::string test_datasets[] = {
    "T-drive_longitude_latitude.csv",
    "Geolife_100k_longitude_latitude.csv"
  };
  
  for (const auto &data_set : test_datasets) {
    std::string file_path = "../test/data_set/" + data_set;
    std::string dataset_name = data_set.substr(0, data_set.find("_longitude_latitude.csv"));
    
    // Test longitude data only
    TestDataType(dataset_name, file_path, ReadLongitudeOnly, "longitude", expr_table_comparison);
    
    // Test latitude data only  
    TestDataType(dataset_name, file_path, ReadLatitudeOnly, "latitude", expr_table_comparison);
  }

  // Export comparison results
  std::ofstream comparison_output("../test/serf_qt_linear_comparison_table.csv");
  if (!comparison_output.is_open()) {
    std::cerr << "Failed to export comparison data." << std::endl;
    return;
  }

  // Write header
  comparison_output << "Method,DataSet,BlockSize,MaxDiff,CompressionRatio,CompressionTime(AvgPerBlock),DecompressionTime(AvgPerBlock)" << std::endl;
  
  // Write records
  for (const auto &conf_record : expr_table_comparison) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    comparison_output << conf.method() << "," << conf.data_set() << "," << conf.block_size() << ","
                     << conf.max_diff() << "," << record.CalCompressionRatio(conf) << ","
                     << record.AvgCompressionTimePerBlock() << ","
                     << record.AvgDecompressionTimePerBlock() << std::endl;
  }

  comparison_output.flush();
  comparison_output.close();
  
  std::cout << "Serf-QT Linear Prediction comparison test completed!" << std::endl;
  std::cout << "Results saved to: ../test/serf_qt_linear_comparison_table.csv" << std::endl;
}

TEST(Perf, GeolifeDetailedComparison) {
  ExprTable expr_table_geolife;
  
  const std::string geolife_dataset = "Geolife_100k_longitude_latitude.csv";
  const std::string file_path = "../test/data_set/" + geolife_dataset;
  
  const int test_block_size = 100;  // 100 data points per block
  const double test_max_diff = 1.0E-3;
  const int max_blocks = 1000;  // Use 1000 blocks = 100,000 data points
  
  std::cout << "=== Geolife Detailed Performance Comparison ===" << std::endl;
  std::cout << "Dataset: " << geolife_dataset << std::endl;
  std::cout << "Block size: " << test_block_size << std::endl;
  std::cout << "Max blocks: " << max_blocks << " (Total: " << (max_blocks * test_block_size) << " data points)" << std::endl;
  std::cout << "Max diff: " << test_max_diff << std::endl << std::endl;
  
  // Test longitude data
  std::cout << "Testing Longitude Data..." << std::endl;
  TestDataType("Geolife_100k", file_path, ReadLongitudeOnly, "longitude", expr_table_geolife, max_blocks);
  
  // Test latitude data  
  std::cout << "Testing Latitude Data..." << std::endl;
  TestDataType("Geolife_100k", file_path, ReadLatitudeOnly, "latitude", expr_table_geolife, max_blocks);
  
  // Export detailed results
  std::ofstream geolife_output("../test/geolife_detailed_comparison.csv");
  if (!geolife_output.is_open()) {
    std::cerr << "Failed to export Geolife comparison data." << std::endl;
    return;
  }
  
  geolife_output << "Method,DataType,BlockSize,MaxDiff,BlockCount,TotalDataPoints,CompressionRatio,CompressionTime(AvgPerBlock),DecompressionTime(AvgPerBlock),TotalCompressedBits" << std::endl;
  
  for (const auto &conf_record : expr_table_geolife) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    
    geolife_output << conf.method() << "," 
                   << conf.data_set() << "," 
                   << conf.block_size() << ","
                   << conf.max_diff() << "," 
                   << record.block_count() << ","
                   << (record.block_count() * std::stoi(conf.block_size())) << ","
                   << record.CalCompressionRatio(conf) << ","
                   << record.AvgCompressionTimePerBlock() << ","
                   << record.AvgDecompressionTimePerBlock() << ","
                   << record.compressed_size_in_bits() << std::endl;
  }
  
  geolife_output.flush();
  geolife_output.close();
  
  // Print summary to console
  std::cout << "\n=== Results Summary ===" << std::endl;
  for (const auto &conf_record : expr_table_geolife) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    
    std::cout << conf.method() << " (" << conf.data_set() << "):" << std::endl;
    std::cout << "  Compression Ratio: " << std::fixed << std::setprecision(6) << record.CalCompressionRatio(conf) << std::endl;
    std::cout << "  Blocks Processed: " << record.block_count() << std::endl;
    std::cout << "  Total Data Points: " << (record.block_count() * std::stoi(conf.block_size())) << std::endl;
    std::cout << "  Avg Compression Time: " << std::fixed << std::setprecision(2) << record.AvgCompressionTimePerBlock() << " μs/block" << std::endl;
    std::cout << "  Avg Decompression Time: " << std::fixed << std::setprecision(2) << record.AvgDecompressionTimePerBlock() << " μs/block" << std::endl;
    std::cout << "  Total Compressed Size: " << record.compressed_size_in_bits() << " bits" << std::endl;
    std::cout << std::endl;
  }
  
  std::cout << "Geolife detailed comparison completed!" << std::endl;
  std::cout << "Results saved to: ../test/geolife_detailed_comparison.csv" << std::endl;
}

TEST(Perf, GeolifeMultiPrecisionComparison) {
  ExprTable expr_table_precision;
  
  const std::string geolife_dataset = "Geolife_100k_longitude_latitude.csv";
  const std::string file_path = "../test/data_set/" + geolife_dataset;
  
  const int test_block_size = 100;
  const int max_blocks = 500;  // Use 500 blocks = 50,000 data points for faster testing
  
  // Test different max_diff values
  const double max_diff_values[] = {1.0E-3, 1.0E-4, 1.0E-5, 1.0E-6, 1.0E-7};
  const int num_precisions = sizeof(max_diff_values) / sizeof(max_diff_values[0]);
  
  std::cout << "=== Geolife Multi-Precision Comparison ===" << std::endl;
  std::cout << "Dataset: " << geolife_dataset << std::endl;
  std::cout << "Block size: " << test_block_size << std::endl;
  std::cout << "Max blocks: " << max_blocks << " (Total: " << (max_blocks * test_block_size) << " data points)" << std::endl;
  std::cout << "Testing " << num_precisions << " different max_diff values..." << std::endl << std::endl;
  
  for (int i = 0; i < num_precisions; i++) {
    double current_max_diff = max_diff_values[i];
    std::cout << "Testing max_diff = " << std::scientific << current_max_diff << std::endl;
    
    // Test longitude data with current max_diff
    std::cout << "  - Longitude data..." << std::endl;
    TestDataType("Geolife_precision", file_path, ReadLongitudeOnly, 
                 "longitude_" + std::to_string(i), expr_table_precision, max_blocks, current_max_diff);
    
    // Test latitude data with current max_diff
    std::cout << "  - Latitude data..." << std::endl;
    TestDataType("Geolife_precision", file_path, ReadLatitudeOnly, 
                 "latitude_" + std::to_string(i), expr_table_precision, max_blocks, current_max_diff);
  }
  
  // Export detailed results
  std::ofstream precision_output("../test/geolife_precision_comparison.csv");
  if (!precision_output.is_open()) {
    std::cerr << "Failed to export precision comparison data." << std::endl;
    return;
  }
  
  precision_output << "Method,DataType,MaxDiff,BlockSize,BlockCount,TotalDataPoints,CompressionRatio,CompressionTime(AvgPerBlock),DecompressionTime(AvgPerBlock),TotalCompressedBits" << std::endl;
  
  for (const auto &conf_record : expr_table_precision) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    
    precision_output << conf.method() << "," 
                     << conf.data_set() << "," 
                     << std::scientific << std::stod(conf.max_diff()) << ","
                     << conf.block_size() << ","
                     << record.block_count() << ","
                     << (record.block_count() * std::stoi(conf.block_size())) << ","
                     << std::fixed << std::setprecision(6) << record.CalCompressionRatio(conf) << ","
                     << std::fixed << std::setprecision(2) << record.AvgCompressionTimePerBlock() << ","
                     << std::fixed << std::setprecision(2) << record.AvgDecompressionTimePerBlock() << ","
                     << record.compressed_size_in_bits() << std::endl;
  }
  
  precision_output.flush();
  precision_output.close();
  
  // Print summary analysis
  std::cout << "\n=== Precision Analysis Summary ===" << std::endl;
  
  // Group results by max_diff for comparison
  std::map<double, std::pair<double, double>> precision_comparison; // max_diff -> (qt_ratio, linear_ratio)
  
  for (const auto &conf_record : expr_table_precision) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    
    double max_diff_val = std::stod(conf.max_diff());
    double compression_ratio = record.CalCompressionRatio(conf);
    
    if (conf.method() == "SerfQt") {
      precision_comparison[max_diff_val].first = compression_ratio;
    } else if (conf.method() == "SerfQtLinear") {
      precision_comparison[max_diff_val].second = compression_ratio;
    }
  }
  
  std::cout << std::left << std::setw(12) << "MaxDiff" 
            << std::setw(15) << "SerfQt Ratio" 
            << std::setw(18) << "SerfLinear Ratio" 
            << std::setw(15) << "Improvement" 
            << "Winner" << std::endl;
  std::cout << std::string(70, '-') << std::endl;
  
  for (const auto &entry : precision_comparison) {
    double max_diff_val = entry.first;
    double qt_ratio = entry.second.first;
    double linear_ratio = entry.second.second;
    
    if (qt_ratio > 0 && linear_ratio > 0) {
      double improvement = ((qt_ratio - linear_ratio) / qt_ratio) * 100.0;
      std::string winner = (linear_ratio < qt_ratio) ? "Linear" : "Original";
      
      std::cout << std::left << std::setw(12) << std::scientific << max_diff_val
                << std::setw(15) << std::fixed << std::setprecision(6) << qt_ratio
                << std::setw(18) << std::fixed << std::setprecision(6) << linear_ratio
                << std::setw(15) << std::fixed << std::setprecision(2) << improvement << "%"
                << winner << std::endl;
    }
  }
  
  std::cout << "\nGeolife multi-precision comparison completed!" << std::endl;
  std::cout << "Results saved to: ../test/geolife_precision_comparison.csv" << std::endl;
}

TEST(Perf, GeolifeOptimalPrecisionTest) {
  ExprTable expr_table_optimal;
  
  const std::string geolife_dataset = "Geolife_100k_longitude_latitude.csv";
  const std::string file_path = "../test/data_set/" + geolife_dataset;
  
  const int test_block_size = 100;
  const int max_blocks = 1000;  // Use 1000 blocks = 100,000 data points
  const double optimal_max_diff = 1.0E-5;  // Optimal precision found in previous test
  
  std::cout << "=== Geolife Optimal Precision Performance Test ===" << std::endl;
  std::cout << "Dataset: " << geolife_dataset << std::endl;
  std::cout << "Block size: " << test_block_size << std::endl;
  std::cout << "Max blocks: " << max_blocks << " (Total: " << (max_blocks * test_block_size) << " data points)" << std::endl;
  std::cout << "Optimal max_diff: " << std::scientific << optimal_max_diff << std::endl << std::endl;
  
  // Test longitude data with optimal precision
  std::cout << "Testing Longitude Data with Optimal Precision..." << std::endl;
  TestDataType("Geolife_optimal", file_path, ReadLongitudeOnly, "longitude", expr_table_optimal, max_blocks, optimal_max_diff);
  
  // Test latitude data with optimal precision
  std::cout << "Testing Latitude Data with Optimal Precision..." << std::endl;
  TestDataType("Geolife_optimal", file_path, ReadLatitudeOnly, "latitude", expr_table_optimal, max_blocks, optimal_max_diff);
  
  // Export detailed results
  std::ofstream optimal_output("../test/geolife_optimal_precision_100k.csv");
  if (!optimal_output.is_open()) {
    std::cerr << "Failed to export optimal precision data." << std::endl;
    return;
  }
  
  optimal_output << "Method,DataType,MaxDiff,BlockSize,BlockCount,TotalDataPoints,CompressionRatio,CompressionTime(AvgPerBlock),DecompressionTime(AvgPerBlock),TotalCompressedBits,CompressionRateBitsPerValue" << std::endl;
  
  for (const auto &conf_record : expr_table_optimal) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    
    int total_data_points = record.block_count() * std::stoi(conf.block_size());
    double bits_per_value = (double)record.compressed_size_in_bits() / total_data_points;
    
    optimal_output << conf.method() << "," 
                   << conf.data_set() << "," 
                   << std::scientific << std::stod(conf.max_diff()) << ","
                   << conf.block_size() << ","
                   << record.block_count() << ","
                   << total_data_points << ","
                   << std::fixed << std::setprecision(6) << record.CalCompressionRatio(conf) << ","
                   << std::fixed << std::setprecision(2) << record.AvgCompressionTimePerBlock() << ","
                   << std::fixed << std::setprecision(2) << record.AvgDecompressionTimePerBlock() << ","
                   << record.compressed_size_in_bits() << ","
                   << std::fixed << std::setprecision(2) << bits_per_value << std::endl;
  }
  
  optimal_output.flush();
  optimal_output.close();
  
  // Calculate and display comprehensive comparison
  std::cout << "\n=== Comprehensive Performance Analysis ===" << std::endl;
  
  // Organize results by data type for comparison
  struct MethodResults {
    double compression_ratio = 0;
    double compression_time = 0;
    double decompression_time = 0;
    long total_bits = 0;
    int total_points = 0;
    double bits_per_value = 0;
  };
  
  std::map<std::string, MethodResults> longitude_results;
  std::map<std::string, MethodResults> latitude_results;
  
  for (const auto &conf_record : expr_table_optimal) {
    auto conf = conf_record.first;
    auto record = conf_record.second;
    
    MethodResults result;
    result.compression_ratio = record.CalCompressionRatio(conf);
    result.compression_time = record.AvgCompressionTimePerBlock();
    result.decompression_time = record.AvgDecompressionTimePerBlock();
    result.total_bits = record.compressed_size_in_bits();
    result.total_points = record.block_count() * std::stoi(conf.block_size());
    result.bits_per_value = (double)result.total_bits / result.total_points;
    
    if (conf.data_set().find("longitude") != std::string::npos) {
      longitude_results[conf.method()] = result;
    } else if (conf.data_set().find("latitude") != std::string::npos) {
      latitude_results[conf.method()] = result;
    }
  }
  
  // Display longitude comparison
  std::cout << "📍 LONGITUDE DATA COMPARISON:" << std::endl;
  if (longitude_results.count("SerfQt") && longitude_results.count("SerfQtLinear")) {
    auto qt = longitude_results["SerfQt"];
    auto linear = longitude_results["SerfQtLinear"];
    
    double ratio_improvement = ((qt.compression_ratio - linear.compression_ratio) / qt.compression_ratio) * 100.0;
    double time_overhead = ((linear.compression_time - qt.compression_time) / qt.compression_time) * 100.0;
    double decomp_overhead = ((linear.decompression_time - qt.decompression_time) / qt.decompression_time) * 100.0;
    
    std::cout << "  Original Serf-QT:" << std::endl;
    std::cout << "    Compression Ratio: " << std::fixed << std::setprecision(6) << qt.compression_ratio << std::endl;
    std::cout << "    Bits per Value: " << std::fixed << std::setprecision(2) << qt.bits_per_value << std::endl;
    std::cout << "    Compression Time: " << std::fixed << std::setprecision(2) << qt.compression_time << " μs/block" << std::endl;
    std::cout << "    Decompression Time: " << std::fixed << std::setprecision(2) << qt.decompression_time << " μs/block" << std::endl;
    
    std::cout << "  Linear Serf-QT:" << std::endl;
    std::cout << "    Compression Ratio: " << std::fixed << std::setprecision(6) << linear.compression_ratio << std::endl;
    std::cout << "    Bits per Value: " << std::fixed << std::setprecision(2) << linear.bits_per_value << std::endl;
    std::cout << "    Compression Time: " << std::fixed << std::setprecision(2) << linear.compression_time << " μs/block" << std::endl;
    std::cout << "    Decompression Time: " << std::fixed << std::setprecision(2) << linear.decompression_time << " μs/block" << std::endl;
    
    std::cout << "  🎯 IMPROVEMENT ANALYSIS:" << std::endl;
    std::cout << "    Compression Ratio: " << (ratio_improvement > 0 ? "+" : "") << std::fixed << std::setprecision(2) << ratio_improvement << "% " << (ratio_improvement > 0 ? "BETTER" : "WORSE") << std::endl;
    std::cout << "    Compression Time: " << (time_overhead > 0 ? "+" : "") << std::fixed << std::setprecision(2) << time_overhead << "% overhead" << std::endl;
    std::cout << "    Decompression Time: " << (decomp_overhead > 0 ? "+" : "") << std::fixed << std::setprecision(2) << decomp_overhead << "% overhead" << std::endl;
  }
  
  // Display latitude comparison
  std::cout << "\n📍 LATITUDE DATA COMPARISON:" << std::endl;
  if (latitude_results.count("SerfQt") && latitude_results.count("SerfQtLinear")) {
    auto qt = latitude_results["SerfQt"];
    auto linear = latitude_results["SerfQtLinear"];
    
    double ratio_improvement = ((qt.compression_ratio - linear.compression_ratio) / qt.compression_ratio) * 100.0;
    double time_overhead = ((linear.compression_time - qt.compression_time) / qt.compression_time) * 100.0;
    double decomp_overhead = ((linear.decompression_time - qt.decompression_time) / qt.decompression_time) * 100.0;
    
    std::cout << "  Original Serf-QT:" << std::endl;
    std::cout << "    Compression Ratio: " << std::fixed << std::setprecision(6) << qt.compression_ratio << std::endl;
    std::cout << "    Bits per Value: " << std::fixed << std::setprecision(2) << qt.bits_per_value << std::endl;
    std::cout << "    Compression Time: " << std::fixed << std::setprecision(2) << qt.compression_time << " μs/block" << std::endl;
    std::cout << "    Decompression Time: " << std::fixed << std::setprecision(2) << qt.decompression_time << " μs/block" << std::endl;
    
    std::cout << "  Linear Serf-QT:" << std::endl;
    std::cout << "    Compression Ratio: " << std::fixed << std::setprecision(6) << linear.compression_ratio << std::endl;
    std::cout << "    Bits per Value: " << std::fixed << std::setprecision(2) << linear.bits_per_value << std::endl;
    std::cout << "    Compression Time: " << std::fixed << std::setprecision(2) << linear.compression_time << " μs/block" << std::endl;
    std::cout << "    Decompression Time: " << std::fixed << std::setprecision(2) << linear.decompression_time << " μs/block" << std::endl;
    
    std::cout << "  🎯 IMPROVEMENT ANALYSIS:" << std::endl;
    std::cout << "    Compression Ratio: " << (ratio_improvement > 0 ? "+" : "") << std::fixed << std::setprecision(2) << ratio_improvement << "% " << (ratio_improvement > 0 ? "BETTER" : "WORSE") << std::endl;
    std::cout << "    Compression Time: " << (time_overhead > 0 ? "+" : "") << std::fixed << std::setprecision(2) << time_overhead << "% overhead" << std::endl;
    std::cout << "    Decompression Time: " << (decomp_overhead > 0 ? "+" : "") << std::fixed << std::setprecision(2) << decomp_overhead << "% overhead" << std::endl;
  }
  
  std::cout << "\n🏆 Geolife optimal precision test (100k data points) completed!" << std::endl;
  std::cout << "Results saved to: ../test/geolife_optimal_precision_100k.csv" << std::endl;
}
