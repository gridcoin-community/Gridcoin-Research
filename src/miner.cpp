// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2013 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "consensus/merkle.h"
#include "txdb.h"
#include "miner.h"
#include "main.h"
#include "gridcoin/beacon.h"
#include "gridcoin/cpid.h"
#include "gridcoin/claim.h"
#include "gridcoin/mrc.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/quorum.h"
#include "gridcoin/researcher.h"
#include "gridcoin/staking/difficulty.h"
#include "gridcoin/staking/kernel.h"
#include "gridcoin/staking/reward.h"
#include "gridcoin/staking/status.h"
#include "gridcoin/tally.h"
#include "policy/policy.h"
#include "policy/fees.h"
#include "random.h"
#include "util.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <memory>
#include <algorithm>
#include <tuple>

using namespace std;

//
// Gridcoin Miner
//

unsigned int nMinerSleep;

namespace {
class COrphan
{
public:
    CTransaction* ptx;
    set<uint256> setDependsOn;
    double dFeePerKb;

    COrphan(CTransaction* ptxIn)
    {
        ptx = ptxIn;
        dFeePerKb = 0;
    }

    void print() const
    {
        LogPrintf("COrphan(hash=%s, dFeePerKb=%.1f)",
               ptx->GetHash().ToString().substr(0,10), dFeePerKb);
        for (auto const& hash : setDependsOn)
            LogPrintf("   setDependsOn %s", hash.ToString().substr(0,10));
    }
};

//!
//! \brief Sign the research reward claim context for a newly-minted block.
//!
//! \param pwallet Supplies beacon private keys for signing.
//! \param claim   An initialized claim to sign for a block.
//! \param block   Block that contains the claim.
//! \param dry_run If true, only check that the node can sign.
//!
//! \return \c true if the miner holds active beacon keys used to successfully
//! sign the claim.
//!
bool TrySignClaim(
    CWallet* pwallet,
    GRC::Claim& claim,
    const CBlock& block,
    const bool dry_run = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    // lock needs to be taken on pwallet here.
    LOCK(pwallet->cs_wallet);

    const GRC::CpidOption cpid = claim.m_mining_id.TryCpid();

    if (!cpid) {
        return true; // Skip beacon signature for investors.
    }

    const GRC::BeaconOption beacon = GRC::GetBeaconRegistry().Try(*cpid);

    if (!beacon) {
        return error("%s: No active beacon", __func__);
    }

    if (beacon->Expired(block.nTime)) {
        return error("%s: Beacon expired", __func__);
    }

    CKey beacon_key;

    if (!pwallet->GetKey(beacon->m_public_key.GetID(), beacon_key)) {
        return error("%s: Missing beacon private key", __func__);
    }

    if (!beacon_key.IsValid()) {
        return error("%s: Invalid beacon key", __func__);
    }

    // CreateGridcoinReward() calls this function to test that we can actually
    // sign the claim before finalizing the research reward claim:
    //
    if (dry_run) {
        return true;
    }

    if (!claim.Sign(beacon_key, block.hashPrevBlock, block.vtx[1])) {
        return error("%s: Signature failed. Check beacon key", __func__);
    }

    LogPrint(BCLog::LogFlags::MINER,
             "%s: Signed for CPID %s and block hash %s with signature %s",
             __func__,
             cpid->ToString(),
             block.hashPrevBlock.ToString(),
             HexStr(claim.m_signature));

    return true;
}

//!
//! \brief Temporary overload to cast const claims for the final signature.
//!
//! TODO: Refactor the block API to provide easier access to the claim contract
//! for this.
//!
bool TrySignClaim(
    CWallet* pwallet,
    const GRC::Claim& claim,
    const CBlock& block,
    const bool dry_run = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    return TrySignClaim(pwallet, const_cast<GRC::Claim&>(claim), block, dry_run);
}
}

// This is in anonymous namespace because it is only to be used by miner code here in this file.
bool CreateMRCRewards(CBlock &blocknew, std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>& mrc_map,
                      std::map<GRC::Cpid, uint256>& mrc_tx_map, CAmount& reward, uint32_t& claim_contract_version,
                      GRC::Claim& claim, CWallet* pwallet) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // For convenience
    CTransaction& coinstake = blocknew.vtx[1];
    std::vector<CTxOut> mrc_outputs;

    // coinstake.vout size should be 2 at this point. If not something is really wrong so assert immediately.
    assert(coinstake.vout.size() == 2);

    CAmount mrc_rewards = 0;
    CAmount staker_fees = 0;
    CAmount foundation_fees = 0;

    // Note that GetMRCOutputLimit handles the situation where FoundationSideStakeAllocation.IsNonZero is false or true.
    unsigned int output_limit = GetMRCOutputLimit(blocknew.nVersion, false);

    if (output_limit > 0) {
        Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

        LogPrint(BCLog::LogFlags::MINER, "INFO: %s: output_limit = %u", __func__, output_limit);

        // We do not need to validate the MRCs here because that was already done in CreateRestOfTheBlock. This also means
        // that the mrc tx hashes have been validated and the mrc_last_pindex matches the pindexPrev here (the head of the
        // chain from the miner's point of view).

        LogPrint(BCLog::LogFlags::MINER, "INFO: %s: mrc_map size = %u", __func__, mrc_map.size());

        for (const auto& iter : mrc_map) {
            GRC::Cpid cpid = iter.first;
            const uint256& tx_hash = iter.second.first;
            const GRC::MRC& mrc = iter.second.second;

            // This really had better be the head of the chain.
            CBlockIndex* mrc_index = mapBlockIndex[mrc.m_last_block_hash];

            LogPrint(BCLog::LogFlags::MINER, "INFO: %s: mrc_index.GetBlockHash() = %s",
                     __func__, mrc_index->GetBlockHash().GetHex());

            const GRC::BeaconOption beacon = GRC::GetBeaconRegistry().TryActive(cpid, mrc_index->nTime);

            if (beacon && mrc_outputs.size() <= output_limit) {
                CBitcoinAddress beacon_address = beacon->GetAddress();
                CScript script_beacon_key;
                script_beacon_key.SetDestination(beacon_address.Get());

                // The net reward paid to the MRC beacon address is the requested research subsidy (reward)
                // minus the fees for the MRC.
                CAmount reward = mrc.m_research_subsidy - mrc.m_fee;

                if (reward) {
                    mrc_outputs.push_back(CTxOut(reward, script_beacon_key));
                    mrc_rewards += reward;
                }

                if (foundation_fee_fraction.IsNonZero()) {
                    CAmount foundation_fee = mrc.m_fee * foundation_fee_fraction.GetNumerator()
                                                       / foundation_fee_fraction.GetDenominator();
                    CAmount staker_fee = mrc.m_fee - foundation_fee;

                    // Accumulate the fees
                    foundation_fees += foundation_fee;
                    staker_fees += staker_fee;
                } else {
                    staker_fees += mrc.m_fee;
                }

                // Put the cpid and tx_hash in the mrc_tx_map to record in the claim. These inserts MUST succeed because
                // of the insurance of uniqueness on CPID that has already been done prior to calling this function.
                mrc_tx_map[cpid] = tx_hash;

                LogPrint(BCLog::LogFlags::VERBOSE, "INFO: %s: mrc %u validated: m_client_version = %u, m_fee = %s, "
                                                   "m_last_block_hash = %s, m_mining_id = %s, m_organization = %s, "
                                                   "m_research_subsidy = %s, m_version = %u, mrc_reward on coinstake = %s, "
                                                   "total mrc rewards on coinstake = %s, "
                                                   "mrc fees = %s, mrc staker_fees = %s, mrc foundation fees = %s",
                         __func__,
                         mrc_outputs.size(),
                         mrc.m_client_version,
                         FormatMoney(mrc.m_fee),
                         mrc.m_last_block_hash.GetHex(),
                         mrc.m_mining_id.ToString(),
                         mrc.m_organization,
                         FormatMoney(mrc.m_research_subsidy),
                         mrc.m_version,
                         FormatMoney(reward),
                         FormatMoney(mrc_rewards),
                         FormatMoney(staker_fees + foundation_fees),
                         FormatMoney(staker_fees),
                         FormatMoney(foundation_fees));

            } // valid beacon
        } // mrc_map iteration

        // Now that the MRC outputs are created, add the fees to the staker to the coinstake output 1 value.
        coinstake.vout[1].nValue += staker_fees;

        // Increment the reward parameter to include the staker_fees. This is important for mandatory sidestakes later.
        reward += staker_fees;

        if (foundation_fees > 0) {
            // TODO: Make foundation address a defaulted but protocol overridable parameter.

            CScript script_foundation_key;
            script_foundation_key.SetDestination(FoundationSideStakeAddress().Get());

            // Put the foundation "sidestake" (MRC fees to the foundation) on the coinstake.
            coinstake.vout.push_back(CTxOut(foundation_fees, script_foundation_key));
        }

        // Put the MRC payments on the coinstake.
        coinstake.vout.insert(coinstake.vout.end(), mrc_outputs.begin(), mrc_outputs.end());

        // Put the mrc_tx_map in the claim
        claim.m_mrc_tx_map = mrc_tx_map;
    }

    // Do a dry run for the claim signature to ensure that we can sign for a
    // researcher claim. We generate the final signature when signing all of
    // the block:
    //
    if (!TrySignClaim(pwallet, claim, blocknew, true)) {
        error("%s: Failed to sign researcher claim. Staking as investor", __func__);

        coinstake.vout[1].nValue -= claim.m_research_subsidy;
        claim.m_mining_id = GRC::MiningId::ForInvestor();
        claim.m_research_subsidy = 0;
        claim.m_magnitude = 0;
    }

    LogPrint(BCLog::LogFlags::VERBOSE, "INFO %s: for %s mint %s magnitude %d Research %s, CBR %s, MRC fees to staker %s, "
                                       "MRC fees to foundation %s, MRC rewards %s",
             __func__,
             claim.m_mining_id.ToString(),
             FormatMoney(claim.m_research_subsidy + claim.m_block_subsidy + staker_fees),
             claim.m_magnitude,
             FormatMoney(claim.m_research_subsidy),
             FormatMoney(claim.m_block_subsidy),
             FormatMoney(staker_fees),
             FormatMoney(foundation_fees),
             FormatMoney(mrc_rewards));

    // Now the claim is complete. Put on the coinbase.
    blocknew.vtx[0].vContracts.emplace_back(GRC::MakeContract<GRC::Claim>(
                                                claim_contract_version,
                                                GRC::ContractAction::ADD,
                                                std::move(claim)));

    return true;
}

