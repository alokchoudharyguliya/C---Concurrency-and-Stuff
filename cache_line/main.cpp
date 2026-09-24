#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#ifdef __cpp_lib_hardware_interference_size
#include <new>
#endif

static std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

struct Packed {
    std::atomic<long> a;
    std::atomic<long> b;
};

// Brief: assumes a 64 B line. On M5, a and b can still share one 128 B line.
struct alignas(64) Padded64 {
    std::atomic<long> a;
    char padding[56];
    std::atomic<long> b;
};

// One full line on your machine (Phase 1: 128 B).
struct alignas(128) Padded128 {
    std::atomic<long> a;
    char padding[128 - sizeof(std::atomic<long>)];
    std::atomic<long> b;
};

static void hammer(std::atomic<long>* p, std::uint64_t trips) {
    for (std::uint64_t i = 0; i < trips; ++i)
        p->fetch_add(1, std::memory_order_relaxed);
}

template <typename C>
static double bench_pair(C& c, std::uint64_t trips) {
    c.a.store(0);
    c.b.store(0);

    const std::uint64_t t0 = now_ns();
    std::thread t1(hammer, &c.a, trips);
    std::thread t2(hammer, &c.b, trips);
    t1.join();
    t2.join();
    const std::uint64_t t1ns = now_ns();

    return static_cast<double>(t1ns - t0) / 1e9;
}

template <typename C>
static void print_layout(const char* name, const C& c) {
    const auto* base = reinterpret_cast<const char*>(&c);
    std::cout << name
              << "  sizeof=" << sizeof(C)
              << "  a@" << static_cast<const void*>(&c.a)
              << "  b@" << static_cast<const void*>(&c.b)
              << "  b-a=" << (reinterpret_cast<const char*>(&c.b) - base)
              << " B\n";
}

int main() {
    const std::uint64_t kTrips = 80000000;

#ifdef __cpp_lib_hardware_interference_size
    std::cout << "destructive interference "
              << std::hardware_destructive_interference_size << " B\n";
    std::cout << "constructive interference "
              << std::hardware_constructive_interference_size << " B\n";
#else
    std::cout << "hardware_interference_size not provided; "
                 "Phase 1 line was 128 B\n";
#endif

    Packed packed{};
    Padded64 p64{};
    Padded128 p128{};

    print_layout("packed ", packed);
    print_layout("pad 64 ", p64);
    print_layout("pad 128", p128);

    const double t_packed = bench_pair(packed, kTrips);
    const double t_64 = bench_pair(p64, kTrips);
    const double t_128 = bench_pair(p128, kTrips);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\nlayout          seconds     a           b\n";
    std::cout << "packed          " << t_packed << "    "
              << packed.a.load() << "    " << packed.b.load() << '\n';
    std::cout << "pad 64          " << t_64 << "    " << p64.a.load()
              << "    " << p64.b.load() << '\n';
    std::cout << "pad 128         " << t_128 << "    " << p128.a.load()
              << "    " << p128.b.load() << '\n';

    std::cout << "\npacked / pad64  = " << (t_packed / t_64) << '\n';
    std::cout << "packed / pad128 = " << (t_packed / t_128) << '\n';
    return 0;
}