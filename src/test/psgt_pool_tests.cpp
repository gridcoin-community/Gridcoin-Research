// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <keystore.h>
#include <main.h>
#include <node/psgt_pool.h>
#include <policy/fees.h>
#include <primitives/transaction.h>
#include <psgt.h>
#include <test/psgt_test_helpers.h>
#include <test/test_gridcoin.h>
#include <txmempool.h>

#include <optional>
#include <vector>

using namespace psgt_test;

BOOST_AUTO_TEST_SUITE(psgt_pool_tests)

//! Register a transaction with the global mempool so the pool's funding-output
//! check (chain-or-mempool existence, mapNextTx spentness) can see it. The
//! mock txdb is empty in unit tests, so mempool residence is how funding
//! transactions are made visible.
static void AddToMempool(const CTransaction& tx)
{
    LOCK(mempool.cs);
    mempool.addUnchecked(tx.GetHash(),
                         CTxMemPoolEntry(tx, 0, 1700000000, 1,
                                         ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)));
}

//! A mempool-funded 2-of-3 P2SH multisig spend: the shape the pool accepts.
//! The unsigned PSGT is kept pristine; Signed() returns signed copies so one
//! fixture can produce competing revisions.
struct FundedMultisig
{
    CKey k1, k2, k3;
    CScript redeem;
    CTransaction funding;
    PartiallySignedTransaction unsigned_psgt;

    //! 0.01 GRC fee by default: above the v2 relay minimum (0.001 GRC/kB
    //! for sub-kB transactions), below the 100x absurd-fee ceiling.
    explicit FundedMultisig(CAmount fee = 1000000)
        : k1(MakeKey()), k2(MakeKey()), k3(MakeKey())
        , redeem(MultisigScript(2, {k1.GetPubKey(), k2.GetPubKey(), k3.GetPubKey()}))
        , funding(MakePrevTxScript(P2SH(CScriptID(redeem)), 10 * COIN))
    {
        AddToMempool(funding);

        CMutableTransaction mtx;
        mtx.nVersion = 2;
        mtx.nTime = 1700002000;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(funding.GetHash(), 0);
        mtx.vout.push_back(CTxOut(10 * COIN - fee, P2PKH(MakeKey().GetPubKey().GetID())));

        unsigned_psgt = PartiallySignedTransaction(mtx);
        unsigned_psgt.inputs[0].non_witness_utxo = funding;
        unsigned_psgt.inputs[0].redeem_script = redeem;
    }

    PartiallySignedTransaction Signed(const std::vector<CKey>& keys) const
    {
        PartiallySignedTransaction psgt = unsigned_psgt;
        for (const CKey& key : keys) {
            CBasicKeyStore keystore;
            keystore.AddKey(key);
            keystore.AddCScript(redeem);
            BOOST_REQUIRE(SignPSGTInput(keystore, psgt, 0));
        }
        return psgt;
    }

    //! A spend of the same funding output to a different destination/amount
    //! (a superseding transaction the initiator might submit).
    PartiallySignedTransaction Superseding(CAmount fee, const std::vector<CKey>& keys) const
    {
        CMutableTransaction mtx = unsigned_psgt.tx;
        mtx.vout[0] = CTxOut(10 * COIN - fee, P2PKH(MakeKey().GetPubKey().GetID()));

        PartiallySignedTransaction psgt(mtx);
        psgt.inputs[0].non_witness_utxo = funding;
        psgt.inputs[0].redeem_script = redeem;

        PartiallySignedTransaction result = psgt;
        for (const CKey& key : keys) {
            CBasicKeyStore keystore;
            keystore.AddKey(key);
            keystore.AddCScript(redeem);
            BOOST_REQUIRE(SignPSGTInput(keystore, result, 0));
        }
        return result;
    }
};

static PSGTPoolReject Validate(const PartiallySignedTransaction& psgt,
                               PSGTPoolEntry& entry, std::string& error)
{
    LOCK(cs_main);
    return ValidatePSGTForPool(SerializePSGT(psgt), 1700003000, entry, error);
}

//! A synthetic entry with a unique image, for container-semantics tests that
//! do not need the cryptographic pipeline (pool-full, expiry).
static PSGTPoolEntry SyntheticEntry(int64_t time_received)
{
    PSGTPoolEntry entry;
    entry.revision_hash = InsecureRand256();
    entry.image = CScriptID(uint160(InsecureRandBytes(20)));
    entry.tx_hash = InsecureRand256();
    entry.time_received = time_received;
    entry.valid_keys_per_input.push_back({CKeyID(uint160(InsecureRandBytes(20)))});
    entry.valid_sigs = 1;
    entry.sigs_required = 2;
    entry.sigs_total = 3;
    return entry;
}

