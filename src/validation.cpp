// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "checkpoints.h"
#include "consensus/merkle.h"
#include "dbwrapper.h"
#include "main.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/mrc.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/staking/chain_trust.h"
#include "gridcoin/staking/reward.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/exceptions.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/spam.h"
#include "gridcoin/tally.h"
#include "node/blockstorage.h"
#include "policy/fees.h"
#include "serialize.h"
#include "util.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <set>
#include <stdexcept>

extern GRC::SeenStakes g_seen_stakes;
extern GRC::ChainTrustCache g_chain_trust;

static constexpr CAmount nGenesisSupply = 340569880;
bool fColdBoot = true;

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

bool DisconnectBlock(CBlock& block, CTxDB& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    bool bDiscTxFailed = false;
    for (int i = block.vtx.size() - 1; i >= 0; i--)
    {
        if (!DisconnectInputs(block.vtx[i], txdb))
        {
            bDiscTxFailed = true;
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    // Brod: I do not like this...
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext.SetNull();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("%s: WriteBlockIndex failed", __func__);
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    for (auto const& tx : block.vtx)
        SyncWithWallets(tx, &block, false, false);

    if (bDiscTxFailed) return error("%s: DisconnectInputs failed", __func__);
    return true;
}

unsigned int GetCoinstakeOutputLimit(const int& block_version)
{
    // This is the coinstake output size limit for block versions up through v9 (i.e. prior to the implementation of
    // splitting and sidestaking).
    unsigned int output_limit = 3;

    // This is the coinstake output size limit for block versions 10 and 11.
    if (block_version >= 10 && block_version < 12) {
        output_limit = 8;
    } else if (block_version >= 12) {
        // For v12 blocks and above, this is increased by two for the non-MRC part to accommodate anticipated mandatory
        // non-MRC foundation sidestakes, and further increased by the GetMRCOutputLimit.
        output_limit = 10 + GetMRCOutputLimit(block_version, true);
    }

    return output_limit;
}

Fraction FoundationSideStakeAllocation()
{
    // Note that the 4/5 (80%) for mainnet was approved by a validated poll,
    // id 651a3d7cbb797ee06bd8c2b17c415223d77bb296434866ddf437a42b6d1e9d89.
    return Fraction(4, 5);
}

CBitcoinAddress FoundationSideStakeAddress() {
    CBitcoinAddress foundation_address;

    // If on testnet set foundation destination address to test wallet address
    if (fTestNet) {
        foundation_address.SetString("mfiy9sc2QEZZCK3WMUMZjNfrdRA6gXzRhr");

        return foundation_address;
    }

    // Will get here if not on testnet. The below address is the current multisignature address for the foundation
    foundation_address.SetString("bc3NA8e8E3EoTL1qhRmeprbjWcmuoZ26A2");

    return foundation_address;
}

unsigned int GetMRCOutputLimit(const int& block_version, bool include_foundation_sidestake)
{
    // For block versions below v12 no MRC outputs are allowed.
    unsigned int output_limit = 0;

    // We have decided for the limit to be 10 rather than 5 for v12+ blocks to provide for more reward slots. This
    // should reduce the pressure for slots in the initial stages of MRC.
    // For testnet is it set to a much lower number 3 to facilitate easier overflow testing.
    if (block_version >= 12) {
        output_limit = fTestNet ? 3 : 10;
    }

    // If the include_foundation_sidestake is false (meaning that the foundation sidestake should not be counted
    // in the returned limit) AND the foundation sidestake allocation is greater than zero, then reduce the reported
    // output limit by 1. If the foundation sidestake allocation is zero, then there will be no foundation sidestake
    // output, so the output_limit should be as above. If the output limit was already zero then it remains zero.
    if (!include_foundation_sidestake && FoundationSideStakeAllocation().isNonZero() && output_limit) {
        --output_limit;
    }

    return output_limit;
}

//
// Gridcoin-specific ConnectBlock() routines:
//
namespace {
int64_t ReturnCurrentMoneySupply(CBlockIndex* pindexcurrent)
{
    if (pindexcurrent->pprev)
    {
        // If previous exists, and previous money supply > Genesis, OK to use it:
        if (pindexcurrent->pprev->nHeight > 11 && pindexcurrent->pprev->nMoneySupply > nGenesisSupply)
        {
            return pindexcurrent->pprev->nMoneySupply;
        }
    }
    // Special case where block height < 12, use standard old logic:
    if (pindexcurrent->nHeight < 12)
    {
        return (pindexcurrent->pprev? pindexcurrent->pprev->nMoneySupply : 0);
    }
    // At this point, either the last block pointer was nullptr, or the client erased the money supply previously, fix it:
    CBlockIndex* pblockIndex = pindexcurrent;
    CBlockIndex* pblockMemory = pindexcurrent;
    int nMinDepth = (pindexcurrent->nHeight)-140000;
    if (nMinDepth < 12) nMinDepth=12;
    while (pblockIndex->nHeight > nMinDepth)
    {
        pblockIndex = pblockIndex->pprev;
        LogPrintf("Money Supply height %d", pblockIndex->nHeight);

        if (pblockIndex == nullptr || !pblockIndex->IsInMainChain()) continue;
        if (pblockIndex == pindexGenesisBlock)
        {
            return nGenesisSupply;
        }
        if (pblockIndex->nMoneySupply > nGenesisSupply)
        {
            //Set index back to original pointer
            pindexcurrent = pblockMemory;
            //Return last valid money supply
            return pblockIndex->nMoneySupply;
        }
    }
    // At this point, we fall back to the old logic with a minimum of the genesis supply (should never happen - if it did, blockchain will need rebuilt anyway due to other fields being invalid):
    pindexcurrent = pblockMemory;
    return (pindexcurrent->pprev? pindexcurrent->pprev->nMoneySupply : nGenesisSupply);
}

bool GetCoinstakeAge(CTxDB& txdb, const CBlock& block, uint64_t& out_coin_age)
{
    out_coin_age = 0;

    // ppcoin: coin stake tx earns reward instead of paying fee
    //
    // With block version 10, Gridcoin switched to constant block rewards
    // that do not depend on coin age, so we can avoid reading the blocks
    // and transactions from the disk. The CheckProofOfStake*() functions
    // of the kernel verify the transaction timestamp and that the staked
    // inputs exist in the main chain.
    //
    if (block.nVersion <= 9 && !GetCoinAge(block.vtx[1], txdb, out_coin_age)) {
        return error("ConnectBlock[] : %s unable to get coin age for coinstake",
            block.vtx[1].GetHash().ToString().substr(0,10));
    }

    return true;
}

//!
//! \brief Checks reward claims in generated blocks.
//!
class ClaimValidator
{
public:
    ClaimValidator(
        const CBlock& block,
        const CBlockIndex* const pindex,
        const CAmount stake_value_in,
        const CAmount total_claimed,
        const CAmount fees,
        const CAmount coin_age)
        : m_block(block)
        , m_pindex(pindex)
        , m_claim(block.GetClaim())
        , m_stake_value_in(stake_value_in)
        , m_total_claimed(total_claimed)
        , m_fees(fees)
        , m_coin_age(coin_age)
    {
    }

    bool Check() const
    {
        return m_claim.HasResearchReward()
            ? CheckResearcherClaim()
            : CheckInvestorClaim();
    }

private:
    const CBlock& m_block;
    const CBlockIndex* const m_pindex;
    const GRC::Claim& m_claim;
    const int64_t m_stake_value_in;
    const int64_t m_total_claimed;
    const int64_t m_fees;
    const uint64_t m_coin_age;

    bool CheckReward(const CAmount& research_owed, CAmount& out_stake_owed,
                     const CAmount& mrc_staker_fees_owed, const CAmount& mrc_fees,
                     const CAmount& mrc_rewards, const unsigned int& mrc_non_zero_outputs) const
    {
        out_stake_owed = GRC::GetProofOfStakeReward(m_coin_age, m_block.nTime, m_pindex);

        if (m_block.nVersion >= 11) {
            // For block version 11, mrc_fees_owed and mrc_rewards are both zero, and there are no MRC outputs, so this is
            // the only check necessary.
            if (m_total_claimed > research_owed + out_stake_owed + m_fees + mrc_fees + mrc_rewards) {
                return error("%s: CheckReward FAILED: m_total_claimed of %s > %s = research_owed %s + out_stake_owed %s + m_fees %s + "
                             "mrc_fees %s + mrc_rewards = %s",
                             __func__,
                             FormatMoney(m_total_claimed),
                             FormatMoney(research_owed + out_stake_owed + m_fees + mrc_fees + mrc_rewards),
                             FormatMoney(research_owed),
                             FormatMoney(out_stake_owed),
                             FormatMoney(m_fees),
                             FormatMoney(mrc_fees),
                             FormatMoney(mrc_rewards)
                             );
            }

            // With MRC there is quite a bit more to do.
            if (m_block.nVersion >= 12) {
                // Check that the portion of the coinstake going to the staker and/or their sidestakes (i.e. the non-MRC
                // part) is proper. The net of the non-MRC outputs and the input cannot exceed
                // research_owed + out_stake_owed + m_fees + staker_fees_owed.
                const CTransaction& coinstake = m_block.vtx[1];

                // Note this uses m_claim.m_mrc_tx_map.size() and not mrc_non_zero_outputs, because if mrcs are forced
                // in the zero payout interval and the foundation side stake is active, there will be a foundation mrc
                // sidestake even though there will not be a corresponding mrc rewards output. (Zero value outputs are
                // suppressed because that is wasteful.
                bool foundation_mrc_sidestake_present = (m_claim.m_mrc_tx_map.size()
                                                         && FoundationSideStakeAllocation().isNonZero()) ? true : false;

                // If there is no mrc, then this is coinstake.vout.size() - 0 - 0, which is one beyond the last coinstake
                // element.
                // If there is one non-zero net reward mrc and the foundation sidestake is turned off, then this would be
                // coinstake.vout.size() - 1 - 0.
                // If there is one non-zero net reward mrc and the foundation sidestake is turned on, then this would be
                // coinstake.vout.size() - 1 - 1.
                // For a "full" coinstake, consisting of eight staker outputs (reward + sidestakes), the foundation
                // sidestake, and the 4 MRC sidestakes (currently 14 total), this would be
                // coinstake.vout.size() = 14 - 4 - 1 = 9, which correctly points to the foundation sidestake index.
                unsigned int mrc_start_index = coinstake.vout.size()
                        - mrc_non_zero_outputs
                        - (unsigned int) foundation_mrc_sidestake_present;

                // Start with the coinstake input value as a negative (because we want the net).
                CAmount total_owed_to_staker = -m_stake_value_in;

                // These are the non-MRC outputs.
                for (unsigned int i = 0; i < mrc_start_index; ++i) {
                    total_owed_to_staker += coinstake.vout[i].nValue;
                }

                if (total_owed_to_staker > research_owed + out_stake_owed + m_fees + mrc_staker_fees_owed) {
                    return error("%s: FAILED: total_owed_to_staker of %s > %s = research_owed %s + out_stake_owed %s + "
                                 "mrc_fees %s + mrc_rewards = %s",
                                 __func__,
                                 FormatMoney(total_owed_to_staker),
                                 FormatMoney(research_owed + out_stake_owed + m_fees + mrc_staker_fees_owed),
                                 FormatMoney(research_owed),
                                 FormatMoney(out_stake_owed),
                                 FormatMoney(mrc_fees),
                                 FormatMoney(mrc_rewards)
                                 );
                }

                // If the foundation mrc sidestake is present, we check the foundation sidestake specifically. The MRC
                // outputs were already checked by CheckMRCRewards.
                if (foundation_mrc_sidestake_present) {
                    // The fee amount to the foundation must be correct.
                    if (coinstake.vout[mrc_start_index].nValue != mrc_fees - mrc_staker_fees_owed) {
                        return error("%s: FAILED: foundation output value of %s != mrc_fees %s - "
                                     "mrc_staker_fees_owed %s",
                                     __func__,
                                     FormatMoney(coinstake.vout[mrc_start_index].nValue),
                                     FormatMoney(mrc_fees),
                                     FormatMoney(mrc_staker_fees_owed)
                                     );
                    }

                    CTxDestination foundation_sidestake_destination;

                    // The foundation sidestake destination must be able to be extracted.
                    if (!ExtractDestination(coinstake.vout[mrc_start_index].scriptPubKey,
                                            foundation_sidestake_destination)) {
                        return error("%s: FAILED: foundation MRC sidestake destination not valid", __func__);
                    }

                    // The sidestake destination must match that specified by FoundationSideStakeAddress().
                    if (foundation_sidestake_destination != FoundationSideStakeAddress().Get()) {
                        return error("%s: FAILED: foundation MRC sidestake destination does not match protocol",
                                     __func__);
                    }

                }
            } // v12+

            // If we get here, we are done with v11 and v12 validation so return true.
            return true;
        } //v11+

        // Blocks version 10 and below represented rewards as floating-point
        // values and needed to accommodate floating-point errors so we'll do
        // the same rounding on the floating-point representations:
        //
        double subsidy = ((double)research_owed / COIN) * 1.25;
        subsidy += (double)out_stake_owed / COIN;

        CAmount max_owed = roundint64(subsidy * COIN) + m_fees;

        // Block version 9 and below allowed a 1 GRC wiggle.
        if (m_block.nVersion <= 9) {
            max_owed += 1 * COIN;
        }

        return m_total_claimed <= max_owed;
    }

    bool CheckInvestorClaim() const
    {
        CAmount mrc_rewards = 0;
        CAmount mrc_staker_fees = 0;
        CAmount mrc_fees = 0;
        CAmount out_stake_owed;
        unsigned int mrc_non_zero_outputs = 0;

        // Even if the block is staked by an investor, the claim can include MRC payments to researchers...
        //
        // If block version 12 or higher, this checks the MRC part of the claim, and also returns the total mrc_fees,
        // which are needed because are part of the total claimed. Note that the DoS and log output for MRC
        // validation failure is handled in the CheckMRCRewards method.
        if (m_block.nVersion >= 12 && !CheckMRCRewards(mrc_rewards, mrc_staker_fees,
                                                       mrc_fees, mrc_non_zero_outputs)) {
            return false;
        }

        if (CheckReward(0, out_stake_owed, mrc_staker_fees, mrc_fees, mrc_rewards, mrc_non_zero_outputs)) {
            LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: CheckReward passed: m_total_claimed = %s, research_owed = %s, "
                                               "out_stake_owed = %s, m_fees = %s, mrc_staker_fees = %s, mrc_fees = %s, "
                                               "mrc_rewards = %s",
                      __func__,
                      FormatMoney(m_total_claimed),
                      FormatMoney(0),
                      FormatMoney(out_stake_owed),
                      FormatMoney(m_fees),
                      FormatMoney(mrc_staker_fees),
                      FormatMoney(mrc_fees),
                      FormatMoney(mrc_rewards)
                      );

            return true;
        }


        if (GRC::GetBadBlocks().count(m_pindex->GetBlockHash())) {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored bad investor claim on block %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        return m_block.DoS(10, error(
            "ConnectBlock[%s]: investor claim %s exceeds %s. Expected %s, fees %s",
            __func__,
            FormatMoney(m_total_claimed),
            FormatMoney(out_stake_owed + m_fees),
            FormatMoney(out_stake_owed),
            FormatMoney(m_fees)));
    }

    bool CheckResearcherClaim() const
    {
        // For version 11 blocks and higher, just validate the reward and check
        // the signature. No need for the rest of these shenanigans.
        //
        if (m_block.nVersion >= 11) {
            return CheckResearchReward() && CheckBeaconSignature();
        }

        if (!CheckResearchRewardLimit()) {
            return false;
        }

        if (!CheckResearchRewardDrift()) {
            return false;
        }

        if (m_block.nVersion <= 8) {
            return true;
        }

        if (!CheckClaimMagnitude()) {
            return false;
        }

        if (!CheckBeaconSignature()) {
            return false;
        }

        if (!CheckResearchReward()) {
            return false;
        }

        return true;
    }

    bool CheckResearchRewardLimit() const
    {
        // TODO: determine max reward from accrual computer implementation:
        const CAmount max_reward = 12750 * COIN;

        return m_claim.m_research_subsidy <= max_reward
            || m_block.DoS(1, error(
                "ConnectBlock[%s]: research claim %s exceeds max %s. CPID %s",
                __func__,
                FormatMoney(m_claim.m_research_subsidy),
                FormatMoney(max_reward),
                m_claim.m_mining_id.ToString()));
    }

    bool CheckResearchRewardDrift() const
    {
        // ResearchAge: Since the best block may increment before the RA is
        // connected but After the RA is computed, the ResearchSubsidy can
        // sometimes be slightly smaller than we calculate here due to the
        // RA timespan increasing.  So we will allow for time shift before
        // rejecting the block.
        const CAmount reward_claimed = m_total_claimed - m_fees;
        CAmount drift_allowed = m_claim.m_research_subsidy * 0.15;

        if (drift_allowed < 10 * COIN) {
            drift_allowed = 10 * COIN;
        }

        return m_claim.TotalSubsidy() + drift_allowed >= reward_claimed
            || m_block.DoS(20, error(
                "ConnectBlock[%s]: reward claim %s exceeds allowed %s. CPID %s",
                __func__,
                FormatMoney(reward_claimed),
                FormatMoney(m_claim.TotalSubsidy() + drift_allowed),
                m_claim.m_mining_id.ToString()));
    }

    bool CheckClaimMagnitude() const
    {
        // Magnitude as of the last superblock:
        const double mag = GRC::Quorum::GetMagnitude(m_claim.m_mining_id).Floating();

        return m_claim.m_magnitude <= (mag * 1.25)
            || m_block.DoS(20, error(
                "ConnectBlock[%s]: magnitude claim %f exceeds superblock %f. CPID %s",
                __func__,
                m_claim.m_magnitude,
                mag,
                m_claim.m_mining_id.ToString()));
    }

    bool CheckBeaconSignature() const
    {
        const GRC::CpidOption cpid = m_claim.m_mining_id.TryCpid();

        if (!cpid) {
            // Investor claims are not signed by a beacon key.
            return false;
        }

        // The legacy beacon functions determined beacon expiration by the time
        // of the previous block. For block version 11+, compute the expiration
        // threshold from the current block:
        //
        const int64_t now = m_block.nVersion >= 11 ? m_block.nTime : m_pindex->pprev->nTime;

        if (const GRC::BeaconOption beacon = GRC::GetBeaconRegistry().TryActive(*cpid, now)) {
            if (m_claim.VerifySignature(
                beacon->m_public_key,
                m_pindex->pprev->GetBlockHash(),
                m_block.vtx[1]))
            {
                return true;
            }
        }

        if (GRC::GetBadBlocks().count(m_pindex->GetBlockHash())) {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored invalid signature in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        // An old bug caused some nodes to sign research reward claims with a
        // previous beacon key (beaconalt). Mainnet declares block exceptions
        // for this problem. To avoid declaring exceptions for the 55 testnet
        // blocks, the following check ignores beaconalt verification failure
        // for the range of heights that include these blocks:
        //
        if (fTestNet
            && (m_pindex->nHeight >= 495352 && m_pindex->nHeight <= 600876))
        {
            LogPrintf(
                "WARNING: %s: likely testnet beaconalt signature ignored in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        return m_block.DoS(20, error(
            "ConnectBlock[%s]: signature verification failed. CPID %s, LBH %s",
            __func__,
            m_claim.m_mining_id.ToString(),
            m_pindex->pprev->GetBlockHash().ToString()));
    }

    bool CheckResearchReward() const
    {
        CAmount research_owed = 0;
        CAmount mrc_rewards = 0;
        CAmount mrc_staker_fees = 0;
        CAmount mrc_fees = 0;
        unsigned int mrc_non_zero_outputs = 0;

        const GRC::CpidOption cpid = m_claim.m_mining_id.TryCpid();

        if (cpid) {
            research_owed = GRC::Tally::GetAccrual(*cpid, m_block.nTime, m_pindex);
        }

        // If block version 12 or higher, this checks the MRC part of the claim, and also returns the staker_fees,
        // which are needed because they are added to the stakers payout. Note that the DoS and log output for MRC
        // validation failure is handled in the CheckMRCRewards method.
        if (m_block.nVersion >= 12 && !CheckMRCRewards(mrc_rewards, mrc_staker_fees,
                                                       mrc_fees, mrc_non_zero_outputs)) {
            return false;
        }

        CAmount out_stake_owed;
        if (CheckReward(research_owed, out_stake_owed, mrc_staker_fees, mrc_fees, mrc_rewards, mrc_non_zero_outputs)) {
            LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: Post CheckReward: m_total_claimed = %s, research_owed = %s, "
                                               "out_stake_owed = %s, mrc_staker_fees = %s, mrc_fees = %s, mrc_rewards = %s",
                     __func__,
                     FormatMoney(m_total_claimed),
                     FormatMoney(research_owed),
                     FormatMoney(out_stake_owed),
                     FormatMoney(mrc_staker_fees),
                     FormatMoney(mrc_fees),
                     FormatMoney(mrc_rewards)
                     );

            return true;
        } else if (m_pindex->nHeight >= GetOrigNewbieSnapshotFixHeight()) {
            // The below is required to deal with a conditional application in historical rewards for
            // research newbies after the original newbie fix height that already made it into the chain.
            // Please see the extensive commentary in the below function.
            CAmount newbie_correction = GRC::Tally::GetNewbieSuperblockAccrualCorrection(*cpid,
                                                                                         GRC::Quorum::CurrentSuperblock());
            research_owed += newbie_correction;

            if (CheckReward(research_owed, out_stake_owed, mrc_staker_fees, mrc_fees, mrc_rewards, mrc_non_zero_outputs)) {
                LogPrintf("WARNING: ConnectBlock[%s]: Added newbie_correction of %s to calculated research owed. "
                          "Total calculated research with correction matches claim of %s in %s.",
                          __func__,
                          FormatMoney(newbie_correction),
                          FormatMoney(m_total_claimed),
                          m_pindex->GetBlockHash().ToString());

                return true;
            }
        }

        // Testnet contains some blocks with bad interest claims that were masked
        // by research age short 10-block-span pending accrual:
        if (fTestNet
            && m_block.nVersion <= 9
            && !CheckReward(0, out_stake_owed, 0, 0, 0, 0))
        {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored bad testnet claim in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        if (GRC::GetBadBlocks().count(m_pindex->GetBlockHash())) {
            LogPrintf(
                "WARNING: ConnectBlock[%s]: ignored bad research claim in %s",
                __func__,
                m_pindex->GetBlockHash().ToString());

            return true;
        }

        return m_block.DoS(10, error(
            "ConnectBlock[%s]: researcher claim %s exceeds %s for CPID %s. "
            "Expected research %s, stake %s, fees %s. "
            "Claimed research %s, stake %s",
            __func__,
            FormatMoney(m_total_claimed),
            FormatMoney(research_owed + out_stake_owed + m_fees),
            m_claim.m_mining_id.ToString(),
            FormatMoney(research_owed),
            FormatMoney(out_stake_owed),
            FormatMoney(m_fees),
            FormatMoney(m_claim.m_research_subsidy),
            FormatMoney(m_claim.m_block_subsidy)));
    }

    // Cf. CreateMRCRewards which is this method's conjugate. Note the parameters are out parameters.
    //
    // Note that it is possible that someone could try and circumvent the 100% fee penalty for submitting an MRC within
    // the zero payout interval by purposefully miscomputing the fee. This is one of the reasons why the fee is
    // recomputed by the checking node as part of the ValidateMRC function. There are a number of checks done here to
    // detect possible abuses of the coinstake and ensure that the MRC portion of the coinstake corresponds to the claim.
    bool CheckMRCRewards(CAmount& mrc_rewards, CAmount& mrc_staker_fees,
                         CAmount& mrc_fees, unsigned int& non_zero_outputs) const
    {
        // For convenience
        const CTransaction& coinstake = m_block.vtx[1];

        unsigned int mrc_outputs = 0;
        unsigned int mrc_output_limit = GetMRCOutputLimit(m_block.nVersion, false);
        unsigned int mrc_claimed_outputs = m_claim.m_mrc_tx_map.size();

        // If the number of MRCs in the claim's mrc tx map exceeds the output limit, then validation fails. This would be
        // a problem introduced by the staking node and should get the same DoS as other claim validation errors above.
        if (mrc_claimed_outputs > mrc_output_limit) {
            return m_block.DoS(10, error(
                                   "ConnectBlock[%s]: MRC claimed outputs, %u, exceeds max of %u excluding foundation "
                                   "sidestake.",
                                   __func__,
                                   mrc_claimed_outputs,
                                   mrc_output_limit));
        }

        if (mrc_output_limit > 0) {
            Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

            for (const auto& tx : m_block.vtx) {
                for (const auto& mrc : m_claim.m_mrc_tx_map) {
                    if (mrc.second == tx.GetHash()) {
                        for (const auto& contract : tx.GetContracts()) {
                            // We only are processing MRC contracts here in this loop.
                            if (contract.m_type != GRC::ContractType::MRC) continue;

                            GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                            if (const GRC::CpidOption cpid = mrc.m_mining_id.TryCpid()) {
                                CBlockIndex* mrc_index = mapBlockIndex[mrc.m_last_block_hash];

                                const GRC::BeaconOption beacon = GRC::GetBeaconRegistry().TryActive(*cpid, mrc_index->nTime);

                                if (beacon) {
                                    CBitcoinAddress beacon_address = beacon->GetAddress();
                                    CScript script_beacon_key;
                                    script_beacon_key.SetDestination(beacon_address.Get());

                                    // If an MRC fails validation no point in continuing. Return false immediately with an
                                    // appropriate DoS.
                                    if (!ValidateMRC(m_pindex->pprev, mrc)) {
                                        return m_block.DoS(10, error(
                                                               "ConnectBlock[%s]: An MRC in the claim failed to validate.",
                                                               __func__));
                                    }

                                    CAmount mrc_reward = mrc.m_research_subsidy - mrc.m_fee;

                                    // From the signature verification above, we already know that the beacon's public key
                                    // matches what is recorded in the beacon registry. We need to check that the actual
                                    // output on the coinstake corresponding to the MRC is paid to the beacon's public key
                                    // (address) and is the correct amount.
                                    CAmount coinstake_mrc_reward = 0;

                                    CScript mrc_beacon_script_public_key;
                                    mrc_beacon_script_public_key.SetDestination(beacon->GetAddress().Get());

                                    // Start at the first actual MRC output. The MRC outputs come last in the coinstake.
                                    // We must exclude the other outputs, because it is possible, and allowed, for a
                                    // sidestake to be specified by the staker that is the same address as the staker's
                                    // beacon address, or some other person's beacon address. We do not want to intermix
                                    // these.
                                    //
                                    // Find the matched output on the coinstake.
                                    if (mrc_reward) {
                                        for (unsigned int i = coinstake.vout.size() - mrc_claimed_outputs;
                                             i < coinstake.vout.size(); ++i) {
                                            // If an output on the coinstake matches the public key on the beacon and the
                                            // rewards match the output, then add the value to coinstake_mrc_reward. To detect
                                            // possible duplicates slipped in, add up all outputs where the public key matches.
                                            // By protocol there should be only one MRC output that matches the key.

                                            if (mrc_beacon_script_public_key == coinstake.vout[i].scriptPubKey) {
                                                LogPrint(BCLog::LogFlags::VERBOSE,
                                                         "INFO: %s: coinstake output matched to MRC.",
                                                         __func__);

                                                coinstake_mrc_reward += coinstake.vout[i].nValue;
                                                ++non_zero_outputs;
                                            }
                                        }
                                    }

                                    // If this doesn't match we have a problem with the correlation between the coinstake
                                    // and the claim in the MRC area.
                                    if (coinstake_mrc_reward != mrc_reward) {
                                        return m_block.DoS(10, error(
                                                               "ConnectBlock[%s]: The rewards in an MRC, %s, do not "
                                                               "correspond to the value of the corresponding output "
                                                               "on the coinstake, %s.",
                                                               __func__,
                                                               FormatMoney(mrc_reward),
                                                               FormatMoney(coinstake_mrc_reward)));
                                    }

                                    // The net reward paid to the MRC beacon address is the requested research subsidy
                                    // (reward) minus the fees for the MRC. Accumulate to mrc_rewards
                                    mrc_rewards += mrc_reward;

                                    mrc_fees += mrc.m_fee;
                                    mrc_staker_fees += mrc.m_fee - mrc.m_fee * foundation_fee_fraction.GetNumerator()
                                                                             / foundation_fee_fraction.GetDenominator();

                                    ++mrc_outputs;

                                    LogPrintf("INFO: %s: mrc %u validated: m_client_version = %u, m_fee = %s, "
                                              "m_last_block_hash = %s, m_mining_id = %s, m_organization = %s, "
                                              "m_research_subsidy = %s, m_version = %u, mrc_rewards = %s, "
                                              "mrc_fees = %s, mrc_staker_fees = %s",
                                              __func__,
                                              mrc_outputs,
                                              mrc.m_client_version,
                                              FormatMoney(mrc.m_fee),
                                              mrc.m_last_block_hash.GetHex(),
                                              mrc.m_mining_id.ToString(),
                                              mrc.m_organization,
                                              FormatMoney(mrc.m_research_subsidy),
                                              mrc.m_version,
                                              FormatMoney(mrc_rewards),
                                              FormatMoney(mrc_fees),
                                              FormatMoney(mrc_staker_fees));

                                } //beacon
                            } // cpid
                        } // GetContracts iteration

                        // There cannot be more than one hash in the mrc_tx_map that matches the iterator tx hash
                        break;
                    } // tx that has hash that matches mrc_tx_map entry
                } // mrc_tx_map iteration
            } // block tx iteration
        } // output_limit > 0

        // The only remaining condition to check is whether the number of claims across the transactions are actually LESS
        // than the number claimed. This should NOT be the case because of the integrity required by mrc_tx_map, but check
        // anyway. The above loop is GUARANTEED not to return a number of validated claims GREATER than the size of the
        // mrc_tx_map, because the hashes in the mrc_tx_map (the values) are also unique. The greater than condition is
        // checked first above.
        if (mrc_outputs < mrc_claimed_outputs) {
            return m_block.DoS(10, error(
                                   "ConnectBlock[%s]: The number of validated claims in MRC transactions, %u, "
                                   "is less than the number of MRCs in the stake claim, %u.",
                                   __func__,
                                   mrc_outputs,
                                   mrc_claimed_outputs));
        }

        // If we arrive here, the MRCRewards are valid.

        // Signal MRCChanged because this method is called from ConnectBlock and the successful validation of an MRC
        // indicates an MRC state change.
        uiInterface.MRCChanged();

        return true;
    }
}; // ClaimValidator

bool TryLoadSuperblock(
    CBlock& block,
    const CBlockIndex* const pindex,
    const GRC::Claim& claim)
{
    GRC::SuperblockPtr superblock = block.GetSuperblock(pindex);

    // TODO: find the invalid historical superblocks so we can remove
    // the fColdBoot condition that skips this check when syncing the
    // initial chain:
    //
    if ((!fColdBoot || block.nVersion >= 11)
        && !GRC::Quorum::ValidateSuperblockClaim(claim, superblock, pindex))
    {
        return block.DoS(25, error("ConnectBlock: Rejected invalid superblock."));
    }

    // Block versions 11+ calculate research rewards from snapshots of
    // accrual taken at each superblock:
    //
    if (block.nVersion >= 11) {
        if (!GRC::Tally::ApplySuperblock(superblock)) {
            return false;
        }

        GRC::GetBeaconRegistry().ActivatePending(
                    superblock->m_verified_beacons.m_verified,
                    superblock.m_timestamp,
                    block.GetHash(),
                    pindex->nHeight);

        // Notify the GUI if present that beacons have changed.
        uiInterface.BeaconChanged();
    }

    GRC::Quorum::PushSuperblock(std::move(superblock));

    return true;
}

bool GridcoinConnectBlock(
    CBlock& block,
    CBlockIndex* const pindex,
    CTxDB& txdb,
    const int64_t stake_value_in,
    const int64_t total_claimed,
    const int64_t fees)
{
    const GRC::Claim& claim = block.GetClaim();

    if (pindex->nHeight > nGrandfather) {
        uint64_t out_coin_age;
        if (!GetCoinstakeAge(txdb, block, out_coin_age)) {
            return false;
        }

        if (!ClaimValidator(block, pindex, stake_value_in, total_claimed, fees, out_coin_age).Check()) {
            return false;
        }

        if (claim.ContainsSuperblock()) {
            if (!TryLoadSuperblock(block, pindex, claim)) {
                return false;
            }

            pindex->MarkAsSuperblock();
        } else if (block.nVersion <= 10) {
            // Block versions 11+ validate superblocks from scraper convergence
            // instead of the legacy quorum system so we only record votes from
            // version 10 blocks and below:
            //
            GRC::Quorum::RecordVote(claim.m_quorum_hash, claim.m_quorum_address, pindex);
        }
    }

    bool found_contract;

    GRC::BeaconRegistry& beacons = GRC::GetBeaconRegistry();

    int beacon_db_height = beacons.GetDBHeight();

    // Note this does NOT handle mrc's. The recording of MRC's is a block level event controlled by the claim.
    // See below.
    GRC::ApplyContracts(block, pindex, beacon_db_height, found_contract);

    if (found_contract) {
        pindex->MarkAsContract();
    }

    double magnitude = 0;

    if (block.nVersion >= 11
        && claim.m_mining_id.Which() == GRC::MiningId::Kind::CPID)
    {
        magnitude = GRC::Quorum::GetMagnitude(claim.m_mining_id).Floating();
    } else {
        magnitude = claim.m_magnitude;
    }

    pindex->SetResearcherContext(claim.m_mining_id, claim.m_research_subsidy, magnitude);

    // This populates the MRC researcher context(s) if there are MRC recipients in the block.
    // We start at 2, because the MRC request transactions themselves cannot be in the coinbase
    // or coinstake.
    for (unsigned int i = 2; i < block.vtx.size(); ++i) {
        //For convenience
        auto& tx = block.vtx[i];

        for (const auto& mrc : claim.m_mrc_tx_map) {
            if (mrc.second == tx.GetHash()) {

                for (const auto& contract : tx.GetContracts()) {
                    if (contract.m_type != GRC::ContractType::MRC) continue;

                    GRC::MRC mrc_payload = contract.CopyPayloadAs<GRC::MRC>();

                    pindex->AddMRCResearcherContext(mrc_payload.m_mining_id,
                                                    mrc_payload.m_research_subsidy,
                                                    mrc_payload.m_magnitude);
                }

                // There cannot be more than one hash in the mrc_tx_map that matches the iterator tx hash
                break;
            }
        }
    }

    GRC::Tally::RecordRewardBlock(pindex);
    GRC::Researcher::Refresh();

    return true;
}
} // Anonymous namespace

bool ConnectBlock(CBlock& block, CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(block, pindex->nHeight, !fJustCheck, !fJustCheck, false, false))
    {
        return error("%s: CheckBlock failed", __func__);
    }

    unsigned int nTxPos;
    if (fJustCheck) {
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    } else {
        nTxPos = pindex->nBlockPos
            + ::GetSerializeSize<CBlockHeader>(block, SER_DISK, CLIENT_VERSION)
            + GetSizeOfCompactSize(block.vtx.size());
    }

    std::map<uint256, CTxIndex> mapQueuedChanges;
    int64_t nFees = 0;
    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    int64_t nStakeReward = 0;
    int64_t stake_value_in = 0;
    unsigned int nSigOps = 0;

    bool bIsDPOR = false;

    if (block.nVersion >= 8 && pindex->nStakeModifier == 0)
    {
        uint256 tmp_hashProof;
        if (!GRC::CheckProofOfStakeV8(txdb, pindex->pprev, block, /*generated_by_me*/ false, tmp_hashProof))
            return error("%s: check proof-of-stake failed", __func__);
    }

    for (auto &tx : block.vtx)
    {
        uint256 hashTx = tx.GetHash();

        // Do not allow blocks that contain transactions which 'overwrite' older transactions,
        // unless those are already completely spent.
        // If such overwrites are allowed, coinbases and transactions depending upon those
        // can be duplicated to remove the ability to spend the first instance -- even after
        // being sent to another address.
        // See BIP30, CVE-2012-1909, and http://r6.ca/blog/20120206T005236Z.html for more information.
        // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
        // already refuses previously-known transaction ids entirely.
        // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
        // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
        // two in the chain that violate it. This prevents exploiting the issue against nodes in their
        // initial block download.
        CTxIndex txindexOld;
        if (txdb.ReadTxIndex(hashTx, txindexOld)) {
            for (auto const& pos : txindexOld.vSpent)
                if (pos.IsNull())
                    return false;
        }

        nSigOps += GetLegacySigOpCount(tx);
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return block.DoS(100, error("%s: too many sigops", __func__));

        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
        {
            nValueOut += tx.GetValueOut();
        }
        else
        {
            bool fInvalid;
            if (!FetchInputs(tx, txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
                return false;

            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += GetP2SHSigOpCount(tx, mapInputs);
            if (nSigOps > MAX_BLOCK_SIGOPS)
                return block.DoS(100, error("%s: too many sigops", __func__));

            CAmount nTxValueIn = GetValueIn(tx, mapInputs);
            CAmount nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
            {
                // Notice that "sidestaking" comes OUT of the research stakers rewards, so the split outputs back to the
                // staker + the sidestake outputs add up to the original unsplit reward + fees from the other transactions.
                //
                // With the addition of MRC, the nStakeReward will also include the MRC outputs, which are additional
                // generated coins on top of the generated coins directly due to the staker. Note that the total mrc fees
                // are split between the staker and a foundation output, but this is added back together by
                // nTxValueOut - nTxValueIn. So... the total claimed is
                // staker outputs + sidestake outputs + foundation output (if present) + MRC outputs.
                // The staker outputs + sidestake outputs + foundation output
                // = staker reward + transaction fees + staker mrc fees + foundation mrc fees
                // = staker reward + transaction fees +  total mrc fees.
                nStakeReward = nTxValueOut - nTxValueIn;
                stake_value_in = nTxValueIn;
                if (tx.vout.size() > 3 && pindex->nHeight > nGrandfather) bIsDPOR = true;

                if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY))
                {
                    int64_t nTotalCoinstake = 0;
                    for (unsigned int i = 0; i < tx.vout.size(); i++)
                    {
                        nTotalCoinstake += tx.vout[i].nValue;
                    }
                    LogPrint(BCLog::LogFlags::NOISY, " nHeight %d; nTCS %f; nTxValueOut %f",
                                              pindex->nHeight,CoinToDouble(nTotalCoinstake),CoinToDouble(nTxValueOut));
                }

                if (pindex->nVersion >= 10)
                {
                    if (tx.vout.size() > GetCoinstakeOutputLimit(pindex->nVersion))
                        return block.DoS(100, error("%s: too many coinstake outputs", __func__));
                }
                else if (bIsDPOR && pindex->nHeight > nGrandfather && pindex->nVersion < 10)
                {
                    // Old rules, does not make sense
                    // Verify no recipients exist after coinstake
                    // (Recipients start at output position 3 (0=Coinstake flag, 1=coinstake amount, 2=splitstake amount)
                    for (unsigned int i = 3; i < tx.vout.size(); i++)
                    {
                        if (CoinToDouble(tx.vout[i].nValue) > 0)
                        {
                            return block.DoS(50, error("%s: coinstake output %u forbidden", __func__, i));
                        }
                    }
                }
            }

            // Validate any contracts published in the transaction:
            if (!tx.GetContracts().empty()) {
                if (!CheckContracts(tx, mapInputs, pindex->nHeight)) {
                    return false;
                }

                int DoS = 0;
                if (block.nVersion >= 11 && !GRC::BlockValidateContracts(pindex, tx, DoS)) {
                    return tx.DoS(DoS, error("%s: invalid contract in tx %s, assigning DoS misbehavior of %i",
                                             __func__,
                                             tx.GetHash().ToString(),
                                             DoS));
                }
            }

            if (!ConnectInputs(tx, txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false))
                return false;
        }

        mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());
    }

    if (IsResearchAgeEnabled(pindex->nHeight)
        && !GridcoinConnectBlock(block, pindex, txdb, stake_value_in, nStakeReward, nFees))
    {
        return false;
    }

    pindex->nMoneySupply = ReturnCurrentMoneySupply(pindex) + nValueOut - nValueIn;

    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("%s: WriteBlockIndex for pindex failed", __func__);

    if (!OutOfSyncByAge())
    {
        fColdBoot = false;
    }

    if (fJustCheck)
        return true;

    // Write queued txindex changes
    for (const auto& [hash, index] : mapQueuedChanges)
    {
        if (!txdb.UpdateTxIndex(hash, index))
            return error("%s: UpdateTxIndex failed", __func__);
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("%s: WriteBlockIndex failed", __func__);
    }

    // Watch for transactions paying to me
    for (auto const& tx : block.vtx)
        SyncWithWallets(tx, &block, true);

    return true;
}

bool AddToBlockIndex(CBlock& block, unsigned int nFile, unsigned int nBlockPos, const uint256& hashProof)
{
    // Check for duplicate
    uint256 hash = block.GetHash(true);
    if (mapBlockIndex.count(hash))
        return error("%s: %s already exists", __func__, hash.ToString().substr(0,20));

    // Construct new block index object
    CBlockIndex* pindexNew = GRC::BlockIndexPool::GetNextBlockIndex();
    *pindexNew = CBlockIndex(nFile, nBlockPos, block);

    if (!pindexNew)
        return error("%s: new CBlockIndex failed", __func__);
    pindexNew->phashBlock = &hash;
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = miPrev->second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // ppcoin: compute stake entropy bit for stake modifier
    if (!pindexNew->SetStakeEntropyBit(block.GetStakeEntropyBit()))
        return error("%s: SetStakeEntropyBit failed", __func__);

    // Record proof hash value
    pindexNew->hashProof = hashProof;

    // ppcoin: compute stake modifier
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    if (!GRC::ComputeNextStakeModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier))
    {
        LogPrintf("%s: ComputeNextStakeModifier failed", __func__);  // div72: Do we really need to log here?
    }
    pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);

    // Add to mapBlockIndex
    BlockMap::iterator mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &(mi->first);

    // Write to disk block index
    CTxDB txdb;
    if (!txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));
    if (!txdb.TxnCommit())
        return false;

    LOCK(cs_main);

    // New best
    if (g_chain_trust.Favors(pindexNew))
        if (!SetBestChain(txdb, block, pindexNew))
            return false;

    if (pindexNew == pindexBest)
    {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = block.vtx[0].GetHash();
    }

    return true;
}

