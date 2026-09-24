#include"metrics.hpp"

BenchResult to_metrics(std::size_t bytes, std::size_t accesses, std::uint64_t ns, Pattern pattern, std::uint64_t sink){
    BenchResult r{};
    r.bytes=bytes;
    r.pattern=pattern;
    r.sink=sink;

    r.seconds=static_cast<double>(ns)/1e9;
    r.ns_per_access=(accesses==0)?0.0:static_cast<double>(ns)/static_cast<double>(accesses);
    const double bytes_touched=static_cast<double>(accesses)*static_cast<double>(sizeof(std::uint64_t));
    r.gb_per_s=(r.seconds == 0.0 )? 0.0:(bytes_touched/r.seconds)/1e9;
    return r;
    
}