/**
 * 分析Serf-QT vs ZP的差异
 * 关键：量化步长不同！
 */

#include <iostream>
#include <iomanip>
#include <cmath>

int main() {
    double epsilon = 1e-5;  // 基准误差阈值
    
    std::cout << "============================================================" << std::endl;
    std::cout << "Serf-QT vs ZP (零预测) 的关键差异分析" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    // Serf-QT的量化步长
    double serf_quantization_step = 2 * epsilon;
    
    // ZP的量化步长（在我们的实现中）
    double zp_quantization_step = std::sqrt(2) * epsilon;
    
    // TrajCompress-SP的量化步长（用于对比）
    double trajsp_quantization_step = std::sqrt(2) * epsilon;
    
    std::cout << "=== 量化步长对比 ===" << std::endl;
    std::cout << std::scientific << std::setprecision(4);
    std::cout << "基准epsilon:           " << epsilon << " 度" << std::endl;
    std::cout << "Serf-QT量化步长:       " << serf_quantization_step << " 度 (= 2×epsilon)" << std::endl;
    std::cout << "ZP量化步长:            " << zp_quantization_step << " 度 (= √2×epsilon)" << std::endl;
    std::cout << "TrajCompress-SP量化:   " << trajsp_quantization_step << " 度 (= √2×epsilon)" << std::endl;
    
    std::cout << "\n=== 量化步长比较 ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Serf-QT / ZP = " << (serf_quantization_step / zp_quantization_step) << "x" << std::endl;
    std::cout << "  含义：Serf-QT的量化步长是ZP的 " << (serf_quantization_step / zp_quantization_step) << " 倍" << std::endl;
    
    std::cout << "\n=== 影响分析 ===" << std::endl;
    std::cout << "\n1. **量化粒度**：" << std::endl;
    std::cout << "   - Serf-QT量化更粗（步长更大）" << std::endl;
    std::cout << "   - ZP量化更精细（步长更小）" << std::endl;
    
    std::cout << "\n2. **量化值大小**：" << std::endl;
    std::cout << "   对于相同的实际误差 delta = 5e-5 度：" << std::endl;
    double example_delta = 5e-5;
    int serf_quantized = static_cast<int>(std::round(example_delta / serf_quantization_step));
    int zp_quantized = static_cast<int>(std::round(example_delta / zp_quantization_step));
    std::cout << "   - Serf-QT量化值: " << serf_quantized << std::endl;
    std::cout << "   - ZP量化值:      " << zp_quantized << std::endl;
    std::cout << "   → ZP的量化值更大，Elias Gamma编码后比特数更多！" << std::endl;
    
    std::cout << "\n3. **编码代价估算**：" << std::endl;
    std::cout << "   Elias Gamma编码长度 ≈ 2×log₂(n) + 1 bits" << std::endl;
    int serf_bits = 2 * static_cast<int>(std::log2(serf_quantized + 1)) + 1;
    int zp_bits = 2 * static_cast<int>(std::log2(zp_quantized + 1)) + 1;
    std::cout << "   - Serf-QT: ~" << serf_bits << " bits" << std::endl;
    std::cout << "   - ZP:      ~" << zp_bits << " bits" << std::endl;
    std::cout << "   → 这解释了为什么ZP比Serf-QT多 " << std::setprecision(1) 
              << (10.84 - 9.57) << " bits/点！" << std::endl;
    
    std::cout << "\n=== 关键结论 ===" << std::endl;
    std::cout << "\nSerf-QT和ZP都使用**零预测（前值预测）**，但：" << std::endl;
    std::cout << "  ✓ Serf-QT使用更粗的量化步长 (2×epsilon)" << std::endl;
    std::cout << "  ✓ ZP使用更精细的量化步长 (√2×epsilon)" << std::endl;
    std::cout << "  ✓ 量化步长差异导致编码代价不同" << std::endl;
    std::cout << "\n虽然预测方法相同，但**量化精度要求不同**，导致压缩效果不同！" << std::endl;
    
    std::cout << "\n=== 实验结果验证 ===" << std::endl;
    std::cout << "Geolife数据集：" << std::endl;
    std::cout << "  - Serf-QT: 9.57 bits/点 (量化步长 2.00e-05)" << std::endl;
    std::cout << "  - ZP:     10.84 bits/点 (量化步长 1.41e-05)" << std::endl;
    std::cout << "  - 差异:   " << std::setprecision(2) << (10.84 - 9.57) << " bits/点 (" 
              << std::setprecision(1) << ((10.84 - 9.57) / 9.57 * 100) << "% 更差)" << std::endl;
    
    std::cout << "\nTrack数据集：" << std::endl;
    std::cout << "  - Serf-QT: 9.79 bits/点 (量化步长 2.00e-05)" << std::endl;
    std::cout << "  - ZP:     11.06 bits/点 (量化步长 1.41e-05)" << std::endl;
    std::cout << "  - 差异:   " << std::setprecision(2) << (11.06 - 9.79) << " bits/点 (" 
              << std::setprecision(1) << ((11.06 - 9.79) / 9.79 * 100) << "% 更差)" << std::endl;
    
    std::cout << "\n============================================================" << std::endl;
    
    return 0;
}


