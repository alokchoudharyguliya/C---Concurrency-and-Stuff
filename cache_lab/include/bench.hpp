#pragma once
#include<cstddef>
#include<cstdint>

enum class Pattern{
    Sequential,
    Stride,
    Random,
};
struct BenchResult{
    std::size_t bytes=0;
    Pattern pattern = Pattern::Sequential;
    double seconds=0.0;
    double ns_per_access=0.0;
    double gb_per_s=0.0;
    std::uint64_t sink=0;
};