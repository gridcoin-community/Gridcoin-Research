// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

//!
//! \file wallet_send_subtract_fee_tests.cpp
//! \brief "Subtract fee from amount" sends, on the regtest fixture chain.
//!
//! Most cases go through the node side of the GUI's send path
//! (interfaces::Wallet::sendCoins); the last calls CWallet::CreateTransaction
//! directly. The wallets hold coins they received from elsewhere, confirmed ten
//! deep: the shape of the testnet report in which a whole-balance send that
//! carried a message could not be created.
//!
//! With subtract-fee set, the sender is debited exactly the amounts entered:
//! the fee and any message burn come out of the flagged recipients.
//!

#include "chain.h"
#include "consensus/consensus.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/tx_message.h"
#include "init.h"
#include "interfaces/wallet.h"
#include "key.h"
#include "key_io.h"
#include "logging.h"
#include "primitives/block.h"
#include "test/chain_setup.h"
#include "test/state_guard.h"
#include "tinyformat.h"
#include "uint256.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

//! The configured fee every case pins, and the size-based fee of a
//! transaction under 1000 bytes.
const CAmount BASE_FEE = MIN_TX_FEE * 10;

const std::string MESSAGE = "Happy now?";

CAmount MessageBurn()
{
    return GRC::MakeContract<GRC::TxMessage>(GRC::ContractAction::ADD, MESSAGE).RequiredBurnAmount();
}

//! Collects the log lines containing \p needle while alive.
class CapturedLogLines
{
public:
    std::vector<std::string> m_lines;

    explicit CapturedLogLines(std::string needle)
        : m_callback(LogInstance().PushBackCallback([this, needle = std::move(needle)](const std::string& s) {
              if (s.find(needle) != std::string::npos) m_lines.push_back(s);
          }))
    {
    }

    ~CapturedLogLines() { LogInstance().DeleteCallback(m_callback); }
    CapturedLogLines(const CapturedLogLines&) = delete;
    CapturedLogLines& operator=(const CapturedLogLines&) = delete;

private:
    std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

//!
//! \brief A file-backed wallet holding one funded key.
//!
//! File-backed because CommitTransaction writes the new transaction through a
//! CWalletDB. In this binary every wallet "file" is the same in-memory Berkeley
//! database (db.cpp opens a mock file with no filename and the logical name
//! "main"), shared with pwalletMain's wallet.dat. So a test wallet must not use
//! the keypool: pool records are keyed by index alone, and topping one wallet's
//! pool up rewrites the entries of every other, pwalletMain's included. Change
//! goes to the wallet's own key instead (ChangeHere), and FundedWallets erases
//! the transaction and address-book records the wallets wrote. The key records
//! stay behind, as other suites' keys already do (see chain_setup.cpp).
//!
struct TestWallet
{
    CWallet wallet;
    CKey key;
    CScript script;

    explicit TestWallet(const std::string& file) : wallet(file)
    {
        key.MakeNewKey(true);
        script.SetDestination(key.GetPubKey().GetID());

        LOCK2(cs_main, wallet.cs_wallet);
        BOOST_REQUIRE(wallet.AddKey(key));
    }

    //! Coin control with nothing selected and change to the funded key.
    interfaces::WalletCoinControl ChangeHere() const
    {
        interfaces::WalletCoinControl coin_control;
        coin_control.dest_change = EncodeDestination(key.GetPubKey().GetID());

        return coin_control;
    }
};

//! One wallet per use, so that no case spends another's coins.
enum class Use
{
    WHOLE_NO_COIN_CONTROL,
    WHOLE_COIN_CONTROL_EMPTY,
    WHOLE_COIN_SELECTED,
    WHOLE_WITH_MESSAGE,
    PARTIAL,
    PARTIAL_WITH_MESSAGE,
    TWO_RECIPIENTS,
    THREE_FLAGGED,
    TOO_SMALL,
    BURN_TOO_LARGE,
    SUB_CENT_CHANGE,
    FEE_STEP,
    LATE_FEE_RISE,
    UNIFORM,
    DIRECT,
};

//! The coin each single-coin wallet holds.
const CAmount SINGLE_COIN = 9000 * COIN;

//!
//! \brief The suite's chain, and the wallets funded on it.
//!
//! Funded once, ten confirmations deep, before the first case: mining is what
//! a case may not do under a StateGuard (test/chain_setup.h, CLOCK), and the
//! cases each restore the mempool and the clock.
//!
struct FundedWallets
{
    grc_test::RegtestChainSetup chain;
    std::map<Use, std::unique_ptr<TestWallet>> wallets;