// ---------------------------------------------------------------------------
// ValidatePSGTForPool
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(validate_accepts_partially_signed_multisig)
{
    mempool.clear();
    FundedMultisig fixture;

    PSGTPoolEntry entry;
    std::string error;
    BOOST_REQUIRE_MESSAGE(
        Validate(fixture.Signed({fixture.k1}), entry, error) == PSGTPoolReject::NONE, error);

    BOOST_CHECK(entry.image == CScriptID(fixture.redeem));
    BOOST_CHECK(entry.tx_hash == CTransaction(fixture.unsigned_psgt.tx).GetHash());
    BOOST_CHECK_EQUAL(entry.valid_sigs, 1);
    BOOST_CHECK_EQUAL(entry.sigs_required, 2);
    BOOST_CHECK_EQUAL(entry.sigs_total, 3);
    BOOST_CHECK_EQUAL(entry.fee, 1000000);
    BOOST_CHECK(!entry.serialized.empty());
    BOOST_CHECK(entry.revision_hash == Hash(entry.serialized));
}

BOOST_AUTO_TEST_CASE(validate_reject_matrix)
{
    mempool.clear();
    FundedMultisig fixture;

    PSGTPoolEntry entry;
    std::string error;

    // No signature at all: the anti-spam floor.
    BOOST_CHECK(Validate(fixture.unsigned_psgt, entry, error) == PSGTPoolReject::NO_VALID_SIG);

    // A corrupted signature poisons the PSGT.
    {
        PartiallySignedTransaction corrupted = fixture.Signed({fixture.k1});
        auto& sig = corrupted.inputs[0].partial_sigs.begin()->second;
        sig[sig.size() / 2] ^= 0x01;
        BOOST_CHECK(Validate(corrupted, entry, error) == PSGTPoolReject::INVALID_SIG);
    }

    // Complete (2-of-3 with 2 valid sigs): belongs in a transaction, not the pool.
    BOOST_CHECK(Validate(fixture.Signed({fixture.k1, fixture.k2}), entry, error)
                == PSGTPoolReject::COMPLETE);

    // Not multisig: a plain P2PKH spend has no image.
    {
        CKey plain = MakeKey();
        CTransaction prev = MakePrevTx(plain, 10 * COIN);
        AddToMempool(prev);
        PartiallySignedTransaction p2pkh = MakeSpendPSGT(prev, 10 * COIN - 1000000);
        BOOST_CHECK(Validate(p2pkh, entry, error) == PSGTPoolReject::STRUCTURAL);
    }

    // Unknown funding transaction (not in mempool, mock txdb is empty).
    {
        FundedMultisig unknown;
        // mempool.remove() takes a const ref and locks mempool.cs internally.
        mempool.remove(unknown.funding);
        const PSGTPoolReject r = Validate(unknown.Signed({unknown.k1}), entry, error);
        BOOST_CHECK_MESSAGE(r == PSGTPoolReject::UTXO_MISSING,
                            PSGTPoolRejectToString(r) + ": " + error);
    }

    // Funding output already spent by a mempool transaction.
    {
        FundedMultisig spent;
        CMutableTransaction spender;
        spender.nVersion = 2;
        spender.nTime = 1700002500;
        spender.vin.resize(1);
        spender.vin[0].prevout = COutPoint(spent.funding.GetHash(), 0);
        spender.vin[0].scriptSig = CScript() << 0;
        spender.vout.push_back(CTxOut(10 * COIN - 1000000, P2PKH(MakeKey().GetPubKey().GetID())));
        AddToMempool(CTransaction(spender));
        BOOST_CHECK(Validate(spent.Signed({spent.k1}), entry, error)
                    == PSGTPoolReject::UTXO_SPENT);
    }

    // Fee bounds.
    {
        FundedMultisig cheap(1000); // 0.00001 GRC: below the v2 relay minimum
        BOOST_CHECK(Validate(cheap.Signed({cheap.k1}), entry, error)
                    == PSGTPoolReject::FEE_TOO_LOW);

        FundedMultisig lavish(1 * COIN); // 100x the ~0.001 GRC minimum and then some
        BOOST_CHECK(Validate(lavish.Signed({lavish.k1}), entry, error)
                    == PSGTPoolReject::FEE_ABSURD);
    }

    // Oversize and malformed wire bytes.
    {
        LOCK(cs_main);
        std::vector<unsigned char> huge(MAX_PSGT_WIRE_SIZE + 1, 0x00);
        BOOST_CHECK(ValidatePSGTForPool(huge, 1700003000, entry, error)
                    == PSGTPoolReject::TOO_LARGE);

        std::vector<unsigned char> garbage = {0xde, 0xad, 0xbe, 0xef};
        BOOST_CHECK(ValidatePSGTForPool(garbage, 1700003000, entry, error)
                    == PSGTPoolReject::MALFORMED);
    }
}

