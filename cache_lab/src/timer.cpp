#include"timer.hpp"
#include<chrono>
std::uint64_t now_ns(){
    using clock=std::chrono::steady_clock;
    const auto t=clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t).count()
    );
}