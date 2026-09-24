
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
static std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

struct alignas(128) Split {
  std::atomic<long> a;
  char pad[128 - sizeof(std::atomic<long>)];
  std::atomic<long> b;
};

struct OrderRow {
  const char *name;
  std::memory_order mo;
};
static const OrderRow kOrders[] = {
    {"relaxed", std::memory_order_relaxed},
    {"acquire", std::memory_order_acquire},
    {"release", std::memory_order_release},
    {"acq_rel", std::memory_order_acq_rel},
    {"seq_cst", std::memory_order_seq_cst},
};
static void hammer(std ::atomic<long> *p, std::uint64_t trips,
                   std::memory_order mo) {
  for (std::uint64_t i = 0; i < trips; i++)
    p->fetch_add(1, mo);
}

static double bench_split(std::memory_order mo, std::uint64_t trips) {
  Split s{};
  s.a.store(0, std::memory_order_relaxed);
  s.b.store(0, std::memory_order_relaxed);
  const std::uint64_t t0 = now_ns();
  std::thread t1(hammer, &s.a, trips, mo);
  std::thread t2(hammer, &s.b, trips, mo);
  t1.join();
  t2.join();
  const double sec = static_cast<double>(now_ns() - t0) / 1e9;

  if (s.a.load(std::memory_order_relaxed) != static_cast<long>(trips) ||
      s.b.load(std::memory_order_relaxed) != static_cast<long>(trips))
    std::cout << "    (sink mismatch)\n";
  return sec;
}
static double bench_shared(std::memory_order mo, std::uint64_t trips) {
  std::atomic<long> c{0};
  const std::uint64_t t0 = now_ns();
  std::thread t1(hammer, &c, trips, mo);
  std::thread t2(hammer, &c, trips, mo);
  t1.join();
  t2.join();
  const double sec = static_cast<double>(now_ns() - t0) / 1e9;
  const long got = c.load(std::memory_order_relaxed);

  if (got != static_cast<long>(2 * trips))
    std::cout << "    (shared sink " << got << ")\n";
  return sec;
}
int main() {
  const std::uint64_t kTrips = 40000000;
  std::cout << "faster is not better - ordering is a correctness contract\n\n";
  std::cout << std::fixed << std::setprecision(4);
  std::cout << "order       split 2-counter(s)      shared 1-coutner(s\n)";
  for (const OrderRow &row : kOrders) {
    const double split = bench_split(row.mo, kTrips);
    const double shared = bench_shared(row.mo, kTrips);
    std::cout << std::left << std::setw(10) << row.name << std::right
              << std::setw(18) << split << std::setw(22) << shared << '\n';
  }
  return 0;
}