// TODO: Move other functions in the miner that should be intentionally non-exportable to this namespace. That will end
// up being most...
//} // anonymous namespace

// CreateRestOfTheBlock: collect transactions into block and fill in header
bool CreateRestOfTheBlock(CBlock &block, CBlockIndex* pindexPrev,
                          std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>>& mrc_map)
{

    int nHeight = pindexPrev->nHeight + 1;

    // This is specifically for BlockValidateContracts, and only the nHeight is filled in.
    CBlockIndex pindex_contract_validate;
    pindex_contract_validate.nHeight = nHeight;

    // Create coinbase tx
    CTransaction &CoinBase = block.vtx[0];
    CoinBase.nTime = block.nTime;
    CoinBase.vin.resize(1);
    CoinBase.vin[0].prevout.SetNull();
    CoinBase.vout.resize(1);
    // Height first in coinbase required for block.version=2
    CoinBase.vin[0].scriptSig = (CScript() << nHeight) + COINBASE_FLAGS;
    assert(CoinBase.vin[0].scriptSig.size() <= 100);
    CoinBase.vout[0].SetEmpty();

    // Note that block.vtx[1], the coinstake, has already been created by CreateCoinStake on block and passed into this
    // function. There is no point in going through this work to compose a complete block unless the CreateCoinStake
    // succeeds. Note that the coinstake transaction here has not gone through the split/sidestake process nor have the MRC
    // outputs been added, so there are currently two outputs on the coinstake, the empty one, and the reward return.
    // We will need to add a reserved size placeholder for the fully built out coinstake, which will include the non-MRC
    // output limit plus the number of MRC's intended to be bound to the block. This is done below using this size.
    size_t coinstake_output_ser_size = GetSerializeSize(block.vtx[1].vout[1], SER_NETWORK, PROTOCOL_VERSION);

    // We need this to detect an MRC in the mempool that is from the staker (i.e. the staker is a researcher with an active
    // beacon). This could happen if they send an MRC and then stake right after.
    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();
    const GRC::CpidOption cpid = researcher->Id().TryCpid();

    // This boolean will be used to ensure that there is only one mandatory sidestake transaction bound into a block. This
    // in combination with the transaction level validation for the maximum mandatory allocation perfects that rule.
    bool mandatory_sidestake_bound = false;

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = gArgs.GetArg("-blockmaxsize", MAX_BLOCK_SIZE_GEN/2);
    // Limit to between 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::clamp<unsigned int>(nBlockMaxSize, 1000, MAX_BLOCK_SIZE - 1000);

    // Fee-per-kilobyte amount considered the same as "free"
    // Be careful setting this: if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 0-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    int64_t nMinTxFee = GetBaseFee(CoinBase);
    if (gArgs.IsArgSet("-mintxfee"))
        ParseMoney(gArgs.GetArg("-mintxfee", "dummy"), nMinTxFee);

    // Collect memory pool transactions into the block
    int64_t nFees = 0;
    {
        LOCK2(cs_main, mempool.cs);
        CTxDB txdb("r");

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;

        // This vector will be sorted into a priority queue:
        std::vector<std::pair<double, CTransaction*>> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());

        Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

        for (auto& [_, tx] : mempool.mapTx)
        {
            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, nHeight))
                continue;

            // Double-check that contracts pass contextual validation again so
            // that we don't include a transaction that disrupts validation of
            // the block. Note that this is especially important now that there
            // are block level rules that cannot be checked for transactions
            // that are just in the mempool. Note that the only block level rules
            // currently implemented depend on block height only, so the
            // pindex_contract_validate only has the block height filled out.
            //
            int DoS = 0; // Unused here.
            if (!tx.GetContracts().empty() && !GRC::BlockValidateContracts(&pindex_contract_validate, tx, DoS)) {
                LogPrint(BCLog::LogFlags::MINER,
                    "%s: contract failed contextual validation. Skipped tx %s",
                    __func__,
                    tx.GetHash().ToString());

                continue;
            }

            COrphan* porphan = nullptr;
            int64_t nTotalIn = 0;
            bool fMissingInputs = false;

            for (auto const& txin : tx.vin)
            {
                // Read prev transaction
                CTransaction txPrev;
                CTxIndex txindex;

                LogPrint(BCLog::LogFlags::NOISY, "Enumerating tx %s ",tx.GetHash().GetHex());

                if (!ReadTxFromDisk(txPrev, txdb, txin.prevout, txindex))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input");

                        fMissingInputs = true;
                        if (porphan) vOrphan.pop_back();

                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                        LogPrint(BCLog::LogFlags::NOISY, "Orphan tx %s ",tx.GetHash().GetHex());
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].vout[txin.prevout.n].nValue;

                    continue;
                }
                int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;
            }
            if (fMissingInputs) continue;

            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

            // This is a more accurate fee-per-kilobyte than is used by the client code, because the
            // client code rounds up the size to the nearest 1K. That's good, because it gives an
            // incentive to create smaller transactions.
            CAmount total_fee = nTotalIn - tx.GetValueOut();
            if (!tx.vContracts.empty() && tx.vContracts[0].m_type == GRC::ContractType::MRC) {
                const auto& mrc = *tx.vContracts[0].SharePayloadAs<GRC::MRC>();

                total_fee += mrc.m_fee - mrc.m_fee * foundation_fee_fraction.GetNumerator()
                                                   / foundation_fee_fraction.GetDenominator();
            }
            double dFeePerKb =  (double)total_fee / (double(nTxSize)/1000.0);

            if (porphan)
            {
                porphan->dFeePerKb = dFeePerKb;
            }
            else
            {
                vecPriority.push_back(std::make_pair(dFeePerKb, &tx));
            }
        }

        // Collect transactions into block
        map<uint256, CTxIndex> mapTestPool;

        // block versions up through v11...
        uint64_t nBlockSize = 1000;

        // If block v12+, start with the size of the block before the rest of the transactions are added plus a correction
        // (reserve) for the output limit for splitting/sidestaking part - note there are 2 outputs already on the coinstake.
        if (block.nVersion >= 12) {
            nBlockSize = GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)
                    + (GetCoinstakeOutputLimit(block.nVersion)
                       - GetMRCOutputLimit(block.nVersion, true)
                       - 2) * coinstake_output_ser_size;
        }

        int nBlockSigOps = 100;

        std::make_heap(vecPriority.begin(), vecPriority.end());

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dFeePerKb = vecPriority.front().first;
            CTransaction& tx = *(vecPriority.front().second);

            std::pop_heap(vecPriority.begin(), vecPriority.end());
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

            if (nBlockSize + nTxSize >= nBlockMaxSize)
            {
                LogPrintf("Tx size too large for tx %s blksize %" PRIu64 ", tx size %" PRId64,
                          tx.GetHash().GetHex(), nBlockSize, nTxSize);
                continue;
            }

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                continue;
            }

            // Timestamp limit
            if (tx.nTime >  block.nTime)
            {
                continue;
            }

            // Transaction fee
            CAmount nMinFee = GetMinFee(tx, nBlockSize, GMF_BLOCK);

            // Connecting shouldn't fail due to dependency on other memory pool transactions
            // because we're already processing them in order of dependency
            map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
            MapPrevTx mapInputs;
            bool fInvalid;
            if (!FetchInputs(tx, txdb, mapTestPoolTmp, false, true, mapInputs, fInvalid))
            {
                LogPrint(BCLog::LogFlags::NOISY, "Unable to fetch inputs for tx %s ", tx.GetHash().GetHex());
                continue;
            }

            CAmount nTxFees = GetValueIn(tx, mapInputs) - tx.GetValueOut();
            if (nTxFees < nMinFee)
            {
                LogPrint(BCLog::LogFlags::NOISY,
                         "Not including tx %s  due to TxFees of %" PRId64 ", bare min fee is %" PRId64,
                         tx.GetHash().GetHex(), nTxFees, nMinFee);

                // Since transactions are sorted by fee, the fee of rest must also be lower
                // than required.
                break;
            }

            nTxSigOps += GetP2SHSigOpCount(tx, mapInputs);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                LogPrint(BCLog::LogFlags::NOISY, "Not including tx %s due to exceeding max sigops of %d, sigops is %d",
                    tx.GetHash().GetHex(), (nBlockSigOps+nTxSigOps), MAX_BLOCK_SIGOPS);
                continue;
            }

            if (!ConnectInputs(tx, txdb, mapInputs, mapTestPoolTmp, CDiskTxPos(1,1,1), pindexPrev, false, true))
            {
                LogPrint(BCLog::LogFlags::NOISY, "Unable to connect inputs for tx %s ",tx.GetHash().GetHex());
                continue;
            }

            // Non-mrc transactions set ignore_transaction to false;
            bool ignore_transaction = false;

            if (!tx.GetContracts().empty()) {
                // By protocol for mrc requests, there can only be one contract on the transaction.
                const GRC::Contract& contract = tx.GetContracts()[0];

                if (contract.m_type == GRC::ContractType::MRC) {
                    // for mrc the default is to ignore the transaction in the mempool, unless it meets all the
                    // conditions for binding into the block and the claim.
                    ignore_transaction = true;

                    // If we have reached the MRC output limit then don't include transactions with MRC contracts in the
                    // mrc_map. The intended foundation sidestake (if active) is excluded from the output limit here,
                    // because that slot will be used in a sidestake of the fees to the foundation rather than an MRC
                    // transaction output.
                    if (mrc_map.size() < GetMRCOutputLimit(block.nVersion, false)) {

                        GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                        // This check as to whether CpidOption actually points to a valid Cpid should not be
                        // strictly necessary, because of the validation done on AcceptToMemoryPool, but is included
                        // here anyway for safety.
                        if (const GRC::CpidOption mrc_cpid = mrc.m_mining_id.TryCpid()) {
                            // To insert an mrc into the claim mrc_map, the mrc_cpid must be a cpid, the mrc_cpid must
                            // not be the same as the stakers cpid, the cpid must be unique (i.e. the same cpid cannot
                            // have more than one MRC per block), and the MRC must validate. This prevents the situation
                            // where a researcher is staking and trying to process an MRC sent from themselves just before.
                            // The below also ensures that only the highest priority mrc in the mempool get processed
                            // for unique cpids, although the one MRC per cpid is also enforced in the AcceptToMemoryPool.
                            // Overflow mrc transactions beyond the output limit will also be ignored.
                            if ((!cpid || (mrc_cpid && cpid && *mrc_cpid != *cpid))
                                    && ValidateMRC(pindexPrev, mrc)) {
                                // Here the insert form instead of [] is used, because we want to use the first
                                // mrc transaction in the mempool for a given cpid in mrc fee order, not the last
                                // for the available slots for mrc. Note that AcceptToMemoryBlock now also enforces
                                // uniqueness of transactions in the mempool for each CPID.
                                if (mrc_map.insert(make_pair(*mrc_cpid, make_pair(tx.GetHash(), mrc))).second) {
                                    // If an entry was successfully inserted into the mrc_map, adjust the block size upward
                                    // by the size of one coinstake output, because there will be one output added per
                                    // mrc entry in this map later in CreateMRCRewards.
                                    nBlockSize += coinstake_output_ser_size;

                                    ignore_transaction = false;
                                }
                            }
                        } //TryCpid()
                    } // output limit
                } // contract type is MRC

                // If a mandatory sidestake contract has not already been bound into the block, then set mandatory_sidestake_bound
                // to true. The ignore_transaction flag is still false, so this mandatory sidestake contract will be bound into the
                // block. Any more mandatory sidestakes in the transaction loop will be ignored because the mandatory_sidestake_bound
                // will be set to true in the second and succeeding iterations in the loop.
                if (contract.m_type == GRC::ContractType::SIDESTAKE) {
                    if (!mandatory_sidestake_bound) {
                        mandatory_sidestake_bound = true;
                    } else {
                        ignore_transaction = true;
                    }
                } // contract type is SIDESTAKE
            } // contracts not empty

            if (ignore_transaction) continue;

            mapTestPoolTmp[tx.GetHash()] = CTxIndex(CDiskTxPos(1,1,1), tx.vout.size());
            swap(mapTestPool, mapTestPoolTmp);

            block.vtx.push_back(tx);
            nBlockSize += nTxSize;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY) || gArgs.GetBoolArg("-printpriority"))
            {
                LogPrintf("feerate %.1f GRC/KB txid %s",
                       dFeePerKb, tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            uint256 hash = tx.GetHash();
            if (mapDependers.count(hash))
            {
                for (auto& porphan : mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(std::make_pair(porphan->dFeePerKb, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end());
                        }
                    }
                }
            }
        }

        if (LogInstance().WillLogCategory(BCLog::LogFlags::NOISY) || gArgs.GetBoolArg("-printpriority"))
            LogPrintf("CreateNewBlock(): total size %" PRIu64, nBlockSize);
    }

    //Add fees to coinbase
    block.vtx[0].vout[0].nValue= nFees;

    // Fill in header
    block.hashPrevBlock  = pindexPrev->GetBlockHash();
    block.vtx[0].nTime=block.nTime;

    return true;
}


