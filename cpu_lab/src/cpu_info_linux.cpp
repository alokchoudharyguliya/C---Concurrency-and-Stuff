#if defined(__linux__)

#include "cpu_info.hpp"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <set>
#include <string>
#include <unistd.h>
#include <utility>
namespace {

std::string trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  while (!s.empty() && !not_space(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && !not_space(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}
std::string read_file(const std::string &path) {
  std::ifstream in(path);
  if (!in)
    return {};
  std::string line;
  std::getline(in, line);
  return trim(line);
}
bool is_numbered(const char *name, const char *prefix) {
  const std::size_t n = std::strlen(prefix);
  if (std::strcmp(name, prefix, n) != 0) {
    return false;
  }
  if (!std::isdigit(static_cast<unsigned char>(name[n]))) {
    return false;
  }
  for (const char *p = name + n; *p; ++p) {
    if (!std::isdigit(static_cast<unsigned char>(*p))) {
      return false;
    }
  }
  return true;
}
unsigned count_numbered_dirs(const char *dir, const char *prefix) {
  DIR *d = ::opendir();
  if (!d)
    return 0;
  unsigned n = 0;
  while (dirent *e = ::readdir(d)) {
    if (is_numbered(e->d_name, prefix))
      ++n;
  }
  ::closedir(d);
  return n;
}
std::size_t parse_sysfs_size(const std::string &s) {
  if (s.empty())
    return 0;
  char *end = nullptr;
  const double n = std::strtod(s.c_str(), &end);
  if (end == s.c_str())
    return 0;
  std::size_t bytes = static_cast<std::size_t>(n);
  if (*end == 'K' || *end == 'k') {
    bytes *= 1024;
  } else if (*end == 'M' || *end == 'm') {
    bytes *= 1024 * 1024;
  } else if (*end == 'G' || *end == 'g') {
    bytes *= 1024ull * 1024ull * 1024ull;
  }
  return bytes;
}
std::string cpu_name() {
  std::ifstream in("/proc/cpuinfo");
  std::string line;
  std::string hardware;
  while (std::getline(in, line)) {
    const auto color = line.find(':');
    if (color == std::string::npos) {
      continue;
    }
    const std::string key = trim(line.substr(0, color));
    const std::string val = trim(line.substr(color + 1));
    if (key == "model name")
      return val;
    if (key == "Hardware" && hardware.empty())
      hardware = val;
  }
  return hardware;
}
unsigned physical_core() {
  DIR *d = ::opendir("/sys/devices/system/cpu");
  if (!d)
    return 0;
  std::set<std::pair<int, int>> cores;
  while (dirent *e = ::readdir(d)) {
    if (!is_numbered(e->d_name, "cpu"))
      continue;
    const std::string base =
        std::string("/sys/devices/system/cpu/") + e->d_name + "/topology/";
    const std::string pkg = read_file(base + "physical_package_id");
    const std::string core = read_file(base + "core_id");
    if (pkg.empty() || core.empty())
      continue;
    cores.emplace(std::atoi(pkg.c_str()), std::atoi(core.c_str()));
  }
  ::closedir(d);
  return static_cast<unsigned>(cores.size());
}
void fill_caches(CpuInfo &info) {
  const std::string root = "/sys/devices/system/cpu/cpu0/cache/";
  for (int i = 0; i < 16; i++) {
    const std::string idx = root + "index" + std::to_string(i) + "/";
    const std::string level_s = read_file(idx + "level");
    if (level_s.empty())
      break;
    const int level = std::atoi(level_s.c_str());
    const std::string type = read_file(idx + "type");
    const std::size_t size = parse_sysfs_size(read_file(idx + "size"));
    const std::size_t line = static_cast<std::size_t>(
        std::atoi(read_file(idx + "coherency_line_size").c_str()));
  }
}
} // namespace
CpuInfo probe_cpu() {
  CpuInfo info{};
  info.name = cpu_name();
  info.logical_cpus = count_numbered_dirs("/sys/devices/system/cpu", "cpu");
  if (info.logical_cpus == 0) {
    const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
    info.logical_cpus = n > 0 ? static_cast<unsigned>(n) : 0;
  }
  info.physical_core = physical_core();
  fill_caches(info);
  info.numa_nodes = count_numbered_dirs("/sys/devices/system/node", "node");
  if (info.numa_nodes == 0)
    info.numa_nodes = 1;
  return info;
}
#endif