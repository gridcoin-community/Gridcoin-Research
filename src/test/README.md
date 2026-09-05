The sources in this directory are unit test cases. The unit tests uses Boost's unit testing framework. 
Since Gridcoin already uses Boost, Gridcoin uses the framework instead of requiring developers
to configure another framework (reduces barriers to creating unit tests).

The build system is setup to compile an executable called "test_gridcoin"
that runs all of the unit tests. The main source file is called
test_gridcoin.cpp, which simply includes other files that contain the
actual unit tests (outside of a couple required preprocessor
directives). The pattern is to create one test file for each class or
source file for which you want to create unit tests. The file naming
convention is "<source_filename>_tests.cpp" and such files should wrap
their tests in a test suite called "<source_filename>_tests". For an
examples of this pattern, examine uint160_tests.cpp and uint256_tests.cpp.

The tests in transaction_tests.cpp are edge cases of Gridcoin transactions.
They are in their current state not relevant for Gridcoin. Unusual transactions
should be collected again from the Gridcoin blockchain and replace
the current test cases.

For further reading, [see Boost's documentation](https://www.boost.org/doc/libs/1_73_0/libs/test/doc/html/boost_test/intro.html)
about how the boost unit test framework works

## Process state, and what a suite must put back

Every suite runs in one process on top of a single global fixture,
`TestingSetup` in test_gridcoin.cpp, which builds the in-memory transaction
database, the mock wallet, ECC and the (quiescent) net managers once for the
whole binary. Nothing rebuilds them per suite: the LevelDB handle must never be
closed, the wallet's Berkeley DB environment is bound to the one data
directory, and the net managers assert they are created exactly once.

The consequence is that anything a suite leaves in a process global is the
next suite's input, and which suite is next is link order -- so a leak that is
harmless on one CI leg is a failure on another (the Windows cross-compile leg
links in a different order from the Linux legs, and has been the one to find
them).

### The leak detector

`state_leak_detector.cpp`, installed from `TestingSetup`, checks the boundary.
It attaches a fixture to every top-level suite that snapshots a fixed set of
process globals at suite entry and compares at suite exit, and fails the run
with one error per invariant that changed, naming the suite, the value before
and after, and the first test case that changed it. The invariant set is
`grc_test::StateSnapshot` in state_guard.h: the chain tip and the mapBlockIndex
key set, the selected network, the consensus globals `LoadBlockIndex()`
overwrites, mock time, `g_mock_deterministic_tests`, the mempool size, the
miner status, `fUseFastIndex`, the ArgsManager settings, and the registry
sizes that have a cheap accessor.

Two rules of the comparison matter when reading a report:

- Size-like fields (mempool, mapBlockIndex, registries, miner status) fail on
  **growth only**. A suite that clears an earlier suite's residue is reported
  as a cleaner in a note, not as a leaker; several fixtures clear on exit by
  design.
- The settings comparison is **presence-sensitive**. A key that is present
  at its default value is a different state from an absent key: an empty
  string parses as `true` for a boolean argument and as `0` for an integer
  one, and `IsArgSet()` changes its answer. "Restoring" an argument by
  writing a value back is therefore still a leak.

`kKnownLeaks` in state_leak_detector.cpp downgrades a listed leak to a printed
note. An entry is a tolerance for a leak that is tracked and not yet fixed,
never a requirement: whether it fires depends on which suite ran first (two
suites that exit with the same value are mutually exclusive) and on
`--run_test` filters. Add one only with an issue reference, and remove it in
the commit that fixes the leak at source. The table is empty at the time of
writing.

### Writing a suite that mutates globals

Use `grc_test::StateGuard` (state_guard.h). It captures on construction and
restores on destruction, and works as a suite decorator, a per-case fixture
base, or a local:

    BOOST_AUTO_TEST_SUITE(x_tests, *boost::unit_test::fixture<grc_test::StateGuard>())
    BOOST_FIXTURE_TEST_SUITE(x_tests, grc_test::StateGuard)
    { grc_test::StateGuard guard; ... }

`CleanStateGuard` also moves mapBlockIndex aside, nulls the tip and clears
the mempool on entry, which is what a suite needs before `LoadBlockIndex()`
will create genesis. `MempoolStateGuard` clears only the mempool, on entry
and exit. The registries are deliberately outside `StateGuard` because
`Reset()` is destructive; use `RegistryResetFor<GRC::ContractType::...>` as a
suite decorator, which resets on entry AND exit. `V15HeightGuard` pins
`-blockv15height` for a scope and restores its presence exactly;
`RequireV15Unscheduled()` is the precondition for tests that rely on V15
rules being inert.

What the guard deliberately does not restore, and why, is in the header
comment of state_guard.h: the LevelDB handle, the wallet, the net managers,
the registered-argument table `ClearArgs()` wipes, and the temporary data
directory on disk.

Two habits keep a new suite out of the detector's report:

- Never `ForceSetArg(name, "")` to "unset" an argument. Use a guard, which
  puts the whole settings map back.
- A fixture that points `pindexBest` / `pindexGenesisBlock` at memory it
  owns must restore the pointers as well as free the memory; a `StateGuard`
  as the fixture's first member does both in the right order.

A `--run_test=<suite>/<case>` filter still runs the suite's fixtures, so a
single case can show a diff set that the whole suite would not.
