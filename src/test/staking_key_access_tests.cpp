// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/mnemonics.h"
#include "init.h"
#include "keystore.h"
#include "random.h"
#include "script/sign.h"
#include "script/standard.h"
#include "test/staking_test_peers.h"
#include "wallet/ismine.h"
#include "wallet/staking_signer.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

using grc_test::EncryptedWallet;
using grc_test::SwapMainWallet;

// Nothing but a friend can make either token, and a grant cannot be copied or
// moved off the scope, and so the thread, that made it. Empty braces must not
// reach the private constructor through aggregate initialisation either.
static_assert(!std::is_default_constructible_v<StakingKeyAccess>);
static_assert(!std::is_aggregate_v<StakingKeyAccess>);
static_assert(std::is_copy_constructible_v<StakingKeyAccess>);
static_assert(!std::is_constructible_v<ElevationGrant, const CCryptoKeyStore&>);
static_assert(!std::is_copy_constructible_v<ElevationGrant>);
static_assert(!std::is_move_constructible_v<ElevationGrant>);
static_assert(!std::is_copy_assignable_v<ElevationGrant>);
static_assert(!std::is_move_assignable_v<ElevationGrant>);

//! Exposes the protected keystore members the master key comparison needs.
struct KeyStoreProbe : public CCryptoKeyStore
{
    using CCryptoKeyStore::EncryptKeys;
    using CCryptoKeyStore::MatchesMasterKey;
    using CCryptoKeyStore::Unlock;
};

CMutableTransaction SpendOf(const CScript& script_pubkey)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(GetRandHash(), 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = CENT;
    mtx.vout[0].scriptPubKey = script_pubkey;

    return mtx;
}

} // namespace

BOOST_AUTO_TEST_SUITE(staking_key_access_tests)

BOOST_AUTO_TEST_CASE(general_getkey_refused_under_staking_only)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    CKey key;
    BOOST_CHECK(!fx.wallet.GetKey(fx.id, key));
    BOOST_CHECK(!key.IsValid());

    // The public half stays available: it is not secret.
    CPubKey pubkey;
    BOOST_CHECK(fx.wallet.GetPubKey(fx.id, pubkey));
    BOOST_CHECK(pubkey == fx.key.GetPubKey());
}

BOOST_AUTO_TEST_CASE(general_getkey_allowed_under_full)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::Full);

    CKey key;
    BOOST_REQUIRE(fx.wallet.GetKey(fx.id, key) && key.IsValid());
    BOOST_CHECK(key.GetPubKey() == fx.key.GetPubKey());
}

BOOST_AUTO_TEST_CASE(getkey_allowed_unencrypted)
{
    CWallet wallet;
    SwapMainWallet swap(&wallet);

    CKey original;
    original.MakeNewKey(true);
    {
        LOCK(wallet.cs_wallet);
        BOOST_REQUIRE(wallet.AddKey(original));
    }
    BOOST_REQUIRE(!wallet.IsCrypted());

    CKey key;
    BOOST_REQUIRE(wallet.GetKey(original.GetPubKey().GetID(), key));
    BOOST_CHECK(key.GetPubKey() == original.GetPubKey());

    // The staking accessor serves an unencrypted wallet too.
    CKey staking_key;
    BOOST_REQUIRE(wallet.GetKeyForStaking(original.GetPubKey().GetID(), staking_key,
                                        StakingKeyAccessTestPeer::Make()));
    BOOST_CHECK(staking_key.GetPubKey() == original.GetPubKey());
}

BOOST_AUTO_TEST_CASE(staking_access_allowed_under_staking_only)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    CKey key;
    BOOST_REQUIRE(fx.wallet.GetKeyForStaking(fx.id, key, StakingKeyAccessTestPeer::Make()) && key.IsValid());
    BOOST_CHECK(key.GetPubKey() == fx.key.GetPubKey());

    // Full permits it as well.
    fx.UnlockFor(UnlockScope::Full);
    CKey full_key;
    BOOST_REQUIRE(fx.wallet.GetKeyForStaking(fx.id, full_key, StakingKeyAccessTestPeer::Make()));
    BOOST_CHECK(full_key.GetPubKey() == fx.key.GetPubKey());

    // A key the wallet does not hold is not found.
    CKey stranger;
    stranger.MakeNewKey(true);
    CKey none;
    BOOST_CHECK(!fx.wallet.GetKeyForStaking(stranger.GetPubKey().GetID(), none, StakingKeyAccessTestPeer::Make()));
}

