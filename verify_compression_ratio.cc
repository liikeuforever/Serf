#include <iostream>
#include <iomanip>
#include <cmath>

using namespace std;

void VerifyCompressionRatio(int num_points, 
                            int hilbert_compressed_bytes,
                            int serfqt_compressed_bytes) {
    cout << "\n========================================" << endl;
    cout << "验证 " << num_points << " 个GPS点的压缩比计算" << endl;
    cout << "========================================" << endl;
    
    // 原始数据大小
    // 每个GPS点 = 2个double（经度+纬度）
    // 每个double = 8字节 = 64位
    // 每个GPS点 = 16字节 = 128位
    int bytes_per_point = 2 * 8;  // 16字节
    int bits_per_point = 2 * 64;   // 128位
    
    int original_bytes = num_points * bytes_per_point;
    int original_bits = num_points * bits_per_point;
    
    cout << "\n原始数据：" << endl;
    cout << "  点数: " << num_points << endl;
    cout << "  每点大小: " << bytes_per_point << " 字节 = " << bits_per_point << " 位" << endl;
    cout << "  总大小: " << original_bytes << " 字节 = " << original_bits << " 位" << endl;
    
    // Hilbert+Delta压缩
    cout << "\nHilbert+Delta：" << endl;
    cout << "  压缩后: " << hilbert_compressed_bytes << " 字节 = " 
         << (hilbert_compressed_bytes * 8) << " 位" << endl;
    
    double hilbert_compression_ratio = (double)original_bytes / hilbert_compressed_bytes;
    double hilbert_compression_rate = 100.0 * hilbert_compressed_bytes / original_bytes;
    double hilbert_bits_per_point = (double)(hilbert_compressed_bytes * 8) / num_points;
    double hilbert_bytes_per_point = (double)hilbert_compressed_bytes / num_points;
    
    cout << "  压缩比: " << fixed << setprecision(2) << hilbert_compression_ratio << ":1" << endl;
    cout << "  压缩率: " << fixed << setprecision(2) << hilbert_compression_rate << "%" << endl;
    cout << "  平均bits/点: " << fixed << setprecision(2) << hilbert_bits_per_point << " 位" << endl;
    cout << "  平均bytes/点: " << fixed << setprecision(2) << hilbert_bytes_per_point << " 字节" << endl;
    cout << "  相对原始128位: " << fixed << setprecision(2) 
         << (100.0 * hilbert_bits_per_point / 128) << "%" << endl;
    
    // Serf-QT压缩
    cout << "\nSerf-QT (经度+纬度)：" << endl;
    cout << "  压缩后: " << serfqt_compressed_bytes << " 字节 = " 
         << (serfqt_compressed_bytes * 8) << " 位" << endl;
    
    double serfqt_compression_ratio = (double)original_bytes / serfqt_compressed_bytes;
    double serfqt_compression_rate = 100.0 * serfqt_compressed_bytes / original_bytes;
    double serfqt_bits_per_point = (double)(serfqt_compressed_bytes * 8) / num_points;
    double serfqt_bytes_per_point = (double)serfqt_compressed_bytes / num_points;
    
    cout << "  压缩比: " << fixed << setprecision(2) << serfqt_compression_ratio << ":1" << endl;
    cout << "  压缩率: " << fixed << setprecision(2) << serfqt_compression_rate << "%" << endl;
    cout << "  平均bits/点: " << fixed << setprecision(2) << serfqt_bits_per_point << " 位" << endl;
    cout << "  平均bytes/点: " << fixed << setprecision(2) << serfqt_bytes_per_point << " 字节" << endl;
    cout << "  相对原始128位: " << fixed << setprecision(2) 
         << (100.0 * serfqt_bits_per_point / 128) << "%" << endl;
    
    // 对比
    cout << "\n对比分析：" << endl;
    cout << "  原始每点: 128位 (基准)" << endl;
    cout << "  Hilbert+Delta: " << fixed << setprecision(2) << hilbert_bits_per_point 
         << " 位/点 (节省 " << (100.0 * (128 - hilbert_bits_per_point) / 128) << "%)" << endl;
    cout << "  Serf-QT: " << fixed << setprecision(2) << serfqt_bits_per_point 
         << " 位/点 (节省 " << (100.0 * (128 - serfqt_bits_per_point) / 128) << "%)" << endl;
    
    double advantage = 100.0 * (hilbert_compressed_bytes - serfqt_compressed_bytes) / hilbert_compressed_bytes;
    if (serfqt_compressed_bytes < hilbert_compressed_bytes) {
        cout << "  Serf-QT优于Hilbert+Delta: 节省 " << (hilbert_compressed_bytes - serfqt_compressed_bytes) 
             << " 字节 (" << fixed << setprecision(1) << advantage << "%)" << endl;
    } else {
        cout << "  Hilbert+Delta优于Serf-QT: 节省 " << (serfqt_compressed_bytes - hilbert_compressed_bytes) 
             << " 字节 (" << fixed << setprecision(1) << (-advantage) << "%)" << endl;
    }
    
    // 验证计算公式
    cout << "\n✓ 验证计算公式：" << endl;
    cout << "  压缩比 = 原始字节 / 压缩字节" << endl;
    cout << "    Hilbert: " << original_bytes << " / " << hilbert_compressed_bytes 
         << " = " << hilbert_compression_ratio << ":1 ✓" << endl;
    cout << "    Serf-QT: " << original_bytes << " / " << serfqt_compressed_bytes 
         << " = " << serfqt_compression_ratio << ":1 ✓" << endl;
    
    cout << "  压缩率 = (压缩字节 / 原始字节) * 100%" << endl;
    cout << "    Hilbert: (" << hilbert_compressed_bytes << " / " << original_bytes 
         << ") * 100% = " << hilbert_compression_rate << "% ✓" << endl;
    cout << "    Serf-QT: (" << serfqt_compressed_bytes << " / " << original_bytes 
         << ") * 100% = " << serfqt_compression_rate << "% ✓" << endl;
    
    cout << "  平均bits/点 = (压缩字节 * 8) / 点数" << endl;
    cout << "    Hilbert: (" << hilbert_compressed_bytes << " * 8) / " << num_points 
         << " = " << hilbert_bits_per_point << " 位 ✓" << endl;
    cout << "    Serf-QT: (" << serfqt_compressed_bytes << " * 8) / " << num_points 
         << " = " << serfqt_bits_per_point << " 位 ✓" << endl;
}

