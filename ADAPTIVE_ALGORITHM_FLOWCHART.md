# 🔄 TrajCompress-SP-Adaptive 算法流程图

## 主流程：AddGpsPoint(point)

```
开始
  ↓
第一个点？
  ├─ 是 → ProcessFirstPoint() → 结束
  └─ 否 → 继续
  ↓
并行预测
  ├─ LDR预测：线性外推
  ├─ CP预测：二阶曲线  
  └─ ZP预测：零预测
  ↓
计算每个预测器成本
  ├─ LDR成本 = Huffman标志位 + 误差编码
  ├─ CP成本 = Huffman标志位 + 误差编码
  └─ ZP成本 = Huffman标志位 + 误差编码
  ↓
选择最优预测器（总成本最小）
  ↓
计算双模型成本
  ├─ Multi模型成本 = 最优预测器成本
  └─ LDR-Only模型成本 = LDR成本
  ↓
更新成本窗口
  ↓
根据当前模式编码
  ├─ LDR-Only模式 → EncodeLDROnly()
  └─ Multi模式 → EncodeMultiPredictor()
  ↓
动态参数调整？（每256点）
  ├─ 是 → UpdateAdaptiveParameters()
  └─ 否 → 继续
  ↓
模式切换决策？（每16点）
  ├─ 是 → EvaluateAndSwitchModeBasedOnCost()
  └─ 否 → 结束
  ↓
结束
```

## 模式切换决策：EvaluateAndSwitchModeBasedOnCost()

```
开始
  ↓
当前模式？
  ├─ Multi模式
  │   ├─ LDR-Only成本 < Multi成本 - 切换成本 - 边际？
  │   │   ├─ 是 → 切换到LDR-Only模式
  │   │   └─ 否 → 保持Multi模式
  │   └─ 结束
  └─ LDR-Only模式
      ├─ Multi成本 < LDR-Only成本 - 切换成本 - 边际？
      │   ├─ 是 → 切换到Multi模式
      │   └─ 否 → 保持LDR-Only模式
      └─ 结束
```

## 动态参数调整：UpdateAdaptiveParameters()

```
开始
  ↓
计算预测器流失率
  ├─ 流失率 = 预测器切换次数 / 观察窗口大小
  └─ 范围：[0, 1]
  ↓
动态调整窗口大小
  ├─ 公式：kCostWindowSize = kMinWindowSize + (kMaxWindowSize - kMinWindowSize) * 流失率
  ├─ 简单轨迹（低流失率）→ 小窗口
  └─ 复杂轨迹（高流失率）→ 大窗口
  ↓
计算成本差标准差
  ├─ 标准差 = stddev(cost_diffs_)
  └─ 反映成本差异的波动性
  ↓
动态调整稳定边际
  ├─ 公式：kStabilityMargin = 4 - 3 * normalized_stddev
  ├─ 低波动性（竞争激烈）→ 高边际
  └─ 高波动性（优劣明显）→ 低边际
  ↓
参数范围检查
  ├─ kCostWindowSize ∈ [kMinWindowSize, kMaxWindowSize]
  └─ kStabilityMargin ∈ [1, 4]
  ↓
结束
```

## 成本窗口管理：UpdateCostWindows()

```
开始
  ↓
入队新成本
  ├─ point_costs_multi_.push_back(multi_cost)
  ├─ point_costs_ldr_only_.push_back(ldr_only_cost)
  ├─ window_total_cost_multi_ += multi_cost
  └─ window_total_cost_ldr_only_ += ldr_only_cost
  ↓
记录成本差异
  ├─ cost_diff = multi_cost - ldr_only_cost
  └─ cost_diffs_.push_back(cost_diff)
  ↓
窗口大小检查
  ├─ 超过kCostWindowSize？
  │   ├─ 是 → 出队旧成本
  │   └─ 否 → 继续
  └─ 结束
```

## 预测器选择：SelectBestPredictorByCost()

```
开始
  ↓
计算每个预测器总成本
  ├─ LDR总成本 = Huffman(LDR) + 误差编码(LDR)
  ├─ CP总成本 = Huffman(CP) + 误差编码(CP)
  └─ ZP总成本 = Huffman(ZP) + 误差编码(ZP)
  ↓
选择最小成本预测器
  ├─ cost_ldr_total ≤ cost_cp_total && cost_ldr_total ≤ cost_zp_total？
  │   ├─ 是 → 选择LDR
  │   └─ 否 → 继续
  ├─ cost_cp_total ≤ cost_zp_total？
  │   ├─ 是 → 选择CP
  │   └─ 否 → 选择ZP
  └─ 结束
```

## 三层决策系统

```
第1层：预测器选择（每点）
  ├─ 输入：当前GPS点
  ├─ 处理：并行预测 + 成本计算
  ├─ 输出：最优预测器
  └─ 频率：每点执行

第2层：模式切换（每16点）
  ├─ 输入：成本窗口数据
  ├─ 处理：成本比较 + 切换决策
  ├─ 输出：模式切换标志
  └─ 频率：每16点执行

第3层：参数调整（每256点）
  ├─ 输入：预测器历史 + 成本差异
  ├─ 处理：流失率计算 + 标准差计算
  ├─ 输出：动态参数更新
  └─ 频率：每256点执行
```

## 关键数据结构

```
成本窗口系统：
├─ point_costs_multi_: deque<int>          // Multi模型逐点成本
├─ point_costs_ldr_only_: deque<int>       // LDR-Only模型逐点成本
├─ window_total_cost_multi_: long long     // Multi模型总成本
└─ window_total_cost_ldr_only_: long long  // LDR-Only模型总成本

动态参数系统：
├─ predictor_history_: deque<PredictorType> // 预测器选择历史
├─ cost_diffs_: deque<int>                  // 成本差异历史
├─ kCostWindowSize: int                     // 动态窗口大小
└─ kStabilityMargin: int                    // 动态稳定边际

Huffman编码系统：
├─ predictor_window_: vector<PredictorType>  // 预测器滑动窗口
├─ predictor_frequency_: int[3]              // 预测器频率统计
└─ huffman_codes_: HuffmanCode[3]            // Huffman编码表
```

## 性能优化点

```
1. 并行预测
   ├─ 同时计算LDR、CP、ZP预测
   └─ 避免重复计算

2. 成本估算
   ├─ 快速Elias Gamma比特数估算
   └─ 避免实际编码开销

3. 滑动窗口
   ├─ 高效的双端队列操作
   └─ 常数时间窗口维护

4. 动态调整
   ├─ 基于轨迹特性的智能参数
   └─ 避免固定参数的局限性
```
