// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_ALT_SIGNAL_STACK_H
#define GRIDCOIN_TEST_ALT_SIGNAL_STACK_H

#include <cstddef>

//! What the pre-main alternate-signal-stack installation actually did.
//!
//! Recorded at static initialisation because it cannot be observed later: Boost's
//! signal_handler destructor disables the alternate stack at the end of every
//! monitored scope, so by the time a test case runs, sigaltstack(2) reports no
//! stack at all regardless of what we installed. See alt_signal_stack.cpp.
struct AltSignalStackReport
{
    bool attempted = false;      //!< false on platforms where we do not install one
    bool installed = false;      //!< sigaltstack(2) returned success
    std::size_t requested = 0;   //!< bytes we asked for
    std::size_t kernel_minimum = 0;  //!< AT_MINSIGSTKSZ, 0 if unpublished
    bool deferred_to_existing = false;  //!< an adequate stack was already installed; left alone
};

const AltSignalStackReport& GetAltSignalStackReport();

#endif // GRIDCOIN_TEST_ALT_SIGNAL_STACK_H
