// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "gridcoin/protocol.h"
#include "primitives/transaction.h"

#include <boost/test/unit_test.hpp>

#include "test/state_guard.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Tests are single-threaded and drive the ProtocolRegistry contract
// handler directly. The handler is EXCLUSIVE_LOCKS_REQUIRED(cs_main);
// suppress the analyzer for this file rather than take a lock the
// tests do not need.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

// anonymous namespace
namespace {
void AddProtocolEntry(const uint32_t& payload_version, const std::string& key, const std::string& value,
                      const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::ProtocolRegistry& registry = GRC::GetProtocolRegistry();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CMutableTransaction dummy_tx;
    CBlockIndex dummy_index = CBlockIndex {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

    if (payload_version < 2) {
        contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
                    uint32_t {2}, // Contract version (pre v13)
                    GRC::ContractAction::ADD,
                    key,
                    value);
    } else {
        contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
                    uint32_t {3}, // Contract version (post v13)
                    GRC::ContractAction::ADD,
                    payload_version, // Protocol payload version (post v13)
                    key,
                    value,
                    GRC::ProtocolEntryStatus::ACTIVE);
    }

    dummy_tx.vContracts.push_back(contract);

    CTransaction ctx_tx(dummy_tx);
    registry.Add({contract, ctx_tx, &dummy_index});
}

void DeleteProtocolEntry(const uint32_t& payload_version, const std::string& key, const std::string& value,
                      const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::ProtocolRegistry& registry = GRC::GetProtocolRegistry();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CMutableTransaction dummy_tx;
    CBlockIndex dummy_index = CBlockIndex {};
    dummy_index.nHeight = height;
    dummy_tx.nTime = time;
    dummy_index.nTime = time;

    GRC::Contract contract;

    if (payload_version < 2) {
        contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
                    uint32_t {2}, // Contract version (pre v13)
                    GRC::ContractAction::REMOVE,
                    key,
                    value);
    } else {
        contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
                    uint32_t {3}, // Contract version (post v13)
                    GRC::ContractAction::REMOVE,
                    payload_version, // Protocol payload version (post v13)
                    key,
                    value,
                    GRC::ProtocolEntryStatus::DELETED);
    }

    dummy_tx.vContracts.push_back(contract);

    CTransaction ctx_tx(dummy_tx);
    registry.Add({contract, ctx_tx, &dummy_index});
}


//! A protocol entry contract in the post-V13 form (contract v3, payload v2).
GRC::Contract MakeProtocolContract(const GRC::ContractAction action, const std::string& key, const std::string& value)
{
    return GRC::MakeContract<GRC::ProtocolEntryPayload>(
                uint32_t {3},
                action,
                uint32_t {2},
                key,
                value,
                action == GRC::ContractAction::ADD ? GRC::ProtocolEntryStatus::ACTIVE : GRC::ProtocolEntryStatus::DELETED);
}

//! The registry's verdict on a contract that would be mined at \p height, through the mempool path (which judges at
//! nBestHeight + 1) and through the block path.
struct Verdict
{
    bool mempool = false;
    int mempool_dos = 0;
    bool block = false;
    int block_dos = 0;
};

Verdict JudgeAt(const GRC::Contract& contract, const int height)
{
    GRC::ProtocolRegistry& registry = GRC::GetProtocolRegistry();

    CMutableTransaction mtx;
    mtx.vContracts.push_back(contract);
    const CTransaction tx(mtx);

    Verdict verdict;

    nBestHeight = height - 1;
    verdict.mempool = registry.Validate(contract, tx, verdict.mempool_dos);

    CBlockIndex index {};
    index.nHeight = height;
    verdict.block = registry.BlockValidate({contract, tx, &index}, verdict.block_dos);

    return verdict;
}

} // anonymous namespace

// Symmetric reset: the cases reset the registry on entry only.
BOOST_AUTO_TEST_SUITE(protocol_tests, *boost::unit_test::fixture<grc_test::RegistryResetFor<GRC::ContractType::PROTOCOL>>())

// Note for these tests we are going to mix payload version 1 and 2 entries to make
// sure they act equivalently.

BOOST_AUTO_TEST_CASE(protocol_WrittenCacheShouldBeReadable)
{
    AddProtocolEntry(1, "key", "hello", 1, 123456789, true);
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key").value == "hello");

    AddProtocolEntry(2, "key", "no hello", 2, 123456790, false);
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key").value == "no hello");
}

