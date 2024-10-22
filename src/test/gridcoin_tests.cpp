// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "uint256.h"
#include "gridcoin/protocol.h"
#include "util.h"
#include "main.h"
#include "gridcoin/staking/reward.h"

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/hex.hpp>
#include <map>
#include <string>

extern bool fTestNet;
extern leveldb::DB *txdb;

namespace {
// Arbitrary random characters generated with Python UUID.
const std::string TEST_CPID("17c65330c0924259b2f93c31d25b03ac");

struct GridcoinTestsConfig
{
    GridcoinTestsConfig()
    {
    }

    ~GridcoinTestsConfig()
    {
    }
};

void AddProtocolEntry(const uint32_t& payload_version, const std::string& key, const std::string& value,
                      const CBlockIndex index, const bool& reset_registry = false)
{
    GRC::ProtocolRegistry& registry = GRC::GetProtocolRegistry();

           // Make sure the registry is reset.
    if (reset_registry) registry.Reset();

    CTransaction dummy_tx;
    dummy_tx.nTime = index.nTime;

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

    registry.Add({contract, dummy_tx, &index});
}

} // anonymous namespace

BOOST_GLOBAL_FIXTURE(GridcoinTestsConfig);

BOOST_AUTO_TEST_SUITE(gridcoin_tests)

BOOST_AUTO_TEST_CASE(gridcoin_V8ShouldBeEnabledOnBlock1010000InProduction)
{
    bool was_testnet = fTestNet;
    fTestNet = false;
    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK(IsV8Enabled(1009999) == false);
    BOOST_CHECK(IsV8Enabled(1010000) == false);
    BOOST_CHECK(IsV8Enabled(1010001) == true);
    fTestNet = was_testnet;
}

BOOST_AUTO_TEST_CASE(gridcoin_V8ShouldBeEnabledOnBlock312000InTestnet)
{
    bool was_testnet = fTestNet;
    fTestNet = true;
    SelectParams(CBaseChainParams::TESTNET);
    // With testnet block 312000 was created as the first V8 block,
    // hence the difference in testing setup compared to the production
    // tests.
    BOOST_CHECK(IsV8Enabled(311999) == false);
    BOOST_CHECK(IsV8Enabled(312000) == true);
    fTestNet = was_testnet;
    SelectParams(CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_SUITE_END()

//
// CBR tests
//

BOOST_AUTO_TEST_SUITE(gridcoin_cbr_tests)

BOOST_AUTO_TEST_CASE(gridcoin_DefaultCBRShouldBe10PreV13)
{
    CBlockIndex index;
    index.nTime = 1538066417;

    auto& registry = GRC::GetProtocolRegistry();
    registry.Reset();

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), 10 * COIN);
}

BOOST_AUTO_TEST_CASE(gridcoin_DefaultCBRShouldBeDefaultConstantBlockRewardForV13)
{
    CBlockIndex index;
    index.nTime = 1538066417;
    index.nHeight = Params().GetConsensus().BlockV13Height;

    auto& registry = GRC::GetProtocolRegistry();
    registry.Reset();

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), Params().GetConsensus().DefaultConstantBlockReward);
}


// Note that payload versions 1 and 2 alternate here. This is to test the constructors and
// the machinery to add to the registry. This alternating approach is not what would
// happen on the real blockchain, but is tolerated by the Registry independent of
// block acceptance rules.

BOOST_AUTO_TEST_CASE(gridcoin_ConfigurableCBRShouldOverrideDefault)
{
    const int64_t time = 123456;
    int64_t cbr = 14.9 * COIN;

    CBlockIndex index_1;
    index_1.nVersion = 10;
    index_1.nTime = time;
    index_1.nHeight = 1;

    auto& registry = GRC::GetProtocolRegistry();

    // Protocol payload version 1

    AddProtocolEntry(1, "blockreward1", ToString(cbr), index_1, true);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index_1), cbr);

    cbr = 16.0 * COIN;

    CBlockIndex index_2;
    index_2.nVersion = 13;
    index_2.nTime = time + 1;
    index_2.nHeight = Params().GetConsensus().BlockV13Height;

    // Protocol payload version 2

    AddProtocolEntry(2, "blockreward1", ToString(cbr), index_2, false);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index_2), cbr);
}

