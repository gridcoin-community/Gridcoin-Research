// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_TEST_STAKING_TEST_PEERS_H
#define GRIDCOIN_TEST_STAKING_TEST_PEERS_H

#include "init.h"
#include "key.h"
#include "keystore.h"
#include "support/allocators/secure.h"
#include "sync.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <memory>

// The two test peers make tokens that elsewhere only the named factories and
// CapabilityScope may make. They must sit at global scope, because the friend
// declarations in keystore.h name ::StakingKeyAccessTestPeer and
// ::ElevationGrantTestPeer. test/lint/lint-staking-key-access.py confines them
// to src/test/.
struct StakingKeyAccessTestPeer
{
    static StakingKeyAccess Make() { return StakingKeyAccess(); }
};

struct ElevationGrantTestPeer
{
    static ElevationGrant Make(const CCryptoKeyStore& store) { return ElevationGrant(store); }

    static std::unique_ptr<ElevationGrant> MakeHeld(const CCryptoKeyStore& store)
    {
        return std::unique_ptr<ElevationGrant>(new ElevationGrant(store));
    }
};

namespace grc_test {

//! Swaps a wallet into pwalletMain for the life of a case, as wallet_tests does.
struct SwapMainWallet
{
    CWallet* m_saved{nullptr};

    explicit SwapMainWallet(CWallet* replacement) : m_saved(pwalletMain) { pwalletMain = replacement; }
    ~SwapMainWallet() { pwalletMain = m_saved; }

    SwapMainWallet(const SwapMainWallet&) = delete;
    SwapMainWallet& operator=(const SwapMainWallet&) = delete;
};

//! An in-memory wallet holding one key added before encryption, left locked,
//! and swapped into pwalletMain for its lifetime.
//!
//! In-memory rather than file-backed: EncryptWallet on a file-backed wallet
//! rewrites wallet.dat, which the mocked Berkeley DB of this binary refuses.
struct EncryptedWallet
{
    static inline const SecureString PASSPHRASE{"staking-key-access-test"};

    CWallet wallet;
    SwapMainWallet swap{&wallet};
    CKey key;
    CKeyID id;

    EncryptedWallet()
    {
        key.MakeNewKey(true);
        id = key.GetPubKey().GetID();

        {
            LOCK(wallet.cs_wallet);
            BOOST_REQUIRE(wallet.AddKey(key));
        }

        BOOST_REQUIRE(wallet.EncryptWallet(PASSPHRASE));
        wallet.Lock();
        BOOST_REQUIRE(wallet.IsLocked());
    }

    ~EncryptedWallet()
    {
        wallet.Lock();
    }

    EncryptedWallet(const EncryptedWallet&) = delete;
    EncryptedWallet& operator=(const EncryptedWallet&) = delete;

    void UnlockFor(UnlockScope scope)
    {
        wallet.Lock();
        BOOST_REQUIRE(wallet.Unlock(PASSPHRASE, scope));
        BOOST_REQUIRE(wallet.GetUnlockScope() == scope);
    }
};

} // namespace grc_test

#endif // GRIDCOIN_TEST_STAKING_TEST_PEERS_H
