// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2014 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "kernel.h"
#include "txdb.h"
#include "main.h"
#include "streams.h"
#include "util.h"

using namespace std;

StructCPID GetStructCPID();
extern double GetLastPaymentTimeByCPID(std::string cpid);

namespace {
//!
//! \brief Calculate a legacy RSA weight value from the supplied claim block.
//!
//! This function exists support the \c CalculateLegacyV3HashProof() function
//! used to carry the stake modifiers and proof hashes of version 7 blocks to
//! version 8 and later. RSA weight is an input parameter to the legacy proof
//! hash algorithm of the version 3 staking kernel, so we reproduce the value
//! from the claim context in the same way as the old version.
//!
//! \param bb A serialized "BoincBlock" string from a coinbase transaction's
//! \c hashBoinc field that contains the claim context to extract RSA weight
//! component values from.
//!
//! \return Sum of the RSA weight and magnitude from the claim, or zero when
//! the claim contains no CPID (for an investor).
//!
int64_t GetRSAWeightByBlock(const std::string& bb)
{
    constexpr size_t cpid_offset = 0;
    constexpr size_t rsa_weight_offset = 13;
    constexpr size_t magnitude_offset = 15;

    int64_t rsa_weight = 0;

    // General-purpose deserialization of claim contexts in the hashBoinc field
    // no longer parses out the RSA weight field, so we handle the special case
    // for legacy version 7 blocks by extracting only the CPID, RSA weight, and
    // magnitude fields:
    //
    for (size_t n = 0, offset = 0, end = bb.find("<|>");
        n <= magnitude_offset && end != std::string::npos;
        n++)
    {
        if (n == cpid_offset && !IsResearcher(bb.substr(offset, end - offset))) {
            return 0;
        } else if (n == rsa_weight_offset || n == magnitude_offset) {
            rsa_weight += std::atoi(bb.substr(offset, end - offset).c_str());
        }

        offset = end + 3;
        end = bb.find("<|>", offset);
    }

    if (rsa_weight < 0) {
        return 0;
    }

    return rsa_weight;
}
} // anonymous namespace

// Get time weight
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    // Kernel hash weight starts from 0 at the min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low
    return min(nIntervalEnd - nIntervalBeginning - nStakeMinAge, (int64_t)nStakeMaxAge);
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: no generation at genesis block");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert (nSection >= 0 && nSection < 64);
    return (nModifierInterval * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval()
{
    int64_t nSelectionInterval = 0;
    for (int nSection=0; nSection<64; nSection++)
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(vector<pair<int64_t, uint256> >& vSortedByTimestamp, map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev, const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    uint256 hashBest = 0;
    *pindexSelected = (const CBlockIndex*) 0;
    for (auto const& item : vSortedByTimestamp)
    {
        const auto mapItem = mapBlockIndex.find(item.second);
        if (mapItem == mapBlockIndex.end())
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s", item.second.ToString().c_str());
        const CBlockIndex* pindex = mapItem->second;
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;
        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;
        // compute the selection hash by hashing its proof-hash and the
        // previous proof-of-stake modifier
        CDataStream ss(SER_GETHASH, 0);
        ss << pindex->hashProof << nStakeModifierPrev;
        uint256 hashSelection = Hash(ss.begin(), ss.end());
        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection >>= 32;
        if (fSelected && hashSelection < hashBest)
        {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
        else if (!fSelected)
        {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
    }
    if (fDebug && GetBoolArg("-printstakemodifier"))
        LogPrintf("SelectBlockFromCandidates: selection hash=%s", hashBest.ToString());
    return fSelected;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier)
{
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev)
    {
        fGeneratedStakeModifier = true;
        return true;  // genesis block's modifier is 0
    }
    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");
    if (fDebug10)
    {
        LogPrintf("ComputeNextStakeModifier: prev modifier=0x%016" PRIx64 " time=%s", nStakeModifier, DateTimeStrFormat(nModifierTime));
    }
    if (nModifierTime / nModifierInterval >= pindexPrev->GetBlockTime() / nModifierInterval)
        return true;

    const uint256 ModifGlitch_hash("439b96fd59c3d585a6b93ee63b6e1d78361d7eb9b299657dee6a2c5400ccba29");
    const uint64_t ModifGlitch_correct=0xdf209a3032807577;
    if(pindexPrev->GetBlockHash()==ModifGlitch_hash)
    {
        nStakeModifier = ModifGlitch_correct;
        fGeneratedStakeModifier = true;
        return true;
    }

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * nModifierInterval / GetTargetSpacing(pindexPrev->nHeight));
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval();
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / nModifierInterval) * nModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart)
    {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    std::sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound=0; nRound<min(64, (int)vSortedByTimestamp.size()); nRound++)
    {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);
        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (fDebug && GetBoolArg("-printstakemodifier"))
            LogPrintf("ComputeNextStakeModifier: selected round %d stop=%s height=%d bit=%d", nRound, DateTimeStrFormat(nSelectionIntervalStop), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    if (fDebug && GetBoolArg("-printstakemodifier"))
    {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate)
        {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (auto const& item : mapSelectedBlocks)
        {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake()? "S" : "W");
        }
        LogPrintf("ComputeNextStakeModifier: selection height [%d, %d] map %s", nHeightFirstCandidate,
            pindexPrev->nHeight, strSelectionMap);
    }
    if (fDebug)
    {
        LogPrintf("ComputeNextStakeModifier: new modifier=0x%016" PRIx64 " time=%s", nStakeModifierNew, DateTimeStrFormat(pindexPrev->GetBlockTime()));
    }

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

double GetLastPaymentTimeByCPID(std::string cpid)
{
    double lpt = 0;
    if (mvMagnitudes.size() > 0)
    {
            StructCPID UntrustedHost = GetStructCPID();
            UntrustedHost = mvMagnitudes[cpid]; //Contains Consensus Magnitude
            if (UntrustedHost.initialized)
            {
                        double mag_accuracy = UntrustedHost.Accuracy;
                        if (mag_accuracy > 0)
                        {
                                lpt = UntrustedHost.LastPaymentTime;
                        }
            }
            else
            {
                if (IsResearcher(cpid))
                {
                        lpt = 0;
                }
            }
    }
    else
    {
        if (IsResearcher(cpid))
        {
                lpt=0;
        }
    }
    return lpt;
}

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex)
{
    assert (pindex->pprev || pindex->GetBlockHash() == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet));
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (pindex->pprev)
        ss << pindex->pprev->nStakeModifierChecksum;
    ss << pindex->nFlags << (pindex->IsProofOfStake() ? pindex->hashProof : 0) << pindex->nStakeModifier;
    uint256 hashChecksum = Hash(ss.begin(), ss.end());
    hashChecksum >>= (256 - 32);
    return hashChecksum.Get64();
}

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum)
{
    return true;
}