bool CheckBlock(const CBlock& block, int height1, bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig, bool fLoadingIndex)
{
    // Allow the genesis block to pass.
    if(block.hashPrevBlock.IsNull() &&
       block.GetHash(true) == (fTestNet ? hashGenesisBlockTestNet : hashGenesisBlock))
        return true;

    if (block.fChecked)
        return true;

    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (block.vtx.empty()
        || block.vtx.size() > MAX_BLOCK_SIZE
        || ::GetSerializeSize(block, (SER_NETWORK & SER_SKIPSUPERBLOCK), PROTOCOL_VERSION) > MAX_BLOCK_SIZE
        || ::GetSerializeSize(block.GetSuperblock(), SER_NETWORK, PROTOCOL_VERSION) > GRC::Superblock::MAX_SIZE)
    {
        return block.DoS(100, error("%s: size limits failed", __func__));
    }

    // Check proof of work matches claimed amount
    if (fCheckPOW && block.IsProofOfWork() && !CheckProofOfWork(block.GetHash(true), block.nBits, Params().GetConsensus()))
        return block.DoS(50, error("%s: proof of work failed", __func__));

    //Reject blocks with diff that has grown to an extraordinary level (should never happen)
    double blockdiff = GRC::GetBlockDifficulty(block.nBits);
    if (height1 > nGrandfather && blockdiff > 10000000000000000)
    {
       return block.DoS(1, error("%s: Block Bits larger than 10000000000000000.", __func__));
    }

    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0].IsCoinBase())
        return block.DoS(100, error("%s: first tx is not coinbase", __func__));
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].IsCoinBase())
            return block.DoS(100, error("%s: more than one coinbase", __func__));

    // Version 11+ blocks store the Gridcoin claim context as a contract in the
    // coinbase transaction instead of the hashBoinc field.
    //
    if (block.nVersion >= 11) {
        if (block.vtx[0].vContracts.empty()) {
            return block.DoS(100, error("%s: missing claim contract", __func__));
        }

        if (block.vtx[0].vContracts.size() > 1) {
            return block.DoS(100, error("%s: too many coinbase contracts", __func__));
        }

        if (block.vtx[0].vContracts[0].m_type != GRC::ContractType::CLAIM) {
            return block.DoS(100, error("%s: unexpected coinbase contract", __func__));
        }

        if (!block.vtx[0].vContracts[0].WellFormed()) {
            return block.DoS(100, error("%s: malformed claim contract", __func__));
        }

        if (block.vtx[0].vContracts[0].m_version <= 1 || block.GetClaim().m_version <= 1) {
            return block.DoS(100, error("%s: legacy claim", __func__));
        }

        if (!fTestNet && block.GetClaim().m_version == 2) {
            return block.DoS(100, error("%s: testnet-only claim", __func__));
        }
    }

    // End of Proof Of Research
    if (block.IsProofOfStake())
    {
        // Gridcoin: check proof-of-stake block signature
        if (height1 > nGrandfather)
        {
            if (fCheckSig && !CheckBlockSignature(block))
                return block.DoS(100, error("%s: bad proof-of-stake block signature", __func__));
        }

        // Coinbase output should be empty if proof-of-stake block
        if (block.vtx[0].vout.size() != 1 || !block.vtx[0].vout[0].IsEmpty())
            return block.DoS(100, error("%s: coinbase output not empty for proof-of-stake block", __func__));

        // Second transaction must be coinstake, the rest must not be
        if (block.vtx.empty() || !block.vtx[1].IsCoinStake())
            return block.DoS(100, error("%s: second tx is not coinstake", __func__));

        for (unsigned int i = 2; i < block.vtx.size(); i++)
        {
            if (block.vtx[i].IsCoinStake())
            {
                return block.DoS(100, error("%s: more than one coinstake (at %d)", __func__, i));
            }
        }
    }

    // Check transactions
    for (auto const& tx : block.vtx)
    {
        if (!CheckTransaction(tx))
            return block.DoS(tx.nDoS, error("%s: CheckTransaction failed", __func__));

        // ppcoin: check transaction timestamp
        if (block.GetBlockTime() < (int64_t)tx.nTime)
            return block.DoS(50, error("%s: block timestamp earlier than transaction timestamp", __func__));
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    std::set<uint256> uniqueTx;
    for (auto const& tx : block.vtx)
    {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != block.vtx.size())
        return block.DoS(100, error("%s: duplicate transaction", __func__));

    unsigned int nSigOps = 0;
    for (auto const& tx : block.vtx)
    {
        nSigOps += GetLegacySigOpCount(tx);
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return block.DoS(100, error("%s: out-of-bounds SigOpCount", __func__));

    // Check merkle root
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot = BlockMerkleRoot(block, &mutated);
        if (block.hashMerkleRoot != hashMerkleRoot)
            return block.DoS(100, error("%s: hashMerkleRoot mismatch", __func__));

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return block.DoS(100, error("%s: duplicate transaction", __func__));
    }

    if (fCheckPOW && fCheckMerkleRoot && fCheckSig)
        block.fChecked = true;

    return true;
}