// ---------------------------------------------------------------------------
// PSGTPool: add / replace / initiator rules
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pool_add_and_signature_progress)
{
    mempool.clear();
    FundedMultisig fixture;
    PSGTPool pool;
    std::string error;
    std::string reject;

    PSGTPoolEntry one_sig;
    BOOST_REQUIRE(Validate(fixture.Signed({fixture.k1}), one_sig, error) == PSGTPoolReject::NONE);
    const uint256 first_revision = one_sig.revision_hash;

    BOOST_CHECK(pool.Add(PSGTPoolEntry(one_sig), reject) == PSGTPoolAddResult::ACCEPTED_NEW);
    BOOST_CHECK_EQUAL(pool.Size(), 1u);
    BOOST_CHECK(pool.Get(one_sig.image).has_value());
    BOOST_CHECK(pool.GetByRevision(first_revision).has_value());
    BOOST_CHECK(pool.GetByTxHash(one_sig.tx_hash).has_value());
    BOOST_CHECK(pool.HaveRevision(first_revision));

    // The first signer is recorded as the initiator.
    const std::set<CKeyID> expected_initiator{fixture.k1.GetPubKey().GetID()};
    BOOST_CHECK(pool.Get(one_sig.image)->initiator_keys == expected_initiator);

    // Same revision again: duplicate gossip, no-op.
    BOOST_CHECK(pool.Add(PSGTPoolEntry(one_sig), reject) == PSGTPoolAddResult::DUPLICATE);

    // A co-signer's revision with k1+k2: strict superset, replaces.
    // (2-of-3 with both sigs is COMPLETE for validation, so build the entry
    // for this container test by amending the validated one-sig entry the way
    // the signing path will: re-validate a k1+k3 pair is still incomplete...
    // it is not -- any two of three completes. Amend manually instead.)
    PSGTPoolEntry two_sigs = one_sig;
    {
        PartiallySignedTransaction psgt = fixture.Signed({fixture.k1, fixture.k2});
        two_sigs.psgt = psgt;
        two_sigs.serialized = SerializePSGT(psgt);
        two_sigs.revision_hash = Hash(two_sigs.serialized);
        two_sigs.valid_keys_per_input[0].insert(fixture.k2.GetPubKey().GetID());
        two_sigs.valid_sigs = 2;
    }

    BOOST_CHECK(pool.Add(PSGTPoolEntry(two_sigs), reject)
                == PSGTPoolAddResult::ACCEPTED_REPLACEMENT);
    BOOST_CHECK_EQUAL(pool.Size(), 1u);
    BOOST_CHECK_EQUAL(pool.Get(one_sig.image)->valid_sigs, 2);

    // Initiator survives the replacement.
    BOOST_CHECK(pool.Get(one_sig.image)->initiator_keys == expected_initiator);

    // The replaced revision is remembered: not listed, but "already have",
    // and re-adding it is a no-op rather than a downgrade fight.
    BOOST_CHECK(!pool.GetByRevision(first_revision).has_value());
    BOOST_CHECK(pool.HaveRevision(first_revision));
    BOOST_CHECK(pool.Add(PSGTPoolEntry(one_sig), reject) == PSGTPoolAddResult::DUPLICATE);

    // A k2-only revision of the same tx is not a superset: rejected.
    PSGTPoolEntry k2_only;
    BOOST_REQUIRE(Validate(fixture.Signed({fixture.k2}), k2_only, error) == PSGTPoolReject::NONE);
    BOOST_CHECK(pool.Add(PSGTPoolEntry(k2_only), reject)
                == PSGTPoolAddResult::REJECTED_NOT_BETTER);
}

