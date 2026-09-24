#include "sequential.hpp"
std::uint64_t seq_sum(const std::uint64_t *data, std::size_t n) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < n; i++)
    sum += data[i];
  return sum;
}