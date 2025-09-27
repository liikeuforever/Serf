# GPS轨迹压缩算法 - 基于固定步长与几何剪枝的率-失真优化

## 算法概述

本算法是一种面向GPS经纬度浮点数流的、有损的、带误差上界的流式压缩算法。其核心是在满足用户定义的单一误差上限 `E_max` 的前提下，通过高效的启发式方法，近似求解率-失真优化（Rate-Distortion Optimization）问题。

### 核心特性

1. **高效的近似最优求解**: 基于几何剪枝的"黄金候选"评估策略，将每个点的核心计算复杂度降低到 **O(1)**
2. **基于先验知识的固定步长**: 针对Geolife数据集优化的固定参数，无需动态计算或用户猜测
3. **高级预测与校正分离**: 明确分解为预测和校正两个阶段，逻辑清晰且性能优越
4. **在抽象经纬度平面操作**: 将经纬度视为笛卡尔坐标进行计算，保证极致的计算速度

## 输入输出格式

- **输入**: 经纬度坐标对，格式为 `longitude,latitude`（例如：`116.321572,40.008773`）
- **输出**: 重构的经纬度坐标对，相同格式
- **误差保证**: 所有重构点与原始点的欧几里得距离不超过用户指定的 `E_max`

## 使用方法

### 1. 基本使用

```cpp
#include "compressor/serf_qt_gps_trajectory_compressor.h"
#include "decompressor/serf_qt_gps_trajectory_decompressor.h"

// 创建压缩器
int block_size = 1000;
double max_error = 1e-4;  // 最大允许误差（度）
SerfQtGpsTrajectoryCompressor compressor(block_size, max_error);

// 添加GPS轨迹点
SerfQtGpsTrajectoryCompressor::GpsPoint point(116.321572, 40.008773);
compressor.AddGpsPoint(point);
// ... 添加更多点

// 完成压缩
compressor.Close();
Array<uint8_t> compressed_data = compressor.compressed_bytes();

// 解压缩
SerfQtGpsTrajectoryDecompressor decompressor(compressed_data);
auto reconstructed_point = decompressor.DecompressNextPoint();
```

### 2. 从CSV文件压缩

```cpp
// 从CSV文件读取GPS数据
std::ifstream file("gps_data.csv");
std::string line;
while (std::getline(file, line)) {
    std::stringstream ss(line);
    std::string lon_str, lat_str;
    if (std::getline(ss, lon_str, ',') && std::getline(ss, lat_str)) {
        double longitude = std::stod(lon_str);
        double latitude = std::stod(lat_str);
        compressor.AddGpsPoint(SerfQtGpsTrajectoryCompressor::GpsPoint(longitude, latitude));
    }
}
```

## 算法参数

### 固定参数（基于Geolife数据集优化）

- **坐标精度基准**: `ε_pos = 10^-5` 度
- **速度量化步长**: `ε_v = 2.5 × 10^-6` 度/步长
- **角度量化步长**: `ε_θ = 0.033` 弧度

### 用户可配置参数

- **块大小** (`block_size`): 建议值 1000
- **最大误差** (`E_max`): 根据应用需求设定，单位为度

## 压缩策略

算法使用四种校正策略：

1. **零校正** (`FLAG_ZERO_CORR`): 预测足够准确，无需校正
2. **纯速度校正** (`FLAG_V_ONLY`): 仅调整移动距离
3. **纯角度校正** (`FLAG_THETA_ONLY`): 仅调整移动方向
4. **完全校正** (`FLAG_BOTH`): 同时调整距离和方向

## 编译和运行演示

### 编译

确保项目已正确配置CMake，然后编译：

```bash
cd build
make gps_trajectory_compression_demo
```

### 运行演示

```bash
# 使用示例数据
./demo/gps_trajectory_compression_demo

# 使用自定义CSV文件
./demo/gps_trajectory_compression_demo path/to/your/gps_data.csv
```

### CSV文件格式

```csv
longitude,latitude
116.321572,40.008773
116.321580,40.008781
116.321588,40.008789
...
```

## 性能特点

- **压缩率**: 通常可达到 5:1 到 20:1 的压缩率（取决于轨迹复杂度和误差阈值）
- **计算复杂度**: 每个点 O(1) 的平均时间复杂度
- **内存使用**: 固定大小的历史状态缓存（默认8个状态）
- **误差保证**: 严格保证所有重构点在误差上界内

## 适用场景

- GPS轨迹数据存储和传输
- 车辆轨迹监控系统
- 移动设备位置数据压缩
- 地理信息系统（GIS）数据处理
- 物联网设备位置数据传输

## 注意事项

1. 算法假设输入的GPS点按时间顺序排列
2. 适用于连续的轨迹数据，不适合离散的随机位置点
3. 误差单位为度，在实际应用中需要根据地理位置转换为米
4. 算法在抽象平面上操作，未考虑地球曲率，适用于小范围区域

## 扩展和优化

- 可以根据不同数据集调整固定参数
- 支持更复杂的预测模型（如二阶、三阶运动模型）
- 可以添加自适应参数调整机制
- 支持多线程并行处理大规模数据集
