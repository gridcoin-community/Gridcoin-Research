// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_BOOST_TEST_PRELUDE_H
#define GRIDCOIN_TEST_BOOST_TEST_PRELUDE_H

// Included ahead of every test_gridcoin source by src/test/CMakeLists.txt, which
// compiles Boost.Test's headers as user headers so that a signed/unsigned
// comparison of ours inside BOOST_CHECK_EQUAL is reported rather than dropped.
// That also exposes warnings in Boost.Test's own declarations. Those are Boost's
// code and cannot be fixed here, so they are silenced while Boost.Test's headers
// are read, and nowhere else.
//
// A diagnostic pragma applies by source location, and only the warnings named
// below are ignored. -Wsign-compare is not among them, so a mixed-sign
// comparison of ours is still reported from inside Boost.Test's templates. A
// class of ours that hid a Boost.Test virtual would be reported at Boost's
// declaration and silenced with Boost's own; StateLeakObserver, the only such
// class, hides none.
//
// -Woverloaded-virtual: GCC reports test_observer::test_unit_skipped(const
// test_unit&) (boost/test/tree/observer.hpp) as hidden by unit_test_log_t's
// two-argument override (boost/test/unit_test_log.hpp).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include <boost/test/unit_test.hpp>
#pragma GCC diagnostic pop

#endif // GRIDCOIN_TEST_BOOST_TEST_PRELUDE_H
