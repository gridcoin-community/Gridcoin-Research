// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "checkpoints.h"
#include "dbwrapper.h"
#include "main.h"
#include "gridcoin/staking/kernel.h"
#include "node/blockstorage.h"
#include "policy/fees.h"
#include "serialize.h"
#include "util.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <set>

bool ReadTxFromDisk(CTransaction& tx, CDiskTxPos pos, FILE** pfileRet)
{
    tx.SetNull();

    CAutoFile filein(OpenBlockFile(pos.nFile, 0, pfileRet ? "rb+" : "rb"), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadTxFromDisk() : OpenBlockFile failed");

    // Read transaction
    if (fseek(filein.Get(), pos.nTxPos, SEEK_SET) != 0)
        return error("ReadTxFromDisk() : fseek failed");

    try {
        filein >> tx;
    }
    catch (std::exception &e) {
        return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
    }

    // Return file pointer
    if (pfileRet)
    {
        if (fseek(filein.Get(), pos.nTxPos, SEEK_SET) != 0)
            return error("ReadTxFromDisk() : second fseek failed");
        *pfileRet = filein.release();
    }
    return true;
}

bool ReadTxFromDisk(CTransaction& tx, CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
{
    tx.SetNull();
    if (!txdb.ReadTxIndex(prevout.hash, txindexRet))
        return false;
    if (!ReadTxFromDisk(tx, txindexRet.pos))
        return false;
    if (prevout.n >= tx.vout.size())
    {
        tx.SetNull();
        return false;
    }
    return true;
}

bool ReadTxFromDisk(CTransaction& tx, CTxDB& txdb, COutPoint prevout)
{
    CTxIndex txindex;
    return ReadTxFromDisk(tx, txdb, prevout, txindex);
}

bool ReadTxFromDisk(CTransaction& tx, COutPoint prevout)
{
    CTxDB txdb("r");
    CTxIndex txindex;
    return ReadTxFromDisk(tx, txdb, prevout, txindex);
}

bool CheckTransaction(const CTransaction& tx)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return tx.DoS(10, error("CheckTransaction() : vin empty"));
    if (tx.vout.empty())
        return tx.DoS(10, error("CheckTransaction() : vout empty"));
    // Size limits - don't count coinbase superblocks--we check this at the block level:
    if (GetSerializeSize(tx, (SER_NETWORK & SER_SKIPSUPERBLOCK), PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return tx.DoS(100, error("CheckTransaction() : size limits failed"));

    // Check for negative or overflow output values (see CVE-2010-5139)
    CAmount nValueOut = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        if (txout.IsEmpty() && !tx.IsCoinBase() && !tx.IsCoinStake())
            return tx.DoS(100, error("CheckTransaction() : txout empty for user transaction"));
        if (txout.nValue < 0)
            return tx.DoS(100, error("CheckTransaction() : txout.nValue negative"));
        if (txout.nValue > MAX_MONEY)
            return tx.DoS(100, error("CheckTransaction() : txout.nValue too high"));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return tx.DoS(100, error("CheckTransaction() : txout total out of range"));
    }
    // Check for duplicate inputs
    std::set<COutPoint> vInOutPoints;
    for (auto const& txin : tx.vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return tx.DoS(100, error("CheckTransaction() : coinbase script size is invalid"));
    }
    else
    {
        for (auto const& txin : tx.vin)
            if (txin.prevout.IsNull())
                return tx.DoS(10, error("CheckTransaction() : prevout is null"));
    }

    return true;
}

bool CheckContracts(const CTransaction& tx, const MapPrevTx& inputs, int block_height)
{
    if (tx.nVersion <= 1) {
        return true;
    }

    // Although v2 transactions support multiple contracts, we just allow one
    // for now to mitigate spam:
    if (tx.GetContracts().size() > 1) {
        return tx.DoS(100, error("%s: only one contract allowed in tx", __func__));
    }

    if ((tx.IsCoinBase() || tx.IsCoinStake())) {
        return tx.DoS(100, error("%s: contract in non-standard tx", __func__));
    }

    CAmount required_burn_fee = 0;

    for (const auto& contract : tx.GetContracts()) {
        if (contract.m_version <= 1) {
            return tx.DoS(100, error("%s: legacy contract", __func__));
        }

        if (!contract.WellFormed()) {
            return tx.DoS(100, error("%s: malformed contract", __func__));
        }

        // Reject any transactions with administrative contracts sent from a
        // wallet that does not hold the master key:
        if (contract.RequiresMasterKey() && !HasMasterKeyInput(tx, inputs, block_height)) {
            return tx.DoS(100, error("%s: contract requires master key", __func__));
        }

        required_burn_fee += contract.RequiredBurnAmount();
    }

    CAmount supplied_burn_fee = 0;

    for (const auto& output : tx.vout) {
        if (output.scriptPubKey[0] == OP_RETURN) {
            supplied_burn_fee += output.nValue;
        }
    }

    if (supplied_burn_fee < required_burn_fee) {
        return tx.DoS(100, error(
            "%s: insufficient burn output. Required: %s, supplied: %s",
            __func__,
            FormatMoney(required_burn_fee),
            FormatMoney(supplied_burn_fee)));
    }

    return true;
}

const CTxOut& GetOutputFor(const CTxIn& input, const MapPrevTx& inputs)
{
    MapPrevTx::const_iterator mi = inputs.find(input.prevout.hash);
    if (mi == inputs.end())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.hash not found");

    const CTransaction& txPrev = (mi->second).second;
    if (input.prevout.n >= txPrev.vout.size())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.n out of range");

    return txPrev.vout[input.prevout.n];
}

CAmount GetValueIn(const CTransaction& tx, const MapPrevTx& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    CAmount nResult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        nResult += GetOutputFor(tx.vin[i], inputs).nValue;
    }
    return nResult;

}

bool HasMasterKeyInput(const CTransaction& tx, const MapPrevTx& inputs, int block_height)
{
    const CTxDestination master_address = CWallet::MasterAddress(block_height).Get();

    for (const auto& input : tx.vin) {
        const CTxOut& prev_out = GetOutputFor(input, inputs);
        CTxDestination dest;

        if (!ExtractDestination(prev_out.scriptPubKey, dest)) {
            continue;
        }

        if (dest == master_address) {
            return true;
        }
    }

    return false;
}

bool DisconnectInputs(CTransaction& tx, CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!tx.IsCoinBase())
    {
        for (auto const& txin : tx.vin)
        {
            COutPoint prevout = txin.prevout;
            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            if (!txdb.UpdateTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : UpdateTxIndex failed");
        }
    }

    // Remove transaction from index
    // This can fail if a duplicate of this transaction was in a chain that got
    // reorganized away. This is only possible if this transaction was completely
    // spent, so erasing it would be a no-op anyway.
    txdb.EraseTxIndex(tx);

    return true;
}


bool FetchInputs(CTransaction& tx, CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool,
                 bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
    // FetchInputs can return false either because we just haven't seen some inputs
    // (in which case the transaction should be stored as an orphan)
    // or because the transaction is malformed (in which case the transaction should
    // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
    fInvalid = false;

    if (tx.IsCoinBase())
        return true; // Coinbase transactions have no inputs to fetch.

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint prevout = tx.vin[i].prevout;
        if (inputsRet.count(prevout.hash))
            continue; // Got it already

        // Read txindex
        CTxIndex& txindex = inputsRet[prevout.hash].first;
        bool fFound = true;
        if ((fBlock || fMiner) && mapTestPool.count(prevout.hash))
        {
            // Get txindex from current proposed changes
            txindex = mapTestPool.find(prevout.hash)->second;
        }
        else
        {
            // Read txindex from txdb
            fFound = txdb.ReadTxIndex(prevout.hash, txindex);
        }
        if (!fFound && (fBlock || fMiner))
            return fMiner ? false : error("FetchInputs() : %s prev tx %s index entry not found", tx.GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());

        // Read txPrev
        CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (!fFound || txindex.pos == CDiskTxPos(1,1,1))
        {
            // Get prev tx from single transactions in memory
            if (!mempool.lookup(prevout.hash, txPrev))
            {
                LogPrint(BCLog::LogFlags::VERBOSE, "FetchInputs() : %s mempool Tx prev not found %s", tx.GetHash().ToString().substr(0,10),  prevout.hash.ToString().substr(0,10));
                return false;
            }
            if (!fFound)
                txindex.vSpent.resize(txPrev.vout.size());
        }
        else
        {
            // Get prev tx from disk
            if (!ReadTxFromDisk(txPrev, txindex.pos))
                return error("FetchInputs() : %s ReadFromDisk prev tx %s failed", tx.GetHash().ToString().substr(0,10).c_str(),  prevout.hash.ToString().substr(0,10).c_str());
        }
    }

    // Make sure all prevout.n indexes are valid:
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const COutPoint prevout = tx.vin[i].prevout;
        assert(inputsRet.count(prevout.hash) != 0);
        const CTxIndex& txindex = inputsRet[prevout.hash].first;
        const CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
        {
            // Revisit this if/when transaction replacement is implemented and allows
            // adding inputs:
            fInvalid = true;
            return tx.DoS(100, error("FetchInputs() : %s prevout.n out of range %d %" PRIszu " %" PRIszu " prev tx %s\n%s", tx.GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));
        }
    }

    return true;
}

