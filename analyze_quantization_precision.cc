/**
 * 分析量化步长与精度保证的关系
 */

#include <iostream>
#include <iomanip>
#include <cmath>

int main() {
    double epsilon = 1e-5;  // 基准误差阈值
    
    std::cout << "============================================================" << std::endl;
    std::cout << "量化步长与二维误差保证的关系" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    std::cout << "=== 误差模型 ===" << std::endl;
    std::cout << "假设：" << std::endl;
    std::cout << "  - 量化步长（每个维度）: q" << std::endl;
    std::cout << "  - 量化误差（单维度最坏情况）: q/2" << std::endl;
    std::cout << "  - 二维欧几里得误差: √((q/2)² + (q/2)²) = √2×(q/2) = q/√2" << std::endl;
    
    std::cout << "\n=== 方案1：Serf-QT ===" << std::endl;
    double serf_step = 2 * epsilon;
    double serf_single_dim_error = serf_step / 2;
    double serf_2d_error = std::sqrt(2) * serf_single_dim_error;
    
    std::cout << std::scientific << std::setprecision(4);
    std::cout << "量化步长 q = 2×ε = " << serf_step << " 度" << std::endl;
    std::cout << "单维度最大误差 = q/2 = " << serf_single_dim_error << " 度 = ε ✅" << std::endl;
    std::cout << "二维最大误差 = q/√2 = " << serf_2d_error << " 度 = √2×ε" << std::endl;
    
    std::cout << "\n目标：" << std::endl;
    std::cout << "  - Serf-QT保证：每个维度误差 ≤ ε" << std::endl;
    std::cout << "  - 导致：二维误差 ≤ √2×ε" << std::endl;
    
    std::cout << "\n=== 方案2：我们的ZP（当前实现）===" << std::endl;
    double zp_step = std::sqrt(2) * epsilon;
    double zp_single_dim_error = zp_step / 2;
    double zp_2d_error = std::sqrt(2) * zp_single_dim_error;
    
    std::cout << "量化步长 q = √2×ε = " << zp_step << " 度" << std::endl;
    std::cout << "单维度最大误差 = q/2 = " << zp_single_dim_error << " 度 = ε/√2 ≈ 0.707×ε" << std::endl;
    std::cout << "二维最大误差 = q/√2 = " << zp_2d_error << " 度 = ε ✅" << std::endl;
    
    std::cout << "\n目标：" << std::endl;
    std::cout << "  - 我们保证：二维误差 ≤ ε" << std::endl;
    std::cout << "  - 这是更严格的精度要求！" << std::endl;
    
    std::cout << "\n=== 方案3：如果ZP也用相同精度要求（二维误差≤√2×ε）===" << std::endl;
    double zp_fair_step = 2 * epsilon;
    double zp_fair_single_dim_error = zp_fair_step / 2;
    double zp_fair_2d_error = std::sqrt(2) * zp_fair_single_dim_error;
    
    std::cout << "量化步长 q = 2×ε = " << zp_fair_step << " 度" << std::endl;
    std::cout << "单维度最大误差 = q/2 = " << zp_fair_single_dim_error << " 度 = ε" << std::endl;
    std::cout << "二维最大误差 = q/√2 = " << zp_fair_2d_error << " 度 = √2×ε ✅" << std::endl;
    
    std::cout << "\n目标：" << std::endl;
    std::cout << "  - 与Serf-QT相同：二维误差 ≤ √2×ε" << std::endl;
    std::cout << "  - 这样就是公平对比！" << std::endl;
    
    std::cout << "\n=== 关键发现 ===" << std::endl;
    std::cout << "\n❌ 当前问题：" << std::endl;
    std::cout << "   我们在测试中让ZP使用 √2×ε 的量化步长" << std::endl;
    std::cout << "   这实际上保证了二维误差 ≤ ε" << std::endl;
    std::cout << "   而Serf-QT只保证二维误差 ≤ √2×ε" << std::endl;
    std::cout << "   → 精度要求更严格，所以压缩率更差！" << std::endl;
    
    std::cout << "\n✅ 用户的观察：" << std::endl;
    std::cout << "   如果都要求二维误差 ≤ √2×ε" << std::endl;
    std::cout << "   那么每个维度都应该用 2×ε 的量化步长" << std::endl;
    std::cout << "   这样ZP和Serf-QT的压缩效果应该相同！" << std::endl;
    
    std::cout << "\n=== 压缩率差异估算 ===" << std::endl;
    std::cout << "\n假设实际误差 Δ = 5e-5 度：" << std::endl;
    
    // 当前ZP (√2×ε量化步长)
    int zp_current_q = static_cast<int>(std::round(5e-5 / zp_step));
    std::cout << "当前ZP:   量化值 = " << zp_current_q << std::endl;
    
    // 修正后的ZP (2×ε量化步长)
    int zp_fair_q = static_cast<int>(std::round(5e-5 / zp_fair_step));
    std::cout << "修正后ZP: 量化值 = " << zp_fair_q << std::endl;
    
    // Serf-QT
    int serf_q = static_cast<int>(std::round(5e-5 / serf_step));
    std::cout << "Serf-QT:  量化值 = " << serf_q << std::endl;
    
    std::cout << "\n预期：修正后的ZP与Serf-QT量化值相同 → 压缩效果应该相同！" << std::endl;
    
    std::cout << "\n============================================================" << std::endl;
    std::cout << "结论：用户说得对！如果使用相同的精度要求（二维误差≤√2×ε），" << std::endl;
    std::cout << "      ZP应该和Serf-QT一样使用2×ε的量化步长，而不是√2×ε。" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    return 0;
}


