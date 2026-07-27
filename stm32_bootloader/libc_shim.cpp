// libc_shim.cpp
#include "libc_shim.h"

extern "C" void* memcpy(void* dst, const void* src, size_t n) {
    auto* d = static_cast<unsigned char*>(dst);
    auto* s = static_cast<const unsigned char*>(src);
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}

extern "C" void* memset(void* dst, int value, size_t n) {
    auto* d = static_cast<unsigned char*>(dst);
    unsigned char v = static_cast<unsigned char>(value);
    for (size_t i = 0; i < n; ++i) d[i] = v;
    return dst;
}

extern "C" int memcmp(const void* a, const void* b, size_t n) {
    auto* pa = static_cast<const unsigned char*>(a);
    auto* pb = static_cast<const unsigned char*>(b);
    for (size_t i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) return pa[i] < pb[i] ? -1 : 1;
    }
    return 0;
}
