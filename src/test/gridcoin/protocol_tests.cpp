// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/protocol.h"

#include <boost/test/unit_test.hpp>

// anonymous namespace
namespace {
void AddProtocolEntry(const uint32_t& payload_version, const std::string& key, const std::string& value,
                      const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::ProtocolRegistry& registry = GRC::GetProtocolRegistry();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
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

    registry.Add({contract, dummy_tx, &dummy_index});
}

void DeleteProtocolEntry(const uint32_t& payload_version, const std::string& key, const std::string& value,
                      const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::ProtocolRegistry& registry = GRC::GetProtocolRegistry();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
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

    registry.Add({contract, dummy_tx, &dummy_index});
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(protocol_tests)

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

    CTransaction dummy_tx;
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

    GRC::ContractContext ctx(contract, dummy_tx, &dummy_index);

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

BOOST_AUTO_TEST_SUITE_END()
