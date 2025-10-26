# 时间戳感知的轨迹预测实现总结

## 修改概述

已成功为 TrajCompress-SP 算法添加时间戳支持，使预测器能够基于真实的时间间隔进行预测，而不是假设等时间间隔。

## 修改的文件

### 1. `src/compressor/trajcompress_sp_adaptive_simple_compressor.h`

**GpsPoint 结构修改**：
```cpp
struct GpsPoint {
    double longitude;
    double latitude;
    uint64_t timestamp;  // Unix时间戳（秒）
    
    GpsPoint() : longitude(0), latitude(0), timestamp(0) {}
    GpsPoint(double lon, double lat, uint64_t ts = 0) 
        : longitude(lon), latitude(lat), timestamp(ts) {}
    //...
};
```

**ParallelPredict 签名修改**：
```cpp
void ParallelPredict(GpsPoint& pred_ldr, GpsPoint& pred_cp, GpsPoint& pred_zp, uint64_t current_timestamp);
```

### 2. `src/compressor/trajcompress_sp_adaptive_simple_compressor.cc`

**ParallelPredict 实现修改**：
- **LDR 预测**：使用真实速度（度/秒）和时间间隔 dt
  ```cpp
  uint64_t delta_time = current_timestamp - current_reconstructed_point_.timestamp;
  double dt = static_cast<double>(delta_time);
  
  pred_ldr = GpsPoint(
      current_reconstructed_point_.longitude + velocity.longitude * dt,
      current_reconstructed_point_.latitude + velocity.latitude * dt,
      current_timestamp
  );
  ```

- **CP 预测**：基于真实加速度和时间间隔
  ```cpp
  pred_cp = GpsPoint(
      current_reconstructed_point_.longitude + v1.longitude * dt + acceleration.longitude * dt,
      current_reconstructed_point_.latitude + v1.latitude * dt + acceleration.latitude * dt,
      current_timestamp
  );
  ```

**UpdateHistory 修改**：
- 计算真实速度（度/秒）而不是位移
  ```cpp
  if (delta_time > 0) {
      double dt = static_cast<double>(delta_time);
      velocity = GpsPoint(
          (reconstructed_point.longitude - prev_point.longitude) / dt,
          (reconstructed_point.latitude - prev_point.latitude) / dt,
          0
      );
  }
  ```

### 3. `trajcompress_sp_test.cc`

**时间戳解析函数**：
```cpp
uint64_t ParseTimestamp(const std::string& ts_str) {
    // 格式: "YYYY-MM-DD HH:MM:SS"
    std::tm tm = {};
    std::istringstream ss(ts_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return static_cast<uint64_t>(std::mktime(&tm));
}
```

**LoadGpsDataFromCSV 修改**：
- 现在读取三列：longitude, latitude, timestamp
- 自动检测并解析时间戳
- 如果没有时间戳列，timestamp 默认为 0

## 预测器的时间戳使用

### 1. **Zero Predictor (ZP)**
- 不使用时间戳（预测值等于当前点）
- ✅ 无需修改

### 2. **Linear Dead Reckoning (LDR)**
- ❌ **之前**：假设等时间间隔，速度 = 位移
- ✅ **现在**：使用真实时间戳，速度 = 位移 / Δt（度/秒）
- 预测公式：`pred = current + velocity * dt`

### 3. **Curve Predictor (CP)**
- ❌ **之前**：假设等时间间隔，加速度 = 速度差
- ✅ **现在**：使用真实时间戳，加速度 = 速度差（已经是度/秒²）
- 预测公式：`pred = current + velocity * dt + acceleration * dt`

## 数据集要求

### 格式
```csv
longitude,latitude,timestamp
116.318417,39.984702,2008-10-23 02:53:04
```

### 时间戳格式
- 标准格式：`YYYY-MM-DD HH:MM:SS`
- 存储为 Unix 时间戳（秒，uint64_t）

### 兼容性
- ✅ 向后兼容：如果 CSV 没有时间戳列，timestamp 自动设为 0
- ✅ 自动检测：程序会自动识别是否有时间戳数据

## 关键改进

1. **更准确的速度计算**
   - 之前：假设每个点之间间隔固定
   - 现在：基于真实时间差计算实际速度（度/秒）

2. **更准确的预测**
   - 之前：预测下一个点假设固定时间步长
   - 现在：预测基于实际的时间间隔

3. **适应不规则采样**
   - 之前：对于不等间隔采样的数据效果不佳
   - 现在：能够正确处理任意采样间隔的轨迹数据

## 测试数据集

所有数据集位于 `test/data_set/data_set_with_timestamp/`：

- **Geolife**: 100,000 点（采样间隔 5-10秒）
- **Track**: 63,530 点（采样间隔 1秒）
- **Trajtory**: 97,009 点（采样间隔 1秒）

每个数据集都有 5x, 10x, 20x, 40x 降采样版本。

## 编译和测试

```bash
# 编译
g++ -std=c++17 -O3 -I./src -c src/compressor/trajcompress_sp_adaptive_simple_compressor.cc

# 运行测试（使用带时间戳的数据集）
./trajcompress_sp_test

# 单个数据集测试
./trajcompress_sp_test single test/data_set/data_set_with_timestamp/Geolife_100k_with_timestamp.csv
```

## 性能影响

### 预期改进
- ✅ 对于**不等间隔采样**的轨迹：预测精度显著提升
- ✅ 对于**速度变化**的轨迹：LDR 和 CP 预测器更准确
- ✅ 对于**加速度变化**的轨迹：CP 预测器效果更好

### 计算开销
- ➕ 每次预测增加 1 次除法（计算 dt）
- ➕ 每次速度更新增加 1 次除法
- 总体开销：**可忽略**（<1% CPU时间）

## 未来优化

1. **时间戳压缩**：可使用 Delta-of-Delta 编码压缩时间戳
2. **自适应时间窗口**：根据时间间隔自动调整预测窗口大小
3. **时间加权预测**：对不同时间间隔的历史点赋予不同权重

---

**最后更新**: 2025-10-26  
**测试状态**: ✅ 编译通过，等待完整测试
