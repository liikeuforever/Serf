#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iomanip>

#include "src/compressor/mbr_geo_qt_compressor.h"
#include "src/decompressor/mbr_geo_qt_decompressor.h"
#include "src/compressor/geo_qt_compressor.h"
#include "src/decompressor/geo_qt_decompressor.h"

/**
 * MBR 优化版 Geo-Qt 测试程序
 * 对比原始版本和 MBR 优化版本的性能
 */

struct TestResult {
  std::string algorithm_name;
  size_t original_size;
  size_t compressed_size;
  double compression_ratio;
  double compression_throughput;
  double decompression_throughput;
  double max_error_meters;
  double avg_error_meters;
  int required_bits;
};

class MbrPerformanceTester {
 public:
  // 加载测试数据
  std::vector<std::pair<double, double>> LoadTestData(const std::string& filename, size_t max_points = 0);
  
  // 测试原始 Geo-Qt 算法
  TestResult TestOriginalGeoQt(const std::vector<std::pair<double, double>>& data);
  
  // 测试 MBR 优化版 Geo-Qt 算法
  TestResult TestMbrGeoQt(const std::vector<std::pair<double, double>>& data);
  
  // 计算地理距离误差
  double CalculateHaversineDistance(double lat1, double lon1, double lat2, double lon2);
  
  // 运行完整测试套件
  void RunFullTestSuite(const std::string& data_file, size_t max_points = 0);
  
  // 打印测试结果
  void PrintResults(const std::vector<TestResult>& results);

 private:
  double CalculateCompressionRatio(size_t original, size_t compressed);
  double CalculateThroughput(size_t points, double time_seconds);
};

std::vector<std::pair<double, double>> MbrPerformanceTester::LoadTestData(const std::string& filename, size_t max_points) {
  std::vector<std::pair<double, double>> data;
  std::ifstream file(filename);
  
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open data file: " + filename);
  }
  
  std::string line;
  size_t count = 0;
  
  while (std::getline(file, line) && (max_points == 0 || count < max_points)) {
    size_t comma_pos = line.find(',');
    if (comma_pos != std::string::npos) {
      double longitude = std::stod(line.substr(0, comma_pos));
      double latitude = std::stod(line.substr(comma_pos + 1));
      data.push_back({latitude, longitude});
      count++;
    }
  }
  
  std::cout << "Loaded " << data.size() << " data points from " << filename << std::endl;
  return data;
}

TestResult MbrPerformanceTester::TestOriginalGeoQt(const std::vector<std::pair<double, double>>& data) {
  TestResult result;
  result.algorithm_name = "Original Geo-Qt";
  
  result.original_size = data.size() * 16;
  
  // 压缩测试
  auto start_time = std::chrono::high_resolution_clock::now();
  
  GeoQtCompressor compressor(11);
  for (const auto& point : data) {
    compressor.AddPoint(point.first, point.second);
  }
  std::vector<uint8_t> compressed_data = compressor.Finish();
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto compression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1e6;
  
  result.compressed_size = compressed_data.size();
  result.compression_ratio = CalculateCompressionRatio(result.original_size, result.compressed_size);
  result.compression_throughput = CalculateThroughput(data.size(), compression_time);
  result.required_bits = 55;  // 11位精度 × 5位/字符
  
  // 解压测试
  start_time = std::chrono::high_resolution_clock::now();
  
  GeoQtDecompressor decompressor(11);
  std::vector<std::pair<double, double>> decompressed_data = decompressor.Decompress(compressed_data);
  
  end_time = std::chrono::high_resolution_clock::now();
  auto decompression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1e6;
  
  result.decompression_throughput = CalculateThroughput(data.size(), decompression_time);
  
  // 计算误差
  double total_error = 0.0;
  double max_error = 0.0;
  
  for (size_t i = 0; i < data.size(); ++i) {
    double error = CalculateHaversineDistance(
      data[i].first, data[i].second,
      decompressed_data[i].first, decompressed_data[i].second
    );
    total_error += error;
    max_error = std::max(max_error, error);
  }
  
  result.avg_error_meters = total_error / data.size();
  result.max_error_meters = max_error;
  
  return result;
}

TestResult MbrPerformanceTester::TestMbrGeoQt(const std::vector<std::pair<double, double>>& data) {
  TestResult result;
  result.algorithm_name = "MBR Geo-Qt";
  
  result.original_size = data.size() * 16;
  
  // 压缩测试
  auto start_time = std::chrono::high_resolution_clock::now();
  
  MbrGeoQtCompressor compressor(data);
  for (const auto& point : data) {
    compressor.AddPoint(point.first, point.second);
  }
  std::vector<uint8_t> compressed_data = compressor.Finish();
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto compression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1e6;
  
  result.compressed_size = compressed_data.size();
  result.compression_ratio = CalculateCompressionRatio(result.original_size, result.compressed_size);
  result.compression_throughput = CalculateThroughput(data.size(), compression_time);
  result.required_bits = compressor.GetRequiredBits();
  
  // 解压测试
  start_time = std::chrono::high_resolution_clock::now();
  
  MbrGeoQtDecompressor decompressor;
  std::vector<std::pair<double, double>> decompressed_data = decompressor.Decompress(compressed_data);
  
  end_time = std::chrono::high_resolution_clock::now();
  auto decompression_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1e6;
  
  result.decompression_throughput = CalculateThroughput(data.size(), decompression_time);
  
  // 计算误差
  double total_error = 0.0;
  double max_error = 0.0;
  
  for (size_t i = 0; i < data.size(); ++i) {
    double error = CalculateHaversineDistance(
      data[i].first, data[i].second,
      decompressed_data[i].first, decompressed_data[i].second
    );
    total_error += error;
    max_error = std::max(max_error, error);
  }
  
  result.avg_error_meters = total_error / data.size();
  result.max_error_meters = max_error;
  
  return result;
}