bool CreateCoinStake(CBlock &blocknew, CKey &key,
    vector<const CWalletTx*> &StakeInputs,
    CWallet &wallet, CBlockIndex* pindexPrev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    std::string function = __func__;
    function += ": ";

    int64_t CoinWeight;
    arith_uint256 StakeKernelHash;
    CTxDB txdb("r");
    int64_t StakeWeightSum = 0;
    double StakeValueSum = 0;
    int64_t StakeWeightMin = MAX_MONEY;
    int64_t StakeWeightMax = 0;
    CTransaction &txnew = blocknew.vtx[1]; // second tx is coinstake

    bool kernel_found = false;

    //initialize the transaction
    txnew.nTime = GRC::MaskStakeTime(blocknew.nTime);
    txnew.vin.clear();
    txnew.vout.clear();

    // Choose coins to use
    vector<pair<const CWalletTx*,unsigned int>> CoinsToStake;
    GRC::MinerStatus::ErrorFlags error_flag;

    // This will be used to calculate the staking efficiency.
    int64_t balance = 0;

    if (!wallet.SelectCoinsForStaking(txnew.nTime, CoinsToStake, error_flag, balance, true))
    {
        g_miner_status.UpdateLastSearch(
            kernel_found,
            txnew.nTime,
            blocknew.nVersion,
            StakeWeightSum,
            StakeValueSum,
            0, // This should be set to zero for an unsuccessful iteration due to no stakeable coins.
            StakeWeightMax,
            GRC::CalculateStakeWeightV8(balance));

        g_miner_status.UpdateCurrentErrors(error_flag);

        LogPrint(BCLog::LogFlags::VERBOSE, "%s: %s", __func__, g_miner_status.FormatErrors());

        return false;
    }

    g_timer.GetTimes(function + "SelectCoinsForStaking", "miner");

    LogPrint(BCLog::LogFlags::MINER, "CreateCoinStake: Staking nTime/16 = %d Bits = %u",
             txnew.nTime/16, blocknew.nBits);

    uint64_t StakeModifier = 0;
    int nHeight_mod = 0;

    if (!GRC::FindStakeModifierRev(StakeModifier, pindexPrev, nHeight_mod)) return false;

    LogPrint(BCLog::LogFlags::MISC, "FindStakeModifierRev(): pindex->nHeight = %i, "
                                    "pindex->nStakeModifier = %" PRId64,
                                    nHeight_mod, StakeModifier);

    for (const auto& pcoin : CoinsToStake)
    {
        const CWalletTx &CoinTx = *pcoin.first; //transaction that produced this coin
        unsigned int CoinTxN = pcoin.second; //index of this coin inside it

        unsigned int block_time;

        block_time = mapBlockIndex[CoinTx.hashBlock]->nTime;

        StakeValueSum += CoinTx.vout[CoinTxN].nValue / (double) COIN;

        CoinWeight = GRC::CalculateStakeWeightV8(CoinTx, CoinTxN);

        StakeKernelHash = UintToArith256(GRC::CalculateStakeHashV8(block_time, CoinTx, CoinTxN, txnew.nTime, StakeModifier));

        arith_uint320 StakeTarget = arith_uint256().SetCompact(blocknew.nBits);
        StakeTarget *= arith_uint320(CoinWeight);
        StakeWeightSum += CoinWeight;
        StakeWeightMin = std::min(StakeWeightMin, CoinWeight);
        StakeWeightMax = std::max(StakeWeightMax, CoinWeight);
        double StakeKernelDiff = GRC::GetBlockDifficulty(StakeKernelHash.GetCompact())*CoinWeight;

        LogPrint(BCLog::LogFlags::MINER,
                 "CreateCoinStake: V%d Time %d, Bits %u, Weight %" PRId64 "\n"
                 " Stk %72s\n"
                 " Trg %72s\n"
                 " Diff %0.7f of %0.7f",
                 blocknew.nVersion,
                 txnew.nTime,
                 blocknew.nBits,
                 CoinWeight,
                 StakeKernelHash.GetHex(),
                 StakeTarget.GetHex(),
                 StakeKernelDiff,
                 GRC::GetBlockDifficulty(blocknew.nBits));

        if (arith_uint320(StakeKernelHash) <= StakeTarget)
        {
            // Found a kernel
            LogPrintf("CreateCoinStake: Found Kernel");
            vector<valtype> vSolutions;
            txnouttype whichType;
            CScript scriptPubKeyOut;
            CScript scriptPubKeyKernel;
            scriptPubKeyKernel = CoinTx.vout[CoinTxN].scriptPubKey;

            if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
            {
                LogPrintf("CreateCoinStake: failed to parse kernel");
                break;
            }

            if (whichType == TX_PUBKEYHASH) // pay to address type
            {
                // convert to pay to public key type
                if (!wallet.GetKey(uint160(vSolutions[0]), key))
                {
                    LogPrintf("CreateCoinStake: failed to get key for kernel type = %d", whichType);
                    break;  // unable to find corresponding public key
                }
                scriptPubKeyOut << key.GetPubKey() << OP_CHECKSIG;
            }
            else if (whichType == TX_PUBKEY)  // pay to public key type
            {
                valtype& vchPubKey = vSolutions[0];
                if (!wallet.GetKey(Hash160(vchPubKey), key)
                    || key.GetPubKey() != CPubKey(vchPubKey))
                {
                    LogPrintf("CreateCoinStake: failed to get key for kernel type = %d", whichType);
                    break;  // unable to find corresponding public key
                }

                scriptPubKeyOut = scriptPubKeyKernel;
            }
            else
            {
                LogPrintf("CreateCoinStake: no support for kernel type = %d", whichType);
                break;  // only support pay to public key and pay to address
            }

            txnew.vin.push_back(CTxIn(CoinTx.GetHash(), CoinTxN));
            StakeInputs.push_back(pcoin.first);

            int64_t nCredit = CoinTx.vout[CoinTxN].nValue;

            txnew.vout.push_back(CTxOut(0, CScript())); // First Must be empty
            txnew.vout.push_back(CTxOut(nCredit, scriptPubKeyOut));

            LogPrintf("CreateCoinStake: added kernel type = %d credit = %f", whichType, CoinToDouble(nCredit));

            kernel_found = true;

            break;
        } // if (StakeKernelHash <= StakeTarget)
    } // for (const auto& pcoin : CoinsToStake)

    g_miner_status.UpdateLastSearch(
        kernel_found,
        txnew.nTime,
        blocknew.nVersion,
        StakeWeightSum,
        StakeValueSum,
        StakeWeightMin,
        StakeWeightMax,
        GRC::CalculateStakeWeightV8(balance));

    g_timer.GetTimes(function + "stake UTXO loop", "miner");

    return kernel_found;
}

