#include "cpu_info.hpp"
#include <iostream>

void print_cpu_report(const CpuInfo &info) {
  auto kib = [](std::size_t bytes) { return bytes / 1024; };
  std::cout << "CPU:                "
            << (info.name.empty() ? "(unknown)" : info.name) << '\n';
  std::cout << "Physical cores:     " << info.physical_core << '\n';
  std::cout << "Logical Cpus:      " << info.logical_cpus << '\n';
  std::cout << "Cache Line size:    " << info.cache_line << "B\n";
  std::cout << "L1 data cache:      " << kib(info.l1d) << "KiB\n";
  std::cout << "L2 cache:           " << kib(info.l2) << "KiB\n";

  if (info.l3 == 0)
    std::cout << "L3 cache:           not reported\n";
  else
    std::cout << "L3 cache:           " << kib(info.l3) << "KiB\n";
  std::cout << "NUMA nodes:         " << info.numa_nodes << '\n';
}