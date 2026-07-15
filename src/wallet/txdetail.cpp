// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "wallet/txdetail.h"

#include "main.h"
#include "txdb.h"
#ifdef GetMessage
#undef GetMessage
#endif
#include "gridcoin/tx_message.h"
#include "key_io.h"
#include "wallet/wallet.h"

// Defined in rpc/rawtransaction.cpp; also used by the RPC layer.
std::vector<std::pair<std::string, std::string>> GetTxStakeBoincHashInfo(const CMerkleTx& mtx) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

namespace GRC {
namespace {

//! Mirror of the old TransactionDesc::FormatTxStatus branch structure, as
//! typed classification instead of a rendered string.
void FillFinality(WalletTxDetail& detail, const CWalletTx& wtx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    if (!IsFinalTx(wtx, nBestHeight + 1))
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
        {
            detail.finality = WalletTxDetail::Finality::OpenForBlocks;
            detail.finality_value = wtx.nLockTime - nBestHeight;
        }
        else
        {
            detail.finality = WalletTxDetail::Finality::OpenUntilTime;
            detail.finality_value = wtx.nLockTime;
        }
    }
    else
    {
        int nDepth = wtx.GetDepthInMainChain();
        detail.depth = nDepth;

        if (nDepth < 0)
            detail.finality = WalletTxDetail::Finality::Conflicted;

        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
            detail.finality = WalletTxDetail::Finality::Offline;

        else if (nDepth < TransactionRecord::RecommendedNumConfirmations)
            detail.finality = WalletTxDetail::Finality::Unconfirmed;

        else
            detail.finality = WalletTxDetail::Finality::Confirmed;
    }
}

} // namespace

