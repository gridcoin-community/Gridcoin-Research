// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUIEVENTLOOP_H
#define BITCOIN_QT_GUIEVENTLOOP_H

#include <utility>

//! Runs the GUI's event loop and the teardown after it, passed in as `run`.
//! A function, so that test_gridcoin-qt can run the scope StartGridcoinQt runs.
template <typename Fn>
void RunGuiEventLoop(Fn&& run)
{
    std::forward<Fn>(run)();
}

#endif // BITCOIN_QT_GUIEVENTLOOP_H
