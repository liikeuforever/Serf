# Timestamp同步问题调试计划

## 当前问题

解压第2个点时，读取的数据完全错误：
- 期望: lon=116.4, lat=40.0, ts=1001
- 实际: lon=116.3, lat=39.9, ts=16778216 (0x01000008)

## 数据流格式（理论）

### Header:
1. block_size (16 bits)
2. epsilon (64 bits)  
3. evaluation_window (16 bits)

### 第1个点:
1. 经度 (64 bits)
2. 纬度 (64 bits)
3. Timestamp (64 bits)

### 第2个点:
1. **Timestamp delta (64 bits)** ← 新增！在AddGpsPoint开头
2. Huffman标志 (1-2 bits，变长)
3. 量化误差经度 (Elias Gamma，变长)
4. 量化误差纬度 (Elias Gamma，变长)

## 问题假设

1. **假设1**: Huffman或Elias Gamma解码读取了额外的bits
2. **假设2**: 压缩器的timestamp编码顺序与解压器不一致
3. **假设3**: 第1个点和第2个点之间有额外的数据被写入

## 调试策略

1. 打印压缩器每次WriteLong/WriteBit的位置和值
2. 打印解压器每次ReadLong/ReadBit的位置和值
3. 对比两者的bit流操作序列

## 快速修复方案

如果调试困难，考虑：
- **方案A**: 完全移除Simple的timestamp支持，回退到等时间间隔
- **方案B**: 参考TrajSP的exact实现，逐字节对比
- **方案C**: 暂时注释Simple版本，先测试TrajSP的结果