BOOST_AUTO_TEST_CASE(protocol_ClearCacheShouldClearEntireSection)
{
    AddProtocolEntry(1, "key1", "hello", 1, 123456789, true);
    AddProtocolEntry(2, "key2", "no hello", 2, 123456790, false);

    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key1").value == "hello");
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key2").value == "no hello");

    GRC::GetProtocolRegistry().Reset();

    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key1").value.empty() == true);
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key2").value.empty() == true);

}

BOOST_AUTO_TEST_CASE(protocol_DeletingEntriesShouldSuppressActiveKeyValues)
{
    AddProtocolEntry(1, "key1", "hello", 1, 123456789, true);
    AddProtocolEntry(2, "key2", "no hello", 2, 123456790, false);

    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key1").value == "hello");
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key2").value == "no hello");

    DeleteProtocolEntry(1, "key1", "hello", 1, 123456791, false);
    DeleteProtocolEntry(2, "key2", "no hello", 2, 123456792, false);

    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key1").value.empty() == true);
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key2").value.empty() == true);
}

BOOST_AUTO_TEST_CASE(protocol_DeletingEntryShouldSuppressReversionShouldRestore)
{
    AddProtocolEntry(1, "key1", "foo", 1, 123456789, true);
    AddProtocolEntry(2, "key2", "fi", 2, 123456790, false);

    // Delete the protocol entry manually to retain the ctx.

    CMutableTransaction dummy_tx;
    CBlockIndex dummy_index {};
    dummy_index.nHeight = 123456791;
    dummy_tx.nTime = 123456791;
    dummy_index.nTime = 123456791;

    GRC::Contract contract;

    contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
                uint32_t {3}, // Contract version (post v13)
                GRC::ContractAction::REMOVE,
                uint32_t {2}, // Protocol payload version (post v13)
                "key2",
                "fi", // The value is actually irrelevant here.
                GRC::ProtocolEntryStatus::DELETED);

    CTransaction ctx_tx(dummy_tx);
    GRC::ContractContext ctx(contract, ctx_tx, &dummy_index);

    GRC::GetProtocolRegistry().Add(ctx);

    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key2").value.empty() == true);

    GRC::ProtocolEntryOption active_entry = GRC::GetProtocolRegistry().TryActive("key2");

    BOOST_CHECK(!active_entry);

    GRC::ProtocolEntryOption entry = GRC::GetProtocolRegistry().Try("key2");

    BOOST_CHECK(entry && entry->m_status == GRC::ProtocolEntryStatus::DELETED);

    // Revert the deletion... the record should be resurrected to the state prior to deletion.

    int db_height = GRC::GetProtocolRegistry().GetDBHeight() - 1;

    GRC::GetProtocolRegistry().Revert(ctx);
    GRC::GetProtocolRegistry().SetDBHeight(db_height);

    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key2").value == "fi");

    GRC::ProtocolEntryOption resurrected_entry = GRC::GetProtocolRegistry().Try("key2");

    BOOST_CHECK(resurrected_entry
                && resurrected_entry->m_value == "fi"
                && resurrected_entry->m_status == GRC::ProtocolEntryStatus::ACTIVE);
}

// ProtocolEntries() returns a by-value snapshot under cs_lock (not a reference to
// the live m_protocol_entries map — see doc/contract_registry_locking_design.md).
// This test pins that contract: a snapshot taken before a mutation must not observe
// the mutation, a fresh snapshot must, and the snapshot includes DELETED entries.
BOOST_AUTO_TEST_CASE(protocol_ProtocolEntriesReturnsIndependentSnapshot)
{
    AddProtocolEntry(2, "key1", "hello", 1, 123456789, true);
    AddProtocolEntry(2, "key2", "world", 2, 123456790, false);

    GRC::ProtocolRegistry::ProtocolEntryMap snapshot1 = GRC::GetProtocolRegistry().ProtocolEntries();
    BOOST_CHECK(snapshot1.size() == 2);
    BOOST_CHECK(snapshot1.at("key1")->m_value == "hello");
    BOOST_CHECK(snapshot1.at("key1")->m_status == GRC::ProtocolEntryStatus::ACTIVE);

    // Mutate the registry after snapshot1 is taken.
    AddProtocolEntry(2, "key3", "again", 3, 123456791, false);

    // snapshot1 is a value copy: it must not have grown.
    BOOST_CHECK(snapshot1.size() == 2);
    BOOST_CHECK(snapshot1.find("key3") == snapshot1.end());

    // A fresh snapshot reflects the mutation.
    GRC::ProtocolRegistry::ProtocolEntryMap snapshot2 = GRC::GetProtocolRegistry().ProtocolEntries();
    BOOST_CHECK(snapshot2.size() == 3);
    BOOST_CHECK(snapshot2.at("key3")->m_value == "again");

    // Delete an entry; the snapshot INCLUDES deleted entries, and the prior snapshot
    // still observes the pre-deletion status (object-level independence — AddDelete
    // swaps in a new entry pointer rather than mutating the published one in place).
    DeleteProtocolEntry(2, "key1", "hello", 4, 123456792, false);

    GRC::ProtocolRegistry::ProtocolEntryMap snapshot3 = GRC::GetProtocolRegistry().ProtocolEntries();
    BOOST_CHECK(snapshot3.size() == 3);
    BOOST_CHECK(snapshot3.at("key1")->m_status == GRC::ProtocolEntryStatus::DELETED);
    BOOST_CHECK(snapshot2.at("key1")->m_status == GRC::ProtocolEntryStatus::ACTIVE);
}