BOOST_AUTO_TEST_CASE(pool_initiator_supersede)
{
    mempool.clear();
    FundedMultisig fixture;
    PSGTPool pool;
    std::string error;
    std::string reject;

    PSGTPoolEntry original;
    BOOST_REQUIRE(Validate(fixture.Signed({fixture.k1}), original, error) == PSGTPoolReject::NONE);
    BOOST_REQUIRE(pool.Add(PSGTPoolEntry(original), reject) == PSGTPoolAddResult::ACCEPTED_NEW);

    // A different unsigned tx signed only by a co-signer (k2): not the
    // initiator, rejected even though the signature is valid.
    PSGTPoolEntry hijack;
    BOOST_REQUIRE(Validate(fixture.Superseding(3000000, {fixture.k2}), hijack, error)
                  == PSGTPoolReject::NONE);
    BOOST_CHECK(pool.Add(PSGTPoolEntry(hijack), reject)
                == PSGTPoolAddResult::REJECTED_NOT_INITIATOR);

    // The initiator (k1) revises destination/fee: accepted although it
    // carries no more signatures than the pooled entry, and the initiator
    // set is carried forward.
    PSGTPoolEntry revised;
    BOOST_REQUIRE(Validate(fixture.Superseding(2000000, {fixture.k1}), revised, error)
                  == PSGTPoolReject::NONE);
    BOOST_CHECK(pool.Add(PSGTPoolEntry(revised), reject)
                == PSGTPoolAddResult::ACCEPTED_REPLACEMENT);

    const auto pooled = pool.Get(original.image);
    BOOST_REQUIRE(pooled.has_value());
    BOOST_CHECK(pooled->tx_hash == revised.tx_hash);
    BOOST_CHECK(pooled->initiator_keys
                == std::set<CKeyID>{fixture.k1.GetPubKey().GetID()});
}

// ---------------------------------------------------------------------------
// PSGTPool: capacity, expiry, eviction
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pool_full_rejects_new_images_but_not_replacements)
{
    PSGTPool pool;
    std::string reject;

    PSGTPoolEntry first = SyntheticEntry(1700003000);
    BOOST_REQUIRE(pool.Add(PSGTPoolEntry(first), reject) == PSGTPoolAddResult::ACCEPTED_NEW);

    for (size_t i = 1; i < PSGTPool::MAX_ENTRIES; ++i) {
        BOOST_REQUIRE(pool.Add(SyntheticEntry(1700003000), reject)
                      == PSGTPoolAddResult::ACCEPTED_NEW);
    }
    BOOST_CHECK_EQUAL(pool.Size(), PSGTPool::MAX_ENTRIES);

    // A new image is rejected -- never evict someone's signing session.
    BOOST_CHECK(pool.Add(SyntheticEntry(1700003000), reject)
                == PSGTPoolAddResult::REJECTED_POOL_FULL);

    // A signature-progress replacement of an existing image is not growth
    // and is still accepted at capacity.
    PSGTPoolEntry progress = first;
    progress.revision_hash = InsecureRand256();
    progress.valid_keys_per_input[0].insert(CKeyID(uint160(InsecureRandBytes(20))));
    progress.valid_sigs = 2;
    BOOST_CHECK(pool.Add(std::move(progress), reject)
                == PSGTPoolAddResult::ACCEPTED_REPLACEMENT);
    BOOST_CHECK_EQUAL(pool.Size(), PSGTPool::MAX_ENTRIES);
}

BOOST_AUTO_TEST_CASE(pool_expiry)
{
    PSGTPool pool;
    std::string reject;

    const int64_t now = 1700003000;
    BOOST_REQUIRE(pool.Add(SyntheticEntry(now - PSGTPool::EXPIRY_SECONDS - 1), reject)
                  == PSGTPoolAddResult::ACCEPTED_NEW);
    BOOST_REQUIRE(pool.Add(SyntheticEntry(now - 60), reject)
                  == PSGTPoolAddResult::ACCEPTED_NEW);

    BOOST_CHECK_EQUAL(pool.EraseExpired(now), 1u);
    BOOST_CHECK_EQUAL(pool.Size(), 1u);
}