void SplitCoinStakeOutput(CBlock &blocknew, int64_t &nReward, bool &fEnableStakeSplit, bool &fEnableSideStaking,
    int64_t &nMinStakeSplitValue, double &dEfficiency)
{
    // When this function is called, CreateCoinStake and CreateGridcoinReward have already been called
    // and there will be a single coinstake output (besides the empty one) that has the combined stake + research
    // reward. This function does the following...
    // 1. Perform reward payment to specified addresses ("sidestaking") in the following manner...
    //      a. Check if both flags false and if so return with no action.
    //      b. Limit number of outputs based on bv: 3 for <= v9, 8 for v10 & v11, and 10 for >= v12.
    //      c. Pull the nValue from the original output and store locally. (nReward was passed in.)
    //      d. Pop the existing outputs.
    //      e. Validate each address provided for redirection in turn. If valid, create an output of the
    //         reward * the specified percentage at the validated address. Keep a running total of the reward
    //         redirected. If the percentages add up to less than 100%, then the running total will be less
    //         than the original specified reward after pushing up to two reward outputs. Check before each allocation
    //         that 100% will not be exceeded. If there is a residual reward left, then add (nReward - running total)
    //         back to the base coinstake. This also works beautifully if the redirection is disabled, because then
    //         no reward outputs will be created, the accumulated total of redirected rewards will be zero,
    //         and therefore the subtraction will put the entire rewards back on the base coinstake.
    // 2. Perform stake output splitting of remaining value.
    //      a. With the remainder value left which is now the original coinstake minus (rewards outputs pushed
    //         - up to two), call GGetNumberOfStakeOutputs(int64_t nValue, unsigned int nOutputsAlreadyPushed)
    //         to determine how many pieces to split into, limiting to 8 - OutputsAlreadyPushed.
    //      b. SplitCoinStakeOutput will then push the remaining value into the determined number of outputs
    //         of equal size using the original coinstake input address.
    //      c. The outputs will be in the wrong order, so add the empty vout to the end and then reverse
    //         vout.begin() to vout.end() to make sure the empty vout is in [0] position and a coinstake
    //         vout is in the [1] position. This is required by block validation.

    // If somehow we got here and both flags are false, return immediately. This should not happen if called
    // from StakeMiner, because one or both of the flags must be set to trigger the call, but check anyway.
    if (!fEnableStakeSplit && !fEnableSideStaking)
        return;

    // Record the script public key for the base coinstake so we can reuse.
    CScript CoinStakeScriptPubKey = blocknew.vtx[1].vout[1].scriptPubKey;

    // The maximum number of outputs allowed on the coinstake txn is 3 for block version 9 and below and
    // 8 for 10 and above (excluding MRC outputs). The first one must be empty, so that gives 2 and 7 usable ones,
    // respectively. MRC outputs are excluded here. They are addressed in CreateMRC separately. Unlike in other areas,
    // the foundation sidestake IS COUNTED in the GetMRCOutputLimit because it is a sidestake, but handled in the
    // CreateMRCRewards function and not here. For block version 12+ nMaxOutputs is 10, which gives 9 usable.
    unsigned int nMaxOutputs = GetCoinstakeOutputLimit(blocknew.nVersion) - GetMRCOutputLimit(blocknew.nVersion, true);

    // Set the maximum number of sidestake outputs to two less than the maximum allowable coinstake outputs
    // to ensure outputs are reserved for the coinstake output itself and the empty one. Any sidestake
    // addresses and percentages in excess of this number will be ignored.
    unsigned int nMaxSideStakeOutputs = nMaxOutputs - 2;

    // Initialize nOutputUsed at 1, because one is already used for the empty coinstake flag output.
    unsigned int nOutputsUsed = 1;

    // If the number of sidestaking allocation entries exceeds nMaxSideStakeOutputs, then shuffle the vSideStakeAlloc
    // to support sidestaking with more than eight entries. This is a super simple solution but has some disadvantages.
    // If the person made a mistake and has the entries in the config file add up to more than 100%, then those entries
    // resulting a cumulative total over 100% will always be excluded, not just randomly excluded, because the cumulative
    // check is done in the order of the entries in the config file. This is not regarded as a big issue, because
    // all of the entries are supposed to add up to less than or equal to 100%. Also when there are more than
    // mMaxSideStakeOutput entries, the residual returned to the coinstake will vary when the entries are shuffled,
    // because the total percentage of the selected entries will be randomized. No attempt to renormalize
    // the percentages is done.
    SideStakeAlloc mandatory_sidestakes
        = GRC::GetSideStakeRegistry().ActiveSideStakeEntries(GRC::SideStake::FilterFlag::MANDATORY, false);
    SideStakeAlloc local_sidestakes
        = GRC::GetSideStakeRegistry().ActiveSideStakeEntries(GRC::SideStake::FilterFlag::LOCAL, false);

    if (mandatory_sidestakes.size() > GetMandatorySideStakeOutputLimit(blocknew.nVersion)) {
        Shuffle(mandatory_sidestakes.begin(), mandatory_sidestakes.end(), FastRandomContext());
    }

    if (local_sidestakes.size() > nMaxSideStakeOutputs
                                      - std::min<unsigned int>(GetMandatorySideStakeOutputLimit(blocknew.nVersion),
                                                                                mandatory_sidestakes.size())) {
        Shuffle(local_sidestakes.begin(), local_sidestakes.end(), FastRandomContext());
    }

    // Initialize remaining stake output value to the total value of output for stake, which also includes
    // (interest or CBR), research rewards, and fees to the miner from the transactions in the block. vout elements at
    // index entries higher than 1 are MRC outputs.
    int64_t nRemainingStakeOutputValue = blocknew.vtx[1].vout[1].nValue;

    // We need the input value later.
    int64_t nInputValue = nRemainingStakeOutputValue - nReward;

    // Save the original coinstake vout and then clear the coinstake vout to prepare for splitting. The saved coinstake vout
    // also contains MRC outputs if MRC requests were bound in the block and validated by CreateMRC. Note that the additional
    // reward to the staker due from the allocated fees from MRC have already been added to the coinstake output. This was
    // the critical step to ensure the splitting is done correctly.
    std::vector<CTxOut> orig_coinstake_vout = blocknew.vtx[1].vout;

    blocknew.vtx[1].vout.clear();

    CScript SideStakeScriptPubKey;
    GRC::Allocation SumAllocation;

    // Lambda for sidestake allocation. This iterates through the provided sidestake vector until either all elements processed,
    // the maximum number of sidestake outputs is reached via the provided output_limit, or accumulated allocation will exceed 100%.
    const auto allocate_sidestakes = [&](SideStakeAlloc sidestakes, unsigned int output_limit) {
        for (auto iterSideStake = sidestakes.begin();
             (iterSideStake != sidestakes.end())
             && (nOutputsUsed <= output_limit);
             ++iterSideStake)
        {
            CBitcoinAddress address(iterSideStake->get()->GetDestination());
            GRC::Allocation allocation = iterSideStake->get()->GetAllocation();

            if (!address.IsValid())
            {
                LogPrintf("WARN: SplitCoinStakeOutput: ignoring sidestake invalid address %s.",
                          address.ToString());
                continue;
            }

            // Do not process a distribution that would result in an output less than 1 CENT. This will flow back into
            // the coinstake below. Prevents dust build-up.
            //
            // This is extremely important for mandatory sidestakes when validating this on a receiving node.
            // This allows the validator to retrace the dust elimination for the coinstake mandatory sidestakes, and
            // verify that EITHER the residual number of mandatory outputs after dust elimination is less than or equal to the
            // maximum, in which case they all must be present and valid, OR, the residual number of outputs is greater than the
            // maximum, which means that the maximum number of mandatory outputs MUST be present and valid.
            //
            // Note that nOutputsUsed is NOT incremented if the output is suppressed by this check.
            if (allocation * nReward < CENT)
            {
                LogPrintf("WARN: SplitCoinStakeOutput: distribution %f too small to address %s.",
                          CoinToDouble((allocation * nReward).ToCAmount()),
                          address.ToString()
                          );
                continue;
            }

            if (SumAllocation + allocation > 1)
            {
                LogPrintf("WARN: SplitCoinStakeOutput: allocation percentage over 100 percent, "
                          "ending sidestake allocations.");
                break;
            }

            // Push to an output the (reward times the allocation) to the address, increment the accumulator for allocation,
            // decrement the remaining stake output value, and increment outputs used.

            SideStakeScriptPubKey.SetDestination(address.Get());

            // It is entirely possible that the coinstake could be from an address that is specified in one of the sidestake
            // entries if the sidestake address(es) are local to the staking wallet. There is no reason to sidestake in that
            // case. The coins should flow down to the coinstake outputs and be returned there. This will also simplify the
            // display logic in the UI, because it makes the sidestake and coinstake outputs disjoint from an address point
            // of view.
            if (SideStakeScriptPubKey == CoinStakeScriptPubKey)
                continue;

            int64_t nSideStake = 0;

            // For allocations ending less than 100% assign using sidestake allocation.
            if (SumAllocation + allocation < 1)
                nSideStake = (allocation * nReward).ToCAmount();
            // We need to handle the final sidestake differently in the case it brings the total allocation up to 100%,
            // because testing showed in corner cases the output return to the staking address could be off by one Halford.
            else if (SumAllocation + allocation == 1)
                // Simply assign the special case final nSideStake the remaining output value minus input value to ensure
                // a match on the output flowing down.
                nSideStake = nRemainingStakeOutputValue - nInputValue;

            blocknew.vtx[1].vout.push_back(CTxOut(nSideStake, SideStakeScriptPubKey));

            LogPrintf("SplitCoinStakeOutput: create sidestake UTXO %i value %f to address %s",
                      nOutputsUsed,
                      CoinToDouble((allocation * nReward).ToCAmount()),
                      address.ToString()
                      );
            SumAllocation += allocation;
            nRemainingStakeOutputValue -= nSideStake;
            nOutputsUsed++;
        }
    };

    if (fEnableSideStaking) {
        // Iterate through mandatory SideStake vector until either all elements processed, the maximum number of
        // mandatory sidestake outputs is reached, or accumulated allocation will exceed 100%.
        allocate_sidestakes(mandatory_sidestakes, GetMandatorySideStakeOutputLimit(blocknew.nVersion));

        // Iterate through local SideStake vector until either all elements processed, the maximum number of
        // sidestake outputs is reached, or accumulated allocation will exceed 100%.
        allocate_sidestakes(local_sidestakes, nMaxSideStakeOutputs);
    }

    // By this point, if SideStaking was used and 100% was allocated nRemainingStakeOutputValue will be
    // the original base coinstake. The other extreme is no sidestaking, in which case nRemainingStakeOutputValue is the
    // base coinstake + all of the reward. In any case, we will now compute the number of split stake outputs needed,
    // limiting actual number of split stake outputs to remaining available (nMaxOutputs - nOutputsUsed). There will be
    // at least one available, because if sidestaking is activated, it is limited to nMaxOutputs - 2.
    // Don't do any work here except pushing a single output if flag is false.
    if (fEnableStakeSplit)
    {
        unsigned int nSplitStakeOutputs = min((nMaxOutputs - nOutputsUsed),
                                              GetNumberOfStakeOutputs(nRemainingStakeOutputValue,
                                                                      nMinStakeSplitValue,
                                                                      dEfficiency));
        LogPrint(BCLog::LogFlags::MINER, "SplitCoinStakeOutput: nStakeOutputs = %u", nSplitStakeOutputs);

        // Set Actual Stake output value for split stakes to remaining output value divided by the number of split
        // stake outputs.
        int64_t nActualStakeOutputValue = nRemainingStakeOutputValue / nSplitStakeOutputs;

        LogPrint(BCLog::LogFlags::MINER, "SplitCoinStakeOutput: nSplitStakeOutputs = %f", nSplitStakeOutputs);
        LogPrint(BCLog::LogFlags::MINER, "SplitCoinStakeOutput: nActualStakeOutputValue = %f",
                 CoinToDouble(nActualStakeOutputValue));

        int64_t nSumStakeOutputValue = 0;
        for (unsigned int i = 1; i < nSplitStakeOutputs; i++)
        {
            blocknew.vtx[1].vout.push_back(CTxOut(nActualStakeOutputValue, CoinStakeScriptPubKey));
            LogPrintf("SplitCoinStakeOutput: create stake UTXO %i value %f", nOutputsUsed,
                      CoinToDouble(nActualStakeOutputValue));
            nOutputsUsed++;
            nSumStakeOutputValue += nActualStakeOutputValue;
        }
        // For the last UTXO, subtract the running sum from the desired total, which does not include the last UTXO
        // so far, to recover anything lost from rounding. This will be a very small difference from the others.
        // The reason we go through this trouble is that integer division rounds down, and we don't even want
        // to have the wallet lose 1 Halford.
        blocknew.vtx[1].vout.push_back(CTxOut((nRemainingStakeOutputValue - nSumStakeOutputValue), CoinStakeScriptPubKey));
        LogPrintf("SplitCoinStakeOutput: create stake UTXO %i value %f", nOutputsUsed,
                  CoinToDouble(nRemainingStakeOutputValue - nSumStakeOutputValue));
    }
    else
    {
        // Stake splitting flag is false, so just push back a single output with the remaining output value.
        blocknew.vtx[1].vout.push_back(CTxOut(nRemainingStakeOutputValue, CoinStakeScriptPubKey));
    }

    // Now, the outputs are in the wrong order. We had to do the rewards distribution first
    // because it can be of variable number of elements, and if someone puts an address on the list,
    // there is a reasonable expectation it should succeed, while the stake splitting is a luxury.
    // One of the coinstake outputs MUST be at vout[1] to pass block validation, so we will
    // add the empty one at the end. And then use std::reverse to reverse vout.begin() to vout.end().
    // This will put the correct vout[0] and vout[1] in the right place.
    blocknew.vtx[1].vout.push_back(CTxOut(0, CScript()));
    std::reverse(blocknew.vtx[1].vout.begin(), blocknew.vtx[1].vout.end());

    // Add the MRC outputs back now that the splitting/sidestaking is done. We start at 2, because the empty output and the
    // original unsplit coinstake have already been handled above.
    for (unsigned int i = 2; i < orig_coinstake_vout.size(); ++i) {
        blocknew.vtx[1].vout.push_back(orig_coinstake_vout[i]);
    }

    // The final state here of the coinstake blocknew.vtx[1].vout is
    // [empty],
    // [reward split 1], [reward split 2], ... , [reward split m],
    // [mandatory sidestake 1], ... , [mandatory sidestake n]
    // [sidestake 1], ... , [sidestake p],
    // [MRC 1], ..., [MRC q].
    //
    // Currently according to the output limit rules encoded in CreateMRC and here:
    // For block version 10-11:
    // one empty, m <= 7, n = 0, n + p <= 6, m + n + p <= 7 (i.e. empty + m + n + p <= 8), and q = 0, total <= 8..
    //
    // For block version 12:
    // one empty, m <= 9, n = 0, n + p <= 8, m + n + p <= 9 (i.e. empty + m + n + p <= 10), and q <= 10, total <= 20.
    // (On testnet q <= 3, total <= 13.)

    // For block version 13+:
    // one empty, m <= 9, n <= 4, n + p <= 8, m + n + p <= 9 (i.e. empty + m + n + p <= 10), and q <= 10, total <= 20.
    // (On testnet q <= 3, total <= 13.)

    // The total generated GRC is the total of the reward splits - the fees (the original GridcoinReward which is the
    // research reward + CBR), plus the total of the MRC outputs 2 to p (these outputs already have the fees subtracted)
    // MRC output 1 is always to the foundation (it is essentially a sidestake) and represents a cut of the MRC fees.
}

