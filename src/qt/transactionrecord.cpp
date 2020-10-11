#include "transactionrecord.h"
#include "wallet/wallet.h"
#include "base58.h"

/* Return positive answer if transaction should be shown in list. */
bool TransactionRecord::showTransaction(const CWalletTx &wtx, bool datetime_limit_flag, const int64_t &datetime_limit)
{

    // Do not show transactions earlier than the datetime_limit if the flag is set.
    if (datetime_limit_flag && (int64_t) wtx.nTime < datetime_limit)
    {
        return false;
    }

    std::string ShowOrphans = GetArg("-showorphans", "false");

	//R Halford - POS Transactions - If Orphaned follow showorphans directive:
	if (wtx.IsCoinStake() && !wtx.IsInMainChain())
	{
	       //Orphaned tx
		   return (ShowOrphans=="true" ? true : false);
    }

    if (wtx.IsCoinBase())
    {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain())
        {
            return false;
        }
    }

    // Suppress OP_RETURN transactions if they did not originate from you.
    // This is not "very" taxing but necessary since the transaction is in the wallet already.
    // We only do this for older transactions, because this legacy error does not occur
    // anymore, and we can't filter entire transactions that have OP_RETURNs, since
    // some outputs are relevant with the new contract types, such as messages.
    //
    // The selected timestamp represents 2018-11-12, a date that shortly follows
    // the next mandatory release (v4.0.0) hard-fork after the fix for OP_RETURN
    // filtering merged. No wallet databases should contain transactions for any
    // !IsFromMe() OP_RETURN outputs created after that time.
    //
    if (wtx.nTime < 1542000000 && !wtx.IsFromMe())
    {
        for (auto const& txout : wtx.vout)
        {
            if (txout.scriptPubKey == (CScript() << OP_RETURN))
                return false;
        }
    }
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.GetTxTime();
    int64_t nCredit = wtx.GetCredit(true);
    int64_t nDebit = wtx.GetDebit();
    int64_t nNet = nCredit - nDebit;
    size_t wtx_size = wtx.vout.size();
    uint256 hash = wtx.GetHash();

    std::map<std::string, std::string> mapValue = wtx.mapValue;

    bool fContractPresent = false;
    // Initialize to unknown to prevent a possible uninitialized warning.
    GRC::ContractType ContractType = GRC::ContractType::UNKNOWN;

    if (!wtx.GetContracts().empty())
    {
         const auto& contract = wtx.GetContracts().begin();
         fContractPresent = true;
         ContractType = contract->m_type.Value();
    }

    // This is legacy CoinBase for PoW, no longer used.
    if (wtx.IsCoinBase())
    {
        for (const auto& txout :wtx.vout)
        {
            if (wallet->IsMine(txout) != ISMINE_NO)
            {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = parts.size(); // sequence number

                if (ExtractDestination(txout.scriptPubKey, address))
                {
                    sub.address = CBitcoinAddress(address).ToString();
                }

                // Generated (proof-of-work)
                sub.type = TransactionRecord::Generated;
                sub.credit = txout.nValue;

                parts.append(sub);
            }
        }
    }
    // Since we are now separating out the sent sidestake info
    // into a separate subtransaction, we need to include the entire
    // value of the coinstake transaction here, rather than the previous
    // counting of only IsMine outputs.
    else if (wtx.IsCoinStake())
    {
        // We check the first coinstake output (zero is empty) for IsMine to
        // determine how to characterize the entire coinstake transaction. The
        // sidestakes to other (not mine) addresses are accounted for as negatives
        // in a separate subtransaction. The first output is ALWAYS guaranteed to be
        // the stake return to the original owner, and so matches the input.
        if (wallet->IsMine(wtx.vout[1]) != ISMINE_NO)
        {
            TransactionRecord sub(hash, nTime);
            CTxDestination address;
            sub.idx = parts.size();
            sub.vout = 1;

            sub.type = TransactionRecord::Generated;
            // The coinstake HAS to be from an address.
            if(ExtractDestination(wtx.vout[1].scriptPubKey, address))
            {
                sub.address = CBitcoinAddress(address).ToString();
            }

            // Here we add up all of the outputs, whether they are ours (the stake return with
            // apportioned reward, or not (sidestake), because the part that is not ours
            // will be accounted in the separated sidestake send transaction.
            sub.credit = 0;
            for (const auto& txout : wtx.vout)
            {
                sub.credit += txout.nValue;
            }

            sub.debit = -nDebit;

            // Append the subtransaction to the parts QList (transaction record).
            parts.append(sub);
        }

        // We only want outputs > 1 because the zeroth output is always empty,
        // and the first output is always the staker's. Output 2 onwards may or
        // may not be a sidestake, depending on whether stakesplitting is active,
        // or whether sidestaking is even turned on.
        // There is no coalescing here. A separate subtransaction is created for each
        // sidestake.
        for (unsigned int t = 2; t < wtx_size; t++)
        {
            // If this is not a stake split AND either vout[1] is mine OR
            // vout[t] is mine
            if (wtx.vout[t].scriptPubKey != wtx.vout[1].scriptPubKey &&
                    (wallet->IsMine(wtx.vout[1]) != ISMINE_NO ||
                     wallet->IsMine(wtx.vout[t]) != ISMINE_NO))
            {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = parts.size(); // sequence number
                sub.vout = t;

                sub.type = TransactionRecord::Generated;

                if (ExtractDestination(wtx.vout[t].scriptPubKey, address))
                {
                    sub.address = CBitcoinAddress(address).ToString();
                }

                int64_t nValue = wtx.vout[t].nValue;

                if (wallet->IsMine(wtx.vout[t]) != ISMINE_NO)
                {
                    sub.credit = nValue;
                }
                else
                {
                    sub.debit = -nValue;
                }

                parts.append(sub);
            }
        }
    }
    else if (nNet > 0)
    {
        for (const auto& txout : wtx.vout)
        {
            if (wallet->IsMine(txout) != ISMINE_NO)
            {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = parts.size(); // sequence number

                if (ExtractDestination(txout.scriptPubKey, address))
                {
                    // Received by Bitcoin Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }

                if (fContractPresent && ContractType == GRC::ContractType::MESSAGE)
                {
                    sub.type = TransactionRecord::Message;
                }

                sub.credit = txout.nValue;

                parts.append(sub);
            }
        }
    }
    else // Everything else
    {
        bool fAllFromMe = true;
        for (auto const& txin : wtx.vin)
        {
            fAllFromMe = fAllFromMe && (wallet->IsMine(txin) != ISMINE_NO);

            // Once false, no point in continuing.
            if (!fAllFromMe) break;
        }

        bool fAllToMe = true;
        for (auto const& txout : wtx.vout)
        {
            fAllToMe = fAllToMe && (wallet->IsMine(txout) != ISMINE_NO);

            // Once false, no point in continuing.
            if (!fAllToMe) break;
        }

        if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            int64_t nChange = wtx.GetChange();

            parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                                           -(nDebit - nChange), nCredit - nChange, 0));
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            int64_t nTxFee = nDebit - wtx.GetValueOut();

            // for tracking message type display
            bool fMessageDisplayed = false;

            for (unsigned int nOut = 0; nOut < wtx.vout.size(); nOut++)
            {
                const CTxOut& txout = wtx.vout[nOut];
                TransactionRecord sub(hash, nTime);
                sub.idx = parts.size();

                if(wallet->IsMine(txout) != ISMINE_NO)
                {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                CTxDestination address;
                if (ExtractDestination(txout.scriptPubKey, address))
                {
                    // Sent to Bitcoin Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
                }

                int64_t nValue = txout.nValue;
                /* Add fee to first output */
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                // Determine if the transaction is a beacon advertisement or a vote.
                // For right now, there should only be one contract in a transaction.
                // We will simply select the first and only one. Note that we are
                // looping through the outputs one by one in the for loop above this,
                // So if we get here, we are not a coinbase or coinstake, and we are on
                // an output that isn't ours. The worst that can happen from this
                // simple approach is to label more than one output with the
                // first found contract type. For right now, this is sufficient, because
                // the contracts that are sent right now only contain two outputs,
                // the burn and the change. We will have to get more sophisticated
                // when we allow more than one contract per transaction.

                // Notice this doesn't mess with the value or debit, it simply
                // overrides the TransactionRecord enum type.
                if (fContractPresent)
                {
                    switch (ContractType)
                    {
                    case GRC::ContractType::BEACON:
                        sub.type = TransactionRecord::BeaconAdvertisement;
                        break;
                    case GRC::ContractType::POLL:
                        sub.type = TransactionRecord::Poll;
                        break;
                    case GRC::ContractType::VOTE:
                        sub.type = TransactionRecord::Vote;
                        break;
                    case GRC::ContractType::MESSAGE:
                        // Only display the message type for the first not is mine output
                        if (!fMessageDisplayed && wallet->IsMine(txout) == ISMINE_NO)
                        {
                            sub.type = TransactionRecord::Message;
                            fMessageDisplayed = true;
                        }
                        // Do not display the op return output for a send message contract separately.
                        else if (txout.scriptPubKey[0] == OP_RETURN)
                        {
                            continue;
                        }
                        break;
                    default:
                        break; // Suppress warning
                    }
                }

                parts.append(sub);
            }
        }
        else
        {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0, 0));
        }
    }

    return parts;
}

void TransactionRecord::updateStatus(const CWalletTx &wtx)
{
    AssertLockHeld(cs_main);
    // Determine transaction status

    // Find the block the tx is in
    CBlockIndex* pindex = NULL;
    BlockMap::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimeReceived,
        idx);
    status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity() > 0);
    status.depth = wtx.GetDepthInMainChain();
    status.cur_num_blocks = nBestHeight;

    if (!IsFinalTx(wtx, nBestHeight + 1))
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
        {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - nBestHeight;
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }

    // For generated transactions, determine maturity
    else if(type == TransactionRecord::Generated)
    {
        if (wtx.GetBlocksToMaturity() > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.IsInMainChain())
            {
                status.matures_in = wtx.GetBlocksToMaturity();

                // Check if the block was requested by anyone
                if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
                    status.status = TransactionStatus::MaturesWarning;
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
        {
            status.status = TransactionStatus::Offline;
        }
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
        }
        else if (status.depth < RecommendedNumConfirmations)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
}

bool TransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != nBestHeight;
}

std::string TransactionRecord::getTxID()
{
    return hash.ToString();
}


