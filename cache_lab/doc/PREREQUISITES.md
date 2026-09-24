# cache_lab — prerequisite APIs, libraries, system calls

Read this **before** any Phase 2 source file. Phase 1 (`cpu_lab`) told you *what* the caches are (L1d / L2 / L3 / line size). Phase 2 *measures* them: allocate a working set, walk it, time the walk, watch latency jump at cache boundaries.

You do not need to memorize every signature. You need to know **which tool does which job**, so each later file has one obvious home.

```
allocate a byte array          →  memory APIs
walk it (seq / stride / rand)  →  your kernels + <random> / <algorithm>
time the walk                  →  <chrono>  (or clock_gettime)
turn times into numbers        →  arithmetic (ns/access, GB/s)
print a table                  →  <iostream> / <iomanip>
```

`CpuInfo` from Phase 1 is the *legend* of the plot (where L1/L2 should break). It is **not** required to compile Phase 2. You may copy `cache_line` / `l1d` / `l2` by hand from `./cpu_lab` output.

Your machine (Apple M5, from Phase 1): 128 B line, 64 KiB L1d, 6 MiB L2, no L3. The size list in the brief is chosen so you cross those walls.

---

# Document 1 — Headers / libraries

Same three kinds as Phase 1:

| Kind | Looks like | Who provides it |
|------|------------|-----------------|
| Your header | `"bench.hpp"` | this lab (later) |
| C++ standard library | `<chrono>`, `<vector>`, `<random>` | libc++ |
| C / POSIX / OS | `<time.h>`, `<stdlib.h>`, `posix_memalign` | C library + kernel |

Quotes search the project. Angle brackets search the system.

---

## Timing

### `<chrono>` (C++11) — **this is the timer you will actually call**

**In this lab:** start a clock, walk the array, stop the clock, convert the difference to nanoseconds.

```cpp
using clock = std::chrono::steady_clock;
auto t0 = clock::now();
// ... work ...
auto t1 = clock::now();
auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
```

| Name | Meaning |
|------|---------|
| `std::chrono::steady_clock` | monotonic: never jumps backward (NTP, sleep). **Use this.** |
| `std::chrono::system_clock` | wall-clock date/time. Can jump. Do **not** time a loop with it. |
| `std::chrono::high_resolution_clock` | “finest available.” On libc++ it is often an alias of `steady_clock` or `system_clock`. Prefer naming `steady_clock` so you know it is monotonic. |
| `now()` | one timestamp (`time_point`) |
| `duration_cast<nanoseconds>(t1 - t0)` | difference as an integer count of ns |
| `.count()` | that integer (`int64_t`-sized) |

**In general:** `<chrono>` is typed time (`duration<Rep, Period>`). You will also see `microseconds`, `milliseconds`, `seconds`. For cache work, **nanoseconds** is the unit; you convert to ns/access yourself.

**Why not `clock()` from `<ctime>`:** `clock()` is *CPU* time, often 1 µs–10 ms resolution, and it ignores time the thread is descheduled. We want *wall* time of the memory walk.

**Why not `gettimeofday`:** microsecond resolution and it follows wall time (can jump). Obsolete.

---

### `<ctime>` / `<time.h>` — POSIX twin (know it, use it only if we say so)

**In this lab (optional fallback):** `clock_gettime(CLOCK_MONOTONIC, &ts)`.

```cpp
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);   // ts.tv_sec, ts.tv_nsec
```

| Piece | Meaning |
|-------|---------|
| `CLOCK_MONOTONIC` | same idea as `steady_clock` |
| `CLOCK_REALTIME` | same idea as `system_clock` — skip |
| `CLOCK_MONOTONIC_RAW` | Linux; not adjusted by adjtime. Not needed here. |
| `timespec::tv_sec` | whole seconds |
| `timespec::tv_nsec` | 0 … 999'999'999 |

On modern macOS this exists. `std::chrono::steady_clock` already wraps a monotonic clock. **Default path: chrono. Know `clock_gettime` so you recognize kernel docs.**

`mach_absolute_time()` + `mach_timebase_info()` is the older Darwin primitive. Do not use it unless a later file asks. Chrono is enough.

---

## Allocation / buffers

### `<cstddef>`

**In this lab:** `std::size_t` for lengths, indices, byte counts. `N` in `for (size_t i = 0; i < N; ++i)` is `std::size_t`.