BOOST_AUTO_TEST_CASE(staking_access_refused_when_locked)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    fx.wallet.Lock();
    BOOST_REQUIRE(fx.wallet.IsLocked());

    CKey key;
    BOOST_CHECK(!fx.wallet.GetKeyForStaking(fx.id, key, StakingKeyAccessTestPeer::Make()));
    BOOST_CHECK(!key.IsValid());
}

BOOST_AUTO_TEST_CASE(grant_allows_getkey_on_this_thread_only)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    BOOST_CHECK(!ElevationGrant::IsActiveFor(&fx.wallet));

    {
        const ElevationGrant grant = ElevationGrantTestPeer::Make(fx.wallet);
        BOOST_CHECK(ElevationGrant::IsActiveFor(&fx.wallet));

        CKey key;
        BOOST_CHECK(fx.wallet.GetKey(fx.id, key) && key.IsValid());

        // Another thread, running while the grant is live here, gets nothing
        // from it. Boost.Test assertions are not thread-safe, so the thread only
        // records what it saw.
        bool other_thread_got_key{true};
        bool other_thread_saw_grant{true};

        std::thread other([&] {
            CKey other_key;
            other_thread_got_key = fx.wallet.GetKey(fx.id, other_key);
            other_thread_saw_grant = ElevationGrant::IsActiveFor(&fx.wallet);
        });
        other.join();

        BOOST_CHECK(!other_thread_got_key);
        BOOST_CHECK(!other_thread_saw_grant);
    }

    BOOST_CHECK(!ElevationGrant::IsActiveFor(&fx.wallet));

    CKey key;
    BOOST_CHECK(!fx.wallet.GetKey(fx.id, key));
}

BOOST_AUTO_TEST_CASE(grants_nest_and_name_one_store)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    CWallet other;

    // No grant names no store, including none at all.
    BOOST_CHECK(!ElevationGrant::IsActiveFor(nullptr));

    {
        const ElevationGrant outer = ElevationGrantTestPeer::Make(fx.wallet);
        BOOST_CHECK(ElevationGrant::IsActiveFor(&fx.wallet));
        BOOST_CHECK(!ElevationGrant::IsActiveFor(&other));
        BOOST_CHECK(!ElevationGrant::IsActiveFor(nullptr));

        {
            const ElevationGrant inner = ElevationGrantTestPeer::Make(fx.wallet);
            BOOST_CHECK(ElevationGrant::IsActiveFor(&fx.wallet));
        }

        // The inner grant restored the outer one rather than clearing it.
        BOOST_CHECK(ElevationGrant::IsActiveFor(&fx.wallet));

        {
            const ElevationGrant inner = ElevationGrantTestPeer::Make(other);
            BOOST_CHECK(ElevationGrant::IsActiveFor(&other));
            BOOST_CHECK(!ElevationGrant::IsActiveFor(&fx.wallet));

            CKey key;
            BOOST_CHECK(!fx.wallet.GetKey(fx.id, key));
        }

        BOOST_CHECK(ElevationGrant::IsActiveFor(&fx.wallet));
        BOOST_CHECK(!ElevationGrant::IsActiveFor(&other));
    }

    BOOST_CHECK(!ElevationGrant::IsActiveFor(&fx.wallet));
}

BOOST_AUTO_TEST_CASE(grants_ended_out_of_order_leave_none_active)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    CWallet other;

    // Held on the heap, so that they can end in either order.
    std::unique_ptr<ElevationGrant> outer = ElevationGrantTestPeer::MakeHeld(fx.wallet);
    std::unique_ptr<ElevationGrant> inner = ElevationGrantTestPeer::MakeHeld(other);
    BOOST_REQUIRE(ElevationGrant::IsActiveFor(&other));

    // The outer grant ends first. Every grant on the thread is withdrawn.
    outer.reset();
    BOOST_CHECK(!ElevationGrant::IsActiveFor(&fx.wallet));
    BOOST_CHECK(!ElevationGrant::IsActiveFor(&other));

    // The inner one ending must not bring back the outer one, which no longer
    // exists.
    inner.reset();
    BOOST_CHECK(!ElevationGrant::IsActiveFor(&fx.wallet));
    BOOST_CHECK(!ElevationGrant::IsActiveFor(&other));

    CKey key;
    BOOST_CHECK(!fx.wallet.GetKey(fx.id, key));

    // The thread is still usable: a grant taken afterwards works and ends cleanly.
    {
        const ElevationGrant grant = ElevationGrantTestPeer::Make(fx.wallet);
        BOOST_CHECK(ElevationGrant::IsActiveFor(&fx.wallet));
    }

    BOOST_CHECK(!ElevationGrant::IsActiveFor(&fx.wallet));
}