unsigned int GetNumberOfStakeOutputs(int64_t &nValue, int64_t &nMinStakeSplitValue, double &dEfficiency)
{
    int64_t nDesiredStakeOutputValue = 0;
    unsigned int nStakeOutputs = 1;

    // For the definition of the constant G, please see
    // https://docs.google.com/document/d/1OyuTwdJx1Ax2YZ42WYkGn_UieN0uY13BTlA5G5IAN00/edit?usp=sharing
    // Refer to page 5 for G. This link is a draft of an upcoming bluepaper section.
    const double G = 9942.2056;

    // If the stake output provided to GetNumberofStakeOutputs is not greater than nMinStakeSplitValue then there will
    // be only one output, so skip calculations and return(nStakeOutputs) which is initialized to 1.
    if (nValue > nMinStakeSplitValue)
    {
        // Set Desired UTXO size post stake based on passed in efficiency and difficulty, but do not allow to go below
        // passed in MinStakeSplitValue. Note that we use GetAverageDifficulty over a 4 hour (160 block period) rather than
        // StakeKernelDiff, because the block to block difficulty has too much scatter. Please refer to the above link,
        // equation (27) on page 10 as a reference for the below formula.
        nDesiredStakeOutputValue = G * GRC::GetAverageDifficulty(160) * (3.0 / 2.0) * (1 / dEfficiency  - 1) * COIN;
        nDesiredStakeOutputValue = max(nMinStakeSplitValue, nDesiredStakeOutputValue);

        LogPrint(BCLog::LogFlags::MINER, "GetNumberOfStakeOutputs: nDesiredStakeOutputValue = %f",
                 CoinToDouble(nDesiredStakeOutputValue));

        // Divide nValue by nDesiredStakeUTXOValue. We purposely want this to be integer division to round down.
        nStakeOutputs = max((int64_t) 1, nValue / nDesiredStakeOutputValue);

        LogPrint(BCLog::LogFlags::MINER, "GetNumberOfStakeOutputs: nStakeOutputs = %u", nStakeOutputs);
    }

    return nStakeOutputs;
}

