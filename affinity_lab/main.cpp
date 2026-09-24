#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#endif

#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <sched.h>
#endif

static std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}
static bool pin_self(int cpu_or_tag) {
#ifdef __linux__
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu_or_tag, &set);
  return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#elif defined(__APPLE__)
  thread_affinity_policy_data_t policy;
  policy.affinity_tag = cpu_or_tag;
  const kern_return_t kr = thread_policy_set(
      pthread_mach_thread_np(pthread_self()), THREAD_AFFINITY_POLICY,
      reinterpret_cast<thread_policy_t>(&policy), THREAD_AFFINITY_POLICY_COUNT);
  return kr == KERN_SUCCESS;
#else
  (void)cpu_or_tag;
  return false;
#endif
}
static void burn(std::uint64_t n) {
  volatile std::uint64_t x = 0;
  for (std::uint64_t i = 0; i < n; i++) {
    x += i;
  }
}

static double bench_pair(int pin_a, int pin_b, std::uint64_t work) {
  const std::uint64_t t0 = now_ns();
  std::thread t0th([=]() {
    pin_self(pin_a);
    burn(work);
  });
  std::thread t1th([=]() {
    pin_self(pin_b);
    burn(work);
  });
  t0th.join();
  t1th.join();
  return static_cast<double>(now_ns() - t0) / 1e9;
}

int main() {
  const std::uint64_t kWork = 200000000;
  const unsigned hc = std::thread::hardware_concurrency();
  std::cout << "hardware_concurrency=" << hc << '\n';
#ifdef __APPLE__
  std::cout << "macOS: THREAD_AFFINITY_POLICY is a hint. \n"
            << "same tag=same place; different tags ~ spread out.\n"
            << "Not a hard pin to 'core 0'. M5 has no SMT siblings\n\n";
  const int a = 1, b = 2;
  const int same = 1;
#elif defined(__linux__)
  std::cout << "Linux: sched_setaffinity hard-pins logical CPUs\n\n";
  const a = 0, b = 1;
  const int same = 0;
#else
  std::cout << "no affinity API on this OS\n";
  return 1;
#endif
  std::cout << std::fixed << std::setprecision(4);
  std::cout << "placement                       seconds\n";
  const double apart = bench_pair(a, b, kWork);
  std::cout << std::left << std::setw(30) << "different cores/tags"
            << std::right << std::setw(10) << apart << '\n';
  const double together = bench_pair(same, same, kWork);
  std::cout << std::left << std::setw(30) << "same core/tag" << std::right
            << std::setw(10) << together << '\n';
#ifdef __linux__
  std::cout << "\nIf /sys/.../thread_siblings_list is '0,8', "
               "rerun mentally: (0,1) = two cores, (0,8)=one core two HT. \n";
#endif
  std::cout << "\n same/different ratio=" << (together / apart) << '\n';
  std::cout << "except same-core slower(they take turns)\n";
  return 0;
}