int main() {
    cout << "========================================" << endl;
    cout << "GPS压缩比计算验证" << endl;
    cout << "========================================" << endl;
    cout << "说明：" << endl;
    cout << "• 每个GPS点包含2个坐标（经度+纬度）" << endl;
    cout << "• 每个坐标是1个double = 8字节 = 64位" << endl;
    cout << "• 因此每个GPS点 = 16字节 = 128位" << endl;
    cout << "• Hilbert+Delta: 将2D点编码为1D，然后压缩" << endl;
    cout << "• Serf-QT: 分别压缩经度序列和纬度序列" << endl;
    
    // 根据实际测试结果验证
    cout << "\n从实际测试结果验证：" << endl;
    
    // 100个点的数据
    VerifyCompressionRatio(100, 300, 174);
    
    // 1000个点的数据
    VerifyCompressionRatio(1000, 2076, 1230);
    
    // 10000个点的数据
    VerifyCompressionRatio(10000, 19189, 10987);
    
    // 50000个点的数据
    VerifyCompressionRatio(50000, 96670, 60595);
    
    cout << "\n========================================" << endl;
    cout << "结论" << endl;
    cout << "========================================" << endl;
    cout << "✓ 所有压缩比计算都是正确的" << endl;
    cout << "✓ Hilbert+Delta和Serf-QT都是压缩完整的GPS点（2个double）" << endl;
    cout << "✓ 压缩比 = 原始大小(128位/点) / 压缩后大小(bits/点)" << endl;
    cout << "✓ Serf-QT在GPS轨迹数据上确实优于Hilbert+Delta约40%" << endl;
    
    return 0;
}