bool SignStakeBlock(CBlock &block, CKey &key,
                    vector<const CWalletTx*> &StakeInputs, CWallet *pwallet) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    //Sign the coinstake transaction
    unsigned nIn = 0;
    for (auto const& pcoin : StakeInputs)
    {
        if (!SignSignature(*pwallet, *pcoin, block.vtx[1], nIn++))
        {
            return error("SignStakeBlock: failed to sign coinstake");
        }
    }

    // Researcher claim signatures depend on the coinstake transaction, so we
    // sign claims after we sign the coinstake:
    //
    if (!TrySignClaim(pwallet, block.GetClaim(), block)) {
        return error("%s: failed to sign claim", __func__);
    }

    //Sign the whole block
    block.hashMerkleRoot = BlockMerkleRoot(block);
    if( !key.Sign(block.GetHash(), block.vchBlockSig) )
    {
        return error("SignStakeBlock: failed to sign block");
    }

    return true;
}

void AddSuperblockContractOrVote(CBlock& blocknew)
{
    if (OutOfSyncByAge()) {
        LogPrintf("AddSuperblockContractOrVote: Out of sync.");
        return;
    }

    if (!GRC::Quorum::SuperblockNeeded(blocknew.nTime)) {
        LogPrintf("AddSuperblockContractOrVote: Not needed.");
        return;
    }

    GRC::Superblock superblock = GRC::Quorum::CreateSuperblock();

    if (!superblock.WellFormed()) {
        LogPrintf("AddSuperblockContractOrVote: Local contract empty.");
        return;
    }

    // TODO: fix the const cast:
    GRC::Claim& claim = const_cast<GRC::Claim&>(blocknew.GetClaim());

    claim.m_quorum_hash = superblock.GetHash();
    claim.m_superblock.Replace(std::move(superblock));

    LogPrintf(
        "AddSuperblockContractOrVote: Added our Superblock (size %" PRIszu ").",
        GetSerializeSize(claim.m_superblock, SER_NETWORK, 1));
}

