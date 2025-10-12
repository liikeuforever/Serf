#include "compressor/sfc_compressor.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <map>
#include <stdexcept>

SFCCompressor::SFCCompressor(CompressionMethod method, double max_error)
    : method_(method), max_error_(max_error) {
    
    // 根据最大误差计算所需的编码比特数（全球范围）
    encoding_bits_ = SpaceFillingCurve::CalculateGeoHashBits(max_error);
    
    // 为Hilbert曲线，确保比特数是偶数（order * 2）
    if (method_ == HILBERT_CURVE && encoding_bits_ % 2 != 0) {
        encoding_bits_++;
    }
    
    output_bit_stream_ = std::make_unique<OutputBitStream>(1024 * 1024); // 1MB初始缓冲
}

SFCCompressor::SFCCompressor(CompressionMethod method, int encoding_bits, 
                             const SpaceFillingCurve::BoundingBox& bbox)
    : method_(method), max_error_(0.0), encoding_bits_(encoding_bits), bbox_(bbox) {
    
    // 为Hilbert曲线，确保比特数是偶数（order * 2）
    if (method_ == HILBERT_CURVE && encoding_bits_ % 2 != 0) {
        encoding_bits_++;
        std::cerr << "警告：Hilbert曲线需要偶数比特，已调整为 " << encoding_bits_ << std::endl;
    }
    
    // 计算实际的max_error（用于header）
    if (method_ == MBR_GEOHASH || method_ == HILBERT_CURVE) {
        int n = (method_ == HILBERT_CURVE) ? (1 << (encoding_bits_ / 2)) : (1 << (encoding_bits_ / 2));
        double cell_width = bbox_.GetWidth() / n;
        double cell_height = bbox_.GetHeight() / n;
        max_error_ = sqrt(cell_width * cell_width + cell_height * cell_height);
    } else {
        // 标准GeoHash，全球范围
        double cell_size = 360.0 / (1 << (encoding_bits_ / 2));
        max_error_ = cell_size * sqrt(2.0);
    }
    
    output_bit_stream_ = std::make_unique<OutputBitStream>(1024 * 1024); // 1MB初始缓冲
}

void SFCCompressor::SetBoundingBox(const SpaceFillingCurve::BoundingBox& bbox) {
    bbox_ = bbox;
}

void SFCCompressor::AddPoint(const SpaceFillingCurve::GeoPoint& point) {
    // 对于MBR和Hilbert方法，MBR必须事先通过SetBoundingBox设置，不能动态更新
    if (first_point_ && (method_ == MBR_GEOHASH || method_ == HILBERT_CURVE)) {
        // 检查bbox是否已设置（如果min > max说明未设置）
        if (bbox_.min_lon > bbox_.max_lon) {
            std::cerr << "致命错误：对于MBR-GeoHash和Hilbert曲线，必须先调用SetBoundingBox()设置全局边界框！" << std::endl;
            std::cerr << "MBR必须通过预处理（扫描所有数据）获得，不能在压缩过程中动态计算。" << std::endl;
            throw std::runtime_error("未设置边界框");
        }
    }
    
    if (first_point_) {
        // 写入头部
        first_point_ = false;
        WriteHeader();
    }
    
    // 编码点
    uint64_t code = EncodePoint(point);
    
    if (stats_.total_points == 0) {
        // 第一个点：写入完整编码
        EncodeFirstPoint(code);
    } else {
        // 后续点：编码差值
        int64_t diff = static_cast<int64_t>(code) - static_cast<int64_t>(previous_code_);
        EncodeDifference(diff);
    }
    
    previous_code_ = code;
    stats_.total_points++;
}

Array<uint8_t> SFCCompressor::GetCompressedData() {
    // 在头部写入点数（在所有数据之后）
    // 我们需要重新组织，在WriteHeader之后立即写入点数
    // 由于已经开始压缩，这里在close时回写点数比较困难
    // 简化方案：解压时用try-catch处理
    
    output_bit_stream_->Flush();
    int byte_length = (compressed_size_in_bits_ + 7) / 8;
    return output_bit_stream_->GetBuffer(byte_length);
}

