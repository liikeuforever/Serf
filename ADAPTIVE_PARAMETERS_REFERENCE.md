# 📋 TrajCompress-SP-Adaptive 参数参考手册

## 🎯 构造函数参数

```cpp
TrajCompressSPAdaptiveCompressor(
    int block_size,           // 缓冲区大小
    double epsilon,          // 误差阈值（度）
    bool enable_adaptive,     // 启用动态参数调整
    int min_window,          // 最小窗口大小
    int max_window,          // 最大窗口大小
    int observe_window       // 观察窗口大小
);
```

## 📊 参数详解表

### 基础参数

| 参数名 | 类型 | 默认值 | 范围 | 作用 | 调优建议 |
|--------|------|--------|------|------|----------|
| `block_size` | `int` | 用户指定 | >0 | 缓冲区大小 | 根据数据量调整 |
| `epsilon` | `double` | 用户指定 | >0 | 误差阈值 | 根据精度要求调整 |
| `enable_adaptive` | `bool` | `true` | true/false | 启用动态调整 | 生产环境建议true |

### 动态参数系统

| 参数名 | 类型 | 默认值 | 范围 | 作用 | 调优建议 |
|--------|------|--------|------|------|----------|
| `min_window` | `int` | `32` | 16-64 | 最小窗口大小 | 简单轨迹用16，复杂轨迹用32 |
| `max_window` | `int` | `128` | 64-256 | 最大窗口大小 | 高采样率用192，低采样率用128 |
| `observe_window` | `int` | `256` | 128-512 | 观察窗口大小 | 实时压缩用128，离线压缩用256 |

### 内部固定参数

| 参数名 | 类型 | 值 | 作用 | 说明 |
|--------|------|-----|------|------|
| `kSwitchCost` | `int` | `4` | 模式切换成本 | 固定值：'111' + '0'/'1' |
| `kEvaluationInterval` | `int` | `16` | 评估间隔 | 每16点评估一次模式切换 |
| `kClearWindowAfterSwitch` | `bool` | `false` | 切换后清空窗口 | 保持窗口连续性 |
| `kSlidingWindowSize` | `int` | `1000` | Huffman滑动窗口 | 用于动态Huffman编码 |
| `kMaxHistorySize` | `int` | `3` | 历史状态数量 | 支持三阶预测 |

### 动态调整参数

| 参数名 | 类型 | 初始值 | 调整范围 | 调整依据 | 公式 |
|--------|------|--------|----------|----------|------|
| `kCostWindowSize` | `int` | `(min+max)/2` | [min_window, max_window] | 预测器流失率 | `min + (max-min) * churn_rate` |
| `kStabilityMargin` | `int` | `1` | [1, 4] | 成本差标准差 | `4 - 3 * normalized_stddev` |

## 🎛️ 配置方案

### 方案1：通用配置（推荐）

```cpp
TrajCompressSPAdaptiveCompressor compressor(
    data_size,      // 根据数据量调整
    epsilon,        // 根据精度要求调整
    true,           // 启用动态调整
    32,             // 中等最小窗口
    128,            // 中等最大窗口
    256             // 标准观察窗口
);
```

**适用场景**：大多数应用场景
**特点**：平衡性能和响应速度

### 方案2：高采样率优化

```cpp
TrajCompressSPAdaptiveCompressor compressor(
    data_size,
    epsilon,
    true,
    16,             // 小最小窗口（快速响应）
    192,            // 大最大窗口（长期观察）
    256             // 标准观察窗口
);
```

**适用场景**：原始高采样率GPS数据
**特点**：针对Geolife、Track原始数据优化

### 方案3：实时压缩

```cpp
TrajCompressSPAdaptiveCompressor compressor(
    data_size,
    epsilon,
    true,
    8,              // 很小最小窗口
    64,             // 较小最大窗口
    128             // 较小观察窗口
);
```

**适用场景**：实时流式压缩
**特点**：快速响应，低延迟

### 方案4：离线压缩

```cpp
TrajCompressSPAdaptiveCompressor compressor(
    data_size,
    epsilon,
    true,
    64,             // 大最小窗口
    256,            // 很大最大窗口
    512             // 大观察窗口
);
```

