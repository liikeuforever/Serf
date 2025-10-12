#include "compressor/space_filling_curve.h"
#include "utils/elias_gamma_codec.h"
#include "utils/zig_zag_codec.h"
#include "utils/output_bit_stream.h"
#include "utils/input_bit_stream.h"
#include <iostream>
#include <vector>

using namespace std;
using GeoPoint = SpaceFillingCurve::GeoPoint;
using BoundingBox = SpaceFillingCurve::BoundingBox;

int main() {
    cout << "手动测试压缩解压缩流程" << endl;
    
    // 创建测试数据
    vector<GeoPoint> points;
    for (int i = 0; i < 5; i++) {
        points.emplace_back(116.3 + i * 0.001, 39.9 + i * 0.001);
    }
    cout << "创建了 " << points.size() << " 个测试点\n" << endl;
    
    // 计算MBR
    BoundingBox bbox;
    for (const auto& p : points) {
        bbox.Update(p);
    }
    cout << "MBR: [" << bbox.min_lon << ", " << bbox.max_lon << "] × [" 
         << bbox.min_lat << ", " << bbox.max_lat << "]\n" << endl;
    
    int encoding_bits = 52;
    
    // 编码所有点
    cout << "=== 编码过程 ===" << endl;
    vector<uint64_t> codes;
    for (size_t i = 0; i < points.size(); i++) {
        uint64_t code = SpaceFillingCurve::EncodeMBRGeoHash(points[i], encoding_bits, bbox);
        codes.push_back(code);
        cout << "点 " << i << ": (" << points[i].longitude << ", " << points[i].latitude 
             << ") -> 编码=" << code << endl;
    }
    
    // 计算差值
    cout << "\n=== 差值计算 ===" << endl;
    vector<int64_t> diffs;
    for (size_t i = 1; i < codes.size(); i++) {
        int64_t diff = static_cast<int64_t>(codes[i]) - static_cast<int64_t>(codes[i-1]);
        diffs.push_back(diff);
        cout << "差值 " << i << ": " << diff << endl;
    }
    
    // ZigZag编码
    cout << "\n=== ZigZag编码 ===" << endl;
    vector<uint64_t> zigzags;
    for (size_t i = 0; i < diffs.size(); i++) {
        uint64_t zigzag = ZigZagCodec::Encode(diffs[i]);
        zigzags.push_back(zigzag);
        cout << "差值 " << i << " (" << diffs[i] << ") -> ZigZag=" << zigzag 
             << " -> +1=" << (zigzag + 1) << endl;
    }
    
    // 使用OutputBitStream写入
    cout << "\n=== 写入比特流 ===" << endl;
    OutputBitStream output(1024);
    
    // 写入第一个完整编码
    cout << "写入第一个编码: " << codes[0] << " (" << encoding_bits << " bits)" << endl;
    output.WriteLong(codes[0], encoding_bits);
    
    // 写入差值的Elias Gamma编码
    for (size_t i = 0; i < zigzags.size(); i++) {
        uint64_t value = zigzags[i] + 1;
        int bits = EliasGammaCodec::Encode(value, &output);
        cout << "写入差值 " << i << ": value=" << value << " 使用了 " << bits << " bits" << endl;
    }
    
    output.Flush();
    int total_bits = encoding_bits;
    for (size_t i = 0; i < zigzags.size(); i++) {
        uint64_t value = zigzags[i] + 1;
        int n = (value <= 1) ? 0 : static_cast<int>(floor(log2(value)));
        total_bits += (n + n + 1);
    }
    int byte_length = (total_bits + 7) / 8;
    
    cout << "\n总共写入 " << total_bits << " bits = " << byte_length << " bytes" << endl;
    
    Array<uint8_t> compressed = output.GetBuffer(byte_length);
    cout << "压缩数据大小: " << compressed.length() << " bytes" << endl;
    
    // 解压缩
    cout << "\n=== 解压缩过程 ===" << endl;
    InputBitStream input;
    input.SetBuffer(compressed);
    
    // 读取第一个编码
    uint64_t first_code = input.ReadLong(encoding_bits);
    cout << "读取第一个编码: " << first_code << endl;
    
    GeoPoint decoded_first = SpaceFillingCurve::DecodeMBRGeoHash(first_code, encoding_bits, bbox);
    cout << "解码第一个点: (" << decoded_first.longitude << ", " << decoded_first.latitude << ")" << endl;
    
    // 读取后续差值
    uint64_t prev_code = first_code;
    for (size_t i = 1; i < points.size(); i++) {
        cout << "\n读取差值 " << i << ":" << endl;
        
        // 读取Elias Gamma
        uint64_t zigzag_plus_1 = EliasGammaCodec::Decode(&input);
        cout << "  Elias Gamma解码: " << zigzag_plus_1 << endl;
        
        uint64_t zigzag = zigzag_plus_1 - 1;
        cout << "  ZigZag值: " << zigzag << endl;
        
        int64_t diff = ZigZagCodec::Decode(zigzag);
        cout << "  差值: " << diff << endl;
        
        uint64_t current_code = static_cast<uint64_t>(static_cast<int64_t>(prev_code) + diff);
        cout << "  当前编码: " << current_code << endl;
        
        GeoPoint decoded = SpaceFillingCurve::DecodeMBRGeoHash(current_code, encoding_bits, bbox);
        cout << "  解码点: (" << decoded.longitude << ", " << decoded.latitude << ")" << endl;
        
        prev_code = current_code;
    }
    
    cout << "\n=== 完成 ===" << endl;
    
    return 0;
}


