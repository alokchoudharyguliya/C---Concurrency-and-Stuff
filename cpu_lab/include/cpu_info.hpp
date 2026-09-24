#pragma once
#include<cstddef>
#include<string>
struct CpuInfo{
    std::string name;
    unsigned physical_core=0;
    unsigned logical_cpus=0;
    std::size_t cache_line=0;
    std::size_t l1d=0;
    std::size_t l2=0;
    std::size_t l3=0;
    unsigned numa_nodes=0;
};

CpuInfo probe_cpu();

void print_cpu_report(const CpuInfo&info);