**适用场景**：离线批量压缩
**特点**：最优质量，高压缩比

## 🔧 参数调优指南

### 窗口大小调优

| 轨迹类型 | min_window | max_window | 说明 |
|----------|------------|------------|------|
| **直线轨迹** | 16-32 | 64-128 | 快速锁定LDR-Only模式 |
| **复杂轨迹** | 32-64 | 128-256 | 长期观察多预测器优势 |
| **混合轨迹** | 24-48 | 96-192 | 平衡响应和稳定性 |

### 观察窗口调优

| 应用场景 | observe_window | 说明 |
|----------|----------------|------|
| **实时压缩** | 128-256 | 快速参数调整 |
| **离线压缩** | 256-512 | 稳定参数调整 |
| **混合场景** | 256 | 标准配置 |

### 动态调整效果

| 参数 | 调整方向 | 效果 | 适用场景 |
|------|----------|------|----------|
| `kCostWindowSize` | 增大 | 更稳定，响应慢 | 复杂轨迹 |
| `kCostWindowSize` | 减小 | 更灵敏，可能抖动 | 简单轨迹 |
| `kStabilityMargin` | 增大 | 更保守，切换少 | 竞争激烈 |
| `kStabilityMargin` | 减小 | 更激进，切换多 | 优劣明显 |

## 📈 性能监控

### 关键指标

| 指标 | 含义 | 理想值 | 监控方法 |
|------|------|--------|----------|
| **模式切换次数** | 模式切换频率 | 适中 | `stats_.mode_switch_count` |
| **LDR-Only使用率** | LDR-Only模式使用比例 | 30-70% | `stats_.ldr_only_mode_points / stats_.total_points` |
| **预测器分布** | 各预测器使用比例 | 平衡 | `stats_.ldr_count`, `stats_.cp_count`, `stats_.zp_count` |
| **压缩比** | 平均每点比特数 | 越小越好 | `compressed_size_in_bits_ / stats_.total_points` |

### 调优建议

| 现象 | 可能原因 | 调优建议 |
|------|----------|----------|
| **模式切换频繁** | 窗口太小，边际太小 | 增大min_window，增大kStabilityMargin |
| **模式切换很少** | 窗口太大，边际太大 | 减小max_window，减小kStabilityMargin |
| **LDR-Only使用率过高** | 轨迹过于简单 | 减小min_window，增大observe_window |
| **LDR-Only使用率过低** | 轨迹过于复杂 | 增大max_window，减小observe_window |

## 🎯 最佳实践

### 1. 参数初始化

```cpp
// 根据数据特性选择初始配置
if (is_high_sampling_rate) {
    // 高采样率：快速响应
    compressor = TrajCompressSPAdaptiveCompressor(size, epsilon, true, 16, 192, 256);
} else if (is_low_sampling_rate) {
    // 低采样率：稳定观察
    compressor = TrajCompressSPAdaptiveCompressor(size, epsilon, true, 32, 128, 256);
} else {
    // 通用配置
    compressor = TrajCompressSPAdaptiveCompressor(size, epsilon, true, 24, 160, 256);
}
```

### 2. 性能监控

```cpp
// 定期检查性能指标
auto stats = compressor.GetStats();
double ldr_ratio = static_cast<double>(stats.ldr_only_mode_points) / stats.total_points;
double switch_rate = static_cast<double>(stats.mode_switch_count) / stats.total_points;

if (ldr_ratio > 0.9) {
    // LDR-Only使用率过高，可能需要调整参数
    std::cout << "Warning: LDR-Only usage too high: " << ldr_ratio << std::endl;
}
```

### 3. 动态调优

```cpp
// 根据运行时性能动态调整
if (compression_ratio > target_ratio) {
    // 压缩比不理想，尝试调整参数
    // 可以通过重新创建压缩器来调整参数
}
```

## 📝 总结

**TrajCompress-SP-Adaptive** 通过智能的参数系统，实现了：

1. **零配置**：默认参数适用于大多数场景
2. **自适应**：根据数据特性自动调整参数
3. **可调优**：提供丰富的参数供高级用户调优
4. **高性能**：14/15数据集最优表现

**推荐配置**：`min_window=16, max_window=192, observe_window=256` 🎉
