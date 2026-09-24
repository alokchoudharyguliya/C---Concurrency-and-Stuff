#include "stride.hpp"
std::uint64_t stride_sum(const std::uint64_t *data, std::size_t n,
                         std::size_t stride) {
  std::uint64_t sum = 0;
  if (n == 0 || stride == 0)
    return 0;
  for (std::size_t i = 0; i < n; i++)
    sum += data[(i * stride) % n];
  return sum;
}