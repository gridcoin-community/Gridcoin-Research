// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "random.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/tx_message.h"
#include "test/state_guard.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

namespace {
//! A transaction carrying one MESSAGE contract, with enough burn to satisfy
//! CheckContracts' fee rule (flat 0.001 GRC for a message of up to 1 KB).
CTransaction MessageTx(const GRC::ContractAction action, const std::string& message)
{
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(GetRandHash(), 0);  // non-null: not a coinbase
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 0.001 * COIN;
    mtx.vout[0].scriptPubKey = CScript() << OP_RETURN;  // the burn CheckContracts counts
    mtx.vContracts.emplace_back(GRC::MakeContract<GRC::TxMessage>(action, message));
    return CTransaction(mtx);
}

bool ContractsPass(const CTransaction& tx, const int height)
{
    CValidationState state;
    const MapPrevTx no_inputs;  // MESSAGE does not require the master key, so inputs are unused
    return CheckContracts(tx, state, no_inputs, height);
}
} // namespace

BOOST_AUTO_TEST_SUITE(message_contract_tests)

//! Below the disable height nothing changes: a message contract is as acceptable
//! as it has always been. Without this the rejection case below would pass just
//! as well for a transaction that was malformed for some unrelated reason.
BOOST_AUTO_TEST_CASE(message_contract_accepted_below_the_disable_height)
{
    grc_test::StateGuard guard;
    gArgs.ForceSetArg("-messagecontractdisableheight", "100");

    LOCK(cs_main);
    BOOST_CHECK(ContractsPass(MessageTx(GRC::ContractAction::ADD, "hello"), 99));
}

//! At the height itself and beyond it, rejected.
BOOST_AUTO_TEST_CASE(message_contract_rejected_from_the_disable_height)
{
    grc_test::StateGuard guard;
    gArgs.ForceSetArg("-messagecontractdisableheight", "100");

    LOCK(cs_main);
    BOOST_CHECK(!ContractsPass(MessageTx(GRC::ContractAction::ADD, "hello"), 100));
    BOOST_CHECK(!ContractsPass(MessageTx(GRC::ContractAction::ADD, "hello"), 101));
}

//! The gate is on the contract TYPE, not on the ADD action, and this is the case
//! that pins it. TxMessage::WellFormed() ignores the action it is given and
//! MESSAGE has no registry handler to constrain one either, so a gate written
//! against ADD alone would be sidestepped by sending the identical payload under
//! any other action.
BOOST_AUTO_TEST_CASE(message_contract_rejected_whatever_the_action)
{
    grc_test::StateGuard guard;
    gArgs.ForceSetArg("-messagecontractdisableheight", "100");

    LOCK(cs_main);
    for (const auto action : {GRC::ContractAction::ADD, GRC::ContractAction::REMOVE}) {
        BOOST_CHECK_MESSAGE(!ContractsPass(MessageTx(action, "hello"), 100),
                            "a MESSAGE contract passed the gate under a non-ADD action");
    }
}

//! Unscheduled by default, so nothing on mainnet changes until a height is pinned.
BOOST_AUTO_TEST_CASE(message_contract_unscheduled_by_default)
{
    grc_test::StateGuard guard;

    BOOST_CHECK_EQUAL(GetMessageContractDisableHeight(), std::numeric_limits<int>::max());

    LOCK(cs_main);
    BOOST_CHECK(ContractsPass(MessageTx(GRC::ContractAction::ADD, "hello"), 10000000));
}

BOOST_AUTO_TEST_SUITE_END()
