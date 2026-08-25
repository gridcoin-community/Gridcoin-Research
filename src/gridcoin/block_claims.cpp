// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// The CBlock reward-claim accessors, moved out of main.cpp (issue #3125 C9).
// These are member functions of CBlock/CBlockIndex declared in
// primitives/block.h, but they cannot live in primitives/block.cpp: their
// bodies drag GRC::Claim / GRC::MRC / CTxDB into the primitives layer, which
// must stay free of gridcoin- and database-layer dependencies.

#include "chain.h"
#include "chainparams.h"
#include "dbwrapper.h"
#include "gridcoin/claim.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/mrc.h"
#include "node/blockstorage.h"
#include "primitives/block.h"
#include "util.h"
#include "validation.h"

const GRC::Claim& CBlock::GetClaim() const
{
    // Take the contract only when there is actually a claim contract to take.
    //
    // The version used to be the whole of this test, which made the accessor
    // partial: a v11+ block with no coinbase contracts indexed an empty vector,
    // and one whose first contract was some other type reached
    // SharePayloadAs<Claim>() over a payload that is not a Claim. Blocks are
    // deserialized from untrusted input and this is reached from CheckBlock()
    // before the coinbase shape has been established, so neither is a shape the
    // caller can be assumed to have ruled out first.
    //
    // Testing the shape rather than the version keeps every input defined and
    // decides nothing about validity: a block that lands in the fallback below
    // is one the coinbase contract checks in CheckBlock() reject anyway. The
    // blocks that satisfy those checks take the same branch they always did.
    if (!vtx.empty()
        && !vtx[0].vContracts.empty()
        && vtx[0].vContracts[0].m_type == GRC::ContractType::CLAIM)
    {
        return *vtx[0].vContracts[0].SharePayloadAs<GRC::Claim>();
    }

    // Before block version 11, the Gridcoin reward claim context is stored
    // in the hashBoinc field of the first transaction. We cache the parsed
    // representation in the block to speed up subsequent access:
    //
    if (m_claim_contract_cache.m_type == GRC::ContractType::UNKNOWN) {
        m_claim_contract_cache = GRC::MakeContract<GRC::Claim>(
            GRC::ContractAction::ADD,
            vtx.empty() ? GRC::Claim() : GRC::Claim::Parse(vtx[0].hashBoinc, nVersion));
    }

    return *m_claim_contract_cache.SharePayloadAs<GRC::Claim>();
}

GRC::Claim CBlock::PullClaim()
{
    // Shape, not version -- see GetClaim() above for why.
    if (!vtx.empty()
        && !vtx[0].vContracts.empty()
        && vtx[0].vContracts[0].m_type == GRC::ContractType::CLAIM)
    {
        // PullPayloadAs operates on the shared_ptr within the Contract,
        // not on the vector element itself, so const vContracts is fine.
        return vtx[0].vContracts[0].CopyPayloadAs<GRC::Claim>();
    }

    if (vtx.empty()) {
        return GRC::Claim();
    }

    // Before block version 11, the Gridcoin reward claim context is stored
    // in the hashBoinc field of the first transaction.
    //
    return GRC::Claim::Parse(vtx[0].hashBoinc, nVersion);
}

GRC::SuperblockPtr CBlock::GetSuperblock() const
{
    return GetClaim().m_superblock;
}

GRC::SuperblockPtr CBlock::GetSuperblock(const CBlockIndex* const pindex) const
{
    GRC::SuperblockPtr superblock = GetSuperblock();
    superblock.Rebind(pindex);

    return superblock;
}

GRC::ClaimOption GetClaimByIndex(const CBlockIndex* const pblockindex)
{
    CBlock block;

    if (!pblockindex || !pblockindex->IsInMainChain()
        || !ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
    {
        return std::nullopt;
    }

    return block.PullClaim();
}

GRC::MintSummary CBlock::GetMint() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    CTxDB txdb("r");
    GRC::MintSummary mint;

    for (const auto& tx : vtx) {
        const CAmount tx_amount_out = tx.GetValueOut();

        if (tx.IsCoinBase()) {
            mint.m_total += tx_amount_out;
            continue;
        }

        CAmount tx_amount_in = 0;

        for (const auto& input : tx.vin) {
            CTransaction input_tx;

            if (txdb.ReadDiskTx(input.prevout.hash, input_tx)) {
                tx_amount_in += input_tx.vout[input.prevout.n].nValue;
            }
        }

        if (tx.IsCoinStake()) {
            mint.m_total += tx_amount_out - tx_amount_in;
        } else {
            mint.m_fees += tx_amount_in - tx_amount_out;
        }
    }

    return mint;
}

GRC::MRCFees CBlock::GetMRCFees() const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    GRC::MRCFees mrc_fees;
    unsigned int mrc_output_limit = GetMRCOutputLimit(nVersion, false);

    // Return zeroes for mrc fees if MRC not allowed. (This could have also been done
    // by block version check, but this is more correct.)
    if (!mrc_output_limit) {
        return mrc_fees;
    }

    Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

    const GRC::Claim claim = GetClaim();

    CAmount mrc_total_fees = 0;

    // This is similar to the code in CheckMRCRewards in the Validator class, but with the validation removed because
    // the block has already been validated. We also only need the MRC fee calculation portion.
    for (const auto& tx: vtx) {
        for (const auto& mrc : claim.m_mrc_tx_map) {
            if (mrc.second == tx.GetHash() && !tx.GetContracts().empty()) {
                // An MRC contract must be the first and only contract on a transaction by protocol.
                GRC::Contract contract = tx.GetContracts()[0];

                if (contract.m_type != GRC::ContractType::MRC) continue;

                GRC::MRC mrc = contract.CopyPayloadAs<GRC::MRC>();

                mrc_fees.m_mrc_minimum_calc_fees += mrc.ComputeMRCFee();

                mrc_total_fees += mrc.m_fee;
                mrc_fees.m_mrc_foundation_fees += mrc.m_fee * foundation_fee_fraction.GetNumerator()
                                                            / foundation_fee_fraction.GetDenominator();
            }
        }
    }

    mrc_fees.m_mrc_staker_fees = mrc_total_fees - mrc_fees.m_mrc_foundation_fees;

    return mrc_fees;
}
