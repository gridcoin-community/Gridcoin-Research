// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_WALLET_FILE_PLACEHOLDER_H
#define GRIDCOIN_TEST_WALLET_FILE_PLACEHOLDER_H

#include "fs.h"
#include "init.h"
#include "util/system.h"
#include "wallet/db.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <string>

namespace grc_test {

//! GRC::BackupWallet copies the wallet only when the wallet is file-backed and
//! nothing has its file open, and returns false without trying otherwise. A
//! case that needs the copy checks this first, so it cannot pass without
//! reaching it.
inline void RequireWalletFileNotInUse()
{
    BOOST_REQUIRE(pwalletMain->fFileBacked);

    LOCK(bitdb.cs_db);

    const auto use_count = bitdb.mapFileUseCount.find(pwalletMain->strWalletFile);

    BOOST_REQUIRE(use_count == bitdb.mapFileUseCount.end() || use_count->second == 0);
}

//! A file with pwalletMain's name in the data directory for one case, removed
//! when the case ends. The fixture's mock database never reads it, so a copy of
//! the wallet file is a copy of these bytes.
struct WalletFilePlaceholder
{
    static inline const std::string CONTENT{"wallet placeholder"};

    const fs::path m_path = GetDataDir() / pwalletMain->strWalletFile;

    WalletFilePlaceholder()
    {
        BOOST_REQUIRE(!fs::exists(m_path));
        fsbridge::ofstream stream(m_path);
        stream << CONTENT;
    }

    ~WalletFilePlaceholder()
    {
        boost::system::error_code ec;
        fs::remove(m_path, ec);
    }

    WalletFilePlaceholder(const WalletFilePlaceholder&) = delete;
    WalletFilePlaceholder& operator=(const WalletFilePlaceholder&) = delete;
};

} // namespace grc_test

#endif // GRIDCOIN_TEST_WALLET_FILE_PLACEHOLDER_H