bool CalculateLegacyV3HashProof(
    const CBlock& block,
    const double por_nonce,
    uint256& out_hash_proof)
{
    const CTransaction& coinstake = block.vtx[1];

    CTxDB txdb("r");
    CTransaction input_tx;
    CTxIndex tx_index;

    if (!input_tx.ReadFromDisk(txdb, coinstake.vin[0].prevout, tx_index)) {
        // Previous tx not in main chain, may occur during initial download:
        return coinstake.DoS(1, error(
            "CalculateLegacyV3HashProof(): Read coinstake input_tx failed."));
    }

    CBlock input_block; // TODO: can we avoid hitting the disk?

    if (!input_block.ReadFromDisk(tx_index.pos.nFile, tx_index.pos.nBlockPos, false)) {
        return error("CalculateLegacyV3HashProof(): Read input_block failed.");
    }

    CDataStream out(SER_GETHASH, 0);

    out << GetRSAWeightByBlock(block.vtx[0].hashBoinc)
        << input_block.nTime
        << input_tx.nTime
        << input_tx.GetHash()
        << coinstake.vin[0].prevout.n
        << coinstake.nTime
        << por_nonce;

    out_hash_proof = CBigNum(Hash(out.begin(), out.end())).getuint256();

    return true;
}

// GridcoinCoin kernel protocol (credit goes to Black-Coin)
// coinstake must meet hash target according to the protocol:
// kernel (input 0) must meet the formula
//     hash(nStakeModifier + txPrev.block.nTime + txPrev.nTime + txPrev.vout.hash + txPrev.vout.n + nTime) < bnTarget * nWeight
// this ensures that the chance of getting a coinstake is proportional to the
// amount of coin age one owns.
// The reason this hash is chosen is the following:
//   nStakeModifier: scrambles computation to make it very difficult to precompute
//                   future proof-of-stake
//   txPrev.block.nTime: prevent nodes from guessing a good timestamp to
//                       generate transaction for future advantage
//   txPrev.nTime: slightly scrambles computation
//   txPrev.vout.hash: hash of txPrev, to reduce the chance of nodes
//                     generating coinstake at the same time
//   txPrev.vout.n: output number of txPrev, to reduce the chance of nodes
//                  generating coinstake at the same time
//   nTime: current timestamp
//   block/tx hash should not be used here as they can be generated in vast
//   quantities so as to generate blocks faster, degrading the system back into
//   a proof-of-work situation.
//


// V8 Kernel Protocol
// Tomas Brod 05.06.2017 (dd.mm.yyy)
// Plug proof-of-work exploit.
// TODO: Stake modifier is included without much understanding.
// Without the modifier, attacker can create transactions which output will
// stake at desired time. In other words attacker can check wheter transaction
// output will stake in the future and create transactions accordingly.
// Thus including modifier, even not completly researched, increases security.
// Note: Rsa or Magnitude weight not included due to multiplication issue.
// Note: Payment age and magnitude restrictions not included as they are not
// important in my view and are too restrictive for honest users.
// TODO: flags?
// Note: Transaction hash is used here even thou ppcoin devs advised against it,
// Gridcoin already used txhash in previous kernel, trying to brute-force
// good tx hash is not possible as it is not known what stake modifier will be
// after the coins mature!