**In general:** unsigned size type. Use it for anything that is a count of bytes or elements. Do not use `int` for a 256 MB array index (256e6 is fine for `int` on this machine, but the type of `.size()` is `size_t`).

---

### `<cstdint>`

**In this lab:** `std::uint64_t` as the **element type** of the array (`data[i]`). Why 8-byte words:

- one access = 8 bytes → GB/s is `bytes / seconds`
- `sum += data[i]` is a real 64-bit add the compiler cannot pretend is a byte load
- matches what most “memory bandwidth” blogs plot

Also useful: `std::uint8_t` if a later experiment walks raw bytes. Start with `uint64_t`.

---

### `<vector>` — **default allocator for the working set**

**In this lab:** `std::vector<std::uint64_t> data(n);` — one contiguous block of `n` words.

| Call | Meaning |
|------|---------|
| `vector<T>(n)` | allocate + **value-init** (`0` for integers). That already *touches* every page. |
| `vector<T>(n, value)` | fill with `value` |
| `data()` | `T*` to the first element (needed if a helper wants a raw pointer) |
| `size()` | element count |
| `size() * sizeof(T)` | bytes (working-set size) |

**In general:** `std::vector` is a heap array with a destructor. Prefer it over `new[]` so you cannot leak.

`std::vector<T> data; data.reserve(n);` does **not** touch pages. `resize` / the sized constructor does.

---

### `<memory>`

**In this lab (later, only if we abandon vector):** `std::unique_ptr<uint64_t[]>` for a `new[]` that still frees itself.

**In general:** `unique_ptr`, `shared_ptr`, `make_unique`. You do not need shared ownership here.

---

### `<cstdlib>` — C allocation (aligned)

**In this lab (optional, stride / line experiments):**

| Call | Meaning |
|------|---------|
| `std::malloc(n)` | unaligned heap (usually 16-byte aligned on 64-bit) |
| `std::free(p)` | pair of malloc |
| `std::aligned_alloc(align, size)` | C11; `size` must be a multiple of `align`. `align` is a power of two. |
| `std::memset` (actually `<cstring>`) | fill bytes; also used to *touch* a malloc'd region |

`aligned_alloc(64, bytes)` / `aligned_alloc(128, bytes)` puts the first element on a cache-line boundary. Phase 1 said your line is **128 B**. Alignment matters when we start talking about *which* byte of the line you hit. Sequential `vector` is already fine for the first sweep.

---

### `<cstring>`

**In this lab:** `std::memset(p, 1, bytes)` as a “touch every byte” warmup if the buffer did not come from `vector(n)`.

**In general:** `memcpy`, `memmove`, `memcmp`. You will not copy arrays for the measurement itself — copying would measure *memcpy*, not your walk.

---

## Access patterns

### `<random>` — **random index stream**

**In this lab:** build an index array *before* the timed loop, then `sum += data[idx[i]]`.

| Piece | Meaning |
|-------|---------|
| `std::mt19937` | 32-bit Mersenne Twister. Fine for indices that fit in 32 bits. |
| `std::mt19937_64` | 64-bit. Use if `n` could exceed `2^32` (256 MB / 8 B = 32 M elements — fits in 32-bit). Either works here. |
| `std::random_device` | seed. On some platforms it is slow; seed **once**. |
| `std::uniform_int_distribution<size_t> dist(0, n - 1)` | inclusive range |
| `dist(rng)` | one random index |

**Do not** call `dist(rng)` *inside* the timed loop if you want to measure *memory*. You would also measure the RNG. Precompute `idx[0..n)`.

**In general:** `<random>` replaced `rand()`. `rand() % n` is biased and low quality. We still will not need distributions other than uniform.

---

### `<algorithm>`

**In this lab:**

| Call | Meaning |
|------|---------|
| `std::iota(idx.begin(), idx.end(), 0)` | fill `0, 1, 2, …` (needs `<numeric>` actually — see below) |
| `std::shuffle(idx.begin(), idx.end(), rng)` | permutation of `0..n-1`. Every element visited once, in random order. |
| `std::fill(data.begin(), data.end(), 1)` | write a known pattern so the compiler cannot assume zeros |

`shuffle` of `0..n-1` is the clean “random access, each element once” pattern. `dist(rng)` can repeat indices (with-replacement). We will use **shuffle** first (fairer comparison to sequential, same `N` touches). A later file can switch to with-replacement if we want.