    FundedWallets();
    ~FundedWallets();
};

FundedWallets* g_funded = nullptr;

TestWallet& WalletFor(Use use)
{
    BOOST_REQUIRE(g_funded != nullptr);
    return *g_funded->wallets.at(use);
}

//! Spend premine output \p out into \p pays, returning the rest to the premine.
CTransaction Fund(const COutPoint& out, const std::vector<std::pair<CScript, CAmount>>& pays)
{
    const CTransaction& coinbase = grc_test::PremineCoinbase();
    // Generous: the larger funding transaction is over 1000 bytes.
    const CAmount funding_fee = 4 * BASE_FEE;

    CMutableTransaction mtx;
    mtx.nTime = static_cast<unsigned int>(grc_test::FixtureTxTime());
    mtx.vin.resize(1);
    mtx.vin[0].prevout = out;

    CAmount paid = 0;

    for (const auto& [script, value] : pays) {
        mtx.vout.emplace_back(value, script);
        paid += value;
    }

    const CAmount rest = coinbase.vout[out.n].nValue - paid - funding_fee;
    BOOST_REQUIRE(rest > 0);
    mtx.vout.emplace_back(rest, grc_test::PremineScript());

    BOOST_REQUIRE(SignSignature(grc_test::PremineKeystore(), coinbase, mtx, 0));

    const CTransaction tx(mtx);
    grc_test::AddToMempool(tx, funding_fee);

    // The fixture wallet owns the premine, so it must see the spend: otherwise
    // the staker can pick the same output as its kernel.
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        pwalletMain->AddToWalletIfInvolvingMe(tx, nullptr, true);
    }

    return tx;
}

FundedWallets::FundedWallets()
{
    const std::vector<Use> single_coin = {
        Use::WHOLE_NO_COIN_CONTROL, Use::WHOLE_COIN_CONTROL_EMPTY, Use::WHOLE_COIN_SELECTED,
        Use::WHOLE_WITH_MESSAGE, Use::PARTIAL, Use::PARTIAL_WITH_MESSAGE, Use::TWO_RECIPIENTS,
        Use::THREE_FLAGGED, Use::TOO_SMALL, Use::BURN_TOO_LARGE, Use::SUB_CENT_CHANGE,
    };

    for (const Use use : single_coin) {
        wallets.emplace(use, std::make_unique<TestWallet>(strprintf("wallet_subfee_%d.dat", static_cast<int>(use))));
    }

    for (const Use use : {Use::FEE_STEP, Use::LATE_FEE_RISE, Use::UNIFORM, Use::DIRECT}) {
        wallets.emplace(use, std::make_unique<TestWallet>(strprintf("wallet_subfee_%d.dat", static_cast<int>(use))));
    }

    const std::vector<COutPoint> premine = grc_test::SpendablePremineOutputs();
    BOOST_REQUIRE_GE(premine.size(), 2u);

    std::vector<std::pair<CScript, CAmount>> first;
    for (const Use use : single_coin) {
        first.emplace_back(wallets.at(use)->script, SINGLE_COIN);
    }

    // Enough coins that a whole-balance send crosses 1000 bytes, twice over;
    // ten equal coins (#2983's 2-cycle shape); two for the direct calls.
    std::vector<std::pair<CScript, CAmount>> second;
    for (int i = 0; i < 8; ++i) second.emplace_back(wallets.at(Use::FEE_STEP)->script, 100 * COIN);
    for (int i = 0; i < 8; ++i) second.emplace_back(wallets.at(Use::LATE_FEE_RISE)->script, 100 * COIN);
    for (int i = 0; i < 10; ++i) second.emplace_back(wallets.at(Use::UNIFORM)->script, 400 * COIN);
    for (int i = 0; i < 2; ++i) second.emplace_back(wallets.at(Use::DIRECT)->script, 1000 * COIN);

    const CTransaction tx_first = Fund(premine[premine.size() - 1], first);
    const CTransaction tx_second = Fund(premine[premine.size() - 2], second);

    CBlock block;
    std::string err;
    BOOST_REQUIRE_MESSAGE(grc_test::CreateAndProcessBlock(block, err), "could not mine: " << err);

    for (const CTransaction& tx : {tx_first, tx_second}) {
        BOOST_REQUIRE_MESSAGE(std::any_of(block.vtx.begin(), block.vtx.end(),
                                          [&](const CTransaction& mined) { return mined.GetHash() == tx.GetHash(); }),
                              "funding transaction not mined");
    }

    // Ten deep. A coin from elsewhere counts as spendable at three (IsTrusted)
    // and passes the first coin-selection tier at ten.
    const int funded_height = WITH_LOCK(cs_main, return nBestHeight);

    for (int i = 0; i < 20 && WITH_LOCK(cs_main, return nBestHeight) < funded_height + 9; ++i) {
        BOOST_REQUIRE_MESSAGE(grc_test::CreateAndProcessBlock(block, err), "could not mine: " << err);
    }

    BOOST_REQUIRE_GE(WITH_LOCK(cs_main, return nBestHeight), funded_height + 9);

    for (auto& [use, w] : wallets) {
        w->wallet.ScanForWalletTransactions(pindexGenesisBlock, true);
    }

    g_funded = this;
}

