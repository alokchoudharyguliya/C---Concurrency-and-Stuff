#pragma once
#include <cstddef>
#include <cstdint>
std::uint64_t stride_sum(const std::uint64_t *data, std::size_t n,
                         std::size_t stride);