bool CreateGridcoinReward(
        CBlock &blocknew,
        CBlockIndex* pindexPrev,
        int64_t &nReward,
        GRC::Claim& claim) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Remove fees from coinbase:
    int64_t nFees = blocknew.vtx[0].vout[0].nValue;
    blocknew.vtx[0].vout[0].SetEmpty();

    const GRC::ResearcherPtr researcher = GRC::Researcher::Get();

    // This is transition code for the V11 to V12 block format change, which also corresponds to a version change
    // for the claim contract from 3 to 4.
    if (!IsV12Enabled(pindexPrev->nHeight + 1)) {
        claim.m_version = 3;
    }

    claim.m_mining_id = researcher->Id();

    // If a researcher's beacon expired, generate the block as an investor. We
    // cannot sign a research claim without the beacon key, so this avoids the
    // issue that prevents a researcher from staking blocks if the beacon does
    // not exist (it expired or it has yet to be advertised).
    //
    if (researcher->Status() == GRC::ResearcherStatus::NO_BEACON) {
        LogPrintf(
            "CreateGridcoinReward: CPID eligible but no active beacon key "
            "found. Staking as investor.");

        claim.m_mining_id = GRC::MiningId::ForInvestor();
    }

    // First argument is coin age - unused since CBR (block version 10)
    nReward = GRC::GetProofOfStakeReward(0, blocknew.nTime, pindexPrev);
    claim.m_block_subsidy = nReward;
    nReward += nFees;

    if (const GRC::CpidOption cpid = claim.m_mining_id.TryCpid()) {
        claim.m_research_subsidy = GRC::Tally::GetAccrual(*cpid, blocknew.nTime, pindexPrev);

        // If no pending research subsidy value exists, build an investor claim.
        // This avoids polluting the block index with non-research reward blocks
        // that contain CPIDs which increases the effort needed to load research
        // age context at start-up:
        //
        if (claim.m_research_subsidy <= 0) {
            LogPrintf(
                "CreateGridcoinReward: No positive research reward pending at "
                "time of stake. Staking as investor.");

            claim.m_mining_id = GRC::MiningId::ForInvestor();
        } else {
            nReward += claim.m_research_subsidy;
            claim.m_magnitude = GRC::Quorum::GetMagnitude(*cpid).Floating();
        }
    }

    claim.m_client_version = FormatFullVersion().substr(0, GRC::Claim::MAX_VERSION_SIZE);
    claim.m_organization = gArgs.GetArg("-org", "").substr(0, GRC::Claim::MAX_ORGANIZATION_SIZE);

    blocknew.vtx[1].vout[1].nValue += nReward;

    return true;
}


bool IsMiningAllowed(CWallet *pwallet)
{
    if (pwallet->IsLocked()) {
        g_miner_status.AddError(GRC::MinerStatus::WALLET_LOCKED);
    }

    if (fDevbuildCripple) {
        g_miner_status.AddError(GRC::MinerStatus::TESTNET_ONLY);
    }

    if (vNodes.size() < GetMinimumConnectionsRequiredForStaking() || (!fTestNet && IsInitialBlockDownload())) {
        g_miner_status.AddError(GRC::MinerStatus::OFFLINE);
    }

    if (!gArgs.GetBoolArg("-staking", true)) {
        g_miner_status.AddError(GRC::MinerStatus::DISABLED_BY_CONFIGURATION);
    }

    return g_miner_status.StakingEnabled();
}

// This function parses the config file for the directives for stake splitting. It is used
// in StakeMiner for the miner loop and also called by rpc getstakinginfo.
bool GetStakeSplitStatusAndParams(int64_t& nMinStakeSplitValue, double& dEfficiency, int64_t& nDesiredStakeOutputValue)
{
    // Parse StakeSplit and SideStaking flags.
    bool fEnableStakeSplit = gArgs.GetBoolArg("-enablestakesplit");
    LogPrint(BCLog::LogFlags::MINER, "StakeMiner: fEnableStakeSplit = %u", fEnableStakeSplit);

    // If stake output splitting is enabled, determine efficiency and minimum stake split value.
    if (fEnableStakeSplit)
    {
        // Pull efficiency for UTXO staking from config, but constrain to the interval [0.75, 0.98]. Use default of 0.90.
        dEfficiency = (double)gArgs.GetArg("-stakingefficiency", 90) / 100;
        if (dEfficiency > 0.98)
            dEfficiency = 0.98;
        else if (dEfficiency < 0.75)
            dEfficiency = 0.75;

        LogPrint(BCLog::LogFlags::MINER, "StakeMiner: dEfficiency = %f", dEfficiency);

        // Pull Minimum Post Stake UTXO Split Value from config or command line parameter.
        // Default to 800 and do not allow it to be specified below 800 GRC.
        nMinStakeSplitValue = max(gArgs.GetArg("-minstakesplitvalue", MIN_STAKE_SPLIT_VALUE_GRC), MIN_STAKE_SPLIT_VALUE_GRC)
                              * COIN;

        LogPrint(BCLog::LogFlags::MINER, "StakeMiner: nMinStakeSplitValue = %f", CoinToDouble(nMinStakeSplitValue));

        // For the definition of the constant G, please see
        // https://docs.google.com/document/d/1OyuTwdJx1Ax2YZ42WYkGn_UieN0uY13BTlA5G5IAN00/edit?usp=sharing
        // Refer to page 5 for G. This link is a draft of an upcoming bluepaper section.
        const double G = 9942.2056;

        // Desired UTXO size post stake based on passed in efficiency and difficulty, but do not allow to go below
        // passed in MinStakeSplitValue. Note that we use GetAverageDifficulty over a 4 hour (160 block period) rather than
        // StakeKernelDiff, because the block to block difficulty has too much scatter. Please refer to the above link,
        // equation (27) on page 10 as a reference for the below formula.
        nDesiredStakeOutputValue = G * GRC::GetAverageDifficulty(160) * (3.0 / 2.0) * (1 / dEfficiency  - 1) * COIN;
        nDesiredStakeOutputValue = max(nMinStakeSplitValue, nDesiredStakeOutputValue);
    }

    return fEnableStakeSplit;
}



