#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

std::vector<std::size_t> make_shuffled_index(std::size_t n);
std::uint64_t random_sum(const std::uint64_t *data, const std::size_t *idx,
                         std::size_t n);