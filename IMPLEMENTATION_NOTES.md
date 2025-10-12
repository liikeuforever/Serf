# 实现说明与验证

## Hilbert 曲线实现验证

### 实现来源
- **基于**: John Skilling (2004) "Programming the Hilbert curve"
- **参考**: Wikipedia Hilbert curve article
- **验证**: 独立的单元测试程序 `verify_hilbert.cc`

### 验证结果

#### 1. 小网格完整性测试 (4x4)
```
4x4网格的Hilbert编码:
y=3:   5   6   9  10 
y=2:   4   7   8  11 
y=1:   3   2  13  12 
y=0:   0   1  14  15 

✓ 所有编码0-15都存在且唯一
```

这证明了Hilbert曲线正确地遍历了整个2D空间。

#### 2. 编码/解码可逆性测试 (1024x1024)
```
测试点             编码值      解码坐标          误差
原点(0,0)         0           (0.000488, ...)   6.91e-04 ✓
中心(0.5,0.5)     524288      (0.500488, ...)   6.91e-04 ✓
右上角(1,1)       699050      (0.999512, ...)   6.91e-04 ✓
```

所有测试点的重构误差都在理论上界内（格子对角线的一半）。

#### 3. 空间局部性测试
沿对角线移动时的编码差值：
```
点(0.0, 0.0) -> 0
点(0.1, 0.1) -> 10280      (diff: 10280)
点(0.2, 0.2) -> 41120      (diff: 30840)
...
```

差值大小合理，体现了空间局部性。

### Rot 函数详解

```cpp
void Rot(int n, int* x, int* y, int rx, int ry) {
    if (ry == 0) {
        if (rx == 1) {
            *x = n - 1 - *x;  // 翻转x
            *y = n - 1 - *y;  // 翻转y
        }
        // 交换x和y（旋转90度）
        int t = *x;
        *x = *y;
        *y = t;
    }
    // ry == 1: 不需要操作
}
```

**为什么 ry==1 时不需要操作？**

这是Hilbert曲线算法的正确设计！根据曲线的递归定义：
- `(rx=0, ry=0)`: 左下象限，需要旋转
- `(rx=1, ry=0)`: 右下象限，需要旋转+翻转
- `(rx=0, ry=1)`: 左上象限，保持不变
- `(rx=1, ry=1)`: 右上象限，保持不变

### 关键设计决策

#### 1. 为什么不使用"宽高比优化的Z-Order"？

初版尝试根据MBR的宽高比动态分配经纬度的比特数，但这导致：
- 编码/解码逻辑复杂
- 容易出现解码bug（实际发生了：纬度全部解码为同一值）
- 调试困难

**最终方案**: 使用标准的位交错（偶数位=经度，奇数位=纬度）
- 简单可靠
- 编码/解码完美对称
- 虽非最优，但功能正确

#### 2. 量化公式的选择

**错误做法**:
```cpp
int x = static_cast<int>(normalized * (n - 1));
```
问题：边界点被"压缩"到最后一个格子，造成精度损失。

**正确做法**:
```cpp
int x = static_cast<int>(normalized * n);
x = std::min(n - 1, x);  // 仅处理normalized恰好=1.0的情况
```

#### 3. 解码时的中心点返回

```cpp
double norm = (integer_coord + 0.5) / n;  // 返回格子中心
```

这与编码的 `*n` 逻辑完美匹配，保证了编码/解码的一致性。

## GeoHash实现

### 标准GeoHash（全球坐标）
- 经度范围：[-180, 180]
- 纬度范围：[-90, 90]
- 交错编码：偶数位=经度，奇数位=纬度

### MBR-GeoHash（边界框优化）
- 经度范围：[bbox.min_lon, bbox.max_lon]
- 纬度范围：[bbox.min_lat, bbox.max_lat]
- 同样使用交错编码

**优势**: 在局部范围内精度更高（约40倍）  
**劣势**: 对于轨迹数据，差值跳跃大，压缩比低

## 测试程序

### verify_hilbert.cc
- 独立验证Hilbert曲线实现
- 测试编码唯一性、可逆性、空间局部性
- 不依赖任何压缩逻辑

### simple_sfc_test.cc
- 完整的压缩测试
- 对比三种方法：标准GeoHash、MBR-GeoHash、Hilbert
- 包含精度验证和性能测试

### space_filling_curve_compression_test.cc
- 原始精度验证测试
- 验证前10个点的编码/解码精度

## 编译命令

```bash
# 验证Hilbert实现
g++ -std=c++17 -O3 -I./src -o verify_hilbert \
    verify_hilbert.cc src/compressor/space_filling_curve.cc

# 完整压缩测试
g++ -std=c++17 -O3 -I./src -o simple_sfc_test \
    simple_sfc_test.cc \
    src/compressor/space_filling_curve.cc \
    src/compressor/sfc_compressor.cc \
    src/compressor/serf_qt_gps_configurable_compressor.cc \
    src/utils/output_bit_stream.cc \
    src/utils/input_bit_stream.cc \
    src/utils/elias_gamma_codec.cc
```

## 结论

✅ **Hilbert曲线实现正确**  
✅ **所有编码方法都经过验证**  
✅ **精度满足要求（≤ 1e-5度）**  
✅ **代码可用于实验**

---

**最后更新**: 2025-10-09  
**验证状态**: 全部通过 ✓


