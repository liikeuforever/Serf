# Simple压缩器Timestamp修复总结

## 问题根源

**Simple版本的timestamp编码在两种模式中位置不一致：**
- **LDR-Only模式**: timestamp在最前面（在量化误差之前）
- **Multi-Predictor模式**: Huffman标志 → timestamp → 量化误差

这导致解压器无法确定timestamp在bit流中的位置，造成同步失败。

## 修复方案（参考TrajSP）

**统一timestamp编码位置**：在`AddGpsPoint`的最开始（所有预测器选择之前）统一编码timestamp delta。

### 数据流格式（修复后）

**每个后续点的编码顺序**：
1. **Timestamp Delta (64位，固定长度）** ← 所有模式统一在这里
2. 根据模式选择：
   - LDR-Only模式：直接编码量化误差（无Huffman标志）
   - Multi-Predictor模式：Huffman标志 → 量化误差

### 代码修改

**压缩器** (`src/compressor/trajcompress_sp_adaptive_simple_compressor.cc`):
- `AddGpsPoint` 开头添加统一的timestamp编码（第36-40行）
- 移除 `EncodeLDROnly` 中的timestamp编码
- 移除 `EncodePrediction` 中的timestamp编码

**解压器**：
- `ReadNextPoint` 开头已经统一读取timestamp（第609-611行），无需修改

## 当前状态

- ✅ 压缩器已修复
- ✅ 解压器timestamp读取位置正确
- ⚠️ 仍存在崩溃/超时问题（可能是其他逻辑问题）

## 下一步

需要深入调试解压器，确认bit流读取是否与压缩器完全同步。
