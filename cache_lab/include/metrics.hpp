#pragma once
#include"bench.hpp"
#include<cstddef>
#include<cstdint>
BenchResult to_metrics(std::size_t bytes,
std::size_t accesses, std::uint64_t ns, Pattern pattern, std::uint64_t sink);