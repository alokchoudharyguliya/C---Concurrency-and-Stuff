#include "alloc.hpp"
#include "bench.hpp"
#include "metrics.hpp"
#include "report.hpp"
#include "sequential.hpp"
#include "sizes.hpp"
#include "timer.hpp"
#include "random.hpp"
#include "stride.hpp"
#include <cstddef>

#include <iostream>

static std::uint64_t one_walk(Pattern pattern, const std::uint64_t *data,
                              std::size_t n, const std::size_t *idx) {
  switch (pattern) {
  case Pattern::Sequential:
    return seq_sum(data, n);
  case Pattern::Stride:
    return stride_sum(data, n, 16);
  case Pattern::Random:
    return random_sum(data, idx, n);
  }
  return 0;
}

static BenchResult run_pattern(std::size_t bytes, std::size_t n,
                               Pattern pattern, const std::uint64_t *data,
                               const std::size_t *idx) {
  const std::uint64_t kTargetNs = 100000000;
  (void)one_walk(pattern, data, n, idx);
  std::uint64_t t0 = now_ns();
  std::uint64_t sink = one_walk(pattern, data, n, idx);
  std::uint64_t t1 = now_ns();
  std::uint64_t one_pass = t1 - t0;
  if (one_pass == 0)
    one_pass = 1;
  std::size_t repeats = static_cast<std::size_t>(kTargetNs / one_pass);
  if (repeats < 1)
    repeats = 1;
  sink = 0;
  t0 = now_ns();
  for (std::size_t r = 0; r < repeats; r++)
    sink += one_walk(pattern, data, n, idx);
  t1 = now_ns();
  return to_metrics(bytes, n * repeats, t1 - t0, pattern, sink);
}

int main() {
  // for(std::size_t i=0;i<kNumSizes;i++){
  //     std::cout<<kWorkingSetBytes[i]<<'\n';
  // }

  // BenchResult r{};
  // r.bytes=kWorkingSetBytes[0];
  // r.pattern=Pattern::Sequential;
  // (void)r;

  // const std::uint64_t t0=now_ns();
  // volatile std::uint64_t sink=0;
  // for(std::uint64_t i=0;i<100000;i++)
  // sink+=i;
  // const std::uint64_t t1=now_ns();
  // std::cout<<(t1-t0)<<"ns\n";
  // std::cout<<"sink="<<sink<<'\n';

  // auto data=allocate(kWorkingSetBytes[0]);
  // std::cout<<"n="<<data.size()<<'\n';
  // std::cout<<"data[0]="<<data[0]<<'\n';

  // auto data=allocate(kWorkingSetBytes[0]);
  // const std::uint64_t sum = seq_sum(data.data(), data.size());
  // std::cout<<"n="<<data.size()<<'\n';
  // std::cout<<"sum="<<sum<<"\n";

  // BenchResult r=to_metrics(
  //     4096,
  //     1000000000,
  //     1000000000,
  //     Pattern::Sequential,
  //     0
  // );
  // std::cout<<r.seconds<<'\n';
  // std::cout<<r.ns_per_access<<'\n';
  // std::cout<<r.gb_per_s<<'\n';

  // BenchResult r=to_metrics(
  //     4096,
  //     1000000000,
  //     1000000000,
  //     Pattern::Sequential,
  //     0
  // );
  // print_header();
  // print_row(r);

  // const std::uint64_t kTargetNs = 100000000;
  // print_header();
  // for (std::size_t s = 0; s < kNumSizes; s++) {
  //   const std::size_t bytes = kWorkingSetBytes[s];
  //   auto data = allocate(bytes);
  //   const std::size_t n = data.size();
  //   (void)seq_sum(data.data(), n);
  //   std::uint64_t t0 = now_ns();
  //   std::uint64_t sink = seq_sum(data.data(), n);
  //   std::uint64_t t1 = now_ns();
  //   std::uint64_t one_pass = t1 - t0;
  //   if (one_pass == 0)
  //     one_pass = 1;
  //   std::size_t repeats = static_cast<std::size_t>(kTargetNs / one_pass);
  //   if (repeats < 1)
  //     repeats = 1;
  //   sink = 0;
  //   t0 = now_ns();
  //   for (std::size_t r = 0; r < repeats; r++)
  //     sink += seq_sum(data.data(), n);
  //   t1 = now_ns();
  //   const std::size_t accesses = n * repeats;
  //   BenchResult row =
  //       to_metrics(bytes, accesses, t1 - t0, Pattern::Sequential, sink);
  //   print_row(row);
  // }
  const Pattern patterns[] = {Pattern::Sequential, Pattern::Stride,
                              Pattern::Random};
  print_header();
  for (std::size_t s = 0; s < kNumSizes; s++) {
    const std::size_t bytes = kWorkingSetBytes[s];
    // auto data = allocate(bytes);
    // const std::size_t n=);
    std::vector<std::uint64_t>data(bytes/sizeof(std::uint64_t),1);
    const std::size_t n = data.size();
    auto idx = make_shuffled_index(n);
    for (Pattern p : patterns)
      print_row(run_pattern(bytes, n, p, data.data(), idx.data()));
  }
  return 0;
}