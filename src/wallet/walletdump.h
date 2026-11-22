// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_WALLETDUMP_H
#define GRIDCOIN_WALLET_WALLETDUMP_H

#include "pubkey.h"
#include "wallet/wallet.h"

#include <univalue.h>

#include <cstddef>
#include <cstdint>
#include <istream>
#include <set>
#include <string>
#include <utility>
#include <vector>

//! \brief The text dumpwallet writes, a few whole lines at a time.
//!
//! The constructor takes what the dump lists (every key's birth time, the key
//! pool, the HD master) and its header lines, which name the time it was made
//! and the best block. Next() then writes the lines in order, reading each key
//! as it writes its line, so a key is read only while the caller may use the
//! keys. The text is the same, byte for byte, as dumpwallet writes it: the HD
//! master's extended key when the wallet holds the master, a line for every
//! key the wallet holds, and none for metadata whose key the wallet does not
//! hold.
//!
//! The constructor and each Next() take LOCK2(cs_main, wallet.cs_wallet) and
//! need wallet.CanUseKeys() on the calling thread. Without it they throw the
//! RPC error RPC_WALLET_UNLOCK_NEEDED "keys are not available". A key the
//! wallet holds but cannot read fails the dump as dumpwallet fails: Next()
//! throws RPC_WALLET_UNLOCK_NEEDED, or RPC_WALLET_ERROR when the wallet may use
//! its keys, "dumpwallet: cannot read the key for <address>".
class WalletDumpGenerator
{
public:
    explicit WalletDumpGenerator(CWallet& wallet);

    WalletDumpGenerator(const WalletDumpGenerator&) = delete;
    WalletDumpGenerator& operator=(const WalletDumpGenerator&) = delete;

    //! Append the next whole lines, at least one, totalling at most
    //! \p max_bytes to \p out. A single line longer than that is appended
    //! whole. Returns false once the "# End of dump" line has been appended;
    //! after that it appends nothing.
    bool Next(std::string& out, size_t max_bytes);

    //! An estimate of the dump's size for a free-space check: the header's
    //! bytes and 160 for each key listed.
    uint64_t SizeHint() const;

private:
    //! The parts of the dump, in order.
    enum class Part { Header, Master, MasterBlank, Keys, Blank, Trailer, Done };

    //! The line at the current position, after skipping a key the dump leaves
    //! out. False at the end. Needs the locks Next() takes.
    bool LineHere(std::string& line) EXCLUSIVE_LOCKS_REQUIRED(m_wallet.cs_wallet);

    //! Move past the line LineHere() gave.
    void Advance();

    CWallet& m_wallet;
    std::vector<std::string> m_header{};
    uint64_t m_header_bytes{0};
    //! (birth time, key), sorted, as dumpwallet orders them.
    std::vector<std::pair<int64_t, CKeyID>> m_keys{};
    std::set<CKeyID> m_key_pool{};
    CKeyID m_master_id{};
    Part m_part{Part::Header};
    size_t m_index{0};
};

//! \brief What importwallet does once its file is open: import the dump that
//! \p in holds into \p wallet.
//!
//! Under LOCK2(cs_main, wallet.cs_wallet) it refuses a wallet that is locked or
//! whose keys the calling thread may not use, as the RPC does, then adds each
//! key it parses, labels it, and rescans from shortly before the oldest key's
//! birth time. Returns and throws what importwallet returns and throws.
UniValue ImportWalletFromStream(CWallet& wallet, std::istream& in);

#endif // GRIDCOIN_WALLET_WALLETDUMP_H