// The rule on the value alone: the reduced numerator and denominator are each at most 1,000,000, the value is
// positive, and the string parses. Reduction comes first, so 2000000/2000000 (= 1/1) passes. The accepted list ends
// with the entries made on mainnet and testnet so far.
BOOST_AUTO_TEST_CASE(protocol_magnitude_entry_value_bounds)
{
    std::string error;

    for (const char* ok : {"1000000/1", "1/1000000", "999999/1000000", "2000000/2000000", "1000000", "5",
                           "103050/100000", "618034/1000000", "201765/100000"}) {
        error.clear();
        BOOST_CHECK_MESSAGE(GRC::CheckMagnitudeEntryValue(ok, error), ok);
        BOOST_CHECK_MESSAGE(error.empty(), ok);
    }

    for (const char* bad : {"1000001/1000000", "1/1000001", "1000001", "0", "0/5", "-1/2", "1/-2",
                            "", "abc", "1/0", "1/2/3", "1.5"}) {
        error.clear();
        BOOST_CHECK_MESSAGE(!GRC::CheckMagnitudeEntryValue(bad, error), bad);
        BOOST_CHECK_MESSAGE(!error.empty(), bad);
    }

    BOOST_CHECK(GRC::IsMagnitudeProtocolKey("magnitudeunit"));
    BOOST_CHECK(GRC::IsMagnitudeProtocolKey("magnitudeweightfactor"));
    BOOST_CHECK(!GRC::IsMagnitudeProtocolKey("MagnitudeUnit"));
    BOOST_CHECK(!GRC::IsMagnitudeProtocolKey("blockreward1"));
}

// From v15 a magnitude entry whose reduced terms exceed the bound is refused on both paths, and only such an entry:
// the same contract passes one block earlier, and entries in force, removals and other keys pass at v15.
BOOST_AUTO_TEST_CASE(protocol_magnitude_entry_refused_from_v15)
{
    grc_test::StateGuard state_guard; // nBestHeight
    grc_test::V15HeightGuard v15(4'200'000);

    const int below = 4'199'999;
    const int at = 4'200'000;

    for (const auto& [key, value] : std::vector<std::pair<std::string, std::string>> {
             {"magnitudeunit", "1000001/1000000"},
             {"magnitudeweightfactor", "1/1000001"}}) {
        const GRC::Contract contract = MakeProtocolContract(GRC::ContractAction::ADD, key, value);

        const Verdict before = JudgeAt(contract, below);
        BOOST_CHECK_MESSAGE(before.mempool && before.block, key + " = " + value + " below v15");

        const Verdict after = JudgeAt(contract, at);
        BOOST_CHECK_MESSAGE(!after.mempool && after.mempool_dos == 25, key + " = " + value + " mempool at v15");
        BOOST_CHECK_MESSAGE(!after.block && after.block_dos == 25, key + " = " + value + " block at v15");
    }

    const std::vector<std::tuple<GRC::ContractAction, std::string, std::string>> unaffected {
        {GRC::ContractAction::ADD, "magnitudeunit", "103050/100000"},
        {GRC::ContractAction::ADD, "magnitudeweightfactor", "618034/1000000"},
        {GRC::ContractAction::REMOVE, "magnitudeunit", "junk"},
        {GRC::ContractAction::ADD, "MagnitudeUnit", "junk"},
        {GRC::ContractAction::ADD, "blockreward1", "1000001/1000000"},
    };

    for (const auto& [action, key, value] : unaffected) {
        const Verdict verdict = JudgeAt(MakeProtocolContract(action, key, value), at);
        BOOST_CHECK_MESSAGE(verdict.mempool && verdict.block, key + " = " + value + " at v15");
    }
}

BOOST_AUTO_TEST_SUITE_END()

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
