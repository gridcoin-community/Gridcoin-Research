// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/consensus/block_rewards.h"
#include "gridcoin/beacon.h"
#include "gridcoin/claim.h"
#include "gridcoin/mrc.h"
#include "gridcoin/quorum.h"
#include "gridcoin/sidestake.h"
#include "gridcoin/staking/exceptions.h"
#include "gridcoin/staking/reward.h"
#include "gridcoin/tally.h"
#include "chainparams.h"
#include "main.h"
#include "random.h"
#include "util.h"
#include "validation.h"

#include <algorithm>
#include <cassert>

using namespace GRC;

BlockRewardRules::BlockRewardRules(
    const CBlockIndex* pindex_prev,
    int block_version,
    int64_t block_time)
    : m_pindex_prev(pindex_prev)
    , m_block_version(block_version)
    , m_block_time(block_time)
    , m_block(nullptr)
    , m_pindex(nullptr)
    , m_stake_value_in(0)
    , m_total_claimed(0)
    , m_fees(0)
    , m_coin_age(0)
{
}

BlockRewardRules::BlockRewardRules(
    const CBlock& block,
    const CBlockIndex* pindex,
    CAmount stake_value_in,
    CAmount total_claimed,
    CAmount fees,
    uint64_t coin_age)
    : m_pindex_prev(pindex->pprev)
    , m_block_version(block.nVersion)
    , m_block_time(block.nTime)
    , m_block(&block)
    , m_pindex(pindex)
    , m_stake_value_in(stake_value_in)
    , m_total_claimed(total_claimed)
    , m_fees(fees)
    , m_coin_age(coin_age)
{
}

// -----------------------------------------------------------------------------
// Shared invariant computation
// -----------------------------------------------------------------------------

std::vector<BlockRewardRules::MandatorySidestakeSpec>
BlockRewardRules::ComputeEligibleMandatorySidestakes(
    const CTxDestination& coinstake_dest,
    CAmount total_owed_to_staker) const
{
    // No mandatory sidestakes before block version 13.
    if (m_block_version < 13) {
        return {};
    }

    std::vector<SideStake_ptr> active = GetSideStakeRegistry()
        .ActiveSideStakeEntries(SideStake::FilterFlag::MANDATORY, false);

    return ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed_to_staker, active);
}

std::vector<BlockRewardRules::MandatorySidestakeSpec>
BlockRewardRules::ComputeEligibleMandatorySidestakes(
    const CTxDestination& coinstake_dest,
    CAmount total_owed_to_staker,
    const std::vector<SideStake_ptr>& active_sidestakes) const
{
    // No mandatory sidestakes before block version 13.
    if (m_block_version < 13) {
        return {};
    }

    std::vector<MandatorySidestakeSpec> specs;
    specs.reserve(active_sidestakes.size());

    for (const auto& ss : active_sidestakes) {
        MandatorySidestakeSpec spec;
        spec.dest = ss->GetDestination();
        spec.alloc = ss->GetAllocation();
        spec.required_amount = (spec.alloc * total_owed_to_staker).ToCAmount();

        // Dust elimination: suppress outputs below 1 CENT.
        // This matches the dust check in the miner's allocate_sidestakes lambda
        // and the validator's pre-filter loop. Written ONCE here.
        if (spec.required_amount < CENT) {
            spec.suppressed_reason = "dust (below CENT)";
        }
        // Coinstake-address filtering: suppress sidestakes to the staker's own
        // address. The miner skips these (returns funds via coinstake split
        // outputs instead). The validator must match this decision.
        else if (spec.dest == coinstake_dest) {
            spec.suppressed_reason = "matches coinstake destination";
        }

        specs.push_back(std::move(spec));
    }

    return specs;
}

