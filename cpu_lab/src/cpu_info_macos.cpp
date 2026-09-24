#if defined(__APPLE__)
#include "cpu_info.hpp"
#include <cstdint>
#include <string>
#include <sys/sysctl.h>
namespace {

bool read_i64(const char *key, std::int64_t &out) {
  std::int64_t v = 0;
  std::size_t n = sizeof(v);
  if (::sysctlbyname(key, &v, &n, nullptr, 0) == 0) {
    out = v;
    return true;
  }
  int i = 0;
  n = sizeof(i);
  if (::sysctlbyname(key, &i, &n, nullptr, 0) == 0) {
    out = i;
    return true;
  }
  return false;
}

std::string read_str(const char *key) {
  std::size_t n = 0;
  if (::sysctlbyname(key, nullptr, &n, nullptr, 0) != 0 || n == 0)
    return {};
  std::string s(n, '\0');
  if (::sysctlbyname(key, s.data(), &n, nullptr, 0) != 0)
    return {};
  if (const auto z = s.find('\0'); z != std::string::npos)
    s.resize(z);
  return s;
}

} // namespace
CpuInfo probe_cpu() {
  CpuInfo info{};
  info.name = read_str("machdep.cpu.brand_string");
  if (info.name.empty())
    info.name = read_str("hw.model");
  std::int64_t v = 0;
  if (read_i64("hw.physicalcpu", v)) {
    info.physical_core = static_cast<unsigned>(v);
  }

  if (read_i64("hw.logicalcpu", v)) {
    info.logical_cpus = static_cast<unsigned>(v);
  }

  if (read_i64("hw.cachelinesize", v)) {
    info.cache_line = static_cast<unsigned>(v);
  }
  if (read_i64("hw.l1dcachesize", v)) {
    info.l1d = static_cast<unsigned>(v);
  }
  if (read_i64("hw.l2cachesize", v)) {
    info.l2 = static_cast<unsigned>(v);
  }
  if (read_i64("hw.l3cachesize", v)) {
    info.l3 = static_cast<unsigned>(v);
  }
  info.numa_nodes = 1;
  return info;
}
#endif

// STUB for compiling on Linux
// CpuInfo probe_cpu(){
// return {};
// }