bool AcceptBlock(CBlock& block, bool generated_by_me) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    if (block.nVersion > CBlock::CURRENT_VERSION)
        return block.DoS(100, error("%s: reject unknown block version %d", __func__, block.nVersion));

    // Check for duplicate
    uint256 hash = block.GetHash(true);
    if (mapBlockIndex.count(hash))
        return error("%s: block already in mapBlockIndex", __func__);

    // Get prev block index
    BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return block.DoS(10, error("%s: prev block not found", __func__));
    CBlockIndex* pindexPrev = mi->second;
    const int nHeight = pindexPrev->nHeight + 1;
    const int checkpoint_height = Params().Checkpoints().GetHeight();

    // Ignore blocks that connect at a height below the hardened checkpoint. We
    // use a cheaper condition than IsInitialBlockDownload() to skip the checks
    // during initial sync:
    if (nBestHeight > checkpoint_height && nHeight <= checkpoint_height) {
        return block.DoS(25, error("%s: rejected height below checkpoint", __func__));
    }

    // The block height at which point we start rejecting v7 blocks and
    // start accepting v8 blocks.
    if ((IsProtocolV2(nHeight) && block.nVersion < 7)
            || (IsV8Enabled(nHeight) && block.nVersion < 8)
            || (IsV9Enabled(nHeight) && block.nVersion < 9)
            || (IsV10Enabled(nHeight) && block.nVersion < 10)
            || (IsV11Enabled(nHeight) && block.nVersion < 11)
            || (IsV12Enabled(nHeight) && block.nVersion < 12)
            ) {
        return block.DoS(20, error("%s: reject too old nVersion = %d", __func__, block.nVersion));
    } else if ((!IsProtocolV2(nHeight) && block.nVersion >= 7)
               || (!IsV8Enabled(nHeight) && block.nVersion >= 8)
               || (!IsV9Enabled(nHeight) && block.nVersion >= 9)
               || (!IsV10Enabled(nHeight) && block.nVersion >= 10)
               || (!IsV11Enabled(nHeight) && block.nVersion >= 11)
               || (!IsV12Enabled(nHeight) && block.nVersion >= 12)
               ) {
        return block.DoS(100, error("%s: reject too new nVersion = %d", __func__, block.nVersion));
    }

    if (block.IsProofOfWork() && nHeight > LAST_POW_BLOCK)
        return block.DoS(100, error("%s: reject proof-of-work at height %d", __func__, nHeight));

    if (nHeight > nGrandfather)
    {
        // Check coinbase timestamp
        if (block.GetBlockTime() > FutureDrift((int64_t)block.vtx[0].nTime, nHeight))
        {
            return block.DoS(80, error("%s: coinbase timestamp is too early", __func__));
        }
        // Check timestamp against prev
        if (block.nVersion < 12 && (block.GetBlockTime() <= pindexPrev->GetPastTimeLimit() || FutureDrift(block.GetBlockTime(), nHeight) < pindexPrev->GetBlockTime()))
            return block.DoS(60, error("%s: block's timestamp is too early", __func__));
        // Check proof-of-work or proof-of-stake
        if (block.nBits != GRC::GetNextTargetRequired(pindexPrev))
            return block.DoS(100, error("%s: incorrect target", __func__));
    }

    if (block.nVersion >= 12) {
        // Check timestamp
        if (pindexPrev->GetBlockTime() - block.GetBlockTime() > 128) {
            return block.DoS(50, error("%s: block timestamp too early", __func__));
        }

        if (block.GetBlockTime() - GetAdjustedTime() > 128) {
            return block.DoS(25, error("%s: block timestamp too far in future", __func__));
        }

        if (block.vtx[1].nTime != block.GetBlockTime()) {
            return block.DoS(50, error("%s: block coinstake time mismatch", __func__));
        }
    }


    for (auto const& tx : block.vtx)
    {
        // Mandatory switch to binary contracts (tx version 2):
        if (block.nVersion >= 11 && tx.nVersion < 2) {
            // Disallow tx version 1 after the mandatory block to prohibit the
            // use of legacy string contracts:
            return block.DoS(100, error("%s: legacy transaction", __func__));
        }

        // Check that all transactions are finalized
        if (!IsFinalTx(tx, nHeight, block.GetBlockTime()))
            return block.DoS(10, error("%s: contains a non-final transaction", __func__));
    }

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return block.DoS(100, error("%s: rejected by hardened checkpoint lock-in at %d", __func__, nHeight));

    uint256 hashProof;

    if (block.nVersion >= 8)
    {
        //must be proof of stake
        //no grandfather exceptions
        //if (IsProofOfStake())
        CTxDB txdb("r");
        if(!GRC::CheckProofOfStakeV8(txdb, pindexPrev, block, generated_by_me, hashProof))
        {
            return error("%s: invalid proof-of-stake for block %s, prev %s",
                __func__,
                hash.ToString(),
                pindexPrev->GetBlockHash().ToString());
        }

        if (g_seen_stakes.ContainsProof(hashProof)
            && mapOrphanBlocksByPrev.find(hash) == mapOrphanBlocksByPrev.end())
        {
            return error(
                "%s: ignored duplicate proof-of-stake (%s) for block %s",
                __func__,
                hashProof.ToString(),
                hash.ToString());
        }

        g_seen_stakes.Remember(hashProof);
    }
    else if (block.nVersion == 7 && (nHeight >= 999000 || nHeight > nGrandfather))
    {
        // Calculate a proof hash for these version 7 blocks for the block index
        // so we can carry the stake modifier into version 8+:
        //
        // mainnet: block 999000 to version 8 (1010000)
        // testnet: nGrandfather (196551) to version 8 (311999)
        //
        CTxDB txdb("r");
        if (!GRC::CalculateLegacyV3HashProof(txdb, block, block.nNonce, hashProof)) {
            return error("%s: Failed to carry v7 proof hash.", __func__);
        }
    }

    // PoW is checked in CheckBlock[]
    if (block.IsProofOfWork())
    {
        hashProof = block.GetHash(true);
    }

    //Grandfather
    if (nHeight > nGrandfather)
    {
        // Enforce rule that the coinbase starts with serialized block height
        CScript expect = CScript() << nHeight;
        if (block.vtx[0].vin[0].scriptSig.size() < expect.size() ||
                !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin()))
            return block.DoS(100, error("%s: block height mismatch in coinbase", __func__));
    }

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(block, SER_DISK, CLIENT_VERSION)))
        return error("%s: out of disk space", __func__);
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    if (!WriteBlockToDisk(block, nFile, nBlockPos, Params().MessageStart()))
        return error("%s: WriteToDisk failed", __func__);
    if (!AddToBlockIndex(block, nFile, nBlockPos, hashProof))
        return error("%s: AddToBlockIndex failed", __func__);

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Params().Checkpoints().GetHeight();
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        for (auto const& pnode : vNodes)
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    return true;
}

