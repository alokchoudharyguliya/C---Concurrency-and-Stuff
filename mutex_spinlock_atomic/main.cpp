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
class SpinLock {
  std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
  void lock() {
    while (flag.test_and_set(std::memory_order_acquire)) {
    }
  }
  void unlock() { flag.clear(std::memory_order_release); }
};
static void burn(std::uint32_t n) {
  volatile std::uint32_t x = 0;
  for (std::uint32_t i = 0; i < n; i++)
    x += i;
}

enum class Kind { Mutex, Spin, Atomic };
static double bench(Kind kind, int nthreads, std::uint64_t incs_per_thread,
                    std::uint32_t pause) {
  std::mutex mx;
  SpinLock sl;
  std::atomic<std::uint64_t> atom{0};
  std::uint64_t plain = 0;
  const std::uint64_t t0 = now_ns();
  std::vector<std::thread> pool;
  pool.reserve(static_cast<std::size_t>(nthreads));
  for (int t = 0; t < nthreads; t++) {
    pool.emplace_back([&, kind] {
      for (std::uint64_t i = 0; i < incs_per_thread; i++) {
        burn(pause);
        switch (kind) {
        case Kind::Mutex:
          mx.lock();
          ++plain;
          mx.unlock();
          break;
        case Kind::Spin:
          sl.lock();
          ++plain;
          sl.unlock();
          break;
        case Kind::Atomic:
          atom.fetch_add(1, std::memory_order_relaxed);
          break;
        }
      }
    });
  }
  for (auto &th : pool)
    th.join();
  const double sec = static_cast<double>(now_ns() - t0) / 1e9;
  const std::uint64_t want =
      static_cast<std::uint64_t>(nthreads) * incs_per_thread;
  const std::uint64_t got =
      (kind == Kind::Atomic) ? atom.load(std::memory_order_relaxed) : plain;
  if (got != want)
    std::cout << "      sink" << got << "   want" << want << "\n";
  return sec;
}

int main() {
  const int kThreads = 8;
  const std::uint64_t kIncs = 200000;
  struct Row {
    const char *name;
    std::uint32_t pause;
  };
  const Row levels[] = {{"low", 400}, {"medium", 40}, {"high", 0}};
  std::cout << "8 threads, " << kIncs
            << " incs each; pause = work outside the lock\n"
            << " atomic is fetch_add, not a lock - not interchangeable\n\n";
  std::cout << std::fixed << std::setprecision(4);
  std::cout
      << "Contention          mutex(s)        spin(s)         atomic(s)\n";
  for (const Row &lv : levels) {
    const double tm = bench(Kind::Mutex, kTreads, kIncs, lv.pause);
    const double ts = bench(Kind::Spin, kThreads, kIncs, lv.pause);
    const double ta = bench(Kind::Atomic, kThreads, kIncs, lv.pause);
    std::cout << std::left << std::setw(14) << lv.name << std::right
              << std::setw(12) << tm << std::setw(12) << ts << std::setw(12)
              << ta << '\n';
  }
  return 0;
}