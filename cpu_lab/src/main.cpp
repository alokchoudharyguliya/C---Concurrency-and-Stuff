#include <thread>
#include "cpu_info.hpp"

int main() {
    CpuInfo info = probe_cpu();
    print_cpu_report(info);   // from report.cpp

    unsigned hc = std::thread::hardware_concurrency();
    // print hc
    // print whether hc == logical_cpus, == physical_cores, or 0
}