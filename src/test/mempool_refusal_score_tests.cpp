// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//! The misbehaviour score of the memory pool's refusals that depend on the
//! height a rule starts at. Such a refusal depends on the node's own state, and
//! carries no score.

#include "amount.h"
#include "consensus/validation.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/cpid.h"
#include "key.h"
#include "primitives/transaction.h"
#include "random.h"
#include "sync.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "test/test_gridcoin.h"
#include "txmempool.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

using grc_test::CreateSpendWithContract;
using grc_test::PremineCoinbase;
using grc_test::SpendablePremineOutputs;

namespace {
//! What the memory pool made of a transaction offered to it, as one from a peer is.
struct Offered
{
    bool m_accepted = false;
    int m_score = -1;       //!< The misbehaviour score of the refusal, or -1 if the transaction was not found invalid.
    std::string m_reason;

    explicit Offered(const CTransaction& tx)
    {
        LOCK(cs_main);

        CTransaction offered = tx;
        CValidationState state;
        m_accepted = AcceptToMemoryPool(mempool, offered, state, nullptr);

        int score = 0;

        if (state.IsInvalid(score)) {
            m_score = score;
        }

        m_reason = state.GetRejectReason();
    }
};

//! A funded transaction that carries \p contract.
CTransaction TransactionWith(const GRC::Contract& contract)
{
    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE(!coins.empty());

    return CreateSpendWithContract(PremineCoinbase(), coins[0].n, COIN, contract);
}
} // anonymous namespace

BOOST_AUTO_TEST_SUITE(mempool_refusal_score_tests, *boost::unit_test::fixture<grc_test::RegtestChainSetup>())

BOOST_AUTO_TEST_CASE(a_claim_outside_a_coinbase_is_refused_without_a_score_from_v15)
{
    const grc_test::V15HeightGuard v15(0);

    const Offered offered(TransactionWith(GRC::MakeContract<GRC::Claim>(GRC::ContractAction::ADD, GRC::Claim())));

    BOOST_CHECK(!offered.m_accepted);
    BOOST_CHECK_EQUAL(offered.m_reason, "claim-outside-coinbase");
    BOOST_CHECK_EQUAL(offered.m_score, 0);
}

BOOST_AUTO_TEST_CASE(a_beacon_removal_without_the_current_key_is_refused_without_a_score_from_v15)
{
    const grc_test::V15HeightGuard v15(0);

    CKey key;
    key.MakeNewKey(false);

    // A removal for a CPID that has no current beacon. The payload is of version 4, as a block of block version 15
    // carries.
    GRC::BeaconPayload payload {4, GRC::Cpid(InsecureRandBytes(16)), GRC::Beacon(key.GetPubKey())};
    BOOST_REQUIRE(payload.Sign(key, GRC::ContractAction::REMOVE, uint256()));

    const Offered offered(TransactionWith(GRC::MakeContract<GRC::BeaconPayload>(GRC::ContractAction::REMOVE, payload)));

    BOOST_CHECK(!offered.m_accepted);
    BOOST_CHECK_EQUAL(offered.m_reason, "bad-beacon-remove");
    BOOST_CHECK_EQUAL(offered.m_score, 0);
}

// An advertisement of a key that is pending for another CPID is refused from v15. The refusal depends on what the
// node's registry holds, so it carries no score.
BOOST_AUTO_TEST_CASE(an_advertisement_of_a_key_held_by_another_cpid_is_refused_without_a_score_from_v15)
{
    const grc_test::V15HeightGuard v15(0);

    CKey key;
    key.MakeNewKey(false);

    const GRC::Cpid holder(InsecureRandBytes(16));
    const GRC::Cpid other(InsecureRandBytes(16));

    // Neither CPID has a record in the registry, so each advertisement is signed over none.
    const auto advertisement = [&key](const GRC::Cpid& cpid) {
        GRC::BeaconPayload payload {4, cpid, GRC::Beacon(key.GetPubKey())};
        BOOST_REQUIRE(payload.Sign(key, GRC::ContractAction::ADD, uint256()));
        return GRC::MakeContract<GRC::BeaconPayload>(GRC::ContractAction::ADD, payload);
    };

    {
        LOCK(cs_main);

        GRC::GetBeaconRegistry().Reset();

        const GRC::Contract contract = advertisement(holder);

        CMutableTransaction pending_tx;
        pending_tx.nTime = pindexBest->nTime;
        pending_tx.vContracts.push_back(contract);

        GRC::GetBeaconRegistry().Add({contract, CTransaction(pending_tx), nullptr});
    }

    const std::vector<COutPoint> coins = SpendablePremineOutputs();
    BOOST_REQUIRE(!coins.empty());

    const GRC::Contract contract = advertisement(other);
    const Offered offered(CreateSpendWithContract(
        PremineCoinbase(), coins[0].n, COIN, contract, 0, contract.RequiredBurnAmount()));

    {
        LOCK(cs_main);
        GRC::GetBeaconRegistry().Reset();
    }

    BOOST_CHECK(!offered.m_accepted);
    BOOST_CHECK_EQUAL(offered.m_reason, "bad-beacon-key");
    BOOST_CHECK_EQUAL(offered.m_score, 0);
}

// A beacon payload is of the version that the block it would be mined in carries: version 4 from block version 15,
// a lower version below it. The refusal depends on the node's height and carries no score.
BOOST_AUTO_TEST_CASE(a_beacon_payload_of_the_other_side_s_version_is_refused_without_a_score)
{
    CKey key;
    key.MakeNewKey(false);

    const auto advertisement = [&key](const uint32_t payload_version) {
        GRC::BeaconPayload payload {payload_version, GRC::Cpid(InsecureRandBytes(16)), GRC::Beacon(key.GetPubKey())};
        BOOST_REQUIRE(payload.Sign(key, GRC::ContractAction::ADD, uint256()));
        return GRC::MakeContract<GRC::BeaconPayload>(GRC::ContractAction::ADD, payload);
    };

    {
        // Below block version 15, which the fixture chain is by default, a version 4 payload is refused.
        const Offered offered(TransactionWith(advertisement(4)));

        BOOST_CHECK(!offered.m_accepted);
        BOOST_CHECK_EQUAL(offered.m_reason, "bad-beacon-version");
        BOOST_CHECK_EQUAL(offered.m_score, 0);
    }

    {
        const grc_test::V15HeightGuard v15(0);

        const Offered offered(TransactionWith(advertisement(2)));

        BOOST_CHECK(!offered.m_accepted);
        BOOST_CHECK_EQUAL(offered.m_reason, "bad-beacon-version");
        BOOST_CHECK_EQUAL(offered.m_score, 0);
    }
}

BOOST_AUTO_TEST_SUITE_END()