---

### `<numeric>`

**In this lab:** `std::iota` — fill a range with consecutive values. That is how you build `idx` before `shuffle`.

`std::accumulate` would sum the array, but we write the loop by hand so the access pattern is visible (`data[i]`, `data[i * stride]`, `data[idx[i]]`).

---

## Output

### `<iostream>`

**In this lab:** `std::cout` for the result table. Same role as `report.cpp` in Phase 1.

Sink the checksum: `std::cout << "sink=" << sum << '\n';` so the compiler must compute `sum`. If `sum` is unused, `-O2` deletes the entire walk and you “measure” an empty loop.

---

### `<iomanip>`

**In this lab:** make the table readable.

| Manipulator | Meaning |
|-------------|---------|
| `std::setw(w)` | column width |
| `std::fixed` | not scientific notation |
| `std::setprecision(p)` | digits after the decimal (for ns/access, GB/s) |
| `std::left` / `std::right` | alignment |

---

### `<fstream>`

**In this lab (last file, optional):** `std::ofstream csv("results.csv")` so you can plot working-set vs latency. Not needed for the first sequential sweep.

---

### `<string>` / `<string_view>`

**In this lab:** pattern names (`"sequential"`, `"stride-16"`, `"random"`) and maybe formatting a size as `"4 KB"`. `std::to_string` lives in `<string>`.

---

## Headers you do **not** need yet (and why)

| Header | Typical use | Why skipped |
|--------|-------------|-------------|
| `<thread>` | `hardware_concurrency` | Phase 1 already did that. This lab is single-threaded on purpose. |
| `<atomic>` | `atomic_thread_fence` | one-thread measurement |
| `<sys/mman.h>` | `mmap`, `madvise`, `mlock` | later extras; vector + first-touch is enough to start |
| `<unistd.h>` | `sysconf(_SC_PAGESIZE)` | useful, not required for file 1 |
| `<sys/resource.h>` | `getrusage` (page faults) | diagnostic, after the plot works |
| `"cpu_info.hpp"` | Phase 1 probe | optional legend; do not link cpu_lab until we decide to |

---

# Document 2 — System calls and C functions (the OS side)

C++ libraries above are wrappers. These are the **kernel / libc** names you should recognize when something is slow or a page fault shows up.

---

## Time

### `clock_gettime(clockid_t id, struct timespec* tp) → 0 or -1`

**Role:** “what time is it on this clock?”

- `id = CLOCK_MONOTONIC` — use this if we drop below chrono
- writes `tp->tv_sec` and `tp->tv_nsec`
- returns `0` on success, `-1` + `errno` on failure

Elapsed ns:

```
(t1.tv_sec - t0.tv_sec) * 1'000'000'000 + (t1.tv_nsec - t0.tv_nsec)
```

`::clock_gettime` — the `::` means the C function in the global namespace (same habit as `::sysctlbyname` in Phase 1).

---

## Memory

### `malloc` / `free`

**Role:** anonymous heap. Alignment is implementation-defined (16 B on your Mac). Not cache-line aligned.

---

### `aligned_alloc(size_t alignment, size_t size) → void*`

**Role:** heap block whose address is a multiple of `alignment`.

Rules:

- `alignment` is a power of two
- `size` is a multiple of `alignment` (C11; violating this is undefined)
- free with `free`

Use when we want `data` to start at a 128-byte boundary.

---

### `posix_memalign(void** ptr, size_t alignment, size_t size) → int`

**Role:** POSIX version of the same idea. Returns `0` or an errno. Writes `*ptr`.

Exists on macOS and Linux. Slightly more awkward than `aligned_alloc`. Either is fine; we will pick **one** in the alloc file and stick to it.

---

### `mmap(void* addr, size_t len, int prot, int flags, int fd, off_t off)`

**Role:** ask the kernel for pages. `mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0)` is “give me anonymous memory.”

`std::vector` already does this under the hood (via malloc, which uses mmap above a size). You do **not** need raw `mmap` for the first sweep.

Later extras (skip until a file asks):

