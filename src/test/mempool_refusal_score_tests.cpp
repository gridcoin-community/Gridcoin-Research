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

    // A removal for a CPID that has no current beacon.
    const Offered offered(TransactionWith(GRC::MakeContract<GRC::BeaconPayload>(
        GRC::ContractAction::REMOVE,
        GRC::Cpid(InsecureRandBytes(16)),
        key.GetPubKey())));

    BOOST_CHECK(!offered.m_accepted);
    BOOST_CHECK_EQUAL(offered.m_reason, "bad-beacon-remove");
    BOOST_CHECK_EQUAL(offered.m_score, 0);
}

BOOST_AUTO_TEST_SUITE_END()
