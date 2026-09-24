#include"report.hpp"
#include<iostream>
#include<iomanip>
#include<string>

static const char*pattern_name(Pattern p){
    switch(p){
        case Pattern::Sequential:   return "sequential";
        case Pattern::Stride:       return "stride-16";
        case Pattern::Random:       return "random";
    }
    return "?";
}

static std::string size_label(std::size_t bytes){
    if(bytes>=1024*1024)return std::to_string(bytes/(1024*1024))+"MiB";
    return std::to_string(bytes/1024)+"KiB";
}
void print_header(){
    std::cout<<std::left
            <<std::setw(10)<<"size"
            <<std::setw(14)<<"pattern"
            <<std::right
            <<std::setw(12)<<"seconds"
            <<std::setw(14)<<"ns/access"
            <<std::setw(10)<<"GB/s"
            <<"     sink\n";
}
void print_row(const BenchResult& r){
    std::cout<<std::left
    <<std::setw(10)<<size_label(r.bytes)
    <<std::setw(14)<<pattern_name(r.pattern)
    <<std::right<<std::fixed
    <<std::setprecision(6)<<std::setw(12)<<r.seconds
    <<std::setprecision(3)<<std::setw(14)<<r.ns_per_access
    <<std::setprecision(3)<<std::setw(10)<<r.gb_per_s
    <<"     "<<r.sink<<'\n';
}