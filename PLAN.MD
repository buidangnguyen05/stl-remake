# STL Remake — Learning Plan

A plan for learning low-level C++ by re-implementing STL containers, allocators and
a malloc, then measuring everything. The goal is to prepare for C++ roles on Linux
and in performance work.

---

## How to use this plan

- Each task (`P2.7` and so on) should fit in **one sitting of about 1–3 hours**. If one runs longer, split it.
- A task is finished only when its **Done when** criterion holds. You check that criterion with a command or a test, not by feel.
- Commit after every task. Put the task ID in the commit message, for example `P2.7: push_back with geometric growth`.
- Keep a `NOTES.md` or a `docs/` folder of short write-ups. Each write-up should cover what you measured, what surprised you and what you would say in an interview. Several tasks below ask for one. They matter as much as the code.

### Assumptions (change them if they're wrong)
- You're comfortable with templates, references, RAII and basic move semantics. This plan goes deeper, but it doesn't start from zero.
- Everything lives in a namespace such as `stl::` inside a header-only library target (`INTERFACE` in CMake), except the malloc `.so`.
- Phase 4's deque is optional. Some other tasks are marked **(stretch)**. Skip them freely.
- The phases run in order. Phase 5 can start any time after Phase 3.

### Environment notes (from this machine)
| Fact                                                                               | Why it matters                                                                                                                                                                                                                      |
| ---------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| g++ 15.2.0, CMake 4.2.3, Ninja, gdb, valgrind and perf are installed               | Use the Ninja generator.                                                                                                                                                                                                            |
| `kernel.perf_event_paranoid = 4`                                                   | `perf` won't work as a normal user until you lower it. See P0.8.                                                                                                                                                                    |
| The project is on an NTFS partition mounted through **fuseblk**                    | NTFS-over-FUSE is slow and adds I/O noise. Put **build directories on ext4** (for example `~/builds/stl-remake/<preset>`). Git may also mark every file as mode-changed; set `git config core.filemode false`.                      |
| GTest is **not** installed as a package. `libbenchmark-dev` 1.9.1 **is** installed | Get GTest with a pinned `FetchContent`. For Benchmark, use either `find_package(benchmark)` or a pinned `FetchContent`. Pick one and write it down.                                                                                 |
| CPU is an i7-11800H (Tiger Lake): 8 cores / 16 threads, all cores the same type    | Per-core **L1d is 48 KiB** and **L2 is 1.25 MiB**. **L3 is 24 MiB** and shared. Cache lines are **64 B**. Size the Phase 5 working sets around these numbers. SMT siblings share L1/L2, so check `lscpu -e` before pinning threads. |

---

## Target repository layout

```
stl-remake/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/                  # warnings.cmake, sanitizers.cmake
├── include/stl/            # vector.hpp, list.hpp, unordered_map.hpp, ...
│   └── detail/             # shared helpers (uninitialized_*, hashing, ...)
├── tests/                  # GTest, one file per container + tooling
│   └── support/            # instrumented type, differential-test harness
├── bench/                  # Google Benchmark binaries
├── experiments/            # Phase 5 standalone programs
├── malloc/                 # Phase 7 shared library
├── docs/                   # write-ups, perf results
└── PLAN.MD
```

---

# Phase 0 — Project setup

**Goal:** one command builds and tests in a sanitized debug build, and one command builds optimized benchmarks. Warnings fail the build.

### Tasks

**P0.1 — Skeleton and git**
- Create the directory layout above and `git init`. Add a `.gitignore` for `build/` and `compile_commands.json`, and set `core.filemode false`.
- Add a trivial `include/stl/version.hpp` so there's something to compile.
- **Done when:** `git status` is clean after the first commit and shows no spurious mode changes.

