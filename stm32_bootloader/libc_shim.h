// libc_shim.h / .cpp
// The vendored Ed25519/SHA-512 library (plain C) expects memcpy/memset/
// memcmp to exist. Rather than link a real libc into a bootloader, we
// provide the tiny handful of functions actually needed, with C linkage
// so both our C++ code and the vendored C code resolve to the same
// symbols.
#pragma once
#include <cstddef>

extern "C" {
void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int value, size_t n);
int   memcmp(const void* a, const void* b, size_t n);
}