bool CheckBlockSignature(const CBlock& block)
{
    if (block.IsProofOfWork())
        return block.vchBlockSig.empty();

    std::vector<valtype> vSolutions;
    txnouttype whichType;

    const CTxOut& txout = block.vtx[1].vout[1];

    if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        valtype& vchPubKey = vSolutions[0];
        if (block.vchBlockSig.empty())
            return false;
        return CPubKey(vchPubKey).Verify(block.GetHash(true), block.vchBlockSig);
    }

    return false;
}

//!
//! \brief Used in GRC::MRCContractHandler::Validate (essentially AcceptToMemoryPool)
//! \param contract The contract that contains the MRC
//! \param tx The transaction that contains the contract
//! \return true if successfully validated
//!
bool ValidateMRC(const GRC::Contract& contract, const CTransaction& tx, int& DoS)
{
    // The MRC transaction should only have one contract on it, and the contract type should be MRC (which we already
    // know to arrive at this virtual method implementation).
    if (tx.GetContracts().size() != 1) {
        DoS = 25;
        return error("%s: Validation failed: The transaction, hash %s, that contains the MRC has more than one contract.",
                     __func__,
                     tx.GetHash().GetHex());
    }

    // Check that the burn in the contract is equal or greater than the required burn.
    CAmount burn_amount = 0;

    for (const auto& output : tx.vout) {
        if (output.scriptPubKey == (CScript() << OP_RETURN)) {
            burn_amount += output.nValue;
        }
    }

    GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: mrc m_client_version = %s, m_fee = %s, m_last_block_hash = %s, "
                                       "m_magnitude = %u, m_magnitude_unit = %f, m_mining_id = %s, m_organization = %s, "
                                       "m_research_subsidy = %s, m_version = %s",
              __func__,
              mrc.m_client_version,
              FormatMoney(mrc.m_fee),
              mrc.m_last_block_hash.GetHex(),
              mrc.m_magnitude,
              mrc.m_magnitude_unit,
              mrc.m_mining_id.ToString(),
              mrc.m_organization,
              FormatMoney(mrc.m_research_subsidy),
              mrc.m_version);

    if (burn_amount < mrc.RequiredBurnAmount()) {
        DoS = 25;
        return error("%s: Burn amount of %s in mrc contract is less than the required %s.",
                     __func__,
                     FormatMoney(mrc.m_fee),
                     FormatMoney(mrc.RequiredBurnAmount())
                     );
    }

    const GRC::CpidOption cpid = mrc.m_mining_id.TryCpid();

    // No Cpid, the MRC must be invalid.
    if (!cpid) {
        DoS = 25;
        return error("%s: Validation failed: MRC has no CPID.");
    }

    // If the mrc last block hash is not pindexBest's block hash return false, because MRC requests can only be valid
    // in the mempool if they refer to the head of the current chain. Note that this can happen even if the MRC is valid
    // if the receiving node is out-of-sync or there is a "crossing in the mail" situation. Therefore we assign a very low
    // DoS of 1 for this, and only if the wallet is in sync. (The DoS parameter is initialized to 0 by the caller.)
    // Note that a flag is set here, and the DoS assignment and return error is delayed until AFTER the payment interval
    // reject code, because the payment_interval_by_tx_time_reject can still be calculated even if the last_block_hash is
    // not matched, and the payment_interval_by_tx_time_reject is a more severe error.
    int64_t mrc_time = 0;
    bool last_block_hash_matched = true;

    if (pindexBest->GetBlockHash() != mrc.m_last_block_hash) {
        last_block_hash_matched = false;
    } else {
        mrc_time = pindexBest->nTime;
    }

    // We are not going to even accept MRC transactions to the memory pool that have a payment interval less than
    // MRCZeroPaymentInterval / 2. This is to prevent a rogue actor from trying to fill slots in a DoS to rightful
    // MRC recipients.
    const int64_t reject_payment_interval = Params().GetConsensus().MRCZeroPaymentInterval / 2;

    const GRC::ResearchAccount& account = GRC::Tally::GetAccount(*cpid);
    const int64_t last_reward_time = account.LastRewardTime();

    const int64_t payment_interval_by_mrc = mrc_time - last_reward_time;
    const int64_t payment_interval_by_tx_time = tx.nTime - last_reward_time;

    // If last_block_hash_matched is false then payment_interval_by_mrc_reject is forced false as it cannot be evaluated.
    bool payment_interval_by_mrc_reject = (last_block_hash_matched && payment_interval_by_mrc < reject_payment_interval);
    bool payment_interval_by_tx_time_reject = (payment_interval_by_tx_time < reject_payment_interval);

    // For mainnet, if either the payment_interval_by_mrc_reject OR the payment_interval_by_tx_time_reject is true,
    // then return false. This is stricter than testnet below. See the commentary below on why.
    if (!fTestNet && (payment_interval_by_mrc_reject || payment_interval_by_tx_time_reject)) {
        DoS = 25;

        return error("%s: Validation failed: MRC payment interval by mrc time, %" PRId64 " sec, is less than 1/2 of the MRC "
                     "Zero Payment Interval of %" PRId64 " sec.",
                     __func__,
                     last_block_hash_matched ?
                         std::min(payment_interval_by_mrc, payment_interval_by_tx_time) : payment_interval_by_tx_time,
                     reject_payment_interval);
    }

    // For testnet, both rejection conditions must be true (i.e. the payment interval by both mrc and tx time is less
    // than 1/2 of MRCZeroPaymentInterval) for the transaction to be rejected. This difference from mainnet is to
    // accommodate a post testnet v12 change in this function that originally shifted from tx time to mrc time for MRC
    // payment interval rejection, after some mrc tests were already done post mandatory, which broke syncing from zero.
    // Note this has the effect of rendering the payment interval check on testnet inoperative if the
    // last_block_hash_matched is false due to the AND instead of OR condition.
    //
    // TODO: On the next mandatory align the restriction to mainnet from that point forward.
    if (fTestNet && payment_interval_by_mrc_reject && payment_interval_by_tx_time_reject) {
        DoS = 25;

        return error("%s: Validation failed: MRC payment interval on testnet by both mrc time, %" PRId64 " sec, "
                     "and tx time, %" PRId64 " is less than 1/2 of the MRC Zero Payment Interval of %" PRId64 " sec.",
                     __func__,
                     payment_interval_by_mrc,
                     payment_interval_by_tx_time,
                     reject_payment_interval);
    }

    // This is the second part of the m_last_block_hash match validation. If the payment interval section above passed,
    // but the last_block_hash did not match, then return false. Assign a DoS of 1 for this if in sync.
    if (!last_block_hash_matched) {
        if (!OutOfSyncByAge()) {
            DoS = 1;
        }

        return error("%s: Validation failed: MRC m_last_block_hash %s cannot be found in the chain.",
                     __func__,
                     mrc.m_last_block_hash.GetHex());
    }

    // Note that the below overload of ValidateMRC repeats the check for a valid Cpid. It is low overhead and worth not
    // repeating a bunch of what would be common code to eliminate.
    if (!ValidateMRC(pindexBest, mrc)) {
        DoS = 25;
        return false;
    }

    // If we get here, return true.
    return true;
}