void StakeMiner(CWallet *pwallet)
{
    // Make this thread recognisable as the mining thread
    RenameThread("grc-stake-miner");

    int64_t nMinStakeSplitValue = 0;
    double dEfficiency = 0;
    int64_t nDesiredStakeOutputValue = 0;
    SideStakeAlloc vSideStakeAlloc;

    std::string function = __func__;
    function += ": ";

    while (!fShutdown)
    {
        // nMinStakeSplitValue and dEfficiency are out parameters.
        bool fEnableStakeSplit = GetStakeSplitStatusAndParams(nMinStakeSplitValue, dEfficiency, nDesiredStakeOutputValue);

        // If the vSideStakeAlloc is not empty, then set fEnableSideStaking to true. Note that vSideStakeAlloc will not be empty
        // if non-zero allocation mandatory sidestakes are set OR local sidestaking is turned on by the -enablesidestaking config
        // option.
        bool fEnableSideStaking = (!GRC::GetSideStakeRegistry().ActiveSideStakeEntries(GRC::SideStake::FilterFlag::ALL, false).empty());

        // wait for next round
        if (!MilliSleep(nMinerSleep)) return;

        g_timer.InitTimer("miner", LogInstance().WillLogCategory(BCLog::LogFlags::MISC));

        //clear miner messages
        g_miner_status.ClearErrors();

        if (!IsMiningAllowed(pwallet))
        {
            g_miner_status.ClearLastSearch();
            continue;
        }

        g_timer.GetTimes(function + "IsMiningAllowed", "miner");

        CBlock StakeBlock;
        std::map<GRC::Cpid, std::pair<uint256, GRC::MRC>> mrc_map;
        std::map<GRC::Cpid, uint256> mrc_tx_map;

        LOCK(cs_main);

        g_timer.GetTimes(function + "lock cs_main", "miner");

        CBlockIndex* pindexPrev = pindexBest;

        // * Create a bare block

        // This transition code is to handle the v12 to v13 transition. The other transition handling
        // for block versions in the stakeminer have been removed, because no blocks of an earlier version
        // than v13 can be staked now, since the chain is past the v12 transition height.
        if (!IsV13Enabled(pindexPrev->nHeight + 1)) {
            StakeBlock.nVersion = 12;
        }

        StakeBlock.nTime = GetAdjustedTime();
        StakeBlock.nNonce = 0;
        StakeBlock.nBits = GRC::GetNextTargetRequired(pindexPrev);
        StakeBlock.vtx.resize(2);
        //tx 0 is coin_base
        CTransaction &StakeTX = StakeBlock.vtx[1]; //tx 1 is coin_stake

        // * Try to create a CoinStake transaction
        CKey BlockKey;
        vector<const CWalletTx*> StakeInputs;

        bool createcoinstake_success = CreateCoinStake(StakeBlock, BlockKey, StakeInputs, *pwallet, pindexPrev);

        g_timer.GetTimes(function + "CreateCoinStake", "miner");

        if (!createcoinstake_success) continue;

        StakeBlock.nTime = StakeTX.nTime;

        // * create rest of the block. Note that there is a little catch-22 here, because to be entirely proper, for block
        // sizing, CreateGridcoinReward and the SplitCoinstakeOutput should be done first because more outputs may be
        // created; hoewever, CreateGridcoinReward needs to know the total fees from the rest of the block. In reality the
        // current order of this is fine, because we are using MAX_BLOCK_SIZE_GEN/2 as default, and in any case we clamp
        // to MAX_BLOCK_SIZE - 1000, which covers the little extra space taken by the up to six additional outputs allowed
        // for sidestaking/stakesplitting and 5 for MRC payments.
        if (!CreateRestOfTheBlock(StakeBlock, pindexPrev, mrc_map)) continue;

        LogPrintf("INFO: %s: created rest of the block", __func__);

        // * add Gridcoin reward to coinstake, fill-in nReward
        int64_t nReward = 0;
        GRC::Claim claim;
        if (!CreateGridcoinReward(StakeBlock, pindexPrev, nReward, claim)) continue;

        g_timer.GetTimes(function + "CreateGridcoinReward", "miner");

        LogPrintf("INFO: %s: added Gridcoin reward to coinstake", __func__);

        uint32_t claim_contract_version = IsV13Enabled(pindexPrev->nHeight + 1) ? 3 : 2;

        // * Add MRC outputs to coinstake. This has to be done before the coinstake splitting/sidestaking, because
        // Some of the MRC fees go to the miner as part of the reward, and this affects the SplitCoinStakeOutput calculation.
        // Note that nReward here now includes the mrc fees to the staker.
        if (!CreateMRCRewards(StakeBlock, mrc_map, mrc_tx_map, nReward, claim_contract_version, claim, pwallet)) continue;

        g_timer.GetTimes(function + "CreateMRC", "miner");

        LogPrintf("INFO: %s: added MRC reward outputs to coinstake", __func__);

        // * If argument is supplied desiring stake output splitting or side staking, then call SplitCoinStakeOutput.
        if (fEnableStakeSplit || fEnableSideStaking)
            SplitCoinStakeOutput(StakeBlock, nReward, fEnableStakeSplit, fEnableSideStaking,
                                 nMinStakeSplitValue, dEfficiency);

        g_timer.GetTimes(function + "SplitCoinStakeOutput", "miner");

        LogPrintf("INFO: %s: performed stakesplitting/sidestaking", __func__);

        AddSuperblockContractOrVote(StakeBlock);

        g_timer.GetTimes(function + "AddSuperblockContractOrVote", "miner");

        // * sign boinchash, coinstake, wholeblock
        if (!SignStakeBlock(StakeBlock, BlockKey, StakeInputs, pwallet)) continue;

        g_timer.GetTimes(function + "SignStakeBlock", "miner");

        LogPrintf("INFO: %s: signed boinchash, coinstake, wholeblock", __func__);

        g_miner_status.IncrementBlocksCreated();

        if (LogInstance().WillLogCategory(BCLog::LogFlags::VERBOSE)) {
            COutPoint stake_prevout = StakeBlock.vtx[1].vin[0].prevout;
            CTransaction stake_input;
            ReadTxFromDisk(stake_input, stake_prevout);

            LogPrintf("INFO: %s: stake input = %s",
                      __func__,
                      FormatMoney(stake_input.vout[stake_prevout.n].nValue));

            for (unsigned int i = 1; i < StakeBlock.vtx[1].vout.size(); ++i) {
                CTxDestination destination;

                ExtractDestination(StakeBlock.vtx[1].vout[i].scriptPubKey, destination);

                LogPrintf("INFO: %s: stake output[%u] = %s, destination = %s",
                          __func__,
                          i,
                          FormatMoney(StakeBlock.vtx[1].vout[i].nValue),
                          CBitcoinAddress(destination).ToString());
            }
        }

        // * delegate to ProcessBlock
        if (!ProcessBlock(nullptr, &StakeBlock, true)) {
            error("%s: Block vehemently rejected", __func__);
            continue;
        }

        LogPrintf("INFO: %s: block processed", __func__);

        g_miner_status.UpdateLastStake(StakeBlock.vtx[1].GetHash());

        g_timer.GetTimes(function + "ProcessBlock", "miner");
    } //end while(!fShutdown)
}
