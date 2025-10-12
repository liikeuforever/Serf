# TrajCompress-SP: 基于多预测器切换的GPS轨迹压缩算法

## 算法概述

TrajCompress-SP (Trajectory Compression with Switched Predictors) 是一个基于Serf-QT优化的GPS轨迹有损压缩算法。该算法通过**多预测器并行计算**和**动态预测器选择**，在保持1e-5精度（约1.1米）的前提下，实现了比Serf-QT更高的压缩比。

## 核心创新

### 1. 多预测器系统
- **LDR (Linear Dead Reckoning)**: 线性航位推算，假设匀速直线运动
- **CP (Curve Predictor)**: 曲线预测，假设匀加速运动，捕捉转弯趋势  
- **ZP (Zero Predictor)**: 零预测，Serf-QT风格的静态预测

### 2. 动态预测器选择
对每个GPS点，算法并行计算三个预测器的预测结果，选择误差最小的预测器进行编码。

### 3. 流式处理
- 每个点来了就立即编码传输，无需等待
- 保持与解压器的状态同步
- 支持实时压缩和解压

## 算法架构

```
GPS点 → 并行预测器 → 最优选择 → 量化编码 → 压缩输出
  ↓         ↓           ↓         ↓
LDR/CP/ZP  误差计算   预测器标志   Elias Gamma
```

## 性能测试结果

### 测试环境
- **数据集**: Geolife GPS轨迹数据
- **误差阈值**: 1e-5度 (约1.1米)
- **测试规模**: 1000-100000点

### 压缩性能对比

| 数据量 | TrajCompress-SP | Serf-QT | 改进幅度 |
|--------|----------------|---------|----------|
| 1,000点 | 9.03 bits/点 | 9.84 bits/点 | **8.2%** |
| 10,000点 | 8.27 bits/点 | 8.79 bits/点 | **6.0%** |
| 50,000点 | 8.07 bits/点 | 9.70 bits/点 | **16.7%** |
| 100,000点 | 8.16 bits/点 | 9.57 bits/点 | **14.8%** |

### 预测器使用分布 (100,000点)
- **LDR (线性预测)**: 62.7% - 主导预测器，适合高采样率轨迹
- **CP (曲线预测)**: 16.4% - 处理转弯和变速运动
- **ZP (零预测)**: 20.8% - 处理静止和突变点

### 压缩比分析
- **TrajCompress-SP**: 15.69:1
- **Serf-QT**: 13.37:1
- **提升**: 17.4%

## 技术特点

### 1. 预测精度
- 平均预测误差: 8.2e-5度 (9.1米)
- P50中位数误差: 2.8e-5度 (3.2米)
- P99误差: 4.0e-4度 (44.8米)

### 2. 编码效率
- 预测器标志: 2.00 bits/点 (24.5%)
- 量化数据: 6.15 bits/点 (75.5%)
- 总编码: 8.15 bits/点

### 3. 实时性能
- 压缩速度: 极快 (毫秒级)
- 解压速度: 极快 (毫秒级)
- 内存占用: 低 (仅保留3个历史状态)

## 文件结构

```
src/compressor/
├── trajcompress_sp_compressor.h    # 压缩器头文件
├── trajcompress_sp_compressor.cc   # 压缩器实现
└── serf_qt_compressor.*            # Serf-QT基准实现

trajcompress_sp_test.cc             # 测试程序
build_trajcompress_sp_test.sh       # 编译脚本
```

## 使用方法

### 编译
```bash
./build_trajcompress_sp_test.sh
```

### 运行测试
```bash
# 基本测试
./trajcompress_sp_test

# 指定参数
./trajcompress_sp_test <数据集路径> <测试点数> <误差阈值>

# 示例
./trajcompress_sp_test test/data_set/Geolife_100k_longitude_latitude.csv 50000 1e-5
```

### API使用
```cpp
// 压缩
TrajCompressSPCompressor compressor(block_size, epsilon);
compressor.AddGpsPoint(GpsPoint(lon, lat));
compressor.Close();
Array<uint8_t> compressed_data = compressor.GetCompressedData();

// 解压
TrajCompressSPDecompressor decompressor(compressed_data.begin(), compressed_data.length());
GpsPoint point;
while (decompressor.ReadNextPoint(point)) {
    // 处理解压后的点
}
```

## 算法优势

1. **更高压缩比**: 相比Serf-QT提升6-17%
2. **智能预测**: 多预测器自适应选择
3. **流式处理**: 实时压缩，无需等待
4. **精度保证**: 满足1e-5度误差要求
5. **高效编码**: Elias Gamma + ZigZag编码

## 适用场景

- GPS轨迹数据压缩
- 实时位置数据传输
- 移动设备存储优化
- 轨迹数据备份
- 位置服务应用

## 技术细节

### 量化策略
- 量化步长: 2 × epsilon
- 编码方式: ZigZag + Elias Gamma
- 误差控制: 严格满足精度要求

### 状态管理
- 历史状态: 保留最近3个点
- 状态同步: 压缩器与解压器完全同步
- 内存效率: 最小化状态存储

### 预测器选择
- 误差计算: 欧氏距离
- 选择策略: 最小误差原则
- 编码开销: 每点2 bits预测器标志

## 结论

TrajCompress-SP算法成功实现了基于多预测器切换的GPS轨迹压缩，在保持高精度的同时显著提升了压缩比。该算法特别适合高采样率的GPS轨迹数据，能够有效处理线性运动、转弯运动和静止状态，为GPS数据压缩提供了新的解决方案。