std::vector<BlockRewardRules::MandatorySidestakeSpec>
BlockRewardRules::FilterEligible(
    const std::vector<MandatorySidestakeSpec>& specs)
{
    std::vector<MandatorySidestakeSpec> result;
    result.reserve(specs.size());

    for (const auto& s : specs) {
        if (!s.IsSuppressed()) {
            result.push_back(s);
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
// Construct mode (miner)
// -----------------------------------------------------------------------------

bool BlockRewardRules::ConstructMandatorySidestakeOutputs(
    CMutableTransaction& mtx,
    const CTxDestination& coinstake_dest,
    CAmount total_owed_to_staker,
    CAmount& allocated_out) const
{
    auto eligible = FilterEligible(
        ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed_to_staker));

    unsigned int output_limit = GetMandatorySideStakeOutputLimit(m_block_version);

    // Shuffle when over the limit — non-deterministic selection.
    // The validator cannot reproduce the shuffle and doesn't need to;
    // it matches against the eligible set regardless of order.
    if (eligible.size() > output_limit) {
        Shuffle(eligible.begin(), eligible.end(), FastRandomContext());
    }

    allocated_out = 0;
    unsigned int count = 0;

    for (const auto& spec : eligible) {
        if (count >= output_limit) break;

        // Amount comes from the spec — not recomputed here.
        CScript script;
        script.SetDestination(spec.dest);
        mtx.vout.push_back(CTxOut(spec.required_amount, script));
        allocated_out += spec.required_amount;
        ++count;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Validate mode (validator)
// -----------------------------------------------------------------------------

bool BlockRewardRules::ValidateMandatorySidestakeOutputs(
    const CTransaction& coinstake,
    const CTxDestination& coinstake_dest,
    CAmount total_owed_to_staker,
    unsigned int mrc_start_index,
    std::string& error_out,
    const std::vector<SideStake_ptr>& active_sidestakes) const
{
    auto eligible = FilterEligible(
        ComputeEligibleMandatorySidestakes(coinstake_dest, total_owed_to_staker, active_sidestakes));

    unsigned int output_limit = GetMandatorySideStakeOutputLimit(m_block_version);
    unsigned int expected = std::min<unsigned int>(output_limit, eligible.size());

    // Track which spec entries have been matched. Each spec entry can match
    // at most one output — prevents a crafted block from satisfying the count
    // by duplicating one sidestake output and omitting another.
    std::vector<bool> matched(eligible.size(), false);
    unsigned int validated = 0;

    // Skip the empty output at index 0, stop before MRC outputs.
    for (unsigned int i = 1; i < mrc_start_index; ++i) {
        CTxDestination output_dest;
        if (!ExtractDestination(coinstake.vout[i].scriptPubKey, output_dest)) {
            error_out = strprintf("Coinstake vout[%u] has invalid destination", i);
            return false;
        }

        // Skip outputs to the coinstake destination — these are stake split
        // outputs, not mandatory sidestakes.
        if (output_dest == coinstake_dest) {
            continue;
        }

        // Scan eligible specs for an unmatched entry matching this output by
        // destination and amount >= required_amount.
        //
        // Why >= instead of ==:
        //
        // The >= provides tolerance for the following edge case:
        //
        // Remainder true-up: When mandatory allocations sum to exactly 100%
        // (theoretically possible if MaxMandatorySideStakeTotalAlloc is
        // raised from the current 25%), the miner assigns the last mandatory
        // output the remainder (nRemainingStakeOutputValue - nInputValue)
        // rather than spec.required_amount, which may be a few Halfords
        // larger due to integer rounding. An exact == match would reject
        // these blocks, creating a consensus-breaking divergence.
        //
        // Why >= does not cause overcounting or false validation:
        //
        // The matched[] array ensures each spec matches at most one output,
        // so validated is bounded by expected (= min(output_limit, eligible.size())). Overcounting
        // is impossible. However, >= can cause a "match steal": if a voluntary
        // sidestake to the same mandatory address appears before the actual
        // mandatory output in vout, and its amount exceeds required_amount,
        // the voluntary output claims the spec slot. The actual mandatory
        // output then goes unmatched. This is harmless because:
        //   - validated still equals expected (the spec was matched).
        //   - The mandatory address received at least the required amount
        //     (the "stealing" output paid MORE than required).
        //   - The actual mandatory output also exists, so the address was
        //     paid twice — once by mandatory, once by voluntary.
        //
        // In the coincidence case where mandatory and voluntary have identical
        // allocations (same computed amount), whichever output appears first
        // claims the spec. The second is unmatched. validated == expected,
        // so validation passes regardless of output ordering.
        for (unsigned int j = 0; j < eligible.size(); ++j) {
            if (matched[j]) continue;
            if (eligible[j].dest != output_dest) continue;

            if (coinstake.vout[i].nValue >= eligible[j].required_amount) {
                matched[j] = true;
                ++validated;
                break;
            }
        }

        // Overflow check — should not happen, but be thorough.
        if (validated > output_limit) {
            error_out = strprintf(
                "Number of mandatory sidestakes (%u) exceeds protocol limit (%u)",
                validated, output_limit);
            return false;
        }
    }

    // See the comments in SplitCoinStakeOutput regarding dust elimination in mandatory sidestake selection. Note
    // that in the miner for mandatory sidestakes, the shuffle is done AFTER the dust elimination, if the number of
    // residual elements is greater than the maximum allowed number of mandatory sidestakes. This leads to the
    // following check.
    //
    // If the residual number of mandatory sidestakes after dust elimination is GREATER than or equal to the output
    // limit, then the number of outputs matched to mandatory sidestakes should be equal to the output limit, because
    // the shuffle in combination with the allocation lambda operating on non-dust outputs will result in exactly
    // output_limit mandatory sidestakes, which means it will pass above, and also pass the check below. We do not
    // have to worry about a cutoff above MaxMandatorySideStakeTotalAlloc because that is handled IN
    // ActiveSideStakeEntries, which is used as the starting point in the miner (and of course here).
    //
    // If the residual number of mandatory sidestakes after dust elimination is less than the output limit, it should
    // be equal in number to the eligible set size from above after the elimination of dust outputs and coinstake
    // destination matches.
    //
    // The combination of these constraints means that the number of validated mandatory sidestakes MUST match
    // the minimum of the output limit and the eligible set size.
    if (validated < expected) {
        error_out = strprintf(
            "Number of validated mandatory sidestakes (%u) is less than required (%u)",
            validated, expected);
        return false;
    }

    return true;
}

bool BlockRewardRules::ValidateMandatorySidestakeOutputs(
    const CTransaction& coinstake,
    const CTxDestination& coinstake_dest,
    CAmount total_owed_to_staker,
    unsigned int mrc_start_index,
    std::string& error_out) const
{
    auto active = GetSideStakeRegistry().ActiveSideStakeEntries(SideStake::FilterFlag::MANDATORY, false);
    return ValidateMandatorySidestakeOutputs(
        coinstake, coinstake_dest, total_owed_to_staker, mrc_start_index, error_out, active);
}

// =============================================================================
// Full claim validation
// =============================================================================

bool BlockRewardRules::Check(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (!m_block) {
        error_out = "BlockRewardRules::Check() requires a block (constructed with full validation state)";
        return false;
    }

    const Claim& claim = m_block->GetClaim();

    return claim.HasResearchReward()
        ? CheckResearcherClaim(error_out)
        : CheckNoncruncherClaim(error_out);
}

// -----------------------------------------------------------------------------
// CheckNoncruncherClaim
// -----------------------------------------------------------------------------

bool BlockRewardRules::CheckNoncruncherClaim(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    CAmount mrc_rewards = 0;
    CAmount mrc_staker_fees = 0;
    CAmount mrc_fees = 0;
    CAmount out_stake_owed;
    unsigned int mrc_non_zero_outputs = 0;

    // Even if the block is staked by a non-cruncher, the claim can include MRC payments to researchers.
    //
    // If block version 12 or higher, this checks the MRC part of the claim, and also returns the total mrc_fees,
    // which are needed because they are part of the total claimed. Note that the DoS and log output for MRC
    // validation failure is handled by the caller.
    if (m_block_version >= 12
        && !CheckMRCRewards(mrc_rewards, mrc_staker_fees, mrc_fees,
                            mrc_non_zero_outputs, error_out))
    {
        return false;
    }

    if (CheckReward(0, out_stake_owed, mrc_staker_fees, mrc_fees,
                    mrc_rewards, mrc_non_zero_outputs, error_out))
    {
        return true;
    }

    if (GetBadBlocks().count(m_pindex->GetBlockHash())) {
        return true;
    }

    // error_out already set by CheckReward.
    return false;
}

// -----------------------------------------------------------------------------
// CheckResearcherClaim
// -----------------------------------------------------------------------------

bool BlockRewardRules::CheckResearcherClaim(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // For version 11 blocks and higher, just validate the reward and check the signature. No need for the rest
    // of these shenanigans.
    if (m_block_version >= 11) {
        return CheckResearchReward(error_out) && CheckBeaconSignature(error_out);
    }

    if (!CheckResearchRewardLimit(error_out)) return false;
    if (!CheckResearchRewardDrift(error_out)) return false;
    if (m_block_version <= 8) return true;
    if (!CheckClaimMagnitude(error_out)) return false;
    if (!CheckBeaconSignature(error_out)) return false;
    if (!CheckResearchReward(error_out)) return false;

    return true;
}

// -----------------------------------------------------------------------------
// CheckReward — core envelope and structural validation
// -----------------------------------------------------------------------------

bool BlockRewardRules::CheckReward(
    CAmount research_owed,
    CAmount& out_stake_owed,
    CAmount mrc_staker_fees_owed,
    CAmount mrc_fees,
    CAmount mrc_rewards,
    unsigned int mrc_non_zero_outputs,
    std::string& error_out) const
{
    out_stake_owed = GetProofOfStakeReward(m_coin_age, m_block_time, m_pindex);

    if (m_block_version >= 11) {
        // For block version 11, mrc_fees_owed and mrc_rewards are both zero, and there are no MRC outputs, so this
        // is the only check necessary. For v12+, the MRC components are non-zero and additional checks follow below.
        if (m_total_claimed > research_owed + out_stake_owed + m_fees + mrc_fees + mrc_rewards) {
            error_out = strprintf(
                "Claim too high: total_claimed %s > %s = research %s + stake %s + fees %s + mrc_fees %s + mrc_rewards %s",
                FormatMoney(m_total_claimed),
                FormatMoney(research_owed + out_stake_owed + m_fees + mrc_fees + mrc_rewards),
                FormatMoney(research_owed),
                FormatMoney(out_stake_owed),
                FormatMoney(m_fees),
                FormatMoney(mrc_fees),
                FormatMoney(mrc_rewards));
            return false;
        }

        if (m_block_version >= 12) {
            // Check that the portion of the coinstake going to the staker and/or their sidestakes (i.e. the non-MRC
            // part) is proper. The net of the non-MRC outputs and the input cannot exceed
            // research_owed + out_stake_owed + m_fees + staker_fees_owed.
            const CTransaction& coinstake = m_block->vtx[1];
            const Claim& claim = m_block->GetClaim();

            // Note this uses claim.m_mrc_tx_map.size() and not mrc_non_zero_outputs, because if mrcs are forced
            // in the zero payout interval and the foundation side stake is active, there will be a foundation mrc
            // sidestake even though there will not be a corresponding mrc rewards output. (Zero value outputs are
            // suppressed because that is wasteful.)
            bool foundation_mrc_sidestake_present =
                (claim.m_mrc_tx_map.size()
                 && FoundationSideStakeAllocation().IsNonZero()) ? true : false;

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
                error_out = strprintf(
                    "Total owed to staker too high: %s > %s = research %s + stake %s + fees %s + mrc_staker_fees %s",
                    FormatMoney(total_owed_to_staker),
                    FormatMoney(research_owed + out_stake_owed + m_fees + mrc_staker_fees_owed),
                    FormatMoney(research_owed),
                    FormatMoney(out_stake_owed),
                    FormatMoney(m_fees),
                    FormatMoney(mrc_staker_fees_owed));
                return false;
            }

            // v13+ mandatory sidestake validation.
            if (m_block_version >= 13) {
                CTxDestination coinstake_dest;
                if (!ExtractDestination(coinstake.vout[1].scriptPubKey, coinstake_dest)) {
                    error_out = "Coinstake destination is invalid";
                    return false;
                }

                std::string ss_error;
                if (!ValidateMandatorySidestakeOutputs(
                        coinstake, coinstake_dest, total_owed_to_staker,
                        mrc_start_index, ss_error))
                {
                    error_out = ss_error;
                    return false;
                }
            }

            // If the foundation mrc sidestake is present, we check the foundation sidestake specifically. The MRC
            // outputs were already checked by CheckMRCRewards.
            if (foundation_mrc_sidestake_present) {
                // The fee amount to the foundation must be correct.
                if (coinstake.vout[mrc_start_index].nValue != mrc_fees - mrc_staker_fees_owed) {
                    error_out = strprintf(
                        "MRC Foundation sidestake amount incorrect: %s != mrc_fees %s - staker_fees %s",
                        FormatMoney(coinstake.vout[mrc_start_index].nValue),
                        FormatMoney(mrc_fees),
                        FormatMoney(mrc_staker_fees_owed));
                    return false;
                }

                CTxDestination foundation_dest;
                // The foundation sidestake destination must be able to be extracted.
                if (!ExtractDestination(coinstake.vout[mrc_start_index].scriptPubKey,
                                        foundation_dest))
                {
                    error_out = "MRC Foundation sidestake destination is invalid";
                    return false;
                }

                // The sidestake destination must match that specified by FoundationSideStakeAddress().
                if (foundation_dest != FoundationSideStakeAddress()) {
                    error_out = "MRC Foundation sidestake destination does not match protocol";
                    return false;
                }
            }
        } // v12+

        // If we get here, we are done with v11, v12, and v13 validation so return true.
        return true;
    } // v11+

    // Blocks version 10 and below represented rewards as floating-point values and needed to accommodate
    // floating-point errors so we'll do the same rounding on the floating-point representations:
    double subsidy = ((double)research_owed / COIN) * 1.25;
    subsidy += (double)out_stake_owed / COIN;

    CAmount max_owed = roundint64(subsidy * COIN) + m_fees;

    // Block version 9 and below allowed a 1 GRC wiggle.
    if (m_block_version <= 9) {
        max_owed += 1 * COIN;
    }

    if (m_total_claimed > max_owed) {
        error_out = strprintf("Claim %s exceeds max %s (legacy v%d)",
            FormatMoney(m_total_claimed), FormatMoney(max_owed), m_block_version);
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// CheckMRCRewards — validate MRC outputs in coinstake
// -----------------------------------------------------------------------------

bool BlockRewardRules::CheckMRCRewards(
    CAmount& mrc_rewards,
    CAmount& mrc_staker_fees,
    CAmount& mrc_fees,
    unsigned int& non_zero_outputs,
    std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const CTransaction& coinstake = m_block->vtx[1];
    const Claim& claim = m_block->GetClaim();

    unsigned int mrc_outputs = 0;
    unsigned int mrc_output_limit = GetMRCOutputLimit(m_block_version, false);
    unsigned int mrc_claimed_outputs = claim.m_mrc_tx_map.size();

    if (mrc_claimed_outputs > mrc_output_limit) {
        error_out = strprintf("MRC claimed outputs %u exceeds limit %u",
            mrc_claimed_outputs, mrc_output_limit);
        return false;
    }

    if (mrc_output_limit > 0) {
        Fraction foundation_fee_fraction = FoundationSideStakeAllocation();

        for (const auto& tx : m_block->vtx) {
            for (const auto& mrc_entry : claim.m_mrc_tx_map) {
                if (mrc_entry.second == tx.GetHash()) {
                    for (const auto& contract : tx.GetContracts()) {
                        if (contract.m_type != ContractType::MRC) continue;

                        MRC mrc = contract.CopyPayloadAs<MRC>();

                        if (const CpidOption cpid = mrc.m_mining_id.TryCpid()) {
                            const auto mrc_it = mapBlockIndex.find(mrc.m_last_block_hash);
                            if (mrc_it == mapBlockIndex.end() || mrc_it->second == nullptr) {
                                error_out = "MRC refers to unknown last-block hash";
                                return false;
                            }

                            CBlockIndex* mrc_index = mrc_it->second;

                            const BeaconOption beacon = GetBeaconRegistry().TryActive(
                                *cpid, mrc_index->nTime);

                            if (beacon) {
                                if (!ValidateMRC(m_pindex->pprev, mrc)) {
                                    error_out = "An MRC in the claim failed to validate";
                                    return false;
                                }

                                CAmount mrc_reward = mrc.m_research_subsidy - mrc.m_fee;

                                CScript mrc_beacon_script;
                                mrc_beacon_script.SetDestination(beacon->GetAddress());

                                CAmount coinstake_mrc_reward = 0;

                                if (mrc_reward) {
                                    for (unsigned int i = coinstake.vout.size() - mrc_claimed_outputs;
                                         i < coinstake.vout.size(); ++i)
                                    {
                                        if (mrc_beacon_script == coinstake.vout[i].scriptPubKey) {
                                            coinstake_mrc_reward += coinstake.vout[i].nValue;
                                            ++non_zero_outputs;
                                        }
                                    }
                                }

                                if (coinstake_mrc_reward != mrc_reward) {
                                    error_out = strprintf(
                                        "MRC reward %s != coinstake output %s",
                                        FormatMoney(mrc_reward),
                                        FormatMoney(coinstake_mrc_reward));
                                    return false;
                                }

                                mrc_rewards += mrc_reward;
                                mrc_fees += mrc.m_fee;
                                mrc_staker_fees += mrc.m_fee
                                    - (static_cast<Allocation>(foundation_fee_fraction) * mrc.m_fee).ToCAmount();

                                ++mrc_outputs;
                            } // beacon
                        } // cpid
                    } // contracts

                    break;
                } // tx hash match
            } // mrc_tx_map
        } // block txns
    } // output_limit > 0

    if (mrc_outputs < mrc_claimed_outputs) {
        error_out = strprintf("Validated MRC claims %u < claimed %u",
            mrc_outputs, mrc_claimed_outputs);
        return false;
    }

    // NOTE: We do NOT signal uiInterface.MRCChanged() — shadow validation
    // must not trigger UI side effects.

    return true;
}

// -----------------------------------------------------------------------------
// CheckResearchReward — accrual lookup with newbie correction fallback
// -----------------------------------------------------------------------------

bool BlockRewardRules::CheckResearchReward(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    const Claim& claim = m_block->GetClaim();

    CAmount research_owed = 0;
    CAmount mrc_rewards = 0;
    CAmount mrc_staker_fees = 0;
    CAmount mrc_fees = 0;
    unsigned int mrc_non_zero_outputs = 0;

    const CpidOption cpid = claim.m_mining_id.TryCpid();

    if (cpid) {
        research_owed = Tally::GetAccrual(*cpid, m_block_time, m_pindex);
    }

    // If block version 12 or higher, this checks the MRC part of the claim, and also returns the staker_fees,
    // which are needed because they are added to the staker's payout. Note that the DoS and log output for MRC
    // validation failure is handled by the caller.
    if (m_block_version >= 12
        && !CheckMRCRewards(mrc_rewards, mrc_staker_fees, mrc_fees,
                            mrc_non_zero_outputs, error_out))
    {
        return false;
    }

    CAmount out_stake_owed;
    if (CheckReward(research_owed, out_stake_owed, mrc_staker_fees, mrc_fees,
                    mrc_rewards, mrc_non_zero_outputs, error_out))
    {
        return true;
    }

    // The below is required to deal with a conditional application in historical rewards for research newbies
    // after the original newbie fix height that already made it into the chain. Please see the extensive
    // commentary in GetNewbieSuperblockAccrualCorrection().
    if (m_pindex->nHeight >= GetOrigNewbieSnapshotFixHeight() && cpid) {
        CAmount newbie_correction = Tally::GetNewbieSuperblockAccrualCorrection(
            *cpid, Quorum::CurrentSuperblock());

        research_owed += newbie_correction;

        if (CheckReward(research_owed, out_stake_owed, mrc_staker_fees, mrc_fees,
                        mrc_rewards, mrc_non_zero_outputs, error_out))
        {
            return true;
        }
    }

    // Testnet v9 exception: some blocks had bad interest claims masked by
    // short 10-block-span pending accrual.
    if (fTestNet && m_block_version <= 9) {
        std::string dummy;
        if (!CheckReward(0, out_stake_owed, 0, 0, 0, 0, dummy)) {
            return true;
        }
    }

    // Bad block exception.
    if (GetBadBlocks().count(m_pindex->GetBlockHash())) {
        return true;
    }

    // error_out already set by CheckReward.
    return false;
}

// -----------------------------------------------------------------------------
// CheckBeaconSignature — verify claim is signed by active beacon key
// -----------------------------------------------------------------------------

bool BlockRewardRules::CheckBeaconSignature(std::string& error_out) const
{
    const Claim& claim = m_block->GetClaim();
    const CpidOption cpid = claim.m_mining_id.TryCpid();

    if (!cpid) {
        error_out = "Non-researcher claim has no beacon signature";
        return false;
    }

    // The legacy beacon functions determined beacon expiration by the time of the previous block. For block
    // version 11+, compute the expiration threshold from the current block:
    const int64_t now = m_block_version >= 11 ? m_block_time : m_pindex->pprev->nTime;

    if (const BeaconOption beacon = GetBeaconRegistry().TryActive(*cpid, now)) {
        if (claim.VerifySignature(
            beacon->m_public_key,
            m_pindex->pprev->GetBlockHash(),
            m_block->vtx[1]))
        {
            return true;
        }
    }

    // Bad block exception.
    if (GetBadBlocks().count(m_pindex->GetBlockHash())) {
        return true;
    }

    // An old bug caused some nodes to sign research reward claims with a previous beacon key (beaconalt).
    // Mainnet declares block exceptions for this problem. To avoid declaring exceptions for the 55 testnet
    // blocks, the following check ignores beaconalt verification failure for the range of heights that include
    // these blocks:
    if (fTestNet
        && (m_pindex->nHeight >= 495352 && m_pindex->nHeight <= 600876))
    {
        return true;
    }

    error_out = strprintf("Beacon signature verification failed for CPID %s at height %d",
        claim.m_mining_id.ToString(), m_pindex->nHeight);
    return false;
}

// -----------------------------------------------------------------------------
// Legacy v9-v10 checks
// -----------------------------------------------------------------------------

bool BlockRewardRules::CheckResearchRewardLimit(std::string& error_out) const
{
    const Claim& claim = m_block->GetClaim();
    const CAmount max_reward = 12750 * COIN;

    if (claim.m_research_subsidy > max_reward) {
        error_out = strprintf("Research claim %s exceeds max %s for CPID %s",
            FormatMoney(claim.m_research_subsidy),
            FormatMoney(max_reward),
            claim.m_mining_id.ToString());
        return false;
    }

    return true;
}

bool BlockRewardRules::CheckResearchRewardDrift(std::string& error_out) const
{
    const Claim& claim = m_block->GetClaim();

    // ResearchAge: Since the best block may increment before the RA is connected but after the RA is computed,
    // the ResearchSubsidy can sometimes be slightly smaller than we calculate here due to the RA timespan
    // increasing. So we will allow for time shift before rejecting the block.
    const CAmount reward_claimed = m_total_claimed - m_fees;
    CAmount drift_allowed = claim.m_research_subsidy * 0.15;

    if (drift_allowed < 10 * COIN) {
        drift_allowed = 10 * COIN;
    }

    if (claim.TotalSubsidy() + drift_allowed < reward_claimed) {
        error_out = strprintf("Reward claim %s exceeds allowed %s for CPID %s",
            FormatMoney(reward_claimed),
            FormatMoney(claim.TotalSubsidy() + drift_allowed),
            claim.m_mining_id.ToString());
        return false;
    }

    return true;
}

bool BlockRewardRules::CheckClaimMagnitude(std::string& error_out) const
{
    const Claim& claim = m_block->GetClaim();
    const double mag = Quorum::GetMagnitude(claim.m_mining_id).Floating();

    if (claim.m_magnitude > mag * 1.25) {
        error_out = strprintf("Magnitude claim %f exceeds superblock %f for CPID %s",
            claim.m_magnitude, mag, claim.m_mining_id.ToString());
        return false;
    }

    return true;
}
