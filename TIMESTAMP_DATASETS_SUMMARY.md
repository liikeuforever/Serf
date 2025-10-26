# 带时间戳的轨迹数据集总结

## 数据集概述

本项目包含3个主要的GPS轨迹数据集，每个数据集都包含原始版本和4个降采样版本（5x, 10x, 20x, 40x）。

### 数据格式

所有数据集使用统一的CSV格式：
```csv
longitude,latitude,timestamp
116.318417,39.984702,2008-10-23 02:53:04
```

- **longitude**: 经度（十进制度数）
- **latitude**: 纬度（十进制度数）
- **timestamp**: 时间戳（格式：`YYYY-MM-DD HH:MM:SS`）

---

## 1. Geolife 数据集

**来源**: 微软亚洲研究院 Geolife 项目  
**区域**: 中国北京及周边  
**描述**: 用户日常活动的GPS轨迹（步行、驾车、骑行等）

### 文件统计

| 文件名 | 点数 | 描述 |
|--------|------|------|
| `Geolife_100k_with_timestamp.csv` | 100,000 | 原始数据集 |
| `Geolife_100k_with_timestamp_downsample_5x.csv` | 20,000 | 每5个点取1个 |
| `Geolife_100k_with_timestamp_downsample_10x.csv` | 10,000 | 每10个点取1个 |
| `Geolife_100k_with_timestamp_downsample_20x.csv` | 5,000 | 每20个点取1个 |
| `Geolife_100k_with_timestamp_downsample_40x.csv` | 2,500 | 每40个点取1个 |

### 数据示例
```
经度: 116.318417, 纬度: 39.984702
时间: 2008-10-23 02:53:04
采样间隔: 约5-10秒
```

---

## 2. Track 数据集

**来源**: 南非开普敦地区GPS轨迹  
**区域**: 南非开普敦  
**描述**: 高频GPS轨迹数据（1秒采样间隔）

### 文件统计

| 文件名 | 点数 | 描述 |
|--------|------|------|
| `Track_63530k_with_timestamp.csv` | 63,530 | 原始数据集 |
| `Track_63530k_with_timestamp_downsample_5x.csv` | 12,706 | 每5个点取1个 |
| `Track_63530k_with_timestamp_downsample_10x.csv` | 6,353 | 每10个点取1个 |
| `Track_63530k_with_timestamp_downsample_20x.csv` | 3,177 | 每20个点取1个 |
| `Track_63530k_with_timestamp_downsample_40x.csv` | 1,589 | 每40个点取1个 |

### 数据示例
```
经度: 18.85666, 纬度: -33.93257
时间: 2022-05-04 9:22:53
采样间隔: 约1秒
```

---

## 3. Trajtory 数据集

**来源**: 美国佛罗里达州GPS轨迹  
**区域**: 美国佛罗里达州  
**描述**: 车辆GPS轨迹数据

### 文件统计

| 文件名 | 点数 | 描述 |
|--------|------|------|
| `Trajtory_100k_with_timestamp.csv` | 97,009 | 原始数据集 |
| `Trajtory_100k_with_timestamp_downsample_5x.csv` | 19,402 | 每5个点取1个 |
| `Trajtory_100k_with_timestamp_downsample_10x.csv` | 9,701 | 每10个点取1个 |
| `Trajtory_100k_with_timestamp_downsample_20x.csv` | 4,851 | 每20个点取1个 |
| `Trajtory_100k_with_timestamp_downsample_40x.csv` | 2,426 | 每40个点取1个 |

### 数据示例
```
经度: -81.4575142, 纬度: 28.5514836
时间: 2022-01-29 23:28:00
采样间隔: 约1秒
```

---

## 文件位置

### 所有数据集（统一位置）
```
test/data_set/data_set_with_timestamp/
├── Geolife_100k_with_timestamp.csv
├── Geolife_100k_with_timestamp_downsample_5x.csv
├── Geolife_100k_with_timestamp_downsample_10x.csv
├── Geolife_100k_with_timestamp_downsample_20x.csv
├── Geolife_100k_with_timestamp_downsample_40x.csv
├── Track_63530k_with_timestamp.csv
├── Track_63530k_with_timestamp_downsample_5x.csv
├── Track_63530k_with_timestamp_downsample_10x.csv
├── Track_63530k_with_timestamp_downsample_20x.csv
├── Track_63530k_with_timestamp_downsample_40x.csv
├── Trajtory_100k_with_timestamp.csv
├── Trajtory_100k_with_timestamp_downsample_5x.csv
├── Trajtory_100k_with_timestamp_downsample_10x.csv
├── Trajtory_100k_with_timestamp_downsample_20x.csv
└── Trajtory_100k_with_timestamp_downsample_40x.csv
```

---

## 数据集特征对比

| 数据集 | 区域 | 原始点数 | 采样间隔 | 轨迹类型 |
|--------|------|----------|----------|----------|
| Geolife | 中国北京 | 100,000 | 5-10秒 | 多模式（步行/驾车/骑行） |
| Track | 南非开普敦 | 63,530 | 1秒 | 高频GPS |
| Trajtory | 美国佛罗里达 | 97,009 | 1秒 | 车辆轨迹 |

---

## 使用说明

### 1. 读取数据（Python）

```python
import pandas as pd

# 读取原始数据集
df = pd.read_csv('test/data_set/data_set_with_timestamp/Geolife_100k_with_timestamp.csv')

# 读取降采样数据集
df_10x = pd.read_csv('test/data_set/data_set_with_timestamp/Geolife_100k_with_timestamp_downsample_10x.csv')

# 解析时间戳
df['timestamp'] = pd.to_datetime(df['timestamp'])
```

### 2. 读取数据（C++）

```cpp
#include <fstream>
#include <sstream>
#include <vector>

struct GpsPoint {
    double longitude;
    double latitude;
    std::string timestamp;
};

std::vector<GpsPoint> LoadGpsData(const std::string& filename) {
    std::vector<GpsPoint> data;
    std::ifstream file(filename);
    std::string line;
    
    // 跳过表头
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string lon_str, lat_str, ts_str;
        
        std::getline(ss, lon_str, ',');
        std::getline(ss, lat_str, ',');
        std::getline(ss, ts_str);
        
        GpsPoint point;
        point.longitude = std::stod(lon_str);
        point.latitude = std::stod(lat_str);
        point.timestamp = ts_str;
        
        data.push_back(point);
    }
    
    return data;
}
```

---

## 处理脚本

使用 `process_timestamp_datasets.py` 重新生成数据集：

```bash
python3 process_timestamp_datasets.py
```

该脚本会：
1. 读取原始带时间戳的数据集
2. 确保不超过10万点
3. 生成4个降采样版本（5x, 10x, 20x, 40x）

---

## 数据集总计

- **原始数据集**: 3个文件，共 260,539 个GPS点
- **降采样数据集**: 12个文件
- **总文件数**: 15个CSV文件
- **总存储大小**: 约 15 MB（未压缩）

---

## 时间戳压缩建议

对于时间戳的压缩，推荐使用 **Delta-of-Delta** 编码：

1. **第一个点**: 直接存储完整的Unix时间戳（64位）
2. **第二个点**: 存储时间差 Δt（使用Elias Gamma编码）
3. **后续点**: 存储时间差的差 ΔΔt（使用ZigZag + Elias Gamma编码）

这种方法对于等间隔或近似等间隔的GPS轨迹非常高效。

---

**最后更新**: 2025-10-26  
**生成工具**: `process_timestamp_datasets.py`

