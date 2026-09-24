#pragma once
#include<cstddef>

// inline constexpr in a header is one copy of the table for every .cpp that includes it, with no linker clash.

inline constexpr std::size_t kKiB=1024;
inline constexpr std::size_t kMiB=1024*1024;
inline constexpr std::size_t kNumSizes=10;
inline constexpr std::size_t kWorkingSetBytes[kNumSizes]={
    4   * kKiB,   // 4 KB
    16  * kKiB,   // 16 KB
    32  * kKiB,   // 32 KB
    64  * kKiB,   // 64 KB   ← ~L1d on your M5
    256 * kKiB,   // 256 KB
    1   * kMiB,   // 1 MB
    4   * kMiB,   // 4 MB
    16  * kMiB,   // 16 MB   ← past 6 MiB L2
    64  * kMiB,   // 64 MB
    256 * kMiB,   // 256 MB
};
