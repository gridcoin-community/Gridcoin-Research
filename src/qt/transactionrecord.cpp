#include "transactionrecord.h"
#include "wallet/wallet.h"
#include "base58.h"

std::string GetTxProject(uint256 hash, int& out_blocknumber, int& out_blocktype, double& out_rac);


/* Return positive answer if transaction should be shown in list. */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{

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
    if (!wtx.IsFromMe())
    {
        for (auto const& txout : wtx.vout)
        {
            if (txout.scriptPubKey == (CScript() << OP_RETURN))
                return false;
        }
    }
    return true;
}

int64_t GetMyValueOut(const CWallet *wallet, const CWalletTx &wtx)
{
    int64_t nValueOut = 0;
    for (auto const& txout : wtx.vout)
    {
       if (wallet->IsMine(txout) != ISMINE_NO)
       {
            nValueOut += txout.nValue;
       }
    }
    return nValueOut;
}


int64_t GetMyValueOut(const CWallet *wallet, const CWalletTx &wtx, unsigned int& index)
{
    int64_t nValueOut = 0;
    if (wallet->IsMine(wtx.vout[index]) != ISMINE_NO)
        nValueOut += wtx.vout[index].nValue;

    return nValueOut;
}

unsigned int GetNumberOfStakeReturnOutputs(const CWallet *wallet,const CWalletTx &wtx)
{
    unsigned int nNumberOfOutputs = 0;

    if (wtx.IsCoinStake())
    {
        for (auto const& txout: wtx.vout)
        {
            // Count the number of outputs that have a pubkey equal to the first (non-empty) output.
            // This are the stakesplits.
            if (txout.scriptPubKey == wtx.vout[1].scriptPubKey)
                nNumberOfOutputs++;
        }
    }

    return nNumberOfOutputs;
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
    uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    int64_t nCoinStakeReturnOutput = 0;
    unsigned int iCoinStakeReturnOutputIndex = 1;

    if (nNet > 0 || wtx.IsCoinBase() || wtx.IsCoinStake())
    {
        // Cannot use range based loop anymore
        for (unsigned int t = 0; t < wtx.vout.size(); t++)
        //for (auto const& txout : wtx.vout)
        {
            if(wallet->IsMine(wtx.vout[t]) != ISMINE_NO)
            {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = parts.size(); // sequence number
                sub.vout = t;

                if (ExtractDestination(wtx.vout[t].scriptPubKey, address) && (IsMine(*wallet, address) != ISMINE_NO))
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

                if (wtx.IsCoinBase())
                {
                    // Generated (proof-of-work)
                    sub.type = TransactionRecord::Generated;
                    // This is legacy.
                    sub.credit = wtx.vout[t].nValue;
                }
                else if (wtx.IsCoinStake())
                {
                    //POR-POS CoinStake, so change transaction record type to generated.
                    sub.type = TransactionRecord::Generated;

                    unsigned int nNumberOfStakeReturnOutputs  = GetNumberOfStakeReturnOutputs(wallet, wtx);

                    // The number of StakeOutputs should never be zero for the coinstake.
                    if (nNumberOfStakeReturnOutputs == 0)
                    {
                        LogPrintf("ERROR: decomposeTransaction: nNumberOfStakeReturnOutputs = 0. Adjusting to 1.");
                        nNumberOfStakeReturnOutputs = 1;
                    }

                    // If the output address does not match the input (which is the same as the first non-empty output),
                    // then this is a sidestake and we are on the receiving side, so no debit, otherwise, the debit is
                    // apportioned and coalesced.
                    if (wtx.vout[t].scriptPubKey != wtx.vout[1].scriptPubKey)
                        sub.credit = GetMyValueOut(wallet, wtx, t);
                    else
                    {
                        // Accumulate/coalesce splitstake output (returns) to the same address
                        nCoinStakeReturnOutput += GetMyValueOut(wallet, wtx, t);

                        if (iCoinStakeReturnOutputIndex < nNumberOfStakeReturnOutputs)
                        {
                            // Do not allow to flow down to parts.append(sub). Increment output index counter.
                            iCoinStakeReturnOutputIndex++;
                            continue;
                        }
                        else
                        {
                            // We are on the last splitstake return, so apply the debit and allow to go to parts.append(sub).
                            sub.credit = nCoinStakeReturnOutput - nDebit;
                        }
                    }
                } else
                    sub.credit = wtx.vout[t].nValue;

                parts.append(sub);
            }
        }
    }
    else
    {
        bool fAllFromMe = true;
        for (auto const& txin : wtx.vin)
            fAllFromMe = fAllFromMe && (wallet->IsMine(txin) != ISMINE_NO);

        bool fAllToMe = true;
        for (auto const& txout : wtx.vout)
            fAllToMe = fAllToMe && (wallet->IsMine(txout) != ISMINE_NO);

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