std::string SFCCompressor::GetMethodName() const {
    switch (method_) {
        case STANDARD_GEOHASH: return "Standard GeoHash";
        case MBR_GEOHASH: return "MBR-Optimized GeoHash";
        case HILBERT_CURVE: return "Hilbert Curve";
        default: return "Unknown";
    }
}

void SFCCompressor::Reset() {
    first_point_ = true;
    previous_code_ = 0;
    compressed_size_in_bits_ = 0;
    bbox_ = SpaceFillingCurve::BoundingBox();
    stats_ = Statistics();
    output_bit_stream_ = std::make_unique<OutputBitStream>(1024 * 1024);
}

void SFCCompressor::WriteHeader() {
    // 写入头部信息
    // 1. 压缩方法 (2 bits)
    compressed_size_in_bits_ += output_bit_stream_->WriteInt(static_cast<int>(method_), 2);
    
    // 2. 最大误差 (64 bits)
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(Double::DoubleToLongBits(max_error_), 64);
    
    // 3. 编码比特数 (8 bits, 最大255)
    compressed_size_in_bits_ += output_bit_stream_->WriteInt(encoding_bits_, 8);
    
    // 4. 如果是MBR或Hilbert，写入边界框 (4 * 64 = 256 bits)
    if (method_ == MBR_GEOHASH || method_ == HILBERT_CURVE) {
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(
            Double::DoubleToLongBits(bbox_.min_lon), 64);
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(
            Double::DoubleToLongBits(bbox_.max_lon), 64);
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(
            Double::DoubleToLongBits(bbox_.min_lat), 64);
        compressed_size_in_bits_ += output_bit_stream_->WriteLong(
            Double::DoubleToLongBits(bbox_.max_lat), 64);
        
        stats_.header_bits = 2 + 64 + 8 + 256 + 32; // 362 bits (包括点数)
    } else {
        stats_.header_bits = 2 + 64 + 8 + 32; // 106 bits (包括点数)
    }
}

uint64_t SFCCompressor::EncodePoint(const SpaceFillingCurve::GeoPoint& point) {
    switch (method_) {
        case STANDARD_GEOHASH:
            return SpaceFillingCurve::EncodeStandardGeoHash(point, encoding_bits_);
        
        case MBR_GEOHASH:
            return SpaceFillingCurve::EncodeMBRGeoHash(point, encoding_bits_, bbox_);
        
        case HILBERT_CURVE: {
            int order = encoding_bits_ / 2;
            return SpaceFillingCurve::EncodeHilbert(point, order, bbox_);
        }
        
        default:
            return 0;
    }
}

void SFCCompressor::EncodeFirstPoint(uint64_t code) {
    // 写入第一个点的完整编码
    compressed_size_in_bits_ += output_bit_stream_->WriteLong(code, encoding_bits_);
    stats_.encoding_bits_used += encoding_bits_;
}

void SFCCompressor::EncodeDifference(int64_t diff) {
    // 使用ZigZag + Elias Gamma编码差值
    uint64_t zigzag = ZigZagCodec::Encode(diff);
    int bits_used = EliasGammaCodec::Encode(zigzag + 1, output_bit_stream_.get());
    
    compressed_size_in_bits_ += bits_used;
    stats_.diff_encoding_bits += bits_used;
    
    // 收集统计信息
    stats_.diff_values.push_back(diff);
    stats_.diff_bit_lengths.push_back(bits_used);
    
    if (diff > stats_.max_diff) stats_.max_diff = diff;
    if (diff < stats_.min_diff) stats_.min_diff = diff;
}