BOOST_AUTO_TEST_CASE(secret_decrypt_refused_under_staking_only)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::Full);

    CSeedPhraseData data;
    data.vchBlob.assign(GRC::Mnemonics::ENCIPHERED_LENGTH, 0x42);
    data.masterKeyID = fx.id;
    data.nBirthday = GRC::Mnemonics::BIRTHDAY_EPOCH;
    BOOST_REQUIRE(fx.wallet.SetSeedPhraseData(data));
    {
        LOCK(fx.wallet.cs_wallet);
        BOOST_REQUIRE(fx.wallet.SeedPhraseBlobCrypted());
    }

    // Full decrypts it, so the refusal below is the scope and not the record.
    CKeyingMaterial blob;
    BOOST_REQUIRE(fx.wallet.GetSeedPhraseBlob(blob));
    BOOST_CHECK(std::equal(blob.begin(), blob.end(), data.vchBlob.begin(), data.vchBlob.end()));

    fx.UnlockFor(UnlockScope::StakingOnly);
    CKeyingMaterial refused;
    BOOST_CHECK(!fx.wallet.GetSeedPhraseBlob(refused));
    BOOST_CHECK(refused.empty());

    // A grant on this thread lifts the refusal.
    const ElevationGrant grant = ElevationGrantTestPeer::Make(fx.wallet);
    CKeyingMaterial granted;
    BOOST_CHECK(fx.wallet.GetSeedPhraseBlob(granted));
    BOOST_CHECK(granted == blob);
}

BOOST_AUTO_TEST_CASE(secret_encrypt_refused_under_staking_only)
{
    EncryptedWallet fx;

    CSeedPhraseData data;
    data.vchBlob.assign(GRC::Mnemonics::ENCIPHERED_LENGTH, 0x24);
    data.masterKeyID = fx.id;
    data.nBirthday = GRC::Mnemonics::BIRTHDAY_EPOCH;

    // Storing the blob encrypts it under the master key, which writes key
    // material. A staking-only unlock withholds that, and the wallet is left
    // without a record.
    fx.UnlockFor(UnlockScope::StakingOnly);
    BOOST_CHECK(!fx.wallet.SetSeedPhraseData(data));
    {
        LOCK(fx.wallet.cs_wallet);
        BOOST_CHECK(!fx.wallet.HasSeedPhrase());
    }

    // A grant on this thread lifts the refusal.
    {
        const ElevationGrant grant = ElevationGrantTestPeer::Make(fx.wallet);
        BOOST_CHECK(fx.wallet.SetSeedPhraseData(data));

        LOCK(fx.wallet.cs_wallet);
        BOOST_CHECK(fx.wallet.HasSeedPhrase());
        BOOST_CHECK(fx.wallet.SeedPhraseBlobCrypted());
    }

    // So does a full unlock. A different blob, so that reading it back shows
    // this write landed.
    CSeedPhraseData replacement = data;
    replacement.vchBlob.assign(GRC::Mnemonics::ENCIPHERED_LENGTH, 0x25);

    fx.UnlockFor(UnlockScope::Full);
    BOOST_CHECK(fx.wallet.SetSeedPhraseData(replacement));

    CKeyingMaterial blob;
    BOOST_REQUIRE(fx.wallet.GetSeedPhraseBlob(blob));
    BOOST_CHECK(std::equal(blob.begin(), blob.end(), replacement.vchBlob.begin(), replacement.vchBlob.end()));
}

BOOST_AUTO_TEST_CASE(add_key_refused_under_staking_only)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    CKey fresh;
    fresh.MakeNewKey(true);

    {
        LOCK(fx.wallet.cs_wallet);
        BOOST_CHECK(!fx.wallet.AddKeyPubKey(fresh, fresh.GetPubKey()));
        BOOST_CHECK(!fx.wallet.HaveKey(fresh.GetPubKey().GetID()));
    }

    // A grant on this thread lifts the refusal, and only while it lasts.
    CKey granted;
    granted.MakeNewKey(true);

    {
        const ElevationGrant grant = ElevationGrantTestPeer::Make(fx.wallet);
        LOCK(fx.wallet.cs_wallet);
        BOOST_CHECK(fx.wallet.AddKeyPubKey(granted, granted.GetPubKey()));
        BOOST_CHECK(fx.wallet.HaveKey(granted.GetPubKey().GetID()));
    }

    {
        LOCK(fx.wallet.cs_wallet);
        BOOST_CHECK(!fx.wallet.AddKeyPubKey(fresh, fresh.GetPubKey()));
        BOOST_CHECK(!fx.wallet.HaveKey(fresh.GetPubKey().GetID()));
    }

    fx.UnlockFor(UnlockScope::Full);

    {
        LOCK(fx.wallet.cs_wallet);
        BOOST_CHECK(fx.wallet.AddKeyPubKey(fresh, fresh.GetPubKey()));
        BOOST_CHECK(fx.wallet.HaveKey(fresh.GetPubKey().GetID()));
    }
}

