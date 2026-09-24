The binary ran. That output is a real Apple M5 topology: 10 cores, no SMT (`physical == logical`), 128-byte lines, 64 KiB L1d, 6 MiB L2, no L3, one NUMA node.

`main.cpp` still does not print `hardware_concurrency()` (those lines are comments). The handbook below is the build side. Add the `hc` prints when you want the last part of Phase 1 on screen.

---

# cpu_lab — short compile / link handbook

All commands assume you are in `cpu_lab/`. Compiler is **AppleClang** (`c++` / `clang++`).

---

## 1. What actually happens

```
.cpp  --preprocessor-->  translation unit
      --compiler------>  assembly  (.s)
      --assembler----->  object    (.o)
      --linker-------->  executable (cpu_lab)
```

CMake hides this. The flags below let you stop at each stage.

| Stage | Flag | Output |
|--------|------|--------|
| Preprocess only | `-E` | expanded source on stdout |
| Compile to assembly | `-S` | `.s` |
| Compile to object | `-c` | `.o` |
| Link objects | (no `-c`) | executable |

Common flags you pass **with** those:

| Flag | Meaning |
|------|---------|
| `-std=c++17` | language version (matches `CMakeLists.txt`) |
| `-Iinclude` | find `"cpu_info.hpp"` |
| `-Wall -Wextra` | warnings |
| `-g` | debug symbols |
| `-O0` / `-O2` / `-O3` | no opt / normal / aggressive |
| `-DNAME=1` | define a macro (like `__APPLE__`, which the compiler already sets) |
| `-pthread` | threads (needed on many Linux hosts; usually implicit on macOS) |

---

## 2. CMake (what you already used)

```bash
cmake -S . -B build          # configure: read CMakeLists.txt, write build/
cmake --build build          # compile + link
./build/cpu_lab              # run
```

Useful extras:

```bash
cmake --build build --clean-first    # rebuild from scratch
cmake --build build -v               # show the real clang++ lines
cmake --build build -- -j 8          # parallel (Ninja/Make)
rm -rf build && cmake -S . -B build  # wipe cache if CMakeLists.txt changed a lot
```

Debug vs release:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug     # -g, -O0 typically
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # -O2/-O3, no debug
```

Objects CMake already made:

```
build/CMakeFiles/cpu_lab.dir/src/main.cpp.o
build/CMakeFiles/cpu_lab.dir/src/report.cpp.o
build/CMakeFiles/cpu_lab.dir/src/cpu_info_macos.cpp.o
build/CMakeFiles/cpu_lab.dir/src/cpu_info_linux.cpp.o   # empty on Mac (the #if strips it)
```

---

## 3. Raw `clang++` (same project, no CMake)

Include path is required. Only **one** of the two `probe_cpu` files must be linked on a given OS.

**Compile one file to an object** (stop before link):

```bash
c++ -std=c++17 -Iinclude -c src/main.cpp            -o main.o
c++ -std=c++17 -Iinclude -c src/report.cpp          -o report.o
c++ -std=c++17 -Iinclude -c src/cpu_info_macos.cpp  -o cpu_info_macos.o
```

**Link objects into the program:**

```bash
c++ main.o report.o cpu_info_macos.o -o cpu_lab
./cpu_lab
```

On Linux, swap in `cpu_info_linux.o` and add `-pthread`.

**One shot** (compile + link):

```bash
c++ -std=c++17 -Iinclude -Wall -Wextra \
    src/main.cpp src/report.cpp src/cpu_info_macos.cpp \
    -o cpu_lab
```

---

## 4. Assembly (`.s`)

Human-readable CPU instructions for **one** translation unit:

```bash
c++ -std=c++17 -Iinclude -S src/cpu_info_macos.cpp -o cpu_info_macos.s
c++ -std=c++17 -Iinclude -S src/report.cpp         -o report.s
```

Apple Silicon: add `-arch arm64` if you want to be explicit (that is already the default on your machine).

Intel-style vs default:

```bash
c++ -std=c++17 -Iinclude -S -masm=intel src/report.cpp -o report.s   # x86 only
```

Optimized assembly (often much shorter, names may vanish):

```bash
c++ -std=c++17 -Iinclude -O2 -S src/report.cpp -o report_O2.s
```

Keep intermediates next to the `.o`:

```bash
c++ -std=c++17 -Iinclude -c -save-temps src/report.cpp
# writes report.ii (preprocessed), report.s, report.o
```

---

## 5. Preprocess only (see the `#if` / `#include` result)

```bash
c++ -std=c++17 -Iinclude -E src/cpu_info_macos.cpp > macos.i
c++ -std=c++17 -Iinclude -E src/cpu_info_linux.cpp > linux.i
```

On your Mac, `linux.i` should be almost empty: `__linux__` is not set, so the whole file is compiled out. That is why the Linux typos did not break your build.

---

## 6. Inspect what you built

```bash
file ./build/cpu_lab              # Mach-O 64-bit executable arm64
nm ./build/cpu_lab | c++filt      # symbols: probe_cpu, print_cpu_report, main
otool -tv ./build/cpu_lab | head  # disassemble the binary (macOS)
c++filt _Z16print_cpu_reportRK7CpuInfo   # decode a mangled name
```

Object-only:

```bash
nm main.o
otool -tv cpu_info_macos.o | head
```

`nm`: `T` = defined in this file, `U` = undefined (must come from another `.o` or a library). `main.o` will show `U probe_cpu` and `U print_cpu_report` until you link.

---

## 7. Minimal “I just want X” recipes

```bash
# only .o files
c++ -std=c++17 -Iinclude -c src/report.cpp -o report.o

# only assembly
c++ -std=c++17 -Iinclude -S src/report.cpp -o report.s

# only preprocessor output
c++ -std=c++17 -Iinclude -E src/report.cpp

# debug build, then run in lldb
c++ -std=c++17 -Iinclude -g -O0 src/main.cpp src/report.cpp src/cpu_info_macos.cpp -o cpu_lab
lldb ./cpu_lab

# warnings as errors
c++ -std=c++17 -Iinclude -Wall -Wextra -Werror -c src/report.cpp
```

---

## 8. How this maps to your four sources

| File | Role at link time |
|------|-------------------|
| `main.cpp` | defines `main`; *uses* `probe_cpu`, `print_cpu_report` |
| `report.cpp` | defines `print_cpu_report` |
| `cpu_info_macos.cpp` | defines `probe_cpu` **on Apple** |
| `cpu_info_linux.cpp` | defines `probe_cpu` **on Linux**; object is nearly empty here |

The linker’s job is: take every `.o`, resolve the `U` symbols, write one Mach-O binary. Miss a `.o` that defines something you call → `undefined symbol`. Define `probe_cpu` in two live files → `duplicate symbol`.

---

CMake remains the default for day-to-day: `cmake --build build`. Use `-c` / `-S` / `-E` when you want to *see* one stage. Next code change, if you want Phase 1 finished on screen: uncomment and actually `std::cout` the `hc` comparison in `main.cpp`.