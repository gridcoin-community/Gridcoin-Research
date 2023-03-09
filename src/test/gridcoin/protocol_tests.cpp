// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/protocol.h"

#include <boost/test/unit_test.hpp>

// anonymous namespace
namespace {
void AddProtocolEntry(const std::string& key, const std::string& value,
                      const int& height, const uint64_t time, const bool& reset_registry = false)
{
    GRC::ProtocolRegistry& registry = GRC::GetProtocolRegistry();

    // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
    CBlockIndex* dummy_index = new CBlockIndex();
    dummy_index->nHeight = height;
    dummy_tx.nTime = time;
    dummy_index->nTime = time;

    // Simulate a protocol control directive with whitelisted teams:
    GRC::Contract contract = GRC::MakeContract<GRC::ProtocolEntryPayload>(
                uint32_t {2}, // Pre v13
                GRC::ContractAction::ADD,
                uint32_t {1}, // Pre v13
                key,
                value,
                GRC::ProtocolEntryStatus::ACTIVE);

    dummy_tx.vContracts.push_back(contract);

    registry.Add({contract, dummy_tx, dummy_index});
}
} // anonymous namespace



BOOST_AUTO_TEST_SUITE(protocol_tests)

BOOST_AUTO_TEST_CASE(protocol_WrittenCacheShouldBeReadable)
{
    AddProtocolEntry("key", "hello", 1, 123456789, true);
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key").value == "hello");
}

BOOST_AUTO_TEST_CASE(protocol_ClearCacheShouldClearEntireSection)
{
    AddProtocolEntry("key1", "hello", 1, 123456789, true);
    AddProtocolEntry("key2", "hello", 2, 123456790, false);

    GRC::GetProtocolRegistry().Reset();

    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key1").value.empty() == true);
    BOOST_CHECK(GRC::GetProtocolRegistry().GetProtocolEntryByKeyLegacy("key2").value.empty() == true);
}

// TODO: Write reversion check similar to beacons.
BOOST_AUTO_TEST_SUITE_END()
