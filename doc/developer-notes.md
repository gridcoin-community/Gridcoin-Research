Developer Notes
===============

<!-- markdown-toc start -->
**Table of Contents**

- [Developer Notes](#developer-notes)
    - [Coding Style (General)](#coding-style-general)
    - [Coding Style (C++)](#coding-style-c)
    - [Coding Style (Python)](#coding-style-python)
    - [Coding Style (Doxygen-compatible comments)](#coding-style-doxygen-compatible-comments)
      - [Generating Documentation](#generating-documentation)
    - [Development tips and tricks](#development-tips-and-tricks)
        - [Compiling for debugging](#compiling-for-debugging)
        - [Compiling for gprof profiling](#compiling-for-gprof-profiling)
        - [`debug.log`](#debuglog)
        - [Testnet mode](#testnet-mode)
        - [DEBUG_LOCKORDER](#debug_lockorder)
        - [Assertions and Checks](#assertions-and-checks)
        - [Valgrind suppressions file](#valgrind-suppressions-file)
        - [Compiling for test coverage](#compiling-for-test-coverage)
        - [Performance profiling with perf](#performance-profiling-with-perf)
        - [Stopwatch timing with MilliTimer](#stopwatch-timing-with-millitimer)
        - [Sanitizers](#sanitizers)
    - [Locking/mutex usage notes](#lockingmutex-usage-notes)
    - [Threads](#threads)
    - [Ignoring IDE/editor files](#ignoring-ideeditor-files)
- [Development guidelines](#development-guidelines)
    - [General Gridcoin Core](#general-gridcoin-core)
    - [Wallet](#wallet)
    - [General C++](#general-c)
    - [C++ data structures](#c-data-structures)
    - [Strings and formatting](#strings-and-formatting)
    - [Shadowing](#shadowing)
    - [Threads and synchronization](#threads-and-synchronization)
    - [Scripts](#scripts)
        - [Shebang](#shebang)
    - [Source code organization](#source-code-organization)
    - [GUI](#gui)
    - [Subtrees](#subtrees)
    - [Upgrading LevelDB](#upgrading-leveldb)
      - [File Descriptor Counts](#file-descriptor-counts)
      - [Consensus Compatibility](#consensus-compatibility)
    - [Scripted diffs](#scripted-diffs)
        - [Suggestions and examples](#suggestions-and-examples)
    - [Release notes](#release-notes)
    - [RPC interface guidelines](#rpc-interface-guidelines)

<!-- markdown-toc end -->

Coding Style (General)
----------------------

Various coding styles have been used during the history of the codebase,
and the result is not very consistent. However, we're now trying to converge to
a single style, which is specified below. The following guidelines apply:

- **New code and new modules** should follow the style guide below.
- **Modifying legacy code:** be consistent with the surrounding scope. Avoid
  gratuitous style churn in otherwise focused patches.
- **Don't mix cosmetic/style changes with domain changes** in the same commit.
  If style cleanup is needed, do it in a separate commit (scripted-diff is
  preferred for large-scale reformatting).
- **New scopes within legacy code** (e.g. a new function in an old file) may
  follow the style guide.
- **Upstream (subtree) code** should not have its style modified in-tree. Send
  style changes upstream.

Coding Style (C++)
------------------

- **Indentation and whitespace rules** as specified in
[src/.clang-format](/src/.clang-format). You can use the provided
[clang-format-diff script](/contrib/devtools/README.md#clang-format-diffpy)
tool to clean up patches automatically before submission.
  - Braces on new lines for classes, functions, methods.
  - Braces on the same line for everything else.
  - 4 space indentation (no tabs) for every block except namespaces.
  - No indentation for `public`/`protected`/`private` or for `namespace`.
  - No extra spaces inside parenthesis; don't do `( this )`.
  - No space after function names; one space after `if`, `for` and `while`.
  - If an `if` only has a single-statement `then`-clause, it can appear
    on the same line as the `if`, without braces. In every other case,
    braces are required, and the `then` and `else` clauses must appear
    correctly indented on a new line.
  - There's no hard limit on line width, but prefer to keep lines to <100
    characters if doing so does not decrease readability. Break up long
    function declarations over multiple lines using the Clang Format
    [AlignAfterOpenBracket](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
    style option.

- **Symbol naming conventions**. These are preferred in new code, but are not
required when doing so would need changes to significant pieces of existing
code.
  - Variable (including function arguments) and namespace names are all lowercase and may use `_` to
    separate words (snake_case).
    - Class member variables have a `m_` prefix.
    - Global variables have a `g_` prefix.
  - Compile-time constant names are all uppercase, and use `_` to separate words.
  - Class names, function names, and method names are UpperCamelCase
    (PascalCase). Do not prefix class names with `C`.
  - Test suite naming convention: The Boost test suite in file
    `src/test/foo_tests.cpp` should be named `foo_tests`. Test suite names
    must be unique.

  - **Legacy conventions:** Older code uses Hungarian notation for variable
    names (`nCount`, `strName`, `fEnabled`, `vItems`, `mapEntries`,
    `pPointer`) and the `C` class prefix (`CBlock`, `CWallet`). The modern
    conventions above (snake_case, `m_` prefix, no `C` prefix) are preferred
    for new code, but existing conventions should be respected per the general
    style guidance above.

- **Miscellaneous**
  - `++i` is preferred over `i++`.
  - `nullptr` is preferred over `NULL` or `(void*)0`.
  - `static_assert` is preferred over `assert` where possible. Generally; compile-time checking is preferred over run-time checking.

Block style example:
```c++
int g_count = 0;

namespace foo {
class Class
{
    std::string m_name;

public:
    bool Function(const std::string& s, int n)
    {
        // Comment summarising what this section of code does
        for (int i = 0; i < n; ++i) {
            int total_sum = 0;
            // When something fails, return early
            if (!Something()) return false;
            ...
            if (SomethingElse(i)) {
                total_sum += ComputeSomething(g_count);
            } else {
                DoSomething(m_name, total_sum);
            }
        }

        // Success return is usually at the end
        return true;
    }
}
} // namespace foo
```

Coding Style (Python)
---------------------

Adhere to [PEP8](https://www.python.org/dev/peps/pep-0008/).

Coding Style (Doxygen-compatible comments)
------------------------------------------

Gridcoin Core uses [Doxygen](https://www.doxygen.nl/) to generate its official documentation.

Use Doxygen-compatible comment blocks for functions, methods, and fields.

For example, to describe a function use:

```c++
/**
 * ... Description ...
 *
 * @param[in]  arg1 input description...
 * @param[in]  arg2 input description...
 * @param[out] arg3 output description...
 * @return Return cases...
 * @throws Error type and cases...
 * @pre  Pre-condition for function...
 * @post Post-condition for function...
 */
bool function(int arg1, const char *arg2, std::string& arg3)
```

A complete list of `@xxx` commands can be found at https://www.doxygen.nl/manual/commands.html.
As Doxygen recognizes the comments by the delimiters (`/**` and `*/` in this case), you don't
*need* to provide any commands for a comment to be valid; just a description text is fine.

To describe a class, use the same construct above the class definition:
```c++
/**
 * Alerts are for notifying old versions if they become too obsolete and
 * need to upgrade. The message is displayed in the status bar.
 * @see GetWarnings()
 */
class CAlert
```

To describe a member or variable use:
```c++
//! Description before the member
int var;
```

or
```c++
int var; //!< Description after the member
```

Also OK:
```c++
///
/// ... Description ...
///
bool function2(int arg1, const char *arg2)
```

Not picked up by Doxygen:
```c++
//
// ... Description ...
//
```

Also not picked up by Doxygen:
```c++
/*
 * ... Description ...
 */
```

A full list of comment syntaxes picked up by Doxygen can be found at
https://www.doxygen.nl/manual/docblocks.html, but the above styles are favored.

Recommendations:

- Avoiding duplicating type and input/output information in function
  descriptions.

- Use backticks (&#96;&#96;) to refer to `argument` names in function and
  parameter descriptions.

- Backticks aren't required when referring to functions Doxygen already knows
  about; it will build hyperlinks for these automatically. See
  https://www.doxygen.nl/manual/autolink.html for complete info.

- Avoid linking to external documentation; links can break.

- Javadoc and all valid Doxygen comments are stripped from Doxygen source code
  previews (`STRIP_CODE_COMMENTS = YES` in [Doxyfile.in](Doxyfile.in)). If
  you want a comment to be preserved, it must instead use `//` or `/* */`.

### Generating Documentation

The documentation can be generated with CMake:

```shell
cmake -B build -DENABLE_DOCS=ON
cmake --build build --target docs
# Output: build/doc/doxygen/html/index.html
```

Before building the docs, you'll need to install these dependencies:

Linux: `sudo apt install doxygen graphviz`

MacOS: `brew install doxygen graphviz`

Development tips and tricks
---------------------------

### Compiling for debugging

Configure with `-DCMAKE_BUILD_TYPE=Debug` to produce debugging builds:

```shell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j $(nproc)
```

Note: The default build type `RelWithDebInfo` provides debug symbols with
optimizations enabled, which is sufficient for most debugging with `gdb`.

### Compiling for gprof profiling

gprof is not currently supported as a dedicated option in the CMake build
system. Use `perf` instead (see [Performance profiling with
perf](#performance-profiling-with-perf) below), which is the preferred modern
approach and does not require recompilation.

### `debug.log`

If the code is behaving strangely, take a look in the `debug.log` file in the
data directory. Error and debugging messages are written there.

The `-debug=<category>` command-line option controls debugging. Multiple options
can be specified. While running the application, logging can be dynamically
changed with the logging rpc call. -debug=VERBOSE corresponds to the most common
needs for debugging, while -debug=NOISY results in extensive debugging entries.

The Qt code routes `qDebug()` output to `debug.log` under category "qt": run
with `-debug=qt` to see it.

### Testnet mode

If you are testing multi-machine code that needs to operate across the internet,
you can run with the `-testnet` config option to test with "play Gridcoins" on
the test network. Specify `org=<identifier>` in your config file when using
testnet so that your nodes can be identified.

### DEBUG_LOCKORDER

Gridcoin is a multi-threaded application, and deadlocks or other
multi-threading bugs can be very difficult to track down. Building with
`-DENABLE_DEBUG_LOCKORDER=ON` adds `-DDEBUG_LOCKORDER` to the compiler
flags. This inserts run-time checks to keep track of which locks are held
and adds warnings to the `debug.log` file if inconsistencies are detected.
This option is independent of `CMAKE_BUILD_TYPE` so it can be used with any
build configuration.

### Assertions and Checks

The util file `src/util/check.h` offers helpers to protect against coding and
internal logic bugs. They must never be used to validate user, network or any
other input.

* `assert` or `Assert` should be used to document assumptions when any
  violation would mean that it is not safe to continue program execution. The
  code is always compiled with assertions enabled.
   - For example, a nullptr dereference or any other logic bug in validation
     code means the program code is faulty and must terminate immediately.
* `CHECK_NONFATAL` should be used for recoverable internal logic bugs. On
  failure, it will throw an exception, which can be caught to recover from the
  error.
   - For example, a nullptr dereference or any other logic bug in RPC code
     means that the RPC code is faulty and can not be executed. However, the
     logic bug can be shown to the user and the program can continue to run.
* `Assume` should be used to document assumptions when program execution can
  safely continue even if the assumption is violated. In debug builds it
  behaves like `Assert`/`assert` to notify developers and testers about
  nonfatal errors. In production it doesn't warn or log anything, though the
  expression is always evaluated.
   - For example it can be assumed that a variable is only initialized once,
     but a failed assumption does not result in a fatal bug. A failed
     assumption may or may not result in a slightly degraded user experience,
     but it is safe to continue program execution.

### Valgrind suppressions file

Valgrind is a programming tool for memory debugging, memory leak detection, and
profiling. The repo contains a Valgrind suppressions file
([`valgrind.supp`](https://github.com/gridcoin-community/Gridcoin-Research/tree/master/contrib/valgrind.supp))
which includes known Valgrind warnings in our dependencies that cannot be fixed
in-tree. Example use:

```shell
$ valgrind --suppressions=contrib/valgrind.supp build/src/test/test_gridcoin
$ valgrind --suppressions=contrib/valgrind.supp --leak-check=full \
      --show-leak-kinds=all build/src/test/test_gridcoin --log_level=test_suite
$ valgrind -v --leak-check=full build/src/gridcoinresearchd -printtoconsole
```

### Compiling for test coverage

Dedicated lcov/coverage support is not yet integrated into the CMake build
system. See [`doc/cmake-options.md`](cmake-options.md) for current build
options. In the meantime, coverage can be achieved manually by adding
`--coverage` to `CMAKE_CXX_FLAGS` and `CMAKE_EXE_LINKER_FLAGS`:

```shell
cmake -B build_cov \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build build_cov -j $(nproc)
ctest --test-dir build_cov --output-on-failure

# Generate report with lcov (install the `lcov` package on Debian/Ubuntu):
lcov --capture --directory build_cov --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
# Open coverage_report/index.html to view.
```

### Performance profiling with perf

Profiling is a good way to get a precise idea of where time is being spent in
code. One tool for doing profiling on Linux platforms is called
[`perf`](http://www.brendangregg.com/perf.html). Perf can observe a running
process and sample (at some frequency) where its execution is.

Perf installation is contingent on which kernel version you're running; see
[this thread](https://askubuntu.com/questions/50145/how-to-install-perf-monitoring-tool)
for specific instructions.

Certain kernel parameters may need to be set for perf to be able to inspect the
running process's stack.

```sh
$ sudo sysctl -w kernel.perf_event_paranoid=-1
$ sudo sysctl -w kernel.kptr_restrict=0
```

Make sure you [understand the security
trade-offs](https://lwn.net/Articles/420403/) of setting these kernel
parameters.

To profile a running gridcoinresearchd process for 60 seconds, you could use an
invocation of `perf record` like this:

```sh
$ perf record \
    -g --call-graph dwarf --per-thread -F 140 \
    -p `pgrep gridcoinresearchd` -- sleep 60
```

You could then analyze the results by running:

```sh
perf report --stdio | c++filt | less
```

or using a graphical tool like [Hotspot](https://github.com/KDAB/hotspot).

To profile specific tests or other commands, run them under `perf record` using
similar options to those shown above.

### Stopwatch timing with MilliTimer

For measuring elapsed time within the application itself, Gridcoin provides the
`MilliTimer` class ([`src/util/time.h`](/src/util/time.h)). It implements a
thread-safe stopwatch-style timer with "lap time" support, useful for profiling
code paths that span multiple functions or stages. A global instance `g_timer`
is available throughout the codebase.

Key methods:

- `InitTimer(label, log)` — Start a new named timer. If `log` is true, lap
  times are written to `debug.log` automatically.
- `GetTimes(log_string, label)` — Return a `timer` struct with `elapsed_time`
  (since init) and `time_since_last_check` (since last call). If logging is
  enabled, emits the times to `debug.log` prefixed with `log_string`.
- `GetElapsedTime(log_string, label)` — Convenience wrapper returning only the
  elapsed time.
- `DeleteTimer(label)` — Remove a timer from the map.

Example — timing stages of the miner loop:

```c++
#include <util/time.h>

extern MilliTimer g_timer;

void ThreadStakeMiner(...)
{
    std::string function = __func__ + std::string(": ");

    // Init the "miner" timer; enable logging if MISC category is active
    g_timer.InitTimer("miner", LogInstance().WillLogCategory(BCLog::LogFlags::MISC));

    // ... do work ...
    g_timer.GetTimes(function + "SelectCoinsForStaking", "miner");

    // ... do more work ...
    g_timer.GetTimes(function + "CreateCoinStake", "miner");

    // ... etc ...
    g_timer.GetTimes(function + "ProcessBlock", "miner");
}
```

With logging enabled, each `GetTimes` call emits a line like:

```
INFO: GetTimes: timer miner: ThreadStakeMiner: CreateCoinStake: elapsed time: 1234 ms, time since last check: 56 ms.
```

The `g_timer` instance is also used during initialization
([`src/init.cpp`](/src/init.cpp)) to measure startup phases. For simple
single-point timings, using `GetTimeMillis()` directly is lighter weight than
constructing a `MilliTimer`.

### Sanitizers

Gridcoin Core can be compiled with various "sanitizers" enabled, which add
instrumentation for issues regarding things like memory safety, thread race
conditions, or undefined behavior. These sanitizers have runtime overhead,
so they are most useful when testing changes or producing debugging builds.
Sanitizers are actively used in CI (see `.github/workflows/cmake_quality.yml`).

To build with AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan),
pass the flags via `CMAKE_CXX_FLAGS` and `CMAKE_EXE_LINKER_FLAGS`:

```bash
cmake -B build_asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DUSE_ASM=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build_asan -j $(nproc)

# Run tests with suppression files:
LSAN_OPTIONS=suppressions=test/sanitizer_suppressions/lsan \
UBSAN_OPTIONS=suppressions=test/sanitizer_suppressions/ubsan:print_stacktrace=1:halt_on_error=1 \
ASAN_OPTIONS=malloc_context_size=0:check_initialization_order=1 \
  ctest --test-dir build_asan --output-on-failure
```

To build with ThreadSanitizer (TSan) instead:

```bash
cmake -B build_tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build_tsan -j $(nproc)

TSAN_OPTIONS=suppressions=test/sanitizer_suppressions/tsan \
  ctest --test-dir build_tsan --output-on-failure
```

Suppression files for known issues in dependencies are located in
`test/sanitizer_suppressions/` (lsan, tsan, ubsan).

Notes:
- `-DUSE_ASM=OFF` is recommended when using ASan, as the hand-written assembly
  can produce false positives. This also disables `USE_ASM_SCRYPT` (scrypt
  assembly). See `doc/cmake-options.md` for details on these flags.
- If you are compiling with GCC you will typically need to install corresponding
  "san" libraries, e.g. `libasan` for ASan, `libtsan` for TSan, and `libubsan`
  for UBSan.
- ASan and TSan are mutually incompatible and cannot be enabled at the same
  time. Refer to your compiler manual for details.

Additional resources:

 * [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
 * [LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html)
 * [MemorySanitizer](https://clang.llvm.org/docs/MemorySanitizer.html)
 * [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html)
 * [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
 * [GCC Instrumentation Options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)
 * [Google Sanitizers Wiki](https://github.com/google/sanitizers/wiki)

Locking/mutex usage notes
-------------------------

The code is multi-threaded and uses mutexes and the
`LOCK` and `TRY_LOCK` macros to protect data structures.

Deadlocks due to inconsistent lock ordering (thread 1 locks `cs_main` and then
`cs_wallet`, while thread 2 locks them in the opposite order: result, deadlock
as each waits for the other to release its lock) are a problem. Compile with
`-DENABLE_DEBUG_LOCKORDER=ON` to get lock order inconsistencies reported in the
`debug.log` file.

### Lock macros

- **`LOCK(cs)`** — Acquires a single lock.
- **`LOCK2(cs1, cs2)`** — Acquires two locks **sequentially in argument order**
  (cs1 first, then cs2). Bitcoin Core later reimplemented `LOCK2` using
  `std::lock()` to provide deadlock avoidance regardless of argument order.
  This codebase does not use that approach — `LOCK2` is simply two sequential
  lock acquisitions. This means **argument order matters**: callers must list
  locks in the canonical order to prevent deadlocks. For example, always write
  `LOCK2(cs_main, cs_wallet)`, never `LOCK2(cs_wallet, cs_main)`.
- **`TRY_LOCK(cs, name)`** — Non-blocking lock attempt.

### Canonical lock ordering

When multiple locks must be held simultaneously, acquire them in the following
order to prevent deadlocks:

1. `cs_main` (blockchain state)
2. `cs_wallet` (wallet operations)
3. Subsystem-specific locks (e.g. `cs_poll_registry`, `cs_ScraperGlobals`)

This ordering applies regardless of whether you use `LOCK2` or separate `LOCK`
statements. The `DEBUG_LOCKORDER` build flag will detect violations at runtime
and report them in `debug.log`.

### Reading DEBUG_LOCKORDER output

When a lock ordering violation is detected, the output looks like this:

```
POTENTIAL DEADLOCK DETECTED
Conflict: 'cs_main' and 'cs_poll_registry' acquired in inconsistent orders.
  Current:    'cs_main' -> 'cs_poll_registry'
  Historical: 'cs_poll_registry' -> 'cs_main'

Historical lock stack (where the reverse order was first seen):
  #1 'cs_main' in src/main.cpp:200 (in thread 'main')  <--
  #2 'cs_poll_registry' in src/gridcoin/voting/registry.cpp:1057 (in thread 'main')  <--
  #3 'cs_main' in src/main.cpp:736 (in thread 'main')  (re-entrant, already held above)

Current lock stack (triggering this warning):
  #1 'cs_main' in src/gridcoin/gridcoin.cpp:636 (in thread '')  <--
  #2 'cs_poll_registry' in src/gridcoin/gridcoin.cpp:636 (in thread '')  <--
```

Key elements:
- **Conflict summary**: Names both locks and shows the two orderings that conflict.
- **Stack entries**: Numbered by acquisition order. `<--` marks the first occurrence of each conflicting lock.
- **Re-entrant flag**: When a lock appears as "already held above", the apparent
  inversion may be a false positive — the lock was already held higher in the stack,
  so the re-acquisition is harmless. This is common with `CCriticalSection`
  (recursive mutex) when a function that takes `LOCK(cs_main)` is called from code
  that already holds `cs_main`.

Re-architecting the core code so there are better-defined interfaces
between the various components is a goal, with any necessary locking
done by the components (e.g. see the self-contained `CKeyStore` class (in `src/keystore.h`)
and its `cs_KeyStore` lock for example).

### Clang thread-safety analysis

Clang ships a static thread-safety analyzer that checks the `GUARDED_BY`,
`EXCLUSIVE_LOCKS_REQUIRED` and related annotations at compile time — it
verifies that a guarded variable is only touched while the right lock is
held, and that a function annotated as requiring a lock is only called with
that lock held. It is complementary to `DEBUG_LOCKORDER`: that flag checks
lock *ordering* at runtime, the analyzer checks lock *coverage* at compile
time.

The analyzer only runs under Clang (and Apple Clang). **gcc ignores the
annotations entirely**, so a gcc-only build never exercises them — a missing
or stale annotation compiles cleanly. The build system auto-enables
`-Wthread-safety` whenever the compiler supports it; by default the
diagnostics are warning-only.

**Do a Clang build before opening a PR** so the analyzer runs against your
changes. `build_targets.sh` honors the `CC` / `CXX` environment variables:

```bash
CC=clang CXX=clang++ ./build_targets.sh TARGET=native BUILD_TYPE=RelWithDebInfo
```

The `Thread Safety (Clang)` job in `.github/workflows/cmake_quality.yml`
builds with `-DWERROR_THREAD_SAFETY=ON`, which promotes the thread-safety
diagnostics to hard errors — a PR that introduces an annotation violation
fails CI. To reproduce that gate locally, pass the same option to CMake:

```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DWERROR_THREAD_SAFETY=ON ...
```

Threads
--------

Gridcoin is a multi-threaded application. Key threads include:

| Thread | Source | Description |
|--------|--------|-------------|
| `ThreadStakeMiner` | `src/net.cpp` | Proof-of-stake block creation (calls `StakeMiner()` in `src/miner.cpp`) |
| `ThreadScraper` | `src/gridcoin/gridcoin.cpp` | Active scraper: downloads BOINC project stats and publishes signed manifests (mutually exclusive with subscriber) |
| `ThreadScraperSubscriber` | `src/gridcoin/gridcoin.cpp` | Subscriber: receives scraper manifests and runs convergence to build superblocks (mutually exclusive with scraper) |
| `ThreadSocketHandler` | `src/net.cpp` | Low-level socket I/O for P2P connections |
| `ThreadMessageHandler` | `src/net.cpp` | Processes incoming P2P messages |
| `ThreadOpenConnections` | `src/net.cpp` | Manages outbound peer connections |
| `ThreadDNSAddressSeed` | `src/net.cpp` | Resolves DNS seeds to bootstrap peer discovery |
| `ThreadDumpAddress` | `src/net.cpp` | Periodically dumps known peer addresses to `peers.dat` |
| RPC threads | `src/rpc/server.cpp` | Thread pool serving JSON-RPC requests |
| Scheduler | `src/scheduler.cpp` | Runs deferred and periodic tasks |

Ignoring IDE/editor files
--------------------------

In closed-source environments in which everyone uses the same IDE, it is common
to add temporary files it produces to the project-wide `.gitignore` file.

However, in open source software such as Gridcoin Core, where everyone uses
their own editors/IDE/tools, it is less common. Only you know what files your
editor produces and this may change from version to version. The canonical way
to do this is thus to create your local gitignore. Add this to `~/.gitconfig`:

```
[core]
        excludesfile = /home/.../.gitignore_global
```

(alternatively, type the command `git config --global core.excludesfile ~/.gitignore_global`
on a terminal)

Then put your favourite tool's temporary filenames in that file, e.g.
```
# NetBeans
nbproject/
```

Another option is to create a per-repository excludes file `.git/info/exclude`.
These are not committed but apply only to one repository.

If a set of tools is used by the build system or scripts in the repository (for
example, lcov) it is perfectly acceptable to add its files to `.gitignore`
and commit them.

Development guidelines
============================

A few non-style-related recommendations for developers, as well as points to
pay attention to for reviewers of Gridcoin Core code.

General Gridcoin Core
----------------------

- New features should be exposed on RPC first, then can be made available in the GUI.

  - *Rationale*: RPC allows for better automatic testing. The test suite for
    the GUI is very limited.

- Make sure pull requests pass CI before merging.

  - *Rationale*: Makes sure that they pass thorough testing, and that the tester will keep passing
     on the master branch. Otherwise, all new pull requests will start failing the tests, resulting in
     confusion and mayhem.

  - *Explanation*: If the test suite is to be updated for a change, this has to
    be done first.

Wallet
-------

- Make sure that no crashes happen with run-time option `-disablewallet`.

- Include `db_cxx.h` (BerkeleyDB header) only when `ENABLE_WALLET` is set.

  - *Rationale*: Otherwise compilation of the disable-wallet build will fail in environments without BerkeleyDB.

General C++
-------------

For general C++ guidelines, you may refer to the [C++ Core
Guidelines](https://isocpp.github.io/CppCoreGuidelines/).

Common misconceptions are clarified in those sections:

- Passing (non-)fundamental types in the [C++ Core
  Guideline](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-conventional).

- Assertions should not have side-effects.

  - *Rationale*: Even though the source code is set to refuse to compile
    with assertions disabled, having side-effects in assertions is unexpected and
    makes the code harder to understand.

- If you use the `.h`, you must link the `.cpp`.

  - *Rationale*: Include files define the interface for the code in implementation files. Including one but
      not linking the other is confusing. Please avoid that. Moving functions from
      the `.h` to the `.cpp` should not result in build errors.

- Use the RAII (Resource Acquisition Is Initialization) paradigm where possible. For example, by using
  `unique_ptr` for allocations in a function.

  - *Rationale*: This avoids memory and resource leaks, and ensures exception safety.

C++ data structures
--------------------

- Never use the `std::map []` syntax when reading from a map, but instead use `.find()`.

  - *Rationale*: `[]` does an insert (of the default element) if the item doesn't
    exist in the map yet. This has resulted in memory leaks in the past, as well as
    race conditions (expecting read-read behavior). Using `[]` is fine for *writing* to a map.

- Do not compare an iterator from one data structure with an iterator of
  another data structure (even if of the same type).

  - *Rationale*: Behavior is undefined. In C++ parlor this means "may reformat
    the universe", in practice this has resulted in at least one hard-to-debug crash bug.

- Watch out for out-of-bounds vector access. `&vch[vch.size()]` is illegal,
  including `&vch[0]` for an empty vector. Use `vch.data()` and `vch.data() +
  vch.size()` instead.

- Vector bounds checking is only enabled in debug mode. Do not rely on it.

- Initialize all non-static class members where they are defined.
  If this is skipped for a good reason (i.e., optimization on the critical
  path), add an explicit comment about this.

  - *Rationale*: Ensure determinism by avoiding accidental use of uninitialized
    values. Also, static analyzers balk about this.
    Initializing the members in the declaration makes it easy to
    spot uninitialized ones.

```cpp
class A
{
    uint32_t m_count{0};
}
```

- By default, declare constructors `explicit`.

  - *Rationale*: This is a precaution to avoid unintended
    [conversions](https://en.cppreference.com/w/cpp/language/converting_constructor).

- Use explicitly signed or unsigned `char`s, or even better `uint8_t` and
  `int8_t`. Do not use bare `char` unless it is to pass to a third-party API.
  This type can be signed or unsigned depending on the architecture, which can
  lead to interoperability problems or dangerous conditions such as
  out-of-bounds array accesses.

- Prefer explicit constructions over implicit ones that rely on 'magical' C++ behavior.

  - *Rationale*: Easier to understand what is happening, thus easier to spot mistakes, even for those
  that are not language lawyers.

- Use `Span` as function argument when it can operate on any range-like container.

  - *Rationale*: Compared to `Foo(const vector<int>&)` this avoids the need for a (potentially expensive)
    conversion to vector if the caller happens to have the input stored in another type of container.
    However, be aware of the pitfalls documented in [span.h](../src/span.h).

```cpp
void Foo(Span<const int> data);

std::vector<int> vec{1,2,3};
Foo(vec);
```

- Prefer `enum class` (scoped enumerations) over `enum` (traditional enumerations) where possible.

  - *Rationale*: Scoped enumerations avoid two potential pitfalls/problems with traditional C++ enumerations: implicit conversions to `int`, and name clashes due to enumerators being exported to the surrounding scope.

- `switch` statement on an enumeration example:

```cpp
enum class Tabs {
    INFO,
    CONSOLE,
    GRAPH,
    PEERS
};

int GetInt(Tabs tab)
{
    switch (tab) {
    case Tabs::INFO: return 0;
    case Tabs::CONSOLE: return 1;
    case Tabs::GRAPH: return 2;
    case Tabs::PEERS: return 3;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}
```

*Rationale*: The comment documents skipping `default:` label, and it complies with `clang-format` rules. The assertion prevents firing of `-Wreturn-type` warning on some compilers.

Strings and formatting
------------------------

- Be careful of `LogPrint` versus `LogPrintf`. `LogPrint` takes a `category` argument, `LogPrintf` does not.

  - *Rationale*: Confusion of these can result in runtime exceptions due to
    formatting mismatch, and it is easy to get wrong because of subtly similar naming.

- Use `std::string`, avoid C string manipulation functions.

  - *Rationale*: C++ string handling is marginally safer, less scope for
    buffer overflows, and surprises with `\0` characters. Also, some C string manipulations
    tend to act differently depending on platform, or even the user locale.

- Use `ParseInt32`, `ParseInt64`, `ParseUInt32`, `ParseUInt64`, `ParseDouble` from `util/strencodings.h` for number parsing.

  - *Rationale*: These functions do overflow checking and avoid pesky locale issues.

- Avoid using locale dependent functions if possible. You can use the provided
  [`lint-locale-dependence.sh`](/test/lint/lint-locale-dependence.sh)
  to check for accidental use of locale dependent functions.

  - *Rationale*: Unnecessary locale dependence can cause bugs that are very tricky to isolate and fix.

  - These functions are known to be locale dependent:
    `alphasort`, `asctime`, `asprintf`, `atof`, `atoi`, `atol`, `atoll`, `atoq`,
    `btowc`, `ctime`, `dprintf`, `fgetwc`, `fgetws`, `fprintf`, `fputwc`,
    `fputws`, `fscanf`, `fwprintf`, `getdate`, `getwc`, `getwchar`, `isalnum`,
    `isalpha`, `isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`,
    `ispunct`, `isspace`, `isupper`, `iswalnum`, `iswalpha`, `iswblank`,
    `iswcntrl`, `iswctype`, `iswdigit`, `iswgraph`, `iswlower`, `iswprint`,
    `iswpunct`, `iswspace`, `iswupper`, `iswxdigit`, `isxdigit`, `mblen`,
    `mbrlen`, `mbrtowc`, `mbsinit`, `mbsnrtowcs`, `mbsrtowcs`, `mbstowcs`,
    `mbtowc`, `mktime`, `putwc`, `putwchar`, `scanf`, `snprintf`, `sprintf`,
    `sscanf`, `stoi`, `stol`, `stoll`, `strcasecmp`, `strcasestr`, `strcoll`,
    `strfmon`, `strftime`, `strncasecmp`, `strptime`, `strtod`, `strtof`,
    `strtoimax`, `strtol`, `strtold`, `strtoll`, `strtoq`, `strtoul`,
    `strtoull`, `strtoumax`, `strtouq`, `strxfrm`, `swprintf`, `tolower`,
    `toupper`, `towctrans`, `towlower`, `towupper`, `ungetwc`, `vasprintf`,
    `vdprintf`, `versionsort`, `vfprintf`, `vfscanf`, `vfwprintf`, `vprintf`,
    `vscanf`, `vsnprintf`, `vsprintf`, `vsscanf`, `vswprintf`, `vwprintf`,
    `wcrtomb`, `wcscasecmp`, `wcscoll`, `wcsftime`, `wcsncasecmp`, `wcsnrtombs`,
    `wcsrtombs`, `wcstod`, `wcstof`, `wcstoimax`, `wcstol`, `wcstold`,
    `wcstoll`, `wcstombs`, `wcstoul`, `wcstoull`, `wcstoumax`, `wcswidth`,
    `wcsxfrm`, `wctob`, `wctomb`, `wctrans`, `wctype`, `wcwidth`, `wprintf`

- For `strprintf`, `LogPrint`, `LogPrintf` formatting characters don't need size specifiers.

  - *Rationale*: Gridcoin Core uses tinyformat, which is type safe. Leave them out to avoid confusion.

- Use `.c_str()` sparingly. Its only valid use is to pass C++ strings to C functions that take NULL-terminated
  strings.

  - Do not use it when passing a sized array (so along with `.size()`). Use `.data()` instead to get a pointer
    to the raw data.

    - *Rationale*: Although this is guaranteed to be safe starting with C++11, `.data()` communicates the intent better.

  - Do not use it when passing strings to `tfm::format`, `strprintf`, `LogPrint[f]`.

    - *Rationale*: This is redundant. Tinyformat handles strings.

  - Do not use it to convert to `QString`. Use `QString::fromStdString()`.

    - *Rationale*: Qt has built-in functionality for converting their string
      type from/to C++. No need to roll your own.

  - In cases where do you call `.c_str()`, you might want to additionally check that the string does not contain embedded '\0' characters, because
    it will (necessarily) truncate the string. This might be used to hide parts of the string from logging or to circumvent
    checks. If a use of strings is sensitive to this, take care to check the string for embedded NULL characters first
    and reject it if there are any (see `ParsePrechecks` in `strencodings.cpp` for an example).

Shadowing
--------------

Although the shadowing warning (`-Wshadow`) is not enabled by default (it prevents issues arising
from using a different variable with the same name),
please name variables so that their names do not shadow variables defined in the source code.

When using nested cycles, do not name the inner cycle variable the same as in
the upper cycle, etc.

Threads and synchronization
----------------------------

- Prefer `Mutex` type to `RecursiveMutex` one

- Consistently use [Clang Thread Safety Analysis](https://clang.llvm.org/docs/ThreadSafetyAnalysis.html) annotations to
  get compile-time warnings about potential race conditions in code. Combine annotations in function declarations with
  run-time asserts in function definitions:

  - In functions that are declared separately from where they are defined, the
    thread safety annotations should be added exclusively to the function
    declaration. Annotations on the definition could lead to false positives
    (lack of compile failure) at call sites between the two.

```C++
// miner.h
bool CreateGridcoinReward(CBlock& blocknew,
                          CBlockIndex* pindexPrev,
                          int64_t& nReward,
                          GRC::Claim& claim) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

// miner.cpp
bool CreateGridcoinReward(CBlock& blocknew,
                          CBlockIndex* pindexPrev,
                          int64_t& nReward,
                          GRC::Claim& claim)
{
    AssertLockHeld(cs_main);
    ...
}
```

```C++
// Example: a function that acquires cs_main internally
// main.h
bool GetTransaction(const uint256& hash, CTransaction& tx, uint256& hashBlock);

// main.cpp
bool GetTransaction(const uint256& hash, CTransaction& tx, uint256& hashBlock)
{
    LOCK(cs_main);
    ...
}
```

- Build and run tests with `-DENABLE_DEBUG_LOCKORDER=ON` to verify that no
  potential deadlocks are introduced.

- When using `LOCK`/`TRY_LOCK` be aware that the lock exists in the context of
  the current scope, so surround the statement and the code that needs the lock
  with braces.

  OK:
```c++
{
    TRY_LOCK(cs_vNodes, lockNodes);
    ...
}
```

  Wrong:
```c++
TRY_LOCK(cs_vNodes, lockNodes);
{
    ...
}
```

Scripts
--------------------------

### Shebang

- Use `#!/usr/bin/env bash` instead of obsolete `#!/bin/bash`.

  - [*Rationale*](https://github.com/dylanaraps/pure-bash-bible#shebang):

    `#!/bin/bash` assumes it is always installed to /bin/ which can cause issues;

    `#!/usr/bin/env bash` searches the user's PATH to find the bash binary.

  OK:
```bash
#!/usr/bin/env bash
```

  Wrong:
```bash
#!/bin/bash
```

Source code organization
--------------------------

- Implementation code should go into the `.cpp` file and not the `.h`, unless
  necessary due to template usage or when performance due to inlining is
  critical.

  - *Rationale*: Shorter and simpler header files are easier to read and reduce
    compile time.

- Use only the lowercase alphanumerics (`a-z0-9`), underscore (`_`) and hyphen
  (`-`) in source code filenames.

  - *Rationale*: `grep`:ing and auto-completing filenames is easier when using a
    consistent naming pattern. Potential problems when building on case-
    insensitive filesystems are avoided when using only lowercase characters in
    source code filenames.

- Every `.cpp` and `.h` file should `#include` every header file it directly
  uses classes, functions or other definitions from, even if those headers are
  already included indirectly through other headers.

  - *Rationale*: Excluding headers because they are already indirectly included
    results in compilation failures when those indirect dependencies change.
    Furthermore, it obscures what the real code dependencies are.

- Don't import anything into the global namespace (`using namespace ...`). Use
  fully specified types such as `std::string`.

  - *Rationale*: Avoids symbol conflicts.

- Terminate namespaces with a comment (`// namespace mynamespace`). The comment
  should be placed on the same line as the brace closing the namespace, e.g.

```c++
namespace mynamespace {
...
} // namespace mynamespace

namespace {
...
} // namespace
```

  - *Rationale*: Avoids confusion about the namespace context.

- Use `#include <primitives/transaction.h>` bracket syntax instead of
  `#include "primitives/transactions.h"` quote syntax.

  - *Rationale*: Bracket syntax is less ambiguous because the preprocessor
    searches a fixed list of include directories without taking location of the
    source file into account. This allows quoted includes to stand out more when
    the location of the source file actually is relevant.

- Use include guards to avoid the problem of double inclusion. Two conventions
  are in use:
  - Bitcoin-inherited headers retain the `BITCOIN_` prefix for upstream
    compatibility: `foo/bar.h` uses `BITCOIN_FOO_BAR_H`.
  - Gridcoin-specific headers use the `GRIDCOIN_` prefix: `gridcoin/foo.h`
    uses `GRIDCOIN_FOO_H`.

```c++
#ifndef GRIDCOIN_FOO_H
#define GRIDCOIN_FOO_H
...
#endif // GRIDCOIN_FOO_H
```

GUI
-----

- Do not display or manipulate dialogs in model code (classes `*Model`).

  - *Rationale*: Model classes pass through events and data from the core, they
    should not interact with the user. That's where View classes come in. The
    converse also holds: try to not directly access core data structures from
    Views.

- Avoid adding slow or blocking code in the GUI thread.

  Prefer to offload work from the GUI thread to worker threads (see
  `RPCExecutor` in console code as an example) or take other steps (see
  https://doc.qt.io/archives/qq/qq27-responsive-guis.html) to keep the GUI
  responsive.

  - *Rationale*: Blocking the GUI thread can increase latency, and lead to
    hangs and deadlocks.

Subtrees
----------

Several parts of the repository are subtrees of software maintained elsewhere.

Some of these are maintained by active developers of Gridcoin Core, in which case changes should probably go
directly upstream without being PRed directly against the project. They will be merged back in the next
subtree merge.

Others are external projects without a tight relationship with our project. Changes to these should also
be sent upstream, but bugfixes may also be prudent to PR against Gridcoin Core so that they can be integrated
quickly. Cosmetic changes should be purely taken upstream.

There is a tool in `test/lint/git-subtree-check.sh` ([instructions](../test/lint#git-subtree-checksh)) to check a subtree directory for consistency with
its upstream repository.

Current subtrees include:

- src/leveldb
  - Upstream at https://github.com/google/leveldb ; Maintained by Google, but
    open important PRs to Core to avoid delay.
  - **Note**: Follow the instructions in [Upgrading LevelDB](#upgrading-leveldb) when
    merging upstream changes to the LevelDB subtree.

- src/crc32c
  - Used by leveldb for hardware acceleration of CRC32C checksums for data integrity.
  - Upstream at https://github.com/google/crc32c ; Maintained by Google.

- src/secp256k1
  - Upstream at https://github.com/bitcoin-core/secp256k1/ ; actively maintained by Core contributors.

- src/crypto/ctaes
  - Upstream at https://github.com/bitcoin-core/ctaes ; actively maintained by Core contributors.

- src/univalue
  - Upstream at https://github.com/bitcoin-core/univalue ; actively maintained by Core contributors, deviates from upstream https://github.com/jgarzik/univalue

Upgrading LevelDB
---------------------

Extra care must be taken when upgrading LevelDB. This section explains issues
you must be aware of.

### File Descriptor Counts

In most configurations, we use the default LevelDB value for `max_open_files`,
which is 1000 at the time of this writing. If LevelDB actually uses this many
file descriptors, it will cause problems with Gridcoin's `select()` loop, because
it may cause new sockets to be created where the fd value is >= 1024. For this
reason, on 64-bit Unix systems, we rely on an internal LevelDB optimization that
uses `mmap()` + `close()` to open table files without actually retaining
references to the table file descriptors. If you are upgrading LevelDB, you must
sanity check the changes to make sure that this assumption remains valid.

In addition to reviewing the upstream changes in `env_posix.cc`, you can use `lsof` to
check this. For example, on Linux this command will show open `.ldb` file counts:

```bash
$ lsof -p $(pidof gridcoinresearchd) |\
    awk 'BEGIN { fd=0; mem=0; } /ldb$/ { if ($4 == "mem") mem++; else fd++ } END { printf "mem = %s, fd = %s\n", mem, fd}'
mem = 119, fd = 0
```

The `mem` value shows how many files are mmap'ed, and the `fd` value shows you
many file descriptors these files are using. You should check that `fd` is a
small number (usually 0 on 64-bit hosts).

### Consensus Compatibility

It is possible for LevelDB changes to inadvertently change consensus
compatibility between nodes. When upgrading LevelDB, you should review the
upstream changes to check for issues affecting consensus compatibility.

For example, if LevelDB had a bug that accidentally prevented a key from being
returned in an edge case, and that bug was fixed upstream, the bug "fix" would
be an incompatible consensus change. In this situation, the correct behavior
would be to revert the upstream fix before applying the updates to Gridcoin's
copy of LevelDB. In general, you should be wary of any upstream changes affecting
what data is returned from LevelDB queries.

Scripted diffs
--------------

For reformatting and refactoring commits where the changes can be easily automated using a bash script, we use
scripted-diff commits. The bash script is included in the commit message and our CI job checks that
the result of the script is identical to the commit. This aids reviewers since they can verify that the script
does exactly what it is supposed to do. It is also helpful for rebasing (since the same script can just be re-run
on the new master commit).

To create a scripted-diff:

- start the commit message with `scripted-diff:` (and then a description of the diff on the same line)
- in the commit message include the bash script between lines containing just the following text:
    - `-BEGIN VERIFY SCRIPT-`
    - `-END VERIFY SCRIPT-`

The scripted-diff is verified by the tool `test/lint/commit-script-check.sh`. The tool's default behavior, when supplied
with a commit is to verify all scripted-diffs from the beginning of time up to said commit. Internally, the tool passes
the first supplied argument to `git rev-list --reverse` to determine which commits to verify script-diffs for, ignoring
commits that don't conform to the commit message format described above.

For development, it might be more convenient to verify all scripted-diffs in a range `A..B`, for example:

```bash
test/lint/commit-script-check.sh origin/master..HEAD
```

### Suggestions and examples

If you need to replace in multiple files, prefer `git ls-files` to `find` or globbing, and `git grep` to `grep`, to
avoid changing files that are not under version control.

For efficient replacement scripts, reduce the selection to the files that potentially need to be modified, so for
example, instead of a blanket `git ls-files src | xargs sed -i s/apple/orange/`, use
`git grep -l apple src | xargs sed -i s/apple/orange/`.

Also, it is good to keep the selection of files as specific as possible — for example, replace only in directories where
you expect replacements — because it reduces the risk that a rebase of your commit by re-running the script will
introduce accidental changes.

Some good examples of scripted-diff:

- [scripted-diff: Rename InitInterfaces to NodeContext](https://github.com/bitcoin/bitcoin/commit/301bd41a2e6765b185bd55f4c541f9e27aeea29d)
uses an elegant script to replace occurrences of multiple terms in all source files.

- [scripted-diff: Remove g_connman, g_banman globals](https://github.com/bitcoin/bitcoin/commit/8922d7f6b751a3e6b3b9f6fb7961c442877fb65a)
replaces specific terms in a list of specific source files.

- [scripted-diff: Replace fprintf with tfm::format](https://github.com/bitcoin/bitcoin/commit/fac03ec43a15ad547161e37e53ea82482cc508f9)
does a global replacement but excludes certain directories.

To find all previous uses of scripted diffs in the repository, do:

```
git log --grep="-BEGIN VERIFY SCRIPT-"
```

Release notes
-------------

Release notes should be written for any PR that:

- introduces a notable new feature
- fixes a significant bug
- changes an API or configuration model
- makes any other visible change to the end-user experience.

Release notes should be added to a PR-specific release note file at
`/doc/release-notes-<PR number>.md` to avoid conflicts between multiple PRs.
All `release-notes*` files are merged into a single
[/doc/release-notes.md](/doc/release-notes.md) file prior to the release.

RPC interface guidelines
--------------------------

A few guidelines for introducing and reviewing new RPC interfaces:

- Method naming: use consecutive lower-case names such as `getrawtransaction` and `submitblock`.

  - *Rationale*: Consistency with the existing interface.

- Argument naming: use snake case `fee_delta` (and not, e.g. camel case `feeDelta`)

  - *Rationale*: Consistency with the existing interface.

- Use the JSON parser for parsing, don't manually parse integers or strings from
  arguments unless absolutely necessary.

  - *Rationale*: Introduces hand-rolled string manipulation code at both the caller and callee sites,
    which is error-prone, and it is easy to get things such as escaping wrong.
    JSON already supports nested data structures, no need to re-invent the wheel.

  - *Exception*: AmountFromValue can parse amounts as string. This was introduced because many JSON
    parsers and formatters hard-code handling decimal numbers as floating-point
    values, resulting in potential loss of precision. This is unacceptable for
    monetary values. **Always** use `AmountFromValue` and `ValueFromAmount` when
    inputting or outputting monetary values. The only exceptions to this are
    `prioritisetransaction` and `getblocktemplate` because their interface
    is specified as-is in BIP22.

- Missing arguments and 'null' should be treated the same: as default values. If there is no
  default value, both cases should fail in the same way. The easiest way to follow this
  guideline is to detect unspecified arguments with `params[x].isNull()` instead of
  `params.size() <= x`. The former returns true if the argument is either null or missing,
  while the latter returns true if it is missing, and false if it is null.

  - *Rationale*: Avoids surprises when switching to name-based arguments. Missing name-based arguments
  are passed as 'null'.

- Try not to overload methods on argument type. E.g. don't make `getblock(true)` and `getblock("hash")`
  do different things.

  - *Rationale*: This is impossible to use with `gridcoinresearchd`, and can be surprising to users.

  - *Exception*: Some RPC calls can take both an `int` and `bool`, most notably when a bool was switched
    to a multi-value, or due to other historical reasons. **Always** have false map to 0 and
    true to 1 in this case.

- Don't forget to fill in the argument names correctly in the RPC command table.

  - *Rationale*: If not, the call can not be used with name-based arguments.

- Add every non-string RPC argument `(method, idx, name)` to the table `vRPCConvertParams` in `rpc/client.cpp`.

  - *Rationale*: `gridcoinresearchd` and the GUI debug console use this table to determine how to
    convert a plaintext command line to JSON. If the types don't match, the method can be unusable
    from there.

- An RPC method must either be a wallet method or a non-wallet method. Do not
  introduce new methods that differ in behavior based on the presence of a wallet.

  - *Rationale*: As well as complicating the implementation and interfering
    with the introduction of multi-wallet, wallet and non-wallet code should be
    separated to avoid introducing circular dependencies between code units.

- Try to make the RPC response a JSON object.

  - *Rationale*: If an RPC response is not a JSON object, then it is harder to avoid API breakage if
    new data in the response is needed.

- Be aware of RPC method aliases and generally avoid registering the same
  callback function pointer for different RPCs.

  - *Rationale*: RPC methods registered with the same function pointer will be
    considered aliases and only the first method name will show up in the
    `help` RPC command list.

  - *Exception*: Using RPC method aliases may be appropriate in cases where a
    new RPC is replacing a deprecated RPC, to avoid both RPCs confusingly
    showing up in the command list.

- Use clearly invalid Base58Check addresses for `RPCExamples` help
  documentation.

  - *Rationale*: Prevent accidental transactions by users.

- Use the `UNIX_EPOCH_TIME` constant when describing UNIX epoch time or
  timestamps in the documentation.

  - *Rationale*: User-facing consistency.

Functional tests
----------------

In addition to the C++ Boost unit tests under `src/test/`, there is a Python
end-to-end functional-test framework under `test/functional/` that starts real
`gridcoinresearchd` nodes in `-regtest` mode and drives them over JSON-RPC and
P2P. It is ported from Bitcoin Core v0.21.2 and adapted for Gridcoin's
proof-of-stake + premine model.

- How to run, environment variables, and regtest gotchas:
  [`test/functional/README.md`](../test/functional/README.md).
- Regtest mode itself (premine, ports, instant staking): [`doc/regtest.md`](regtest.md).
- Build with `-DENABLE_TESTS=ON` and run `ctest -R functional_tests`, or
  `cmake --build build --target check-functional`.

Conventions when adding a test:

- Name it by area prefix (`feature_`, `wallet_`, `mempool_`, `rpc_`, `p2p_`) and
  register it in `BASE_SCRIPTS` in `test/functional/test_runner.py`.
- Subclass `GridcoinTestFramework`; set `self.chain = "regtest"` and
  `self.setup_clean_chain = True`, and override `setup_network()` to bypass the
  base `createwallet` path (Gridcoin has a single default wallet). See
  `feature_regtest_staking.py` as the canonical template.
- Keep each test small and deterministic (pass `-staking=0` and advance the
  chain with explicit `generatetoaddress` calls).
- Strip unused function parameters/locals: the lint gate runs `vulture
  --min-confidence 100` over all tracked `*.py` and will fail on dead code.