| Flag / call | Meaning |
|-------------|---------|
| `madvise(p, n, MADV_SEQUENTIAL)` | hint: I will walk forward (helps prefetch) |
| `madvise(p, n, MADV_RANDOM)` | hint: I will jump around |
| `mlock(p, n)` | pin pages in RAM (no swap). Needs privilege / ulimit. |
| `munlock` | undo |
| `sysconf(_SC_PAGESIZE)` | 4096 on your Mac; 16 KB on some Apple / ARM. A *page* is not a *cache line*. |

---

### Page faults (no extra header — this is kernel behavior)

The first **write** to a fresh page triggers a **soft page fault** (kernel maps a physical page). If you time the first walk of a 256 MB `malloc` without touching it, you measure faults + caches, not caches.

**Rule we will bake into the alloc file:** after allocate, **write every page once** (constructor of `vector(n)` already does this; `reserve` does not; `malloc` does not). Then, optionally, walk once more as a *cache warmup* (that one is **not** timed, or is timed separately). The measured loop is the third visit.

---

# Document 3 — Functions you will write (preview)

None of these exist yet. This is the map so the chronology makes sense. Each name will get its own file later.

| Function (planned) | Job | Talks to |
|--------------------|-----|----------|
| `now_ns()` | one monotonic timestamp in ns | `<chrono>` |
| `allocate(bytes)` | heap + first-touch | `<vector>` / `aligned_alloc` |
| `seq_sum(data, n)` | `sum += data[i]` | your loop |
| `stride_sum(data, n, stride)` | `sum += data[i * stride]` (with wrap or clipped `N`) | your loop |
| `random_sum(data, idx, n)` | `sum += data[idx[i]]` | precomputed `idx` |
| `make_shuffled_index(n)` | `iota` + `shuffle` | `<numeric>`, `<algorithm>`, `<random>` |
| `run_bench(...)` | warmup + timed repeats → raw ns + sink | timer + kernel |
| `to_metrics(bytes, accesses, ns)` | ns/access, GB/s | arithmetic |
| `print_table(...)` | working-set, pattern, time, ns/access, GB/s | `<iostream>` |

Public types we will introduce in a header (not now):

```
enum class Pattern { Sequential, Stride, Random };
struct BenchResult {
    std::size_t bytes;
    const char* pattern;
    double seconds;       // or ns
    double ns_per_access;
    double gb_per_s;
    std::uint64_t sink;   // checksum, printed so it stays live
};
```

---

# Document 4 — Metrics (not an API, but you must know the formulas)

Let

- `B` = working-set size in **bytes** (`N * sizeof(uint64_t)`)
- `A` = number of **element accesses** in the timed region  
  sequential: `A = N` (per pass) × passes  
  stride: fewer touches if you do `i += stride` without wrapping; **same `A`** if you wrap or visit `N` times  
- `T` = elapsed **nanoseconds**

Then

```
time (s)         = T / 1e9
ns/access        = T / A
GB/s             = (A * sizeof(uint64_t)) / (T / 1e9) / 1e9
                 = (A * 8 * 1e9) / (T * 1e9)
                 = (A * 8) / T          if T is in seconds
```

Careful with SI vs IEC:

| You print | Meaning |
|-----------|---------|
| `4 KB` in the brief | 4 × 1024 bytes (KiB). We will use **binary**: 4 KiB = 4096. |
| `GB/s` | 10^9 bytes per second (decimal, usual bandwidth unit). |

So: sizes are 4 KiB, 16 KiB, … 256 MiB. Bandwidth is decimal GB/s. Label the columns so the plot is honest.

**What ns/access is *not*:** it is not “L1 latency in cycles.” It is *average time per timed load*, including loop overhead, add, and whatever the prefetcher hid. Sequential will look far cheaper than random even in DRAM because of **spatial locality** + **hardware prefetch**. That is the lesson.

---

# Document 5 — Compiler / measurement hygiene

These are not libraries. If you skip them, the plot is fiction.

### 1. The optimizer deletes dead work

`sum += data[i]` with `sum` unused → entire loop gone at `-O2`.

**Fix:** return `sum` from the kernel and print it (`sink`). `volatile uint64_t sink = sum;` also works. We will print it.

### 2. `-O0` vs `-O2`

| Build | What you measure |
|-------|------------------|
| `-O0` | interpreter-ish: extra loads of `i`, no register reuse. Inflated ns/access. |
| `-O2` / Release | real code. Prefetch and vectorization may help sequential. |

