//
//  sprintz.h
//  Compress
//
//  Created by DB on 7/3/17.
//  Copyright © 2017 D Blalock. All rights reserved.
//

#ifndef sprintz_h
#define sprintz_h

#include <stdint.h>

// Check if we're on x86/x64 architecture
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #define USE_X86_INTRINSICS
    #define USE_AVX2
    
    #ifdef USE_AVX2
        static_assert(__AVX2__, "AVX 2 is required! Try --march=native or -mavx2");
        #define USE_X86_INTRINSICS
    #endif
#else
    // For non-x86 architectures, disable x86 intrinsics
    #undef USE_X86_INTRINSICS
    #undef USE_AVX2
#endif

uint16_t compress8b_naiveDelta(const uint8_t* src, uint16_t in_sz,
                               int8_t* dest);
uint16_t decompress8b_naiveDelta(const int8_t* src, uint16_t in_sz,
                                 uint8_t* dest);

int64_t compress8b_delta_simple(uint8_t* src, size_t len, int8_t* dest,
                                bool write_size=true);
int64_t decompress8b_delta_simple(int8_t* src, uint8_t* dest,
                                  uint64_t orig_len=0);

int64_t compress8b_delta(uint8_t* src, size_t len, int8_t* dest,
                         bool write_size=true);
int64_t decompress8b_delta(int8_t* src, uint8_t* dest);

int64_t compress8b_online(uint8_t* src, size_t len, int8_t* dest,
                          bool write_size=true);
int64_t decompress8b_online(int8_t* src, uint8_t* dest);

int64_t compress8b_delta_online(uint8_t* src, size_t len, int8_t* dest,
                                bool write_size=true);
int64_t decompress8b_delta_online(int8_t* src, uint8_t* dest);

int64_t compress8b_delta2_online(uint8_t* src, size_t len, int8_t* dest,
                                 bool write_size=true);
int64_t decompress8b_delta2_online(int8_t* src, uint8_t* dest);

int64_t compress8b_delta_rle(uint8_t* src, size_t len, int8_t* dest,
                             bool write_size=true);
int64_t decompress8b_delta_rle(int8_t* src, uint8_t* dest);

int64_t compress8b_delta_rle2(uint8_t* src, size_t len, int8_t* dest,
                              bool write_size=true);
int64_t decompress8b_delta_rle2(int8_t* src, uint8_t* dest);

int64_t compress8b_doubledelta(uint8_t* src, size_t len, int8_t* dest,
                               bool write_size=true);
int64_t decompress8b_doubledelta(int8_t* src, uint8_t* dest);

int64_t compress8b_dyndelta(uint8_t* src, size_t len, int8_t* dest,
                            bool write_size=true);
int64_t decompress8b_dyndelta(int8_t* src, uint8_t* dest);



#endif /* sprintz_h */