void SFCCompressor::Statistics::Calculate() {
    if (total_points == 0) return;
    
    int total_bits = header_bits + encoding_bits_used + diff_encoding_bits;
    avg_bits_per_point = static_cast<double>(total_bits) / total_points;
    
    // 原始数据：每个点2个double = 128 bits
    double original_bits = total_points * 128.0;
    compression_ratio = original_bits / total_bits;
    
    if (!diff_values.empty()) {
        double sum = 0;
        for (int64_t diff : diff_values) {
            sum += std::abs(diff);
        }
        avg_diff_magnitude = sum / diff_values.size();
    }
}

void SFCCompressor::Statistics::Print() const {
    std::cout << "\n=== 压缩统计信息 ===" << std::endl;
    std::cout << "总点数: " << total_points << std::endl;
    
    int total_bits = header_bits + encoding_bits_used + diff_encoding_bits;
    std::cout << "\n编码开销分解:" << std::endl;
    std::cout << "  头部开销:       " << std::setw(8) << header_bits << " bits (" 
              << std::fixed << std::setprecision(2) << (100.0 * header_bits / total_bits) << "%)" << std::endl;
    std::cout << "  第一个点编码:   " << std::setw(8) << encoding_bits_used << " bits (" 
              << (100.0 * encoding_bits_used / total_bits) << "%)" << std::endl;
    std::cout << "  差值编码总开销: " << std::setw(8) << diff_encoding_bits << " bits (" 
              << (100.0 * diff_encoding_bits / total_bits) << "%)" << std::endl;
    std::cout << "  总编码开销:     " << std::setw(8) << total_bits << " bits" << std::endl;
    
    std::cout << "\n压缩效率:" << std::endl;
    std::cout << "  平均每点编码: " << std::setprecision(2) << avg_bits_per_point << " bits/点" << std::endl;
    std::cout << "  压缩比: " << std::setprecision(2) << compression_ratio << ":1" << std::endl;
    std::cout << "  压缩率: " << std::setprecision(1) << (100.0 / compression_ratio) << "%" << std::endl;
    
    if (!diff_values.empty()) {
        std::cout << "\n差值编码分析:" << std::endl;
        std::cout << "  差值范围: [" << min_diff << ", " << max_diff << "]" << std::endl;
        std::cout << "  平均差值绝对值: " << std::setprecision(2) << avg_diff_magnitude << std::endl;
        std::cout << "  平均差值编码长度: " << std::setprecision(2) 
                  << (static_cast<double>(diff_encoding_bits) / diff_values.size()) << " bits" << std::endl;
        
        // 差值编码长度分布
        std::map<int, int> bit_dist;
        for (int bits : diff_bit_lengths) {
            bit_dist[bits]++;
        }
        
        std::cout << "\n  差值编码长度分布 (前10个):" << std::endl;
        int count = 0;
        for (const auto& pair : bit_dist) {
            std::cout << "    " << pair.first << " bits: " << pair.second << " 次 (" 
                      << std::setprecision(1) << (100.0 * pair.second / diff_values.size()) << "%)" << std::endl;
            if (++count >= 10) break;
        }
        
        // 差值分布（前10个最常见的）
        std::map<int64_t, int> diff_dist;
        for (int64_t diff : diff_values) {
            diff_dist[diff]++;
        }
        
        std::vector<std::pair<int, int64_t>> diff_freq;
        for (const auto& pair : diff_dist) {
            diff_freq.emplace_back(pair.second, pair.first);
        }
        std::sort(diff_freq.rbegin(), diff_freq.rend());
        
        std::cout << "\n  最常见的差值 (前10个):" << std::endl;
        for (int i = 0; i < std::min(10, (int)diff_freq.size()); ++i) {
            std::cout << "    diff=" << diff_freq[i].second << ": " << diff_freq[i].first << " 次 ("
                      << std::setprecision(1) << (100.0 * diff_freq[i].first / diff_values.size()) << "%)" << std::endl;
        }
    }
}

