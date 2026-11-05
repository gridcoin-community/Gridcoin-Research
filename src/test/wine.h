// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_WINE_H
#define GRIDCOIN_TEST_WINE_H

namespace grc_test {

//! Whether this test binary runs under Wine, as the cross-compiled Windows suite does in
//! CI. Wine accepts an access control list without keeping its protection, so a case
//! that needs Windows' own access control asks this first. False except on Windows.
bool RunningUnderWine();

} // namespace grc_test

#endif // GRIDCOIN_TEST_WINE_H