FundedWallets::~FundedWallets()
{
    g_funded = nullptr;

    // The records the test wallets wrote to the shared database (TestWallet).
    // Erasing a funding transaction's record also takes pwalletMain's copy,
    // which the chain fixture is about to erase anyway.
    for (auto& [use, w] : wallets) {
        CWalletDB walletdb(w->wallet.strWalletFile);

        LOCK(w->wallet.cs_wallet);

        for (const auto& [hash, wtx] : w->wallet.mapWallet) {
            walletdb.EraseTx(hash);
        }

        for (const auto& [dest, entry] : w->wallet.mapAddressBook) {
            walletdb.EraseName(EncodeDestination(dest));
            walletdb.ErasePurpose(EncodeDestination(dest));
        }
    }

    // The test wallets' AddToWallet calls wrote their own order counters over
    // pwalletMain's.
    LOCK(pwalletMain->cs_wallet);
    CWalletDB(pwalletMain->strWalletFile).WriteOrderPosNext(pwalletMain->nOrderPosNext);
}

//! Per case: the configured fee pinned, and the mempool (and the clock) put
//! back. No case mines, so the StateGuard may restore the clock.
struct CaseScope
{
    grc_test::MempoolStateGuard mempool_guard;
    const int64_t saved_transaction_fee{nTransactionFee};

    CaseScope() { nTransactionFee = BASE_FEE; }
    ~CaseScope() { nTransactionFee = saved_transaction_fee; }
};

interfaces::WalletSendRecipient Recipient(CAmount amount, bool subtract_fee, const std::string& message = "")
{
    CKey payee;
    payee.MakeNewKey(true);

    interfaces::WalletSendRecipient recipient;
    recipient.address = EncodeDestination(payee.GetPubKey().GetID());
    recipient.amount = amount;
    recipient.subtract_fee_from_amount = subtract_fee;
    recipient.message = message;

    return recipient;
}

std::string StatusName(interfaces::SendCoinsStatus status)
{
    switch (status) {
    case interfaces::SendCoinsStatus::OK: return "OK";
    case interfaces::SendCoinsStatus::InvalidAddress: return "InvalidAddress";
    case interfaces::SendCoinsStatus::InvalidAmount: return "InvalidAmount";
    case interfaces::SendCoinsStatus::AmountExceedsBalance: return "AmountExceedsBalance";
    case interfaces::SendCoinsStatus::AmountWithFeeExceedsBalance: return "AmountWithFeeExceedsBalance";
    case interfaces::SendCoinsStatus::FeeExceedsSubtractedAmount: return "FeeExceedsSubtractedAmount";
    case interfaces::SendCoinsStatus::TransactionCreationFailed: return "TransactionCreationFailed";
    case interfaces::SendCoinsStatus::TransactionCommitFailed: return "TransactionCommitFailed";
    case interfaces::SendCoinsStatus::WalletUnlockedForStakingOnly: return "WalletUnlockedForStakingOnly";
    case interfaces::SendCoinsStatus::FeeConfirmationRequired: return "FeeConfirmationRequired";
    }
    return "unknown";
}