CMake: `CMAKE_BUILD_TYPE=Release` for the “real” plot. Debug is fine while a file is still being born.

If sequential GB/s looks *too* close to memcpy, the compiler vectorized the loop. That is still a valid measurement of “sequential traffic.” We can add `volatile` loads or `-fno-tree-vectorize` later if we want a scalar walk.

### 3. Repeat until `T` is large

A 4 KiB walk is ~512 loads. That can be **sub-microsecond**. Timer granularity + context switches dominate.

**Rule:** for each size, repeat the walk until total timed work is at least ~50–200 ms, **or** a fixed large repeat count. Report **average** ns/access.

### 4. Warmup ≠ measure

| Pass | Purpose |
|------|---------|
| first-touch (alloc) | pay page faults |
| 1 untimed walk | pull the working set into whatever cache it fits |
| timed repeats | the number we plot |

If you skip warmup, small sizes show a one-time L1 fill tax.

### 5. One thread, quiet machine

This lab is single-threaded. Close browsers if the 256 MB point is noisy. Do not `std::thread` the walk.

### 6. Stride definition (agree now)

`data[i * 16]` in the brief is **element** stride 16: skip 15 `uint64_t`s, step **128 bytes**. On a 128 B line that is **one access per line** (no spatial reuse inside the line).

| stride (elements) | bytes between loads | on a 128 B line |
|-------------------|---------------------|-----------------|
| 1 | 8 B | 16 hits per line (spatial locality) |
| 2 | 16 B | 8 hits per line |
| 16 | 128 B | 1 hit per line |
| 32 | 256 B | 1 hit per *two* lines (worse) |

We will start with stride **16** as in the brief, and maybe add stride 1 vs 16 as a pair.

If `i * 16` runs `i = 0 .. N-1`, you go **past** the allocation (`data[(N-1)*16]`). Two legal designs — pick one in the stride file and keep it:

1. **Clip:** loop `for (i = 0; i < N; i += stride)` — fewer accesses (`N/stride`).
2. **Wrap:** `data[(i * stride) % N]` for `i in 0..N` — same access count as sequential.

**Prefer wrap or a visit-count argument** so ns/access is comparable across patterns. We will decide in that file; remember the trap.

### 7. Random vs pointer chase

`data[random_index]` is **index-driven** random. A harder classic is **pointer chasing** (`p = p->next`) which serializes latency. We do **index-driven** first (the brief). Pointer chase can be an extra file at the end.

---

# Document 6 — Mental model (so the APIs have a reason)

```
CPU  →  L1d (64 KiB)  →  L2 (6 MiB)  →  [no L3]  →  DRAM
         128 B line      128 B line
```

| Working set | Sequential (prefetcher helps) | Random (prefetcher mostly loses) |
|-------------|-------------------------------|----------------------------------|
| ≤ L1 (4–64 KiB) | ~L1 latency, high GB/s | still L1, but less ILP / more index traffic |
| L1 < set ≤ L2 (256 KiB–4 MiB) | L2-ish | L2 latency more visible |
| > L2 (16–256 MiB) | DRAM bandwidth (prefetch hides latency) | DRAM **latency** (ns/access jumps) |

You are learning, by experiment:

| Idea | What the experiment shows |
|------|---------------------------|
| cache | plot bends at ~64 KiB and ~6 MiB |
| cache line | stride 16 (128 B) vs stride 1 (8 B) |
| spatial locality | sequential >> stride >> random in GB/s |
| cache miss | random + large set → ns/access climbs |
| memory latency | that high plateau is DRAM |

Phase 1 numbers are the *expected* bend points. Phase 2 is the *measured* curve.

---

# How to use this file

1. Skim Document 1 tables. Do not memorize.
2. Remember four names: `steady_clock`, `vector`, `mt19937` + `shuffle`, `aligned_alloc`.
3. Remember three formulas: seconds, ns/access, GB/s.
4. Remember three hygiene rules: sink the sum, warmup, repeat until ~100 ms.
5. When a later file says “Next file: `src/timer.cpp`”, come back here and read **only** the `<chrono>` section.

You do not need `mmap` / `mlock` / `madvise` / `clock_gettime` to start. They are here so you are not surprised when a later hint mentions them.

---

# After this file

Chronology is the same style as `cpu_lab` / `cam_lab`: **one source file per step**, prove it, then the next name only. No code until you ask for the first file.