//!
//! \brief Used in ConnectBlock and CreateRestOfTheBlock for the binding to the claim
//! \param mrc_last_pindex The pindex of the head of the chain when the mrc was created
//! \param mrc The MRC contract
//! \return true if successfully validated
//!
bool ValidateMRC(const CBlockIndex* mrc_last_pindex, const GRC::MRC &mrc)
{
    int64_t research_owed = 0;
    const int64_t& mrc_time = mrc_last_pindex->nTime;

    const GRC::CpidOption cpid = mrc.m_mining_id.TryCpid();

    // No Cpid, the MRC must be invalid.
    if (!cpid) return error("%s: Validation failed: MRC has no CPID.");

    // Check to ensure the beacon was active at the mrc_last_pindex time of the MRC and the MRC signature. This also
    // via the signature checks whether the supplied last block pindex hash matches that recorded in the MRC contract.
    // Note that the TryActive for the beacon at the mrc_time specified using the mrc_last_index is actually slightly
    // MORE lenient than the above in the normal case where the tx time is after mrc_last_index time, but the validation
    // of the signature using mrc_last_pindex here, which is effectively pindexBest, is far stricter and is used in the
    // miner and the block validations.
    // If this fails, no point in going further.
    if (const GRC::BeaconOption beacon = GRC::GetBeaconRegistry().TryActive(*cpid, mrc_time)) {
        if (!mrc.VerifySignature(
            beacon->m_public_key,
            mrc_last_pindex->GetBlockHash())) {
            return error("%s: Validation failed: MRC signature validation failed for MRC for CPID %s.",
                         __func__,
                         cpid->ToString()
                         );
        }
    } else {
        return error("%s: Validation failed: The beacon for the cpid %s referred to in the MRC is not active.",
                     __func__,
                     cpid->ToString()
                     );
    }

    // The GetAccrual remains valid here because of the mrc_last_pindex limitation imposed in the block binding (and
    // ConnectBlock checking where mrc_last_pindex must be the same as the block previous to the just staked block, i.e.
    // one block lower than the head).
    research_owed = GRC::Tally::GetAccrual(*cpid, mrc_time, mrc_last_pindex);

    if (mrc_last_pindex->nHeight >= GetOrigNewbieSnapshotFixHeight()) {
        // The below is required to deal with a conditional application in historical rewards for
        // research newbies after the original newbie fix height that already made it into the chain.
        // Please see the extensive commentary in the below function.
        CAmount newbie_correction = GRC::Tally::GetNewbieSuperblockAccrualCorrection(*cpid,
                                                                                     GRC::Quorum::CurrentSuperblock());
        research_owed += newbie_correction;
    }

    // If claimed research subsidy is greater than computed research_owed, MRC must be invalid.
    if (mrc.m_research_subsidy > research_owed) {
        return error("%s: Validation failed: The research reward, %s, specified in the MRC for CPID %s exceeds the "
                     "calculated research owed of %s",
                     __func__,
                     FormatMoney(mrc.m_research_subsidy),
                     cpid->ToString(),
                     FormatMoney(research_owed)
                     );
    }

    // Now that we have confirmed the research rewards match, recompute fees using the MRC ComputeMRCFees() method. This
    // needs to match the fees recorded by the sending node in mrc.m_fee.
    if (mrc.m_fee < mrc.ComputeMRCFee()) {
        return error("%s: Validation failed: The MRC fee, %s, specified in the MRC for CPID %s, does not satisfy the "
                     "computed MRC fee %s.",
                     __func__,
                     FormatMoney(mrc.m_fee),
                     cpid->ToString(),
                     FormatMoney(mrc.ComputeMRCFee())
                     );
    }

    if (mrc.m_fee > mrc.m_research_subsidy) {
        return error("%s: Validation failed: MRC fee higher than research subsidy for cpid %s. (%s > %s)",
                     __func__, cpid->ToString(), FormatMoney(mrc.m_fee), FormatMoney(mrc.m_research_subsidy));
    }

    // If we get here, everything succeeded so the MRC is valid.

    return true;
}