struct Sent
{
    interfaces::SendCoinsResult result;
    //! "Failed to select coins" lines logged during the whole send.
    size_t select_failures{0};
    //! The fees the user was asked to confirm, in order.
    std::vector<CAmount> confirmed_fees;
};

//!
//! \brief Send as the GUI's send dialog does.
//!
//! A fee above the configured one comes back as FeeConfirmationRequired
//! without a commit; SendCoinsDialog::on_sendButton_clicked asks the user and
//! re-invokes with the accepted fee, at most ten times. This accepts every fee.
//!
Sent Send(TestWallet& w, const std::vector<interfaces::WalletSendRecipient>& recipients,
          const std::optional<interfaces::WalletCoinControl>& coin_control = std::nullopt)
{
    CapturedLogLines failures("Failed to select coins");
    std::unique_ptr<interfaces::Wallet> iface = interfaces::MakeWallet(&w.wallet);

    Sent sent;
    int64_t accepted_fee = 0;

    for (int prompt = 0; ; ++prompt) {
        sent.result = iface->sendCoins(recipients, coin_control, accepted_fee);

        if (sent.result.status != interfaces::SendCoinsStatus::FeeConfirmationRequired || prompt >= 10) {
            break;
        }

        accepted_fee = sent.result.fee;
        sent.confirmed_fees.push_back(accepted_fee);
    }

    sent.select_failures = failures.m_lines.size();

    return sent;
}

//! What a committed send did, read back from the sending wallet.
struct Outcome
{
    CAmount inputs{0};
    CAmount change{0};
    CAmount burn{0};
    CAmount fee{0};
    //! Per recipient, in the order given.
    std::vector<CAmount> paid;
    size_t bytes{0};
    bool has_message{false};

    //! What the sender's balance went down by.
    CAmount Debit() const { return inputs - change; }
};

Outcome Inspect(TestWallet& w, const Sent& sent, const std::vector<interfaces::WalletSendRecipient>& recipients)
{
    BOOST_REQUIRE_MESSAGE(sent.result.status == interfaces::SendCoinsStatus::OK,
                          "send ended " << StatusName(sent.result.status) << ", fee " << sent.result.fee);

    LOCK2(cs_main, w.wallet.cs_wallet);

    const auto it = w.wallet.mapWallet.find(uint256S(sent.result.txid_hex));
    BOOST_REQUIRE(it != w.wallet.mapWallet.end());
    const CWalletTx& tx = it->second;

    Outcome outcome;

    for (const CTxIn& in : tx.vin) {
        const auto prev = w.wallet.mapWallet.find(in.prevout.hash);
        BOOST_REQUIRE(prev != w.wallet.mapWallet.end());
        outcome.inputs += prev->second.vout[in.prevout.n].nValue;
    }

    CAmount outputs = 0;

    for (const CTxOut& out : tx.vout) {
        outputs += out.nValue;

        if (out.scriptPubKey == (CScript() << OP_RETURN)) {
            outcome.burn += out.nValue;
        } else if (w.wallet.IsMine(out) != ISMINE_NO) {
            outcome.change += out.nValue;
        }
    }

    for (const interfaces::WalletSendRecipient& recipient : recipients) {
        CScript script;
        script.SetDestination(DecodeDestination(recipient.address));

        const auto out = std::find_if(tx.vout.begin(), tx.vout.end(),
                                      [&](const CTxOut& o) { return o.scriptPubKey == script; });
        BOOST_REQUIRE(out != tx.vout.end());
        outcome.paid.push_back(out->nValue);
    }

    outcome.fee = outcome.inputs - outputs;
    outcome.bytes = ::GetSerializeSize(static_cast<const CTransaction&>(tx), SER_NETWORK, PROTOCOL_VERSION);
    outcome.has_message = !tx.vContracts.empty() && tx.vContracts[0].m_type == GRC::ContractType::MESSAGE;

    return outcome;
}