BOOST_AUTO_TEST_CASE(gridcoin_NegativeCBRShouldClampTo0PreV13)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;
    index.nHeight = 3;

    auto& registry = GRC::GetProtocolRegistry();

    AddProtocolEntry(1, "blockreward1", ToString(-1 * COIN), index, true);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), 0);
}

BOOST_AUTO_TEST_CASE(gridcoin_NegativeCBRShouldClampToConstantBlockRewardFloorForV13)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;
    index.nHeight = Params().GetConsensus().BlockV13Height;

    auto& registry = GRC::GetProtocolRegistry();

    AddProtocolEntry(2, "blockreward1", ToString(-1 * COIN), index, true);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), Params().GetConsensus().ConstantBlockRewardFloor);
}

BOOST_AUTO_TEST_CASE(gridcoin_ConfigurableCBRShouldClampTo2xDefaultPreV13)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;
    index.nHeight = 4;

    auto& registry = GRC::GetProtocolRegistry();

    AddProtocolEntry(2, "blockreward1", ToString(Params().GetConsensus().DefaultConstantBlockReward * 3), index, true);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), Params().GetConsensus().DefaultConstantBlockReward * 2);
}

BOOST_AUTO_TEST_CASE(gridcoin_ConfigurableCBRShouldClampToConstantBlockRewardCeilingForV13)
{
    const int64_t time = 123456;
    CBlockIndex index;
    index.nTime = time;
    index.nHeight = Params().GetConsensus().BlockV13Height;

    auto& registry = GRC::GetProtocolRegistry();

    AddProtocolEntry(2, "blockreward1", ToString(Params().GetConsensus().ConstantBlockRewardCeiling * 2), index, true);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index), Params().GetConsensus().ConstantBlockRewardCeiling);
}

BOOST_AUTO_TEST_CASE(gridcoin_ObsoleteConfigurableCBRShouldResortToDefaultPreV13)
{
    CBlockIndex index_check;
    index_check.nTime = 1538066417;
    index_check.nHeight = 6;
    const int64_t max_message_age = 60 * 60 * 24 * 180;

    CBlockIndex index_add;
    index_add.nTime = index_check.nTime - max_message_age - 1;
    index_add.nHeight = 5;

    auto& registry = GRC::GetProtocolRegistry();

    AddProtocolEntry(1, "blockreward1", ToString(3 * COIN), index_add, true);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index_check), Params().GetConsensus().DefaultConstantBlockReward);
}

BOOST_AUTO_TEST_CASE(gridcoin_PriorObsoleteConfigurableCBRShouldNotResortToDefaultforV13)
{
    CBlockIndex index_check;
    index_check.nTime = 1538066417;
    index_check.nHeight = Params().GetConsensus().BlockV13Height;
    const int64_t max_message_age = 60 * 60 * 24 * 180;

    CBlockIndex index_add;
    index_add.nTime = index_check.nTime - max_message_age - 1;
    index_add.nHeight = 5;

    auto& registry = GRC::GetProtocolRegistry();

    AddProtocolEntry(2, "blockreward1", ToString(3 * COIN), index_add, true);

    if (GRC::ProtocolEntryOption entry = registry.TryActive("blockreward1")) {
        LogPrint(BCLog::LogFlags::CONTRACT, "INFO: %s: Protocol entry: m_key %s, m_value %s, m_hash %s, m_previous_hash %s, "
                                            "m_timestamp %" PRId64 ", m_status string %s",
                 __func__,
                 entry->m_key,
                 entry->m_value,
                 entry->m_hash.ToString(),
                 entry->m_previous_hash.ToString(),
                 entry->m_timestamp,
                 entry->StatusToString()
                 );
    }

    BOOST_CHECK_EQUAL(GRC::GetConstantBlockReward(&index_check), 3 * COIN);
}


BOOST_AUTO_TEST_SUITE_END()
