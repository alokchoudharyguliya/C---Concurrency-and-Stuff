#include "random.hpp"
#include <algorithm>
#include <cstddef>
#include <numeric>
#include <random>
std::vector<std::size_t> make_shuffled_index(std::size_t n) {
  std::vector<std::size_t> idx(n);
  std::iota(idx.begin(), idx.end(), 0);
  std::mt19937 rng(1);
  std::shuffle(idx.begin(), idx.end(), rng);
  return idx;
}
std::uint64_t random_sum(const std::uint64_t *data, const std::size_t *idx,
                          std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; i++)
    sum += data[idx[i]];
  return sum;
}