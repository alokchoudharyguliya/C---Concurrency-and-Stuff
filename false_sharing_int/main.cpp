#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#ifdef __cpp_lib_hardware_interference_size
#include <new>
#endif

static std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

struct A {
  int x;
  int y;
};
struct B {
  alignas(64) int x;
  alignas(64) int y;
};

struct C {
  alignas(128) int x;
  alignas(128) int y;
};

#ifdef __cpp_lib_hardware_interference_size
struct D {
  alignas(std::hardware_destructive_interference_size) int x;
  alignas(std::hardware_destructive_interference_size) int y;
};
#endif

static void hammer(volatile int *p, std::uint64_t trips) {
  for (std::uint64_t i = 0; i < trips; i++)
    ++*p;
}
static void hammer_both(volatile int *x,volatile int *y, std::uint64_t trips) {
  for (std::uint64_t i = 0; i < trips; i++) {
    ++*x;
    ++*y;
  }
}

template <typename S>
static double bench_two_threads(S &s, std::uint64_t trips) {
  s.x = 0;
  s.y = 0;
  const std::uint64_t t0 = now_ns();
  std::thread t1(hammer, &s.x, trips);
  std::thread t2(hammer, &s.y, trips);
  t1.join();
  t2.join();
  return static_cast<double>(now_ns() - t0) / 1e9;
}

template <typename S>
static double bench_one_thread(S &s, std::uint64_t trips) {
  s.x = 0;
  s.y = 0;
  const std::uint64_t t0 = now_ns();
  hammer_both(&s.x, &s.y, trips);
  return static_cast<double>(now_ns() - t0) / 1e9;
}

template <typename S> static void print_layout(const char *name, const S &s) {
  const char *base = reinterpret_cast<const char *>(&s);
  std::cout << name << " sizeof=" << sizeof(S) << " alignof=" << alignof(S)
            << " y-x=" << (reinterpret_cast<const char *>(&s.y) - base)
            << " B\n";
}

int main() {
  const std::uint64_t kTrips = 80000000;

#ifdef __cpp_lib_hardware_interference_size
  std::cout << "destructive " << std::hardware_destructive_interference_size
            << " B constructive" << std::hardware_constructive_interference_size
            << " B\n";
#else
  std::cout << "hardware_intereference_size not provided; use 128B line\n";
#endif

  A a{};
  B b{};
  C c{};
  print_layout("A packed        ", a);
  print_layout("B align 64      ", b);
  print_layout("C align 128     ", c);

#ifdef __cpp_lib_hardware_interference_size
  D d{};
  print_layout("D destructive   ", d);
#endif
  const double a2 = bench_two_threads(a, kTrips);
  const double b2 = bench_two_threads(b, kTrips);
  const double c2 = bench_two_threads(c, kTrips);
#ifdef __cpp_lib_hardware_interference_size
  const double d2 = bench_two_threads(d, kTrips);
#endif
  const double a1 = bench_one_thread(a, kTrips);
  const double b1 = bench_one_thread(b, kTrips);
  const double c1 = bench_one_thread(c, kTrips);
#ifdef __cpp_lib_hardware_interference_size
  const double d1 = bench_one_thread(d, kTrips);
#endif

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "\n struct           2-threads(s)        1-thread(s)         x  "
               "         y\n";
  std::cout << "A packed                " << a2 << "            " << a1
            << "        " << a.x << "       " << a.y << '\n';
  std::cout << "B align 64              " << b2 << "            " << b1
            << "        " << b.x << "       " << b.y << '\n';
  std::cout << "C align 128             " << c2 << "            " << c1
            << "        " << c.x << "       " << c.y << '\n';

#ifdef __cpp_lib_hardware_interference_size
  std::cout << "D destructive           " << d2 << "            " << d1 << "        " << d.x
            << "        " << d.y << '\n';
#endif
  return 0;
}