BOOST_AUTO_TEST_CASE(staking_signing_provider_signs_under_staking_only)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    // Pay to the key's hash, to the key itself (the coinstake output form), and
    // to a script hash wrapping the first, which reaches GetCScript.
    CScript to_key_hash;
    to_key_hash.SetDestination(fx.id);

    CScript to_key;
    to_key << fx.key.GetPubKey() << OP_CHECKSIG;

    BOOST_REQUIRE(fx.wallet.AddCScript(to_key_hash));
    CScript to_script_hash;
    to_script_hash.SetDestination(CScriptID(to_key_hash));

    const StakingSigningProvider provider(fx.wallet, StakingKeyAccessTestPeer::Make());

    for (const CScript& script_pubkey : {to_key_hash, to_key, to_script_hash}) {
        CMutableTransaction staking_spend = SpendOf(script_pubkey);
        BOOST_CHECK(SignSignature(provider, script_pubkey, staking_spend, 0));

        CMutableTransaction general_spend = SpendOf(script_pubkey);
        BOOST_CHECK(!SignSignature(fx.wallet, script_pubkey, general_spend, 0));
    }

    // Locked, the provider has nothing to sign with.
    fx.wallet.Lock();
    CMutableTransaction locked_spend = SpendOf(to_key_hash);
    BOOST_CHECK(!SignSignature(provider, to_key_hash, locked_spend, 0));
}

BOOST_AUTO_TEST_CASE(staking_signing_provider_answers_key_and_script_lookups)
{
    EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    CScript to_key_hash;
    to_key_hash.SetDestination(fx.id);
    BOOST_REQUIRE(fx.wallet.AddCScript(to_key_hash));

    const StakingSigningProvider provider(fx.wallet, StakingKeyAccessTestPeer::Make());

    // Which keys and scripts the store holds is not secret, so the provider
    // answers as the store does, and IsMine through it agrees with the store.
    BOOST_CHECK(provider.HaveKey(fx.id));
    BOOST_CHECK(provider.HaveCScript(CScriptID(to_key_hash)));
    BOOST_CHECK(IsMine(provider, to_key_hash) == ISMINE_SPENDABLE);

    CKey stranger;
    stranger.MakeNewKey(true);
    CScript to_stranger;
    to_stranger.SetDestination(stranger.GetPubKey().GetID());

    BOOST_CHECK(!provider.HaveKey(stranger.GetPubKey().GetID()));
    BOOST_CHECK(!provider.HaveCScript(CScriptID(to_stranger)));
}

BOOST_AUTO_TEST_CASE(master_key_comparison)
{
    KeyStoreProbe store;

    CKey key;
    key.MakeNewKey(true);
    BOOST_REQUIRE(store.AddKey(key));

    // Unencrypted, there is no master key to match, empty or otherwise.
    BOOST_CHECK(!store.MatchesMasterKey(CKeyingMaterial()));

    CKeyingMaterial master(WALLET_CRYPTO_KEY_SIZE);
    GetStrongRandBytes(master);
    BOOST_REQUIRE(store.EncryptKeys(master));

    // Locked: false even for the right key.
    BOOST_CHECK(!store.MatchesMasterKey(master));

    BOOST_REQUIRE(store.Unlock(master, UnlockScope::StakingOnly));
    BOOST_CHECK(store.MatchesMasterKey(master));

    CKeyingMaterial last_byte_differs = master;
    last_byte_differs.back() ^= 0x01;
    BOOST_CHECK(!store.MatchesMasterKey(last_byte_differs));

    CKeyingMaterial first_byte_differs = master;
    first_byte_differs.front() ^= 0x80;
    BOOST_CHECK(!store.MatchesMasterKey(first_byte_differs));

    const CKeyingMaterial prefix(master.begin(), master.end() - 1);
    BOOST_CHECK(!store.MatchesMasterKey(prefix));
    BOOST_CHECK(!store.MatchesMasterKey(CKeyingMaterial()));

    CKeyingMaterial longer = master;
    longer.push_back(0x00);
    BOOST_CHECK(!store.MatchesMasterKey(longer));

    BOOST_REQUIRE(store.Lock());
    BOOST_CHECK(!store.MatchesMasterKey(master));
}

BOOST_AUTO_TEST_SUITE_END()