double MbrPerformanceTester::CalculateHaversineDistance(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0;  // 地球半径（米）
  
  double dlat = (lat2 - lat1) * M_PI / 180.0;
  double dlon = (lon2 - lon1) * M_PI / 180.0;
  
  double a = std::sin(dlat/2) * std::sin(dlat/2) +
             std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
             std::sin(dlon/2) * std::sin(dlon/2);
  
  double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
  
  return R * c;
}

double MbrPerformanceTester::CalculateCompressionRatio(size_t original, size_t compressed) {
  return static_cast<double>(compressed) / static_cast<double>(original);
}

double MbrPerformanceTester::CalculateThroughput(size_t points, double time_seconds) {
  return time_seconds > 0 ? static_cast<double>(points) / time_seconds : 0.0;
}

void MbrPerformanceTester::RunFullTestSuite(const std::string& data_file, size_t max_points) {
  std::cout << "=== MBR 优化版 Geo-Qt 性能测试套件 ===" << std::endl;
  std::cout << "数据文件: " << data_file << std::endl;
  if (max_points > 0) {
    std::cout << "最大点数: " << max_points << std::endl;
  }
  std::cout << std::endl;
  
  // 加载数据
  auto data = LoadTestData(data_file, max_points);
  
  std::vector<TestResult> results;
  
  // 测试原始 Geo-Qt
  std::cout << "测试原始 Geo-Qt 算法..." << std::endl;
  results.push_back(TestOriginalGeoQt(data));
  
  // 测试 MBR 优化版 Geo-Qt
  std::cout << "测试 MBR 优化版 Geo-Qt 算法..." << std::endl;
  results.push_back(TestMbrGeoQt(data));
  
  // 打印结果
  PrintResults(results);
}

void MbrPerformanceTester::PrintResults(const std::vector<TestResult>& results) {
  std::cout << "\n=== 测试结果 ===" << std::endl;
  std::cout << std::left << std::setw(20) << "算法" 
            << std::setw(12) << "压缩率" 
            << std::setw(15) << "压缩吞吐量" 
            << std::setw(15) << "解压吞吐量"
            << std::setw(12) << "最大误差"
            << std::setw(12) << "平均误差"
            << std::setw(10) << "位数" << std::endl;
  std::cout << std::string(100, '-') << std::endl;
  
  for (const auto& result : results) {
    std::cout << std::left << std::setw(20) << result.algorithm_name
              << std::setw(12) << std::fixed << std::setprecision(4) << result.compression_ratio
              << std::setw(15) << std::fixed << std::setprecision(0) << result.compression_throughput
              << std::setw(15) << std::fixed << std::setprecision(0) << result.decompression_throughput
              << std::setw(12) << std::fixed << std::setprecision(2) << result.max_error_meters
              << std::setw(12) << std::fixed << std::setprecision(2) << result.avg_error_meters
              << std::setw(10) << result.required_bits << std::endl;
  }
  
  std::cout << "\n=== 详细统计 ===" << std::endl;
  for (const auto& result : results) {
    std::cout << "\n" << result.algorithm_name << ":" << std::endl;
    std::cout << "  原始大小: " << result.original_size << " 字节" << std::endl;
    std::cout << "  压缩大小: " << result.compressed_size << " 字节" << std::endl;
    std::cout << "  压缩率: " << std::fixed << std::setprecision(4) << result.compression_ratio << std::endl;
    std::cout << "  压缩吞吐量: " << std::fixed << std::setprecision(0) << result.compression_throughput << " 点/秒" << std::endl;
    std::cout << "  解压吞吐量: " << std::fixed << std::setprecision(0) << result.decompression_throughput << " 点/秒" << std::endl;
    std::cout << "  最大误差: " << std::fixed << std::setprecision(2) << result.max_error_meters << " 米" << std::endl;
    std::cout << "  平均误差: " << std::fixed << std::setprecision(2) << result.avg_error_meters << " 米" << std::endl;
    std::cout << "  所需位数: " << result.required_bits << " 位" << std::endl;
  }
}

int main() {
  MbrPerformanceTester tester;
  
  // 运行完整测试 - 使用完整数据集
  tester.RunFullTestSuite("../test/data_set/Geolife_100k_longitude_latitude.csv", 0);
  
  return 0;
}