CBigNum CalculateStakeHashV8(
    const CBlock &CoinBlock,
    const CTransaction &CoinTx,
    unsigned CoinTxN,
    unsigned nTimeTx,
    uint64_t StakeModifier)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << StakeModifier;
    ss << (CoinBlock.nTime & ~STAKE_TIMESTAMP_MASK);
    ss << CoinTx.GetHash();
    ss << CoinTxN;
    ss << (nTimeTx & ~STAKE_TIMESTAMP_MASK);
    CBigNum hashProofOfStake( Hash(ss.begin(), ss.end()) );
    return hashProofOfStake;
}

int64_t CalculateStakeWeightV8(const CTransaction &CoinTx, unsigned CoinTxN)
{
    int64_t nValueIn = CoinTx.vout[CoinTxN].nValue;
    nValueIn /= 1250000;
    return nValueIn;
}

// Another version of GetKernelStakeModifier (TomasBrod)
// Todo: security considerations
bool FindStakeModifierRev(uint64_t& nStakeModifier,CBlockIndex* pindexPrev)
{
    nStakeModifier = 0;
    const CBlockIndex* pindex = pindexPrev;

    while (1)
    {
        if(!pindex)
            return error("FindStakeModifierRev: no previous block from %d",pindexPrev->nHeight);

        if (pindex->GeneratedStakeModifier())
        {
            nStakeModifier = pindex->nStakeModifier;
            return true;
        }
        pindex = pindex->pprev;
    }
}

// Block Version 8+ check procedure

bool CheckProofOfStakeV8(
    CBlockIndex* pindexPrev, //previous block in chain index
    CBlock &Block, //block to check
    bool generated_by_me,
    uint256& hashProofOfStake) //proof hash out-parameter
{
    //Block Transaction 0 is coin:base
    //Block Transaction 1 is coin:stake
    //First input of coinstake is the kernel

    CTransaction *p_coinstake;

    if(Block.nVersion>=8 && Block.nVersion<=10) {
        if (!Block.IsProofOfStake())
            return error("CheckProofOfStakeV8() : called on non-coinstake block %s", Block.GetHash().ToString().c_str());
        p_coinstake = &Block.vtx[1];
    }
    //for future coin:stake:base merging into one tx
    else return false;

    if (!p_coinstake->IsCoinStake())
        return error("CheckProofOfStakeV8() : called on non-coinstake tx %s", Block.vtx[1].GetHash().ToString().c_str());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTransaction& tx = (*p_coinstake);
    const CTxIn& txin = (*p_coinstake).vin[0];

    // First try finding the previous transaction in database
    CTxDB txdb("r");
    CTransaction txPrev;
    CTxIndex txindex;
    if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
        return tx.DoS(1, error("CheckProofOfStake() : INFO: read txPrev failed"));  // previous transaction not in main chain, may occur during initial download

    // Verify signature
    if (!VerifySignature(txPrev, tx, 0, 0))
        return tx.DoS(100, error("CheckProofOfStake() : VerifySignature failed on coinstake %s", tx.GetHash().ToString().c_str()));

    // Read block header
    CBlock blockPrev;
    if (!blockPrev.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
        return fDebug? error("CheckProofOfStake() : read block failed") : false; // unable to read block of previous transaction

    // Check times (todo: add some more, like mask check)
    if (tx.nTime < txPrev.nTime)  // Transaction timestamp violation
        return error("CheckProofOfStakeV8: nTime violation");

    if (blockPrev.nTime + nStakeMinAge > tx.nTime) // Min age requirement
        return error("CheckProofOfStakeV8: min age violation");

    uint64_t StakeModifier = 0;
    if(!FindStakeModifierRev(StakeModifier,pindexPrev))
        return error("CheckProofOfStakeV8: unable to find stake modifier");

    //Stake refactoring TomasBrod
    int64_t Weight = CalculateStakeWeightV8(txPrev, txin.prevout.n);
    CBigNum bnHashProof = CalculateStakeHashV8(blockPrev, txPrev, txin.prevout.n, tx.nTime, StakeModifier);

    // Base target
    CBigNum bnTarget;
    bnTarget.SetCompact(Block.nBits);
    // Weighted target
    bnTarget *= Weight;

    hashProofOfStake=bnHashProof.getuint256();
    //targetProofOfStake=bnTarget.getuint256();

    if(fDebug) LogPrintf(
"CheckProofOfStakeV8:%s Time1 %.f, Time2 %.f, Time3 %.f, Bits %u, Weight %.f\n"
" Stk %72s\n"
" Trg %72s", generated_by_me?" Local,":"",
        (double)blockPrev.nTime, (double)txPrev.nTime, (double)tx.nTime,
        Block.nBits, (double)Weight,
        CBigNum(hashProofOfStake).GetHex(), bnTarget.GetHex()
    );

    // Now check if proof-of-stake hash meets target protocol

    if (bnHashProof > bnTarget)
    {
        return false;
    }
    return true;
}