//! The size-based fee for a transaction of \p bytes (CreateTransaction's nPayFee).
CAmount SizeFee(size_t bytes)
{
    return BASE_FEE * (1 + static_cast<CAmount>(bytes) / 1000);
}

COutPoint OnlyCoin(TestWallet& w)
{
    std::vector<COutput> coins;
    w.wallet.AvailableCoins(coins, true, nullptr, false);
    BOOST_REQUIRE_EQUAL(coins.size(), 1u);

    return COutPoint(coins[0].tx->GetHash(), coins[0].i);
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(wallet_send_subtract_fee_tests, CaseScope, *boost::unit_test::fixture<FundedWallets>())

//! The report's shape without a message, in each coin-control mode the GUI can
//! pass. The send succeeds and logs no coin-selection failure: a failure that a
//! later attempt recovers from is not one.
BOOST_AUTO_TEST_CASE(whole_balance_goes_out_and_logs_no_selection_failure)
{
    interfaces::WalletCoinControl selected;
    selected.Select(OnlyCoin(WalletFor(Use::WHOLE_COIN_SELECTED)));

    const std::vector<std::pair<Use, std::optional<interfaces::WalletCoinControl>>> modes = {
        {Use::WHOLE_NO_COIN_CONTROL, std::nullopt},
        // Coin-control features on with nothing selected: the GUI passes the
        // dialog's empty selection rather than nullopt.
        {Use::WHOLE_COIN_CONTROL_EMPTY, interfaces::WalletCoinControl{}},
        {Use::WHOLE_COIN_SELECTED, selected},
    };

    for (const auto& [use, coin_control] : modes) {
        BOOST_TEST_CONTEXT("wallet " << static_cast<int>(use))
        {
            TestWallet& w = WalletFor(use);
            const std::vector<interfaces::WalletSendRecipient> recipients = {Recipient(SINGLE_COIN, true)};

            const Sent sent = Send(w, recipients, coin_control);
            BOOST_REQUIRE_MESSAGE(sent.result.status == interfaces::SendCoinsStatus::OK, StatusName(sent.result.status));
            BOOST_CHECK_EQUAL(sent.select_failures, 0u);

            const Outcome outcome = Inspect(w, sent, recipients);
            BOOST_CHECK_EQUAL(outcome.Debit(), SINGLE_COIN);
            BOOST_CHECK_EQUAL(outcome.change, 0);
            BOOST_CHECK_EQUAL(outcome.fee, BASE_FEE);
            BOOST_CHECK_EQUAL(outcome.paid[0], SINGLE_COIN - BASE_FEE);
        }
    }
}

//! The report: the whole balance with a message attached. The burn comes out of
//! the recipient along with the fee, so the send can be funded at all.
BOOST_AUTO_TEST_CASE(whole_balance_with_a_message_goes_out)
{
    TestWallet& w = WalletFor(Use::WHOLE_WITH_MESSAGE);
    const std::vector<interfaces::WalletSendRecipient> recipients = {Recipient(SINGLE_COIN, true, MESSAGE)};

    const Sent sent = Send(w, recipients);
    BOOST_REQUIRE_MESSAGE(sent.result.status == interfaces::SendCoinsStatus::OK, StatusName(sent.result.status));
    BOOST_CHECK_EQUAL(sent.select_failures, 0u);

    const Outcome outcome = Inspect(w, sent, recipients);
    BOOST_CHECK(outcome.has_message);
    BOOST_CHECK_EQUAL(outcome.burn, MessageBurn());
    BOOST_CHECK_EQUAL(outcome.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(outcome.change, 0);
    BOOST_CHECK_EQUAL(outcome.paid[0], SINGLE_COIN - BASE_FEE - MessageBurn());
    BOOST_CHECK_EQUAL(outcome.Debit(), SINGLE_COIN);
}

//! Part of the balance, with and without a message: the change is the coin
//! less the amount entered, to the satoshi. With a message the burn used to
//! come out of the change instead (the report's "should be 40").
BOOST_AUTO_TEST_CASE(partial_amount_debits_exactly_the_amount_entered)
{
    const CAmount amount = 1000 * COIN;

    for (const auto& [use, message] : std::vector<std::pair<Use, std::string>>{
             {Use::PARTIAL, ""}, {Use::PARTIAL_WITH_MESSAGE, MESSAGE}}) {
        BOOST_TEST_CONTEXT("message \"" << message << "\"")
        {
            TestWallet& w = WalletFor(use);
            const std::vector<interfaces::WalletSendRecipient> recipients = {Recipient(amount, true, message)};
            const CAmount burn = message.empty() ? 0 : MessageBurn();

            const Sent sent = Send(w, recipients, w.ChangeHere());
            const Outcome outcome = Inspect(w, sent, recipients);

            BOOST_CHECK_EQUAL(outcome.change, SINGLE_COIN - amount);
            BOOST_CHECK_EQUAL(outcome.Debit(), amount);
            BOOST_CHECK_EQUAL(outcome.burn, burn);
            BOOST_CHECK_EQUAL(outcome.fee, BASE_FEE);
            BOOST_CHECK_EQUAL(outcome.paid[0], amount - BASE_FEE - burn);
        }
    }
}

//! Only a flagged recipient pays; an unflagged one receives what was entered.
BOOST_AUTO_TEST_CASE(only_flagged_recipients_pay)
{
    TestWallet& w = WalletFor(Use::TWO_RECIPIENTS);
    const std::vector<interfaces::WalletSendRecipient> recipients = {Recipient(1000 * COIN, true),
                                                                     Recipient(2000 * COIN, false)};

    const Sent sent = Send(w, recipients, w.ChangeHere());
    const Outcome outcome = Inspect(w, sent, recipients);

    BOOST_CHECK_EQUAL(outcome.paid[0], 1000 * COIN - BASE_FEE);
    BOOST_CHECK_EQUAL(outcome.paid[1], 2000 * COIN);
    BOOST_CHECK_EQUAL(outcome.change, SINGLE_COIN - 3000 * COIN);
    BOOST_CHECK_EQUAL(outcome.Debit(), 3000 * COIN);
}

//! Fee and burn split evenly across flagged recipients, the first paying the
//! remainder: the rule the GUI loop used and Bitcoin Core uses.
BOOST_AUTO_TEST_CASE(three_flagged_recipients_split_fee_and_burn)
{
    TestWallet& w = WalletFor(Use::THREE_FLAGGED);
    const CAmount amount = 1000 * COIN;
    const std::vector<interfaces::WalletSendRecipient> recipients = {
        Recipient(amount, true, MESSAGE), Recipient(amount, true), Recipient(amount, true)};

    const Sent sent = Send(w, recipients, w.ChangeHere());
    const Outcome outcome = Inspect(w, sent, recipients);

    const CAmount shared = BASE_FEE + MessageBurn();
    BOOST_REQUIRE_NE(shared % 3, 0); // the case is about the remainder

    BOOST_CHECK_EQUAL(outcome.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(outcome.paid[0], amount - shared / 3 - shared % 3);
    BOOST_CHECK_EQUAL(outcome.paid[1], amount - shared / 3);
    BOOST_CHECK_EQUAL(outcome.paid[2], amount - shared / 3);
    BOOST_CHECK_EQUAL(outcome.change, SINGLE_COIN - 3 * amount);
    BOOST_CHECK_EQUAL(outcome.Debit(), 3 * amount);
}

//! A flagged amount that the fee would use up entirely is refused, naming the fee.
BOOST_AUTO_TEST_CASE(fee_larger_than_the_flagged_amount_is_refused)
{
    TestWallet& w = WalletFor(Use::TOO_SMALL);
    const Sent sent = Send(w, {Recipient(BASE_FEE, true)}, w.ChangeHere());

    BOOST_CHECK(sent.result.status == interfaces::SendCoinsStatus::FeeExceedsSubtractedAmount);
    BOOST_CHECK_EQUAL(sent.result.fee, BASE_FEE);
}

//! With a message the burn comes out of the flagged amount too, and can be what
//! uses it up. Refused, and the fee reported includes the burn, so the GUI's
//! message names an amount that does exceed what was entered.
BOOST_AUTO_TEST_CASE(burn_that_uses_up_the_flagged_amount_is_refused)
{
    TestWallet& w = WalletFor(Use::BURN_TOO_LARGE);
    const CAmount amount = BASE_FEE + MessageBurn() / 2;

    const Sent sent = Send(w, {Recipient(amount, true, MESSAGE)}, w.ChangeHere());

    BOOST_CHECK(sent.result.status == interfaces::SendCoinsStatus::FeeExceedsSubtractedAmount);
    BOOST_CHECK_EQUAL(sent.result.fee, BASE_FEE + MessageBurn());
    BOOST_CHECK_GT(sent.result.fee, amount);
}

//! The fee can outgrow a flagged amount after the first pass: eight selected
//! coins take the transaction past 1000 bytes, and the doubled fee leaves
//! nothing of an amount the first pass's fee did not use up. Refused, naming
//! the fee that did it.
BOOST_AUTO_TEST_CASE(fee_that_outgrows_the_amount_on_a_later_pass_is_refused)
{
    TestWallet& w = WalletFor(Use::LATE_FEE_RISE);

    std::vector<COutput> coins;
    w.wallet.AvailableCoins(coins, true, nullptr, false);
    BOOST_REQUIRE_EQUAL(coins.size(), 8u);

    interfaces::WalletCoinControl coin_control = w.ChangeHere();
    for (const COutput& coin : coins) coin_control.Select(COutPoint(coin.tx->GetHash(), coin.i));

    const CAmount amount = BASE_FEE + BASE_FEE / 2;

    const Sent sent = Send(w, {Recipient(amount, true)}, coin_control);

    BOOST_CHECK(sent.result.status == interfaces::SendCoinsStatus::FeeExceedsSubtractedAmount);
    BOOST_CHECK_EQUAL(sent.result.fee, 2 * BASE_FEE);
}

//! With -paytxfee below the base fee and change under a CENT, the recipient
//! pays the minimum fee and the change is left whole: the sender is still
//! debited exactly the amount entered. The fee is above the configured one, so
//! the GUI's confirmation round trip comes first.
BOOST_AUTO_TEST_CASE(sub_cent_change_is_left_to_the_sender)
{
    nTransactionFee = MIN_TX_FEE; // restored by CaseScope

    TestWallet& w = WalletFor(Use::SUB_CENT_CHANGE);
    const CAmount change = CENT / 2;
    const std::vector<interfaces::WalletSendRecipient> recipients = {Recipient(SINGLE_COIN - change, true)};

    const Sent sent = Send(w, recipients, w.ChangeHere());
    BOOST_CHECK(sent.confirmed_fees == std::vector<CAmount>{BASE_FEE});

    const Outcome outcome = Inspect(w, sent, recipients);

    BOOST_CHECK_EQUAL(outcome.change, change);
    BOOST_CHECK_EQUAL(outcome.fee, BASE_FEE);
    BOOST_CHECK_EQUAL(outcome.paid[0], SINGLE_COIN - change - BASE_FEE);
    BOOST_CHECK_EQUAL(outcome.Debit(), SINGLE_COIN - change);
}

//! #2981: a whole-balance send from enough coins that the signed transaction
//! crosses 1000 bytes pays the higher fee, out of the recipient, with no
//! selection failure on the way.
BOOST_AUTO_TEST_CASE(whole_balance_across_the_1000_byte_fee_step)
{
    TestWallet& w = WalletFor(Use::FEE_STEP);
    const CAmount balance = 8 * 100 * COIN;
    const std::vector<interfaces::WalletSendRecipient> recipients = {Recipient(balance, true)};

    const Sent sent = Send(w, recipients);
    BOOST_REQUIRE_MESSAGE(sent.result.status == interfaces::SendCoinsStatus::OK, StatusName(sent.result.status));
    BOOST_CHECK_EQUAL(sent.select_failures, 0u);

    const Outcome outcome = Inspect(w, sent, recipients);
    BOOST_REQUIRE_GE(outcome.bytes, 1000u); // the case is about the step
    BOOST_CHECK_EQUAL(outcome.fee, SizeFee(outcome.bytes));
    BOOST_CHECK(sent.confirmed_fees == std::vector<CAmount>{outcome.fee});
    BOOST_CHECK_EQUAL(outcome.paid[0], balance - outcome.fee);
    BOOST_CHECK_EQUAL(outcome.change, 0);
}

//! #2983's shape: ten equal coins and an amount that sits on a coin boundary.
//! The fee matches the size of what was built and the sender pays exactly the
//! amount entered. This pins the outcome only: the old loop's pinned-input
//! fallback reached the same one.
BOOST_AUTO_TEST_CASE(equal_coins_settle_on_one_fee)
{
    TestWallet& w = WalletFor(Use::UNIFORM);
    const CAmount amount = 2400 * COIN + BASE_FEE;
    const std::vector<interfaces::WalletSendRecipient> recipients = {Recipient(amount, true)};

    const Sent sent = Send(w, recipients, w.ChangeHere());
    const Outcome outcome = Inspect(w, sent, recipients);

    BOOST_CHECK_EQUAL(outcome.fee, SizeFee(outcome.bytes));
    BOOST_CHECK_EQUAL(outcome.paid[0], amount - outcome.fee);
    BOOST_CHECK_EQUAL(outcome.Debit(), amount);
}

//! CreateTransaction itself: an out-of-range index is refused; an empty set
//! keeps the fee on top of the outputs; a flagged output pays it.
BOOST_AUTO_TEST_CASE(create_transaction_with_and_without_fee_outputs)
{
    TestWallet& w = WalletFor(Use::DIRECT);

    std::vector<COutput> coins;
    w.wallet.AvailableCoins(coins, true, nullptr, false);
    BOOST_REQUIRE_EQUAL(coins.size(), 2u);

    // Both coins selected, so every call spends the same, known inputs; change
    // to the funded key, not the keypool (TestWallet).
    CCoinControl coin_control;
    for (const COutput& coin : coins) coin_control.Select(COutPoint(coin.tx->GetHash(), coin.i));
    coin_control.destChange = w.key.GetPubKey().GetID();
    const CAmount inputs = 2 * 1000 * COIN;

    CKey payee;
    payee.MakeNewKey(true);
    CScript payee_script;
    payee_script.SetDestination(payee.GetPubKey().GetID());

    const auto paid_to_payee = [&](const CWalletTx& wtx) {
        CAmount paid = 0;
        for (const CTxOut& out : wtx.vout) if (out.scriptPubKey == payee_script) paid += out.nValue;
        return paid;
    };

    const auto change_of = [&](const CWalletTx& wtx) {
        CAmount change = 0;
        for (const CTxOut& out : wtx.vout) if (w.wallet.IsMine(out) != ISMINE_NO) change += out.nValue;
        return change;
    };

    {
        CWalletTx wtx;
        CReserveKey reserve(&w.wallet);
        int64_t fee = 0;
        BOOST_CHECK(!w.wallet.CreateTransaction({{payee_script, 500 * COIN}}, wtx, reserve, fee, &coin_control,
                                                false, false, {1}));
    }

    {
        CWalletTx wtx;
        CReserveKey reserve(&w.wallet);
        int64_t fee = 0;
        BOOST_REQUIRE(w.wallet.CreateTransaction({{payee_script, 500 * COIN}}, wtx, reserve, fee, &coin_control));
        BOOST_CHECK_EQUAL(fee, BASE_FEE);
        BOOST_CHECK_EQUAL(paid_to_payee(wtx), 500 * COIN);
        BOOST_CHECK_EQUAL(change_of(wtx), inputs - 500 * COIN - BASE_FEE);
    }

    {
        CWalletTx wtx;
        CReserveKey reserve(&w.wallet);
        int64_t fee = 0;
        BOOST_REQUIRE(w.wallet.CreateTransaction({{payee_script, inputs}}, wtx, reserve, fee, &coin_control,
                                                 false, false, {0}));
        BOOST_CHECK_EQUAL(fee, BASE_FEE);
        BOOST_CHECK_EQUAL(paid_to_payee(wtx), inputs - BASE_FEE);
        BOOST_CHECK_EQUAL(change_of(wtx), 0);
    }
}

BOOST_AUTO_TEST_SUITE_END()
