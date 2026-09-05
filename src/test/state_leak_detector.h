// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_STATE_LEAK_DETECTOR_H
#define GRIDCOIN_TEST_STATE_LEAK_DETECTOR_H

namespace grc_test {

//!
//! \brief Fail the run when a suite leaves process state behind.
//!
//! Call once from the global TestingSetup constructor. It registers a
//! test_observer that captures the StateSnapshot around every test CASE (for
//! blame: which case first changed an invariant), and attaches a suite-level
//! fixture to every top-level suite that captures at suite entry, compares at
//! suite exit, and raises one BOOST_ERROR per leaked invariant.
//!
//! The split is forced by Boost.Test: an assertion raised from an observer hook
//! is either banked against no test unit (test_unit_finish) or discarded
//! outright (test_start / test_finish), so only a fixture teardown -- which
//! runs with the suite as the current test unit -- can put a failure on the
//! exit code. Anything that goes wrong inside this function itself is reported
//! by throwing, which the framework turns into "Test setup error" and a
//! nonzero exit; a BOOST_ERROR here would be silently dropped.
//!
//! Leaks that are known and tracked are listed in kKnownLeaks
//! (state_leak_detector.cpp) and downgraded to a printed note. An entry there
//! is a tolerance, not a requirement: whether it fires depends on suite order
//! and on --run_test filters.
//!
void InstallStateLeakDetector();

} // namespace grc_test

#endif // GRIDCOIN_TEST_STATE_LEAK_DETECTOR_H