**P0.2 — Minimal CMake project**
- Use `cmake_minimum_required(VERSION 3.28)`, `project(stl_remake CXX)`, `CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_STANDARD_REQUIRED ON` and `CMAKE_CXX_EXTENSIONS OFF`.
- Add an `INTERFACE` library `stl` with `target_include_directories(stl INTERFACE include)`.
- Set `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd or IDE support.
- **Done when:** `cmake -S . -B ~/builds/stl-remake/tmp -G Ninja && cmake --build ...` succeeds.

**P0.3 — Warnings as errors, on your own targets only**
- Write `cmake/warnings.cmake` with a function `stl_set_warnings(target)` that applies:
  `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wcast-align -Wuseless-cast`
- ⚠️ Use `target_compile_options`. **Never** put `-Werror` in `CMAKE_CXX_FLAGS`, because third-party code fetched with FetchContent (GTest) will fail to build under GCC 15's warnings.
- `-Wuseless-cast` and `-Wnull-dereference` sometimes give false positives in templated code or at `-O3`. If one of them fires on correct code, drop that flag and record why, rather than contorting the code.
- **Done when:** a deliberate `int x = 3.5;` in a test file breaks the build, and removing it fixes the build.

**P0.4 — Sanitizer debug build**
- Write `cmake/sanitizers.cmake` with an option `STL_SANITIZE` whose value is `address,undefined` or `thread`.
- Flags: `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g -O1`. Use them both when compiling **and** when linking.
- Also define `_GLIBCXX_ASSERTIONS` in debug, so that bugs where your code misuses `std::` fail loudly.
- **Done when:** a scratch test with a heap-buffer-overflow fails with an ASan report, a scratch test with signed overflow fails with a UBSan report (it must fail, not just print), and you've deleted both scratch tests.

**P0.5 — CMakePresets.json**
- Configure presets (all using `binaryDir: $env{HOME}/builds/stl-remake/${presetName}`):
  - `debug`: plain Debug with no sanitizers. Needed for valgrind (P2.11) and for binaries run under your `LD_PRELOAD` malloc (P7.6).
  - `debug-asan`: Debug with `STL_SANITIZE=address,undefined`
  - `debug-tsan`: Debug with `STL_SANITIZE=thread`. It's used in Phase 8. TSan can't be combined with ASan.
  - `release`: `-O3 -DNDEBUG`, plus `-g -fno-omit-frame-pointer` so perf gets symbols and stack walks
  - (optional) `relwithdebinfo-native`: adds `-march=native`, for comparing experiments
- Add build and test presets for each one.
- **Done when:** `cmake --preset debug-asan && cmake --build --preset debug-asan` and the same for `release` both work.

**P0.6 — GoogleTest**
- Pull it in with `FetchContent_Declare(googletest URL/GIT_TAG <pinned release>)`, `FetchContent_MakeAvailable`, `enable_testing()`, `include(GoogleTest)` and `gtest_discover_tests`.
- Pass `SYSTEM` to `FetchContent_Declare` (CMake ≥ 3.25), so GTest's headers are treated as system headers. Otherwise macros like `EXPECT_EQ(v.size(), 3)` trigger `-Wsign-conversion` and other warnings *inside gtest headers* in your own test files, and `-Werror` fails them.
- Set `INSTALL_GTEST OFF`. (`gtest_force_shared_crt` is a Windows-only setting, so you can ignore it.)
- Add `tests/smoke_test.cpp` with one `TEST`.
- **Done when:** `ctest --preset debug-asan --output-on-failure` runs and passes one test.

**P0.7 — Google Benchmark (release only)**
- Either `find_package(benchmark REQUIRED)` (system 1.9.1) or pin it with FetchContent, setting `BENCHMARK_ENABLE_TESTING OFF`.
- Build `bench/` **only** when the build type is not Debug, or behind `option(STL_BUILD_BENCH)`, so it's never built with sanitizers.
- Write `bench/smoke_bench.cpp` that benchmarks `std::vector<int>::push_back`.
- **Done when:** `~/builds/stl-remake/release/bench/smoke_bench` prints a results table.

**P0.8 — perf and benchmark machine hygiene**
- Run `sudo sysctl kernel.perf_event_paranoid=1` and `sudo sysctl kernel.kptr_restrict=0`. To make these persistent, put them in a file under `/etc/sysctl.d/` (only if you're comfortable with that on this machine).
- Write `scripts/bench_env.sh` that:
  - sets the governor to performance (`cpupower frequency-set -g performance`, or write to sysfs)
  - optionally turns turbo off (`/sys/devices/system/cpu/intel_pstate/no_turbo`)
  - prints the current state
- Also write a matching `scripts/bench_env_restore.sh`.
- **Done when:** `perf stat ls` works **without sudo**, and a benchmark run no longer prints the "CPU scaling is enabled" warning.

**P0.9 — Convenience and CI-like script (optional)**
- `scripts/check.sh` builds and tests `debug-asan`, then builds `release`.
- (stretch) Add a GitHub Actions workflow that runs the same thing.
- **Done when:** one command runs everything green.

### Key concepts
- CMake targets vs global flags, `PUBLIC`/`PRIVATE`/`INTERFACE`, generator expressions, presets
- What ASan (shadow memory, redzones, quarantine) and UBSan instrument, and what they can't catch
- Why ASan and TSan can't be combined, and why sanitizers change timing and so must never be used in benchmarks
- `-O1` vs `-O2` vs `-O3`, `-fno-omit-frame-pointer`, what `-march=native` changes
- `perf_event_paranoid` and what the kernel is protecting

### Tests to write
- The smoke test and the smoke benchmark, plus the "deliberately broken" checks in P0.3 and P0.4. Delete those checks afterwards, but record in NOTES what each one printed.

### Resources
- *Professional CMake: A Practical Guide*, Craig Scott (the authoritative book, updated with each release)
- The official CMake docs: `cmake-presets(7)`, `FetchContent`, `GoogleTest` module
- The GoogleTest Primer and the "Advanced GoogleTest Topics" doc
- Google Benchmark `docs/user_guide.md`, especially the sections on `DoNotOptimize`, `ClobberMemory`, and "Reducing Variance"
- The GCC manual: "Options to Request or Suppress Warnings", "Program Instrumentation Options"
- The AddressSanitizer and UndefinedBehaviorSanitizer pages on the google/sanitizers GitHub wiki and in the Clang docs (the concepts carry over to GCC)

---

# Phase 1 — Test tooling

**Goal:** a type that lets you *prove* how many objects your container created, copied, moved and destroyed, and lets you inject exceptions at exact points.

### Tasks

**P1.1 — `Tracked` type, counting only**
- `tests/support/tracked.hpp`: `struct Tracked { int value; ... }` with static (or thread-local) counters for:
  - default constructions, value constructions, copy constructions, move constructions
  - copy assignments, move assignments, destructions
- A `live()` count equal to constructions minus destructions.
- A `Counters::reset()` and a `snapshot()` that returns a plain struct, so tests can compare "before" and "after".
- **Done when:** a unit test on `Tracked` itself shows that `Tracked a; Tracked b = a; Tracked c = std::move(a);` gives exactly 1 default, 1 copy, 1 move and, after scope exit, 3 destructions.

**P1.2 — Detecting double destroy and use-after-destroy**
- Give `Tracked` a magic field that the constructor sets to `0xA11CE` and the destructor sets to `0xDEAD`. Every member function and the destructor `assert` or `ADD_FAILURE` when the magic value is wrong.
- ⚠️ GCC's `-flifetime-dse` (on by default from `-O1`) may remove the `0xDEAD` store in the destructor as a dead store, because the object's lifetime ends right there. Write the magic value through a `volatile` access, or compile the tests with `-fno-lifetime-dse`.
- A moved-from object should hold a well-defined `value` (for example `-1`), so tests can check moved-from state.
- **Done when:** a test that manually calls the destructor twice (on raw storage) is reported. This is a scratch test; mark it `DISABLED_` afterwards.

**P1.3 — Throw on the Nth copy / Nth move / Nth construction**
- Add static "throw countdowns": `throw_on_copy_after(n)`, `throw_on_move_after(n)`, `throw_on_construct_after(n)`. When the countdown hits zero, the operation throws a `TrackedError` **before** changing any state.
- **Done when:** a test sets `throw_on_copy_after(3)`, copies 3 times successfully, and the 4th copy throws. Afterwards `live()` is still consistent, with no leak.

**P1.4 — noexcept-move vs throwing-move variants**
- Make the type a template or add a policy: `Tracked<MoveNoexcept::yes>` and `Tracked<MoveNoexcept::no>`.
- The `no` variant has `noexcept(false)` move operations. This is needed later to test `std::move_if_noexcept` and the strong guarantee during reallocation.
- Also add a **move-only** type, `TrackedMoveOnly`, and a **non-default-constructible** type.
- **Done when:** `static_assert(std::is_nothrow_move_constructible_v<Tracked<yes>>)` and `static_assert(!std::is_nothrow_move_constructible_v<Tracked<no>>)` both hold.

**P1.5 — GTest fixture with leak and balance checks**
- `class TrackedTest : public ::testing::Test` resets the counters in `SetUp()`, and in `TearDown()` asserts `live() == 0` and that no magic-value errors happened.
- **Done when:** a test that leaks a `Tracked` (for example by using `new` without `delete`) fails in `TearDown` with a clear message. That test is a scratch test; disable it afterwards.

**P1.6 — Differential testing harness**
- `tests/support/differential.hpp` runs a seeded random sequence of operations, such as push, pop, insert-at-random-index, erase, clear, reserve, resize and copy/move-assign, **on both `stl::X` and `std::X`**. After each operation it compares size and contents.
- Print the seed on failure so you can reproduce.
- **Done when:** running it against `std::vector` vs `std::vector` passes 10k operations × 100 seeds, and a deliberately wrong adapter makes it fail and print a seed.

**P1.7 — Allocation-counting helper (required; P2.7 depends on it)**
- A test-only way to count calls to `::operator new` and `::operator delete`: replace the global operators (all the sized and aligned overloads) in a **separate test binary**, so they don't interfere with other tests. Phase 6's counting allocator replaces this later.
- **Done when:** a test can assert "`reserve(100)` then 100 × `push_back` performed exactly one allocation".

### Key concepts
- Special member functions and when the compiler generates them (the Hinnant table)
- Static vs `thread_local` counters (think ahead to Phase 8)
- Exception-safety testing by fault injection: "throw at each possible point, check invariants"
- Why tests must check *counts*, not just *values*. Two implementations can produce the same contents with very different amounts of work.

### Tests to write
- Self-tests of `Tracked` (P1.1–P1.5)
- A differential-harness self-test (P1.6)

### Resources
- Howard Hinnant's "Everything You Ever Wanted To Know About Move Semantics" (ACCU talk slides) and his special-members table
- *Exceptional C++*, Herb Sutter — the exception-safety items (the `Stack` example)
- David Abrahams, "Exception-Safety in Generic Components" (the original basic/strong/nothrow guarantee paper)
- GoogleTest docs: test fixtures, `SCOPED_TRACE`, and value-parameterized/typed tests

---

# Phase 2 — `stl::vector<T>`

**Goal:** a `std::vector` workalike (without allocator support for now) that is correct under exceptions, allocates the minimum amount and passes the differential tests.

> **Design rule, stated now, for Phase 6:** every raw-memory operation goes through a small set of private helpers: `allocate_(n)`, `deallocate_(p, n)`, `construct_at_(p, args...)`, `destroy_(p)`, `destroy_range_(first, last)`. Nothing else in the class calls `::operator new` or placement new directly. Then adding allocators in Phase 6 is a refactor, not a rewrite.

### Tasks

**P2.1 — Layout and raw memory**
- Three pointers: `begin_`, `end_` and `cap_`. This is the libstdc++ layout; compare it with pointer + size + capacity.
- `allocate_(n)` is `static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{alignof(T)}))` when `alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__`, otherwise the plain form. Check `n` against `max_size()` first and throw `std::length_error`.
- `deallocate_` must use the matching `::operator delete` overload (sized and aligned).
- **Done when:** a default-constructed vector has `size() == capacity() == 0` and allocates nothing, and a vector of an `alignas(64)` type has properly aligned `data()`. Test both.

**P2.2 — Constructors and destructor**
- `vector()`, `explicit vector(size_type n)`, `vector(size_type n, const T&)`, `vector(std::initializer_list<T>)` and the destructor.
- Use your own `detail::uninitialized_fill_n` and `uninitialized_value_construct_n` that **destroy what they already built if a constructor throws** (the "construct and roll back" loop). Then compare them with `std::uninitialized_*`.
- **Done when:** with `throw_on_construct_after(5)`, `vector<Tracked>(10)` throws, `live() == 0`, and ASan reports no leak.

**P2.3 — Rule of five**
- Copy constructor, move constructor (`noexcept`, steals pointers), copy assignment, move assignment (`noexcept`) and a member `swap` (`noexcept`) plus a non-member `swap`.
- Implement copy assignment in two ways and compare them:
  - (a) copy-and-swap: strong guarantee, always allocates
  - (b) reuse existing capacity when `other.size() <= capacity()`: basic guarantee, faster. Pick (b) as the final version, like the standard library, and write down why.
- Handle self-assignment.
- **Done when:** the tests check exact copy and move counts for each operation, self-assignment is safe, and moving a vector performs **zero** element moves.

**P2.4 — Element access and capacity queries**
- `operator[]`, `at` (throws `std::out_of_range`), `front`, `back`, `data`, `size`, `capacity`, `empty` and `max_size`.
- Use `[[nodiscard]]` where it helps, and `constexpr` where it's possible. A `constexpr` vector is a (stretch) goal — transient constexpr allocation has been legal since C++20. You can't do it with `::operator new` or placement new, though, because constant evaluation allows neither. It requires `std::allocator<T>::allocate`, `std::construct_at`, and `if consteval` branches. Leave it until after P6.1.
- Deducing `this` (C++23) is optional: try it for `operator[]` to remove the const/non-const duplication, and note whether it's worth it.
- **Done when:** the tests cover const and non-const access, `at` throws on out-of-range, and `-Wall -Werror` is clean.

**P2.5 — Iterators**
- First version: `T*` as the iterator. Then (the real exercise) write a class `stl::detail::vector_iterator<T, IsConst>` that satisfies `std::contiguous_iterator`.
- Include `iterator_concept`, `operator<=>`, a conversion from non-const to const, and `std::to_address` support (`element_type` or `pointer_traits`).
- Add `begin/end/cbegin/cend/rbegin/rend` using `std::reverse_iterator`.
- **Done when:**
  - `static_assert(std::contiguous_iterator<stl::vector<int>::iterator>)` passes
  - `static_assert(std::ranges::contiguous_range<stl::vector<int>>)` passes
  - `std::sort`, `std::ranges::sort` and a range-for loop work on your vector
  - converting const→non-const does **not** compile. Check this with a `requires`-expression `static_assert`.

**P2.6 — `reserve`, `shrink_to_fit` and relocation**
- Write `relocate_(new_buf)`, which moves elements with `std::move_if_noexcept` into the new buffer. If a move or copy throws, destroy the partially built new buffer, free it and rethrow; the old buffer stays untouched. This is the **strong guarantee**.
- (stretch) Fast path: when `T` is trivially copyable, use `std::memcpy`. Look up "trivially relocatable" (P1144/P2786) and write a short note.
- **Done when:**
  - `vector<Tracked<yes>>::reserve` performs only moves, zero copies
  - `vector<Tracked<no>>::reserve` performs only copies, zero moves (this is the whole point of `move_if_noexcept`)
  - with `throw_on_copy_after(k)` during reallocation, the vector's contents and capacity are unchanged afterwards, for every `k`

**P2.7 — `push_back`, `emplace_back`, `pop_back`**
- Use geometric growth. Try growth factors of 2 and 1.5, and write down the "can you reuse freed memory" argument for 1.5.
- **Alias trap:** `v.push_back(v[0])` and `v.emplace_back(v[0])` while reallocating. Construct the new element in the new buffer **before** relocating the old elements.
- `emplace_back` returns a `reference` (C++17).
- **Done when:**
  - 1M `push_back`s take O(log n) allocations (assert the count with P1.7)
  - the aliasing test passes under ASan
  - `emplace_back(args...)` constructs in place: exactly one value construction, zero moves, when capacity is available
  - a throw during `emplace_back` on the realloc path leaves the vector unchanged (strong guarantee)

**P2.8a — Single-element `insert` / `emplace` at a position**
- Handle both the "fits in capacity" path (shift the tail with move-construct into uninitialized space plus move-assign) and the "reallocate" path.
- Handle the aliasing case `v.insert(v.begin(), v.back())`.
- **Done when:** the differential tests with random single-element insert positions pass across 100 seeds, and the aliasing test passes under ASan.

**P2.8b — Multi-element `insert`: `n` copies, iterator range, `initializer_list`**
- Range insert: for `std::forward_iterator`s compute the distance once; for input iterators insert one at a time.
- **Done when:** the differential tests cover all overloads (including an input-iterator range from `std::istream_iterator`), and there are basic-guarantee tests with throw injection. Contents may change, but the vector is valid and `live()` stays balanced.

**P2.9 — `erase`, `clear`, `resize`, `assign`**
- `erase(pos)` and `erase(first, last)` move-assign the tail down and destroy the leftover suffix.
- `resize(n)` and `resize(n, value)`, `assign(n, v)`, `assign(first, last)`, and the `initializer_list` overloads.
- **Done when:** `erase` of the middle third of 1000 `Tracked` elements does exactly the expected number of move-assignments and destructions (compute and assert them), and the differential tests pass.

**P2.10 — Comparison, `noexcept` audit, and conditional `noexcept`**
- `operator==` and `operator<=>` using `std::lexicographical_compare_three_way` (synthesize three-way for `T` without `<=>`).
- Go through every member: which are `noexcept` unconditionally, which conditionally (`swap`?), and which can throw. Add `static_assert`s in the tests that encode those answers.
- **Done when:** a test file of `static_assert(noexcept(...))` / `static_assert(!noexcept(...))` compiles, and `std::vector<stl::vector<Tracked<yes>>>` relocates its inner vectors by **move** (proof that your move constructor is `noexcept`).

**P2.11 — Hardening pass**
- Run the full test suite with the `debug-asan` preset. Also run it once under `valgrind --tool=memcheck` with the plain `debug` preset.
- (optional) `-D_GLIBCXX_DEBUG` changes the ABI and layout of `std::` containers, so *everything*, GTest included, must be built with it. If you want it, give it its own preset. `_GLIBCXX_ASSERTIONS` (already on in debug) is ABI-safe.
- Add a debug-only precondition check (`assert` or a `STL_ASSERT` macro) in `operator[]`, `front`, `back` and `pop_back`.
- **Done when:** ASan, UBSan and valgrind are all clean, and the write-up `docs/vector.md` lists every exception-safety guarantee per member with its test.

### Key concepts
- Object lifetime: storage vs objects, placement new, `std::construct_at` and `std::destroy_at`, `std::launder` (when it's needed, and why vector usually doesn't need it)
- The basic, strong and nothrow guarantees; `std::move_if_noexcept`; why `noexcept` on move constructors changes the *algorithm* (not just the signature)
- Amortized O(1) growth, and growth factor trade-offs
- Iterator categories and C++20 iterator concepts; `contiguous_iterator` and `std::to_address`
- Aliasing inside modifiers (`push_back(v[0])`)
- Alignment: `alignof`, `std::align_val_t`, over-aligned types
- Iterator invalidation rules for each vector operation. Memorize the table.

### Tests to write
- Per-member tests with exact `Tracked` counts
- Exception injection loops: `for k in 0..N: throw_on_copy_after(k); try op; check invariants`
- Differential random tests against `std::vector` (including `Tracked`, move-only and `std::string` element types)
- `static_assert` concept and `noexcept` tests
- Aliasing tests
- Over-aligned element tests

### Resources
- cppreference: `std::vector` (the invalidation notes and complexity section), `std::uninitialized_*`, `std::construct_at`, the iterator concepts library
- The C++ standard draft (eel.is/c++draft): [vector], [container.requirements], [iterator.requirements]
- *Effective Modern C++*, Scott Meyers — items on move semantics and `noexcept`
- *C++ Move Semantics — The Complete Guide*, Nicolai Josuttis
- *Exceptional C++*, Herb Sutter; the Abrahams exception-safety paper (see Phase 1)
- Read the libstdc++ source (`bits/stl_vector.h`, `bits/vector.tcc` in `/usr/include/c++/15/`) *after* you finish each task, not before
- Stephan T. Lavavej's STL lecture series (Channel 9 / YouTube)
- Proposals P1144 (trivially relocatable) and P2786, for the relocation stretch note

---

# Phase 3 — Benchmarking and profiling your vector

**Goal:** reliable, reproducible numbers comparing `stl::vector` with `std::vector`, and the ability to explain *why* they differ using perf.

### Tasks

**P3.1 — Benchmark hygiene checklist**
- Write `docs/benchmarking.md` covering:
  - the governor/turbo script from P0.8
  - pinning with `taskset -c 2` to one physical core
  - closing noisy apps
  - `--benchmark_repetitions=10 --benchmark_report_aggregates_only=true`
  - looking at the stddev and CV columns
  - `DoNotOptimize` / `ClobberMemory` semantics
- **Done when:** running the same benchmark 3 times gives a mean that varies by less than 2–3%. Record the observed CV.

**P3.2 — Benchmark suite, templated over the container**
- `bench/vector_bench.cpp` uses `BENCHMARK_TEMPLATE(BM_x, std::vector<T>)` and `(..., stl::vector<T>)` for:
  - `push_back` with and without `reserve`
  - `emplace_back` of a `std::string`
  - iteration/sum
  - `insert` at the front
  - `erase` in the middle
  - copy-construction
  - `resize`
- Use `->Range(8, 1<<20)` / `->RangeMultiplier(4)`, and element types `int`, `std::string` and a 64-byte struct.
- **Done when:** the suite runs in under 2 minutes and outputs JSON (`--benchmark_out=... --benchmark_out_format=json`).

**P3.3 — Compare runs**
- Use `tools/compare.py` from the Google Benchmark repo, or your own small script, to diff `std` vs `stl` and to compare your own versions over time (before/after an optimization).
- **Done when:** you have a table in `docs/vector_perf.md` with every benchmark as a ratio against `std`.

**P3.4 — perf stat basics**
- Run `perf stat -e cycles,instructions,cache-references,cache-misses,branches,branch-misses,L1-dcache-load-misses` on a single benchmark (filter with `--benchmark_filter`).
- Learn to read IPC.
- **Done when:** you can explain, in your notes, the IPC and miss rates of `push_back` with vs without `reserve`.

**P3.5 — perf record / report / annotate**
- `perf record -g --call-graph=fp` (frame pointers are enabled in the release preset), then `perf report`, then `perf annotate` on your hottest function.
- Also try `--call-graph=dwarf` and note the difference.
- **Done when:** you can point at the exact instructions in `perf annotate` where your slowest benchmark spends its time.

**P3.6 — Flame graphs**
- Use Brendan Gregg's FlameGraph scripts (`stackcollapse-perf.pl`, `flamegraph.pl`), or `perf script` into a viewer such as speedscope.
- **Done when:** there is an SVG flame graph in `docs/` for one benchmark.

**P3.7 — Read the generated assembly**
- Look at your `push_back` and `std::vector::push_back` with `objdump -d --no-show-raw-insn -C` or Compiler Explorer (godbolt.org).
- Check: is the slow path (reallocation) outlined? Does `[[unlikely]]` or a separate `noinline` realloc function help? Is the trivial-copy path using `memmove`?
- **Done when:** you apply at least one change driven by the assembly (for example outlining `realloc_insert_`) and measure it with P3.3.

**P3.8 — Close the gap (or explain it)**
- For each benchmark where you're more than 10% slower than `std`, find the cause with perf and the assembly, and fix it or explain it.
- **Done when:** `docs/vector_perf.md` has a one-paragraph explanation for every remaining gap.

### Key concepts
- Benchmark pitfalls: dead-code elimination, constant folding, warm-up, frequency scaling, turbo, noisy neighbours, ASLR and code alignment effects
- Statistical thinking: variance, repetitions, medians vs means
- Hardware counters: cycles, instructions, IPC, cache and branch misses; PMU multiplexing (the `(xx.xx%)` column in `perf stat`)
- Sampling profilers vs counting; call-graph modes (fp, dwarf, lbr)
- Inlining, hot/cold splitting, `[[likely]]`/`[[unlikely]]`, `noinline`

### Tests to write
- Benchmarks are not tests. Still, keep the Phase 2 tests green after every optimization. Run `ctest --preset debug-asan` before each perf commit.

### Resources
- Google Benchmark user guide
- Brendan Gregg: *Systems Performance* (2nd ed.), his "perf Examples" page, and the "Flame Graphs" page
- `man perf-stat`, `man perf-record`, `man perf-report`, `man perf-annotate`
- Chandler Carruth, "Tuning C++: Benchmarks, and CPUs, and Compilers! Oh My!" (CppCon 2015)
- Denis Bakhvalov, *Performance Analysis and Tuning on Modern CPUs* (free book), and his easyperf.net blog
- Emery Berger, "Performance Matters" (Strange Loop 2019), on measurement bias
- Compiler Explorer, godbolt.org

---

# Phase 4 — `list`, `unordered_map` (chaining, then open addressing), optional `deque`

### 4A — `stl::list<T>` (doubly linked, sentinel node)

**P4.1 — Node layout and sentinel**
- Split the node into a `node_base { node_base* prev; node_base* next; }` and a `node : node_base { T value; }`.
- Use a **sentinel** `node_base` stored in the list object (a circular list), so `end()` is O(1) and there are no null checks.
- **Done when:** an empty list has `begin() == end()` and performs no allocations.

**P4.2 — Iterators**
- A bidirectional iterator plus a const iterator; `static_assert(std::bidirectional_iterator<...>)`.
- **Done when:** the concept checks pass, and `std::ranges::reverse_view` works on your list.

**P4.3 — push/emplace/pop at the front and back, insert/emplace/erase at an iterator**
- **Done when:** the differential tests against `std::list` pass, and `Tracked` counts show exactly one construction per emplace.

**P4.4 — Rule of five, including move that leaves the source valid**
- Watch out: the sentinel lives *inside* the object, so moving must re-point the neighbours of the first and last nodes.
- **Done when:** a moved-from list is empty and usable, and ASan is clean.

**P4.5a — `splice` (all three overloads), `reverse`, `remove_if`, `unique`**
- **Done when:** the tests check that `splice` performs **zero** allocations and zero element copies or moves, and that iterators into the spliced elements remain valid (now pointing into the destination list).

**P4.5b — `merge` and `sort`**
- Implement `sort` as a merge sort on nodes, with no allocation. Try both bottom-up and top-down versions, and check that the sort is stable.
- **Done when:** `sort` on 1M elements performs zero allocations and zero element moves, a stability test passes (sorting pairs by key only), and the result matches `std::list::sort`.

### 4B — `stl::unordered_map<K, V, Hash, Eq>`, version 1: separate chaining

**P4.6 — Buckets and nodes**
- Try the libstdc++ design: a singly linked list of all nodes, with buckets pointing to the node *before* the first node of that bucket. This gives O(1) `begin()` and forward iteration across buckets.
- Write down how that compares with the naïve "vector of lists" design.
- Nodes store `std::pair<const K, V>`, and (optionally) the cached hash.
- **Done when:** a design sketch is in `docs/unordered_map.md`, and the empty map compiles.

**P4.7 — `find`, `insert`, `emplace`, `try_emplace`, `operator[]`, `at`**
- **Done when:** `try_emplace` does **not** construct `V` if the key exists (verify with `Tracked` counts), and the differential tests against `std::unordered_map` pass.

**P4.8 — Rehash and load factor**
- `max_load_factor`, `rehash(n)` and `reserve(n)`. Use prime or power-of-two bucket counts and write down the trade-off: modulo cost vs a bad hash under a mask.
- **Done when:**
  - 1M inserts keep `load_factor() <= max_load_factor()`
  - `reserve(n)` before n inserts causes no rehash
  - references to elements stay valid across a rehash (a standard guarantee for node-based maps)

**P4.9 — `erase` (by key and by iterator), iterators, `swap`, rule of five**
- **Done when:** the differential tests pass with inserts and erases interleaved over 100 seeds, and ASan is clean.

**P4.10 — Heterogeneous lookup (stretch)**
- Support `is_transparent` hash and equality, so that `find(std::string_view)` works on `unordered_map<std::string, V>` without constructing a string.
- **Done when:** a test proves no `std::string` is constructed during lookup, using a counting allocator or a `Tracked`-like key.

### 4C — `stl::flat_hash_map`: open addressing (a separate class)

**P4.11 — Linear probing with tombstones**
- Use a contiguous array of slots, a power-of-two capacity and a mask.
- Mark erased slots as tombstones. Try backward-shift deletion (no tombstones) as a variant.
- **Done when:** the differential tests pass, including heavy erase/insert churn over 100 seeds.

**P4.12 — Control bytes, SwissTable-style (stretch, but very worthwhile)**
- Keep a separate `int8_t` control array (empty/deleted/full + 7 hash bits) and probe groups of 16 using SSE2 (`_mm_movemask_epi8`).
- **Done when:** the tests pass, and a benchmark shows your version beats your own linear probing at high load factors.

**P4.13 — Benchmark all three maps**
- Compare `std::unordered_map`, your chaining map and your open-addressing map on:
  - insert
  - successful lookup
  - unsuccessful lookup
  - erase
  - iteration
- Use sizes that fit in L1, L2, L3 and DRAM, for example 1K, 16K, 256K and 4M keys of `uint64_t`.
- **Done when:** there's a results table plus `perf stat` cache-miss numbers explaining the gaps, in `docs/hashmap_perf.md`.

### 4D — `stl::deque<T>` (optional)

**P4.14 — Map of fixed-size blocks**
- A block-pointer array (the "map") plus start and finish iterators, with O(1) amortized push and pop at both ends.
- **Done when:** the differential tests pass, and references stay valid on `push_front`/`push_back` (a standard guarantee; iterators may not).

**P4.15 — Random-access iterator**
- **Done when:** `static_assert(std::random_access_iterator<...>)` passes, and `std::sort` works on your deque.

### Key concepts
- Node-based vs contiguous containers: allocation count, pointer chasing and invalidation guarantees
- Sentinel nodes and intrusive-style base nodes
- Hash-function quality, avalanche, bucket-count choice (prime vs power of two), load factor
- Chaining vs open addressing; linear, quadratic and Robin Hood probing; tombstones vs backward shift; SIMD group probing
- Why `std::unordered_map`'s interface (reference stability, bucket API) forces chaining
- Transparent comparators and hashers (`is_transparent`)

### Tests to write
- Differential random tests against `std::list`, `std::unordered_map` and `std::deque`
- Allocation-count tests (`splice`/`sort` should do zero)
- Reference/iterator stability tests after rehash, `splice` and deque pushes
- Adversarial hash tests: a hash that returns a constant (everything collides). It should stay correct, just slow.
- `Tracked` count tests for `try_emplace`, `emplace` and `operator[]`

### Resources
- cppreference: `std::list`, `std::unordered_map`, `std::deque` (especially the invalidation sections)
- libstdc++ source: `bits/stl_list.h`, `bits/hashtable.h`, `bits/hashtable_policy.h`, `bits/stl_deque.h`
- Matt Kulukundis, "Designing a Fast, Efficient, Cache-friendly Hash Table, Step by Step" (CppCon 2017)
- The Abseil docs: "Swiss Tables Design Notes"
- Malte Skarupke's blog posts on hash tables ("I Wrote The Fastest Hashtable" and follow-ups)
- *Introduction to Algorithms* (CLRS), the hashing chapter
- Martin Ankerl's hashmap benchmark write-ups

---

# Phase 5 — Memory-hierarchy experiments

**Goal:** see cache effects with your own eyes and measure them with `perf stat`. Each experiment is a small standalone program in `experiments/`, built in the `release` preset, pinned with `taskset`, with a short write-up.

**Machine reference:** L1d is 48 KiB per core, L2 is 1.25 MiB per core, L3 is 24 MiB shared, lines are 64 B. Sweep working-set sizes across these boundaries: 16 KiB, 32 KiB, 64 KiB, 512 KiB, 1 MiB, 2 MiB, 16 MiB, 64 MiB and 256 MiB.

### Tasks

**P5.1 — Latency ladder (pointer chasing)**
- Build a random cyclic permutation in an array of size N, then chase `i = a[i]` for many iterations and measure ns per load across the sizes above.
- **Done when:** you have a plot or table that shows clear steps at about 48 KiB, 1.25 MiB and 24 MiB, and your write-up gives the latency for each level.

**P5.2 — Bandwidth vs stride**
- Do a sequential sum at strides 1, 2, 4, 8, 16 and 32 `int`s over a large array.
- **Done when:** you can show and explain why time stays about flat up to a stride of 16 ints (64 B) and then changes. That's lines, not elements.

**P5.3 — vector vs list traversal**
- Sum N ints in `stl::vector` vs `stl::list`, in three cases:
  - (a) list nodes allocated back to back, so they're accidentally contiguous
  - (b) list nodes shuffled, by building the list and then relinking it in random order, or by interleaving allocations
  - (c) (after Phase 6) a list using a pool allocator
- Run `perf stat -e cycles,instructions,L1-dcache-load-misses,LLC-load-misses`. On Intel you can also use `mem_load_retired.l1_miss`, `mem_load_retired.l3_miss`, and so on; see `perf list`.
- **Done when:** a table shows ns per element and misses per element for each case and size, and the write-up explains why (b) is so much worse than (a).

**P5.4 — AoS vs SoA**
- Use `struct Particle { float x, y, z, vx, vy, vz; int id; char name[36]; }` (AoS) against separate arrays per field (SoA).
- Kernel 1: update only the positions (`x += vx*dt` etc.). Kernel 2: touch every field.
- Also check whether the compiler vectorised: use `-fopt-info-vec-optimized`, or look at the assembly for `ymm` registers (with `-march=native`).
- **Done when:** you have perf stat numbers for both layouts and both kernels, and a write-up explaining when SoA wins, when it doesn't, and the role of vectorisation.

**P5.5 — False sharing**
- Two to eight threads each increment their own counter in an array of `std::atomic<uint64_t>`, first with adjacent counters and then with counters padded to separate cache lines using `alignas(64)`.
- ⚠️ Don't use `std::hardware_destructive_interference_size` under `-Werror`: GCC emits `-Winterference-size`. Hardcode `64`, with a comment and a `static_assert`, or suppress that one warning locally.
- Pin threads to **different physical cores**. Use `lscpu -e` to find which logical CPUs share a core, then `pthread_setaffinity_np`. As a control, also pin to two SMT siblings of the same core; there the effect mostly disappears, because they share L1.
- **Done when:** the padded vs unpadded speedup is measured for 2, 4 and 8 threads, and `perf c2c record/report` on the unpadded version shows the contended cache line (HITM).

**P5.6 — Prefetcher and branch-prediction side experiments (stretch)**
- (a) Sum over a sorted vs shuffled index array.
- (b) The classic "sorted array is faster" branch example; compare it with a branchless version, and use `perf stat -e branch-misses`.
- (c) `__builtin_prefetch` in a pointer-chasing loop.
- **Done when:** each has numbers and a two-line explanation.

**P5.7 — Summary write-up**
- `docs/memory_hierarchy.md`: the latency ladder, plus the three to five lessons you'd bring up in an interview.
- **Done when:** you could present it in 5 minutes.

### Key concepts
- Cache lines, sets and associativity, hardware prefetchers, TLB and page walks (plus the effect of huge pages; try `madvise(MADV_HUGEPAGE)` as a stretch)
- Latency vs bandwidth; memory-level parallelism, and why pointer chasing kills it
- MESI/MESIF coherence, why false sharing costs what it costs, and HITM
- SMT: what hyper-threads share
- Auto-vectorisation requirements (contiguous, aligned, no aliasing, simple loops)

### Tests to write
- These are experiments, but each program should **verify its own result** (for example, print a checksum) so the optimiser can't delete the work and you can't fool yourself.
- Add a tiny GTest for any data-structure helper you reuse, such as the random-permutation builder.

### Resources
- Ulrich Drepper, *What Every Programmer Should Know About Memory* (2007; still the reference)
- Scott Meyers, "CPU Caches and Why You Care" (code::dive 2014)
- Mike Acton, "Data-Oriented Design and C++" (CppCon 2014)
- *Performance Analysis and Tuning on Modern CPUs*, Bakhvalov
- Intel® 64 and IA-32 Architectures Optimization Reference Manual
- `man perf-c2c`, plus Joe Mario's `perf c2c` write-up on the Red Hat developer blog
- Igor Ostrovsky, "Gallery of Processor Cache Effects" (blog post)

---

# Phase 6 — Arena and pool allocators; allocator support in your containers

**Goal:** allocator-aware containers that follow `std::allocator_traits`, plus two custom allocators, with measured wins.

### Tasks

**P6.1 — Refactor `stl::vector` to `vector<T, Alloc = std::allocator<T>>`**
- Store the allocator using `[[no_unique_address]]`, so an empty allocator takes no space.
- Replace the private helpers from Phase 2 with `allocator_traits<Alloc>::allocate`, `deallocate`, `construct` and `destroy`.
- Use `traits::pointer` everywhere, which may be a fancy pointer: use `std::to_address` when you need a raw pointer.
- **Done when:** all Phase 2 tests still pass unchanged, and `sizeof(stl::vector<int>) == 3 * sizeof(void*)`.

**P6.2 — Allocator propagation**
- Implement `propagate_on_container_copy_assignment`, `..._move_assignment` and `..._swap`, plus `select_on_container_copy_construction`.
- Move assignment with unequal, non-propagating allocators must move element by element, not steal the buffer.
- **Done when:** tests using a stateful test allocator (with an ID; `==` compares IDs) cover all POCMA/POCCA/POCS combinations, and each result matches the standard wording.

**P6.3 — Counting / tracing test allocator**
- `tests/support/counting_allocator.hpp` counts allocations, deallocations and bytes, with configurable propagation traits, and (optionally) throws on the Nth allocation.
- **Done when:** it replaces the P1.7 hack, and a test asserts that `bad_alloc` thrown during `push_back` leaves the vector unchanged (strong guarantee).

**P6.4 — Allocators for `list` and `unordered_map`**
- Rebind the allocator: `allocator_traits<A>::rebind_alloc<node>`.
- **Done when:** `stl::list<int, counting_allocator<int>>` counts exactly one allocation per node, with none for the sentinel.

**P6.5 — Arena (monotonic / bump) allocator**
- An `Arena` class owns a buffer, which can be a stack buffer or a chain of `mmap` blocks. `allocate(bytes, align)` bumps a pointer, `deallocate` is a no-op, and `reset()` frees everything.
- Add a thin `ArenaAllocator<T>` that satisfies the Allocator requirements (holds an `Arena*`, rebind-able, `==` compares arenas).
- **Done when:** `stl::vector<int, ArenaAllocator<int>>` works, alignment tests for over-aligned types pass, and exceeding capacity either chains a new block or throws `bad_alloc`, as documented.

**P6.6 — Pool (fixed-size block) allocator**
- Keep a free list threaded through the free blocks and allocate chunks of N blocks at a time. It's ideal for list and hashmap nodes.
- Requests for `n != 1` fall back to `::operator new`.
- **Done when:** after warm-up, `stl::list<int, PoolAllocator<int>>` performs zero calls to `::operator new` in steady-state push/pop cycles, and ASan is clean.

**P6.7 — Make the allocators ASan-aware (stretch)**
- Use `ASAN_POISON_MEMORY_REGION` / `ASAN_UNPOISON_MEMORY_REGION` (from `<sanitizer/asan_interface.h>`) on free arena and pool memory, so a use-after-free inside your pool is caught.
- **Done when:** a deliberate use-after-free through the pool is reported by ASan.

**P6.8 — `std::pmr` comparison**
- Rewrite the arena as a `std::pmr::memory_resource`. Compare it with `std::pmr::monotonic_buffer_resource` and `unsynchronized_pool_resource`.
- Write down the trade-off: a template allocator changes the container *type*, while pmr pays a virtual call.
- **Done when:** a benchmark compares `std::allocator`, your pool, your arena and pmr for list insert and hashmap insert.

**P6.9 — Revisit P5.3 with the pool allocator**
- **Done when:** the P5.3 table has a "pooled list" row, and there's an explanation of how locality changes.

### Key concepts
- The Allocator named requirements; `allocator_traits` defaults; `rebind`
- Stateful vs stateless allocators; `is_always_equal`; POCCA/POCMA/POCS semantics
- Fancy pointers (`pointer_traits`, `to_address`)
- `[[no_unique_address]]` / the EBO
- Monotonic vs pool vs general-purpose allocation strategies; fragmentation
- `std::pmr` design: type erasure vs template parameters

### Tests to write
- The full Phase 2 and Phase 4 suites re-run with `counting_allocator` (use typed tests over allocator types)
- Propagation-matrix tests with a stateful allocator
- `bad_alloc` injection → strong-guarantee checks
- Alignment tests (`alignas(32)`, `alignas(64)`) through the arena
- Steady-state zero-allocation tests for the pool

### Resources
- cppreference: Allocator requirements, `std::allocator_traits`, `std::pmr::memory_resource`
- Alisdair Meredith and Pablo Halpern's CppCon talks on allocators and pmr
- John Lakos, "Local ('Arena') Memory Allocators" (CppCon 2017), and the related paper P0089
- Andrei Alexandrescu, "std::allocator Is to Allocation what std::vector Is to Vexation" (CppCon 2015)
- *Modern C++ Design*, Alexandrescu — the small-object allocator chapter
- Arthur O'Dwyer's blog posts on allocators and `allocator_traits`, and his book *Mastering the C++17 STL* (allocator chapter)

---

# Phase 7 — `mmap`/`munmap`, your own `malloc`/`free`, and `LD_PRELOAD`

**Goal:** a working general-purpose allocator in a shared library that can run real programs (`ls`, `git`, your own test binaries) via `LD_PRELOAD`.

### Ground rules (read these first)
- The `.so` is built **without sanitizers**, because ASan replaces `malloc` itself. Use `-fPIC -O2 -g`, and set the default visibility carefully.
- Inside your allocator, **never call anything that might allocate**: no `printf`, `std::cout` or `std::string`, and no exceptions. For logging, use `write(2, buf, len)` and a hand-rolled integer formatter.
- It's C++, but it exports `extern "C"` symbols. Don't use static objects with non-trivial constructors, because initialization order is unknowable under preload.
- Implement the **whole family**, or programs will mix your `free` with glibc's `malloc` and crash: `malloc`, `free`, `calloc` (with an overflow check!), `realloc`, `posix_memalign`, `aligned_alloc`, `memalign`, `valloc`/`pvalloc` (optional), and `malloc_usable_size`.

### Tasks

**P7.1 — mmap / munmap playground**
- Small programs that:
  - `mmap` anonymous memory, touch pages and watch RSS in `/proc/self/status`
  - `mprotect` a page and catch `SIGSEGV`
  - look at `/proc/self/maps`
  - measure page-fault counts with `perf stat -e page-faults,minor-faults`
- **Done when:** you can explain demand paging, why the first touch is slow, and what `MAP_POPULATE` changes, with numbers.

**P7.2a — Build layout that keeps tests isolated**
- Put the allocator core in a **static/object library** that exports only prefixed names (`mm_malloc`, `mm_free`, …).
- A separate **shared library** target, `libmymalloc.so`, adds the thin `extern "C"` `malloc`/`free`/… wrappers. It's used *only* through `LD_PRELOAD`.
- The direct GTest tests link the static library. If they linked the `.so`, ordinary symbol resolution would route GTest's own allocations through your half-finished malloc.
- **Done when:** the direct test binary's `nm -D` output shows no exported `malloc`, and the `.so`'s does.

**P7.2b — Version 0: mmap every allocation**
- Each `malloc` is an `mmap` with a header storing the size; `free` is a `munmap`.
- **Done when:** direct tests pass (alignment of 16, `realloc` growing and shrinking, `calloc` zeroing, `calloc` overflow returning NULL), and `LD_PRELOAD=./libmymalloc.so ls` works (slowly).

**P7.3a — Version 1: heap region, block headers, first fit**
- Take large chunks from `mmap`. Each block gets a header with the size and an in-use bit, plus boundary tags (a footer).
- Use first fit with splitting of large blocks, but no coalescing yet.
- **Done when:** the direct tests pass, and a heap-walk debug function can iterate over every block and check its header and footer.

**P7.3b — Coalescing, best fit, and the shadow-map stress test**
- Coalesce with neighbours on `free` using the boundary tags. Add best fit as a compile-time option.
- Write a randomized alloc/free stress test with a shadow map (a `std::map` of live ranges, kept in the test, not the allocator) that checks no two live blocks overlap and that contents are preserved.
- **Done when:** 1M random operations pass with both fit policies, the heap walk after `free`-ing everything shows one free block per chunk, and you've recorded the fragmentation difference between first fit and best fit.

**P7.4 — Version 2: size-class segregated free lists**
- Small sizes (≤ 256 B, or similar) use per-size-class slabs, in the spirit of Phase 6's pool. Large sizes (≥ 128 KiB) go straight to `mmap`. Medium sizes use the V1 free list.
- **Done when:** the stress test passes, and a microbenchmark shows V2 is at least as fast as V1 for small allocations. Compare it with glibc too.

**P7.5 — Thread safety**
- Step 1: one global lock. Use a spin lock built on `std::atomic_flag` or a `pthread_mutex_t` with a static initializer. **Not** `std::mutex`, because of global constructor concerns.
- Step 2 (stretch): per-thread caches (tcache-style) using `thread_local` pointers with **initial-exec TLS**. `__attribute__((tls_model("initial-exec")))` avoids the TLS allocation recursion problem.
- **Done when:** a multithreaded stress test (8 threads doing random malloc/free, including cross-thread frees) passes, and `LD_PRELOAD` into a multithreaded program works.

**P7.6 — LD_PRELOAD validation**
- Run these under `LD_PRELOAD`:
  - `ls -la /usr`, `git status`, `python3 -c "print(sum(range(10**6)))"`
  - your own GTest binary, built from a **non-sanitized** debug preset
  - one of your benchmark binaries
- Add an env var (`MYMALLOC_STATS=1`) that prints allocation stats at exit using `write(2)`, from a destructor-attributed function.
- **Done when:** all of those run correctly, and your stats line shows non-zero counts, which proves your allocator really was used.

**P7.7 — Debug features**
- Add optional canaries before and after each block, checked on `free`; fill freed memory with a pattern such as `0xDD`; and detect double frees.
- **Done when:** a test program with a buffer overflow and one with a double free are both caught by your allocator, with a message written through `write(2)`.

**P7.8 — Compare with glibc**
- Benchmark a malloc-heavy workload, such as your `stl::list` benchmark or `std::map` inserts, with glibc malloc vs yours, and optionally jemalloc or tcmalloc if they're installed.
- Use `perf stat` for page faults, dTLB misses and cycles.
- **Done when:** `docs/malloc.md` has a results table and explains the design trade-offs you made.

### Key concepts
- Virtual memory, pages, demand paging, RSS vs VSZ, `brk`/`sbrk` vs `mmap`, `madvise(MADV_DONTNEED)`
- Alignment requirements of `malloc` (`alignof(std::max_align_t)`, 16 on x86-64)
- Fragmentation (internal and external), coalescing, boundary tags, size classes, slabs
- The dynamic linker: symbol interposition, `LD_PRELOAD`, `dlsym(RTLD_NEXT, ...)` (and why calling it inside `malloc` recurses: `dlsym` itself may allocate)
- Thread-local storage models and why the initial-exec model matters here
- How glibc ptmalloc works (arenas, bins, tcache); how jemalloc and tcmalloc differ

### Tests to write
- Direct API tests: alignment, zero-size, `realloc(NULL, n)`, `realloc(p, 0)`, `calloc` overflow, `posix_memalign` with bad alignment returning `EINVAL`
- Randomized single-thread stress test with a shadow map (no overlap, contents preserved)
- Multithreaded stress test with cross-thread frees
- LD_PRELOAD smoke scripts (a `ctest` test that runs `LD_PRELOAD=... ls` and checks the exit code)
- Canary and double-free detection tests

### Resources
- `man 2 mmap`, `man 2 munmap`, `man 2 mprotect`, `man 3 malloc`, `man 8 ld.so` (the `LD_PRELOAD` section), `man 3 dlsym`, `man 5 proc`
- The glibc wiki page "MallocInternals"
- *Computer Systems: A Programmer's Perspective* (Bryant & O'Hallaron) — chapter 9, Virtual Memory, including the malloc lab design
- Doug Lea, "A Memory Allocator" (the dlmalloc design write-up)
- Jason Evans, "A Scalable Concurrent malloc(3) Implementation for FreeBSD" (the jemalloc paper)
- The TCMalloc design doc (google.github.io/tcmalloc/design.html)
- Ulrich Drepper, "ELF Handling For Thread-Local Storage"
- *The Linux Programming Interface*, Michael Kerrisk — the chapters on memory allocation, virtual memory and shared libraries

---

# Phase 8 — Thread-safe structures, lock-free structures, memory ordering

**Goal:** correct concurrent data structures, first with locks and then lock-free, with a real understanding of the C++ memory model, verified by TSan and stress tests.

> Use the `debug-tsan` preset from P0.5 for every task here. GCC warns `-Wtsan` when `std::atomic_thread_fence` is used under TSan. Prefer operations with an explicit order on atomics, or suppress that warning locally where a fence is really the point of the exercise.
> If a TSan binary aborts at startup with an "unexpected memory mapping" error (this happens on some newer kernels with high ASLR entropy), the usual fix is to run it under `setarch -R`, or to lower `vm.mmap_rnd_bits` with sysctl.

### Tasks

**P8.1 — Memory-model warm-up: litmus tests**
- Write small programs for:
  - message passing (release/acquire vs relaxed)
  - store buffering (why `seq_cst` is needed)
  - IRIW (stretch)
- Run each one millions of times and count the outcomes.
- **Done when:** you have observed the store-buffering "impossible" outcome on this x86 machine with relaxed and with acq/rel ordering, and shown that `seq_cst` (or `mfence`) prevents it. Also write down why you *won't* observe message-passing violations on x86: TSO. The code is still wrong, and it would break on ARM.

**P8.2 — Spinlock**
- Build it on `std::atomic<bool>` with `exchange(acquire)` and `store(release)`. Add test-and-test-and-set, a `_mm_pause()` in the spin loop, and exponential backoff.
- **Done when:** a TSan-clean counter test with 8 threads × 1M increments gives the exact total, and a benchmark compares TAS, TTAS, TTAS+pause and `std::mutex` under contention.

**P8.3 — Mutex-based thread-safe queue**
- `ThreadSafeQueue<T>`: `std::mutex` and `std::condition_variable`; `push`, `try_pop`, `wait_pop`, `close()`.
- Watch out for spurious wake-ups, lost wake-ups, and exception safety of `pop` (why `std::stack::pop` returns `void`).
- **Done when:** a multi-producer, multi-consumer test (4 producers × 4 consumers, 1M items) sees every item exactly once (checked with a checksum and a count), is TSan-clean and shuts down cleanly with `close()`.

**P8.4 — Fine-grained locking**
- A queue with separate head and tail mutexes and a dummy node (Williams, chapter 6), **or** a hashmap with one mutex per bucket (or per stripe).
- **Done when:** the P8.3 tests pass against it, and a benchmark shows a throughput gain over the single-lock version at 4 or more threads.

**P8.5 — Lock-free SPSC ring buffer**
- A power-of-two capacity, `head_` and `tail_` atomics on **separate cache lines**, acquire/release ordering, and cached copies of the other side's index (the classic optimization).
- **Done when:** a test pushes 100M items through without loss or reordering, is TSan-clean, and a benchmark shows ops/sec with and without the padding and the index cache (connect this back to P5.5).

**P8.6a — Treiber lock-free stack, leaking version, plus ABA**
- `push`/`pop` with `compare_exchange_weak`. To begin with, **never free popped nodes** (leak them), so the structure is correct but not yet memory-safe.
- *Demonstrate the ABA problem*. Write a deterministic, hand-sequenced interleaving (driven by a test hook) or a written trace showing how naïve `pop` with immediate `delete` is a use-after-free and corrupts the stack.
- **Done when:** an 8-thread push/pop stress test of the leaking version is TSan-clean and conserves the item count, and the ABA trace is in `docs/lockfree.md`.

**P8.6b — Memory reclamation for the Treiber stack**
- Fix memory reclamation using one of:
  - (a) tagged pointers / a 16-byte CAS (`cmpxchg16b`; check `std::atomic<...>::is_always_lock_free`, and note you may need `-mcx16` and libatomic)
  - (b) hazard pointers
  - (c) a simple epoch-based reclamation
  - (d) (C++26) look at `std::hazard_pointer` / RCU in the standard proposals, and note what libstdc++ 15 does or doesn't have
- **Done when:** an 8-thread push/pop stress test runs for 10M operations, is ASan-clean *and* TSan-clean, and `docs/lockfree.md` explains the reclamation scheme you chose.

**P8.7 — Michael–Scott MPMC queue (stretch) or a bounded MPMC queue (Vyukov-style)**
- The bounded Vyukov queue (a sequence number per slot) is easier to get right; start there.
- **Done when:** the MPMC tests from P8.3 pass against it, and a benchmark compares it with the mutex queue at 1, 2, 4 and 8 threads.

**P8.8 — Relaxing the orderings, carefully**
- For each atomic operation in P8.5 and P8.7, justify its memory order in a comment. Try weakening one, and see whether any test catches it. Usually none will on x86, which is the lesson.
- **Done when:** every atomic operation has a comment saying which "happens-before" edge it creates, or why relaxed is enough.

**P8.9 — Concurrency benchmarking and perf**
- Use `perf stat` on the contended benchmarks, look at `perf c2c` for the ring buffer without padding, and try `perf lock` or `perf sched` (optional).
- **Done when:** `docs/concurrency_perf.md` has scaling curves (ops/sec vs threads) for mutex queue, fine-grained queue and lock-free queue, with an explanation.

### Key concepts
- Data races vs race conditions; the C++ memory model; happens-before; synchronizes-with
- `memory_order_relaxed`, `acquire`, `release`, `acq_rel`, `seq_cst` (and why `consume` is effectively `acquire`)
- x86 TSO vs weaker models (ARM, POWER): what hardware reorders and what compilers reorder
- `compare_exchange_weak` vs `strong`, spurious failure, and CAS loops
- The ABA problem; memory reclamation (hazard pointers, epochs, RCU)
- Lock-free vs wait-free vs obstruction-free; progress guarantees
- Contention, cache-line ping-pong, backoff; `std::atomic::wait`/`notify` (C++20)
- Condition variables: predicates, spurious wake-ups, lost wake-ups

### Tests to write
- Linearizability-style stress tests: every item produced is consumed exactly once (count plus checksum); FIFO order per producer
- TSan runs of every test (the `debug-tsan` preset), plus ASan runs, because lock-free reclamation bugs show up as use-after-free
- Shutdown/close tests (no deadlock; use a GTest-level timeout wrapper)
- Litmus-test programs with printed outcome histograms
- (stretch) Run the stress tests under `stress-ng` load or with random `std::this_thread::yield()` injected, to shake out interleavings

### Resources
- Anthony Williams, *C++ Concurrency in Action*, 2nd ed. — chapters 5 (memory model), 6 (lock-based structures) and 7 (lock-free structures). This is the core text for this phase.
- Herlihy & Shavit, *The Art of Multiprocessor Programming* (2nd ed. with Luchangco and Spear)
- Jeff Preshing's blog (preshing.com): "Memory Ordering at Compile Time", "Memory Barriers Are Like Source Control Operations", "Acquire and Release Semantics", "An Introduction to Lock-Free Programming"
- Herb Sutter, "atomic<> Weapons" (C++ and Beyond 2012, two parts)
- Fedor Pikus's CppCon talks on lock-free programming and atomics
- Paul McKenney, *Is Parallel Programming Hard, And, If So, What Can You Do About It?* (free)
- Dmitry Vyukov's 1024cores.net (the bounded MPMC queue)
- Maged Michael, "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects"; Michael & Scott, "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms"
- Mara Bos, *Rust Atomics and Locks*. Its memory-model chapters are one of the clearest explanations anywhere, and they apply directly to C++.
- cppreference: `std::memory_order`, `std::atomic`, `std::atomic_ref`

---

## Cross-cutting checklist (apply in every phase)

- [ ] `ctest --preset debug-asan` is green before every commit
- [ ] New containers have differential tests against their `std::` counterpart
- [ ] Every exception-safety claim has a fault-injection test
- [ ] Every performance claim has a benchmark plus `perf stat` numbers, recorded with machine state (governor, turbo, pinning)
- [ ] Every phase ends with a short `docs/` write-up: what you built, what you measured, what surprised you, what you'd say in an interview

## Interview-readiness milestones

| After phase | You should be able to explain, without notes |
|---|---|
| 2 | Why `vector` needs `noexcept` move constructors; what the strong guarantee costs; what invalidates iterators |
| 3 | How you'd benchmark a change so it's trustworthy; how to read `perf stat` and `perf report` |
| 4 | Chaining vs open addressing, and why `std::unordered_map` is slow |
| 5 | The cache hierarchy with real numbers from your machine; false sharing and how to find it |
| 6 | When a custom allocator helps, and POCMA & friends |
| 7 | How `malloc` gets memory from the kernel; fragmentation; how `LD_PRELOAD` interposition works |
| 8 | acquire/release vs seq_cst with an example; the ABA problem; how you'd make a queue lock-free and prove it's correct |
