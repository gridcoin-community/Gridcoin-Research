// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_ENCRYPTED_WALLET_H
#define GRIDCOIN_TEST_ENCRYPTED_WALLET_H

#include "init.h"
#include "key.h"
#include "support/allocators/secure.h"
#include "sync.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

namespace grc_test {

//! An in-memory wallet holding one key, encrypted with Passphrase() and left Locked, in place of
//! pwalletMain for the helper's lifetime.
//!
//! In-memory rather than file-backed: EncryptWallet on a file-backed wallet rewrites wallet.dat,
//! which the mocked Berkeley DB of this binary refuses.
class EncryptedWalletSwap
{
public:
    static SecureString Passphrase() { return SecureString("k3-test-passphrase"); }

    EncryptedWalletSwap()
        : m_saved(pwalletMain)
    {
        CKey key;
        key.MakeNewKey(true);

        {
            LOCK(m_wallet.cs_wallet);
            BOOST_REQUIRE(m_wallet.AddKey(key));
        }

        BOOST_REQUIRE(m_wallet.EncryptWallet(Passphrase()));
        m_wallet.Lock();
        BOOST_REQUIRE(m_wallet.IsLocked());

        pwalletMain = &m_wallet;
    }

    ~EncryptedWalletSwap() { pwalletMain = m_saved; }

    EncryptedWalletSwap(const EncryptedWalletSwap&) = delete;
    EncryptedWalletSwap& operator=(const EncryptedWalletSwap&) = delete;

    CWallet& wallet() { return m_wallet; }

private:
    CWallet m_wallet;
    CWallet* m_saved{nullptr};
};

} // namespace grc_test

#endif // GRIDCOIN_TEST_ENCRYPTED_WALLET_H