BOOST_AUTO_TEST_CASE(pool_utxo_conflict_eviction)
{
    mempool.clear();
    FundedMultisig fixture;
    PSGTPool pool;
    std::string error;
    std::string reject;

    // Capture notifications to check the eviction reason surfaces.
    std::vector<std::pair<PSGTPoolChangeType, std::optional<PSGTRemovalReason>>> events;
    pool.m_notify_hook = [&](const PSGTPoolEntry&, PSGTPoolChangeType change,
                             std::optional<PSGTRemovalReason> reason) {
        events.emplace_back(change, reason);
    };

    PSGTPoolEntry entry;
    BOOST_REQUIRE(Validate(fixture.Signed({fixture.k1}), entry, error) == PSGTPoolReject::NONE);
    const CScriptID image = entry.image;
    const uint256 revision = entry.revision_hash;
    BOOST_REQUIRE(pool.Add(std::move(entry), reject) == PSGTPoolAddResult::ACCEPTED_NEW);

    // A transaction spending the PSGT's input enters the mempool (in the
    // common case, the completed PSGT itself): evict immediately.
    CMutableTransaction spender;
    spender.nVersion = 2;
    spender.nTime = 1700002500;
    spender.vin.resize(1);
    spender.vin[0].prevout = COutPoint(fixture.funding.GetHash(), 0);
    spender.vin[0].scriptSig = CScript() << 0;
    spender.vout.push_back(CTxOut(10 * COIN - 1000000, P2PKH(MakeKey().GetPubKey().GetID())));

    pool.TransactionAddedToMempool(MakeTransactionRef(CTransaction(spender)));

    BOOST_CHECK_EQUAL(pool.Size(), 0u);
    BOOST_CHECK(!pool.Get(image).has_value());
    BOOST_CHECK(pool.HaveRevision(revision)); // recently-removed damping

    BOOST_REQUIRE_EQUAL(events.size(), 2u); // ADDED + REMOVED
    BOOST_CHECK(events[1].first == PSGTPoolChangeType::REMOVED);
    BOOST_REQUIRE(events[1].second.has_value());
    BOOST_CHECK(*events[1].second == PSGTRemovalReason::CONFLICT_MEMPOOL);

    // Block-connect path: fresh entry, same spender confirmed in a block.
    PSGTPoolEntry again;
    BOOST_REQUIRE(Validate(fixture.Signed({fixture.k2}), again, error) == PSGTPoolReject::NONE);
    BOOST_REQUIRE(pool.Add(std::move(again), reject) == PSGTPoolAddResult::ACCEPTED_NEW);

    CBlock block;
    block.vtx.push_back(CTransaction(spender));
    pool.BlockConnected(block, 100);

    BOOST_CHECK_EQUAL(pool.Size(), 0u);
    BOOST_REQUIRE_EQUAL(events.size(), 4u);
    BOOST_REQUIRE(events[3].second.has_value());
    BOOST_CHECK(*events[3].second == PSGTRemovalReason::CONFLICT_BLOCK);

    // A transaction spending unrelated outputs is a no-op.
    PSGTPoolEntry third;
    BOOST_REQUIRE(Validate(fixture.Signed({fixture.k3}), third, error) == PSGTPoolReject::NONE);
    BOOST_REQUIRE(pool.Add(std::move(third), reject) == PSGTPoolAddResult::ACCEPTED_NEW);

    CMutableTransaction unrelated = spender;
    unrelated.vin[0].prevout = COutPoint(InsecureRand256(), 0);
    pool.TransactionAddedToMempool(MakeTransactionRef(CTransaction(unrelated)));
    BOOST_CHECK_EQUAL(pool.Size(), 1u);
}

BOOST_AUTO_TEST_CASE(pool_local_remove)
{
    mempool.clear();
    FundedMultisig fixture;
    PSGTPool pool;
    std::string error;
    std::string reject;

    PSGTPoolEntry entry;
    BOOST_REQUIRE(Validate(fixture.Signed({fixture.k1}), entry, error) == PSGTPoolReject::NONE);
    const CScriptID image = entry.image;
    BOOST_REQUIRE(pool.Add(std::move(entry), reject) == PSGTPoolAddResult::ACCEPTED_NEW);

    BOOST_CHECK(pool.Remove(image, PSGTRemovalReason::LOCAL_REMOVE));
    BOOST_CHECK_EQUAL(pool.Size(), 0u);
    BOOST_CHECK(!pool.Remove(image, PSGTRemovalReason::LOCAL_REMOVE));
}

BOOST_AUTO_TEST_SUITE_END()