bool ConnectInputs(CTransaction& tx, CTxDB& txdb, MapPrevTx inputs, std::map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx,
    const CBlockIndex* pindexBlock, bool fBlock, bool fMiner)
{
    // Take over previous transactions' spent pointers
    // fBlock is true when this is called from AcceptBlock when a new best-block is added to the blockchain
    // fMiner is true when called from the internal bitcoin miner
    // ... both are false when called from CTransaction::AcceptToMemoryPool
    if (!tx.IsCoinBase())
    {
        int64_t nValueIn = 0;
        int64_t nFees = 0;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            COutPoint prevout = tx.vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex& txindex = inputs[prevout.hash].first;
            CTransaction& txPrev = inputs[prevout.hash].second;

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
                return tx.DoS(100, error("ConnectInputs() : %s prevout.n out of range %d %" PRIszu " %" PRIszu " prev tx %s\n%s", tx.GetHash().ToString().substr(0,10).c_str(), prevout.n, txPrev.vout.size(), txindex.vSpent.size(), prevout.hash.ToString().substr(0,10).c_str(), txPrev.ToString().c_str()));

            // If prev is coinbase or coinstake, check that it's matured
            if (txPrev.IsCoinBase() || txPrev.IsCoinStake())
                for (const CBlockIndex* pindex = pindexBlock; pindex && pindexBlock->nHeight - pindex->nHeight < nCoinbaseMaturity; pindex = pindex->pprev)
                    if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
                        return error("ConnectInputs() : tried to spend %s at depth %d", txPrev.IsCoinBase() ? "coinbase" : "coinstake", pindexBlock->nHeight - pindex->nHeight);

            // ppcoin: check transaction timestamp
            if (txPrev.nTime > tx.nTime)
                return tx.DoS(100, error("ConnectInputs() : transaction timestamp earlier than input transaction"));

            // Check for negative or overflow input values
            nValueIn += txPrev.vout[prevout.n].nValue;
            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return tx.DoS(100, error("ConnectInputs() : txin values out of range"));

        }
        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            COutPoint prevout = tx.vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex& txindex = inputs[prevout.hash].first;
            CTransaction& txPrev = inputs[prevout.hash].second;

            // Check for conflicts (double-spend)
            // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
            // for an attacker to attempt to split the network.
            if (!txindex.vSpent[prevout.n].IsNull())
            {
                if (fMiner)
                {
                    return false;
                }
                if (!txindex.vSpent[prevout.n].IsNull())
                {
                    if (fTestNet && pindexBlock->nHeight < nGrandfather)
                    {
                        return fMiner ? false : true;
                    }
                    if (!fTestNet && pindexBlock->nHeight < nGrandfather)
                    {
                        return fMiner ? false : true;
                    }

                    if (fMiner) return false;
                    return LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) ? error("ConnectInputs() : %s prev tx already used at %s", tx.GetHash().ToString().c_str(), txindex.vSpent[prevout.n].ToString().c_str()) : false;
                }

            }

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.

            if (!(fBlock && (nBestHeight < Params().Checkpoints().GetHeight())))
            {
                // Verify signature
                if (!VerifySignature(txPrev, tx, i, 0))
                {
                    return tx.DoS(100,error("ConnectInputs() : %s VerifySignature failed", tx.GetHash().ToString().substr(0,10).c_str()));
                }
            }

            // Mark outpoints as spent
            txindex.vSpent[prevout.n] = posThisTx;

            // Write back
            if (fBlock || fMiner)
            {
                mapTestPool[prevout.hash] = txindex;
            }
        }

        if (!tx.IsCoinStake())
        {
            if (nValueIn < tx.GetValueOut())
            {
                LogPrintf("ConnectInputs(): VALUE IN < VALUEOUT ");
                return tx.DoS(100, error("ConnectInputs() : %s value in < value out", tx.GetHash().ToString().substr(0,10).c_str()));
            }

            // Tally transaction fees
            CAmount nTxFee = nValueIn - tx.GetValueOut();
            if (nTxFee < 0)
                return tx.DoS(100, error("ConnectInputs() : %s nTxFee < 0", tx.GetHash().ToString().substr(0,10).c_str()));

            // enforce transaction fees for every block
            if (nTxFee < GetMinFee(tx))
                return fBlock? tx.DoS(100, error("ConnectInputs() : %s not paying required fee=%s, paid=%s", tx.GetHash().ToString().substr(0,10).c_str(), FormatMoney(GetMinFee(tx)).c_str(), FormatMoney(nTxFee).c_str())) : false;

            nFees += nTxFee;
            if (!MoneyRange(nFees))
                return tx.DoS(100, error("ConnectInputs() : nFees out of range"));
        }
    }

    return true;
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool GetCoinAge(const CTransaction& tx, CTxDB& txdb, uint64_t& nCoinAge)
{
    CBigNum bnCentSecond = 0;  // coin age in the unit of cent-seconds
    nCoinAge = 0;

    if (tx.IsCoinBase())
        return true;

    for (auto const& txin : tx.vin)
    {
        // First try finding the previous transaction in database
        CBlockHeader header;
        CTransaction txPrev;

        if (!GRC::ReadStakedInput(txdb, txin.prevout.hash, header, txPrev))
        {
            return false;
        }

        if (tx.nTime < txPrev.nTime)
        {
            return false; // Transaction timestamp violation
        }

        if (header.GetBlockTime() + nStakeMinAge > tx.nTime)
        {
            continue; // only count coins meeting min age requirement
        }

        CAmount nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += CBigNum(nValueIn) * (tx.nTime - txPrev.nTime) / CENT;

        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && gArgs.GetBoolArg("-printcoinage"))
            LogPrintf("coin age nValueIn=%" PRId64 " nTimeDiff=%d bnCentSecond=%s", nValueIn, tx.nTime - txPrev.nTime, bnCentSecond.ToString());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE) && gArgs.GetBoolArg("-printcoinage"))
        LogPrintf("coin age bnCoinDay=%s", bnCoinDay.ToString());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}


int GetDepthInMainChain(const CTxIndex& txi)
{
    // Read block header
    CBlock block;
    if (!ReadBlockFromDisk(block, txi.pos.nFile, txi.pos.nBlockPos, Params().GetConsensus(), false))
        return 0;
    // Find the block in the index
    BlockMap::iterator mi = mapBlockIndex.find(block.GetHash(true));
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = mi->second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}


bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return error("%s: nBits below minimum work", __func__);

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return error("%s: hash doesn't match nBits", __func__);

    return true;
}

