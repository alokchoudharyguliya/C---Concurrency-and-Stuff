#include"alloc.hpp"
std::vector<std::uint64_t>allocate(std::size_t bytes){
    const std::size_t n=bytes/sizeof(std::uint64_t);
    std::vector<std::uint64_t>data(n,1);
    return data;
}