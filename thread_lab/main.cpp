#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using clock_ta = std::chrono::steady_clock;

static std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock_ta::now().time_since_epoch())
            .count());
}

// One thread: walk [begin, end) in order, several passes (sink).
static std::uint64_t walk_slice(const std::uint64_t* data,
                                std::size_t begin,
                                std::size_t end,
                                std::size_t passes) {
    std::uint64_t sum = 0;
    for (std::size_t p = 0; p < passes; ++p)
        for (std::size_t i = begin; i < end; ++i)
            sum += data[i];
    return sum;
}

int main() {
    const std::size_t kBytes = 256 * 1024 * 1024;  // 256 MiB — past L2
    const std::size_t kPasses = 32;                // ~8 GiB of reads total
    const int kThreads[] = {1, 2, 4, 8, 16};

    const std::size_t n = kBytes / sizeof(std::uint64_t);
    std::vector<std::uint64_t> data(n, 1);  // first-touch here, not on the clock

    double t1_sec = 0.0;

    std::cout << std::left
              << std::setw(8) << "threads"
              << std::right
              << std::setw(12) << "seconds"
              << std::setw(12) << "GB/s"
              << std::setw(12) << "speedup"
              << std::setw(12) << "efficiency"
              << "    sink\n";

    for (int N : kThreads) {
        const std::size_t chunk = n / static_cast<std::size_t>(N);
        std::vector<std::thread> pool;
        std::vector<std::uint64_t> sink(static_cast<std::size_t>(N), 0);

        const std::uint64_t t0 = now_ns();
        for (int t = 0; t < N; ++t) {
            const std::size_t begin = static_cast<std::size_t>(t) * chunk;
            const std::size_t end =
                (t == N - 1) ? n : begin + chunk;
            pool.emplace_back([&, t, begin, end]() {
                sink[static_cast<std::size_t>(t)] =
                    walk_slice(data.data(), begin, end, kPasses);
            });
        }
        for (auto& th : pool)
            th.join();
        const std::uint64_t t1 = now_ns();

        const double seconds = static_cast<double>(t1 - t0) / 1e9;
        if (N == 1)
            t1_sec = seconds;

        // All threads together still touch the whole array, kPasses times.
        const double bytes_touched =
            static_cast<double>(n) * sizeof(std::uint64_t) *
            static_cast<double>(kPasses);
        const double gb_per_s =
            (seconds == 0.0) ? 0.0 : (bytes_touched / seconds) / 1e9;
        const double speedup = (seconds == 0.0) ? 0.0 : t1_sec / seconds;
        const double efficiency = speedup / static_cast<double>(N);

        std::uint64_t total_sink = 0;
        for (std::uint64_t s : sink)
            total_sink += s;

        std::cout << std::left << std::setw(8) << N << std::right << std::fixed
                  << std::setprecision(4) << std::setw(12) << seconds
                  << std::setprecision(3) << std::setw(12) << gb_per_s
                  << std::setprecision(3) << std::setw(12) << speedup
                  << std::setprecision(3) << std::setw(12) << efficiency
                  << "    " << total_sink << '\n';
    }

    return 0;
}