WalletTxDetail FillWalletTxDetail(CWallet* wallet, CWalletTx& wtx, unsigned int vout)
{
    WalletTxDetail detail;
    detail.found = true;

    LOCK2(cs_main, wallet->cs_wallet);

    int64_t nTime = wtx.GetTxTime();
    int64_t nCredit = wtx.GetCredit();
    int64_t nDebit = wtx.GetDebit();
    int64_t nNet = nCredit - nDebit;

    FillFinality(detail, wtx);
    detail.requests = wtx.GetRequestCount();
    detail.time = nTime;

    // From / Source
    if (wtx.IsCoinBase())
    {
        detail.source = WalletTxDetail::Source::CoinBase;
    }
    else if (wtx.IsCoinStake())
    {
        detail.source = WalletTxDetail::Source::CoinStake;
        detail.generated_type = GetGeneratedType(wallet, wtx.GetHash(), vout);
    }
    else if (wtx.mapValue.count("from") && !wtx.mapValue["from"].empty())
    {
        detail.source = WalletTxDetail::Source::OnlineFrom;
        detail.from_value = wtx.mapValue["from"];
    }
    else if (nNet > 0)
    {
        // Offline credit: the first own output, when its destination has an
        // address-book entry, renders as "From: unknown / To: <own address>".
        for (auto const& txout : wtx.vout)
        {
            if (wallet->IsMine(txout) != ISMINE_NO)
            {
                CTxDestination address;

                if (ExtractDestination(txout.scriptPubKey, address) && (IsMine(*wallet, address) != ISMINE_NO))
                {
                    if (wallet->mapAddressBook.count(address))
                    {
                        detail.has_own_credit_line = true;
                        detail.own_credit_address = EncodeDestination(address);
                        detail.own_credit_label = wallet->mapAddressBook[address].name;
                    }
                }

                break;
            }
        }
    }

    // To
    if (wtx.mapValue.count("to") && !wtx.mapValue["to"].empty())
    {
        detail.has_to = true;
        detail.to_value = wtx.mapValue["to"];

        CTxDestination dest = DecodeDestination(detail.to_value);

        if (wallet->mapAddressBook.count(dest) && !wallet->mapAddressBook[dest].name.empty())
            detail.to_label = wallet->mapAddressBook[dest].name;
    }

    // Amount
    if (wtx.IsCoinBase() && nCredit == 0)
    {
        detail.amount_form = WalletTxDetail::AmountForm::CoinbaseImmature;

        int64_t nUnmatured = 0;

        for (auto const& txout : wtx.vout)
            nUnmatured += wallet->GetCredit(txout);

        detail.unmatured = nUnmatured;
        detail.in_main_chain = wtx.IsInMainChain();
        detail.matures_in = wtx.GetBlocksToMaturity();
    }
    else if (nNet > 0)
    {
        detail.amount_form = WalletTxDetail::AmountForm::NetCredit;
    }
    else
    {
        bool fAllFromMe = true;

        for (auto const& txin : wtx.vin)
            fAllFromMe = fAllFromMe && (wallet->IsMine(txin) != ISMINE_NO);

        bool fAllToMe = true;

        for (auto const& txout : wtx.vout)
            fAllToMe = fAllToMe && (wallet->IsMine(txout) != ISMINE_NO);

        if (fAllFromMe)
        {
            detail.amount_form = WalletTxDetail::AmountForm::FromMe;

            const bool have_map_to = wtx.mapValue.count("to") && !wtx.mapValue["to"].empty();

            for (auto const& txout : wtx.vout)
            {
                if (wallet->IsMine(txout) != ISMINE_NO)
                    continue;

                WalletTxDetail::DebitOut debit;
                debit.amount = txout.nValue;

                if (!have_map_to)
                {
                    CTxDestination address;

                    if (ExtractDestination(txout.scriptPubKey, address))
                    {
                        debit.has_address = true;
                        debit.address = EncodeDestination(address);

                        if (wallet->mapAddressBook.count(address))
                            debit.label = wallet->mapAddressBook[address].name;
                    }
                }

                detail.debits.push_back(std::move(debit));
            }

            if (fAllToMe)
            {
                detail.to_self = true;
                detail.self_value = nCredit - wtx.GetChange();
            }

            int64_t nTxFee = nDebit - wtx.GetValueOut();

            if (nTxFee > 0)
            {
                detail.has_fee = true;
                detail.fee = nTxFee;
            }
        }
        else
        {
            detail.amount_form = WalletTxDetail::AmountForm::Mixed;

            for (auto const& txin : wtx.vin)
                if (wallet->IsMine(txin) != ISMINE_NO)
                    detail.mixed_debits.push_back(wallet->GetDebit(txin));

            for (auto const& txout : wtx.vout)
                if (wallet->IsMine(txout) != ISMINE_NO)
                    detail.mixed_credits.push_back(wallet->GetCredit(txout));
        }
    }

    detail.net = nNet;

    if (wtx.mapValue.count("message") && !wtx.mapValue["message"].empty())
        detail.map_message = wtx.mapValue["message"];

    if (wtx.mapValue.count("comment") && !wtx.mapValue["comment"].empty())
        detail.map_comment = wtx.mapValue["comment"];

    detail.txid = wtx.GetHash().ToString();

    if (const auto* conf = wtx.state<TxStateConfirmed>())
    {
        detail.has_block_hash = true;
        detail.block_hash = conf->m_confirmed_block_hash.ToString();
    }

    detail.contract_message = GetMessage(wtx);

    if (wtx.IsCoinBase() || wtx.IsCoinStake())
    {
        detail.is_generated = true;
        detail.stake_info = GetTxStakeBoincHashInfo(wtx);
    }

    // The always-rendered debits/credits + raw-data section (the old renderer's
    // WillLogCategory(VERBOSE) || true block).
    for (auto const& txin : wtx.vin)
        if (wallet->IsMine(txin) != ISMINE_NO)
            detail.verbose_debits.push_back(wallet->GetDebit(txin));

    for (auto const& txout : wtx.vout)
        if (wallet->IsMine(txout) != ISMINE_NO)
            detail.verbose_credits.push_back(wallet->GetCredit(txout));

    detail.tx_dump = wtx.ToString();

    {
        CTxDB txdb("r"); // To fetch source txouts

        for (auto const& txin : wtx.vin)
        {
            COutPoint prevout = txin.prevout;

            CTransaction prev;

            if (txdb.ReadDiskTx(prevout.hash, prev))
            {
                if (prevout.n < prev.vout.size())
                {
                    const CTxOut& out = prev.vout[prevout.n];

                    WalletTxDetail::InputInfo input;
                    input.amount = out.nValue;
                    input.is_mine = wallet->IsMine(out) != ISMINE_NO;

                    CTxDestination address;

                    if (ExtractDestination(out.scriptPubKey, address))
                    {
                        input.has_address = true;
                        input.address = EncodeDestination(address);
                    }

                    detail.inputs.push_back(std::move(input));
                }
            }
        }
    }

    return detail;
}

} // namespace GRC
