// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CONSENSUS_BLOCK_REWARDS_H
#define GRIDCOIN_CONSENSUS_BLOCK_REWARDS_H

#include "amount.h"
#include "primitives/transaction.h"
#include "gridcoin/sidestake.h"
#include "script.h"
#include "sync.h"

#include <string>
#include <vector>

class CBlock;
class CBlockIndex;
class CTransaction;
extern CCriticalSection cs_main;

namespace GRC {

class Claim;
class Cpid;

//! Unified consensus rules for block reward construction and validation.
//!
//! This class provides a single source of truth for the consensus rules that
//! govern coinstake output construction (miner) and verification (validator).
//! Both modes consume the same shared spec computations, eliminating the class
//! of bugs where miner and validator implementations drift apart (see #2848).
//!
//! The class operates in two modes:
//!   - **Construct** — used by the miner to build coinstake outputs.
//!     Methods take CMutableTransaction& and mutate it.
//!   - **Validate** — used by the validator to verify coinstake outputs.
//!     Methods take const CTransaction& and read from it.
//!
//! Both modes share the same invariant computation (the "spec") which is the
//! single point where eligible sets, amounts, and suppression decisions are
//! determined. Neither the construct nor validate path independently re-queries
//! registries or recomputes amounts — they consume the spec.
//!
//! \sa #2880 (consensus rules unification issue)
//! \sa #2877 (PSGT plan — CMutableTransaction split)
//!
class BlockRewardRules
{
public:
    //! Construct/spec mode (miner) — lightweight, no block context needed.
    BlockRewardRules(
        const CBlockIndex* pindex_prev,
        int block_version,
        int64_t block_time);

    //! Full validation mode. Enables Check() for complete claim validation.
    BlockRewardRules(
        const CBlock& block,
        const CBlockIndex* pindex,
        CAmount stake_value_in,
        CAmount total_claimed,
        CAmount fees,
        uint64_t coin_age);

    // --- Full claim validation ------------------------------------------------

    //! Run the complete claim validation suite: research/non-research
    //! branching, reward envelope, MRC outputs, beacon signature, mandatory
    //! sidestakes, and all legacy version-specific checks.
    //!
    //! Returns true if the claim is valid. On failure, error_out contains a
    //! descriptive error message. This method does NOT call CBlock::DoS() —
    //! the caller decides on DoS scoring.
    //!
    //! Requires the full validation constructor.
    //!
    bool Check(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    // --- Shared spec types ---------------------------------------------------

    //! A single eligible mandatory sidestake with pre-computed required amount.
    //!
    //! Rounding semantics: Allocation inherits from Fraction, which is quantized
    //! to 1/10000 increments and simplified at construction. ToCAmount() uses
    //! integer division (truncation toward zero for positive values). The dust
    //! threshold comparison (required_amount < CENT) is applied after truncation,
    //! matching existing consensus behavior.
    struct MandatorySidestakeSpec
    {
        CTxDestination dest;
        Allocation alloc;
        CAmount required_amount;         //!< (alloc * total_owed_to_staker).ToCAmount()

        //! Non-empty if this entry was filtered out (dust, address match).
        //! For debug/test visibility only — MUST NOT affect consensus behavior.
        //! Only used for logging and assertions.
        std::string suppressed_reason;

        bool IsSuppressed() const { return !suppressed_reason.empty(); }
    };

    // --- Shared invariant computations (specs) -------------------------------

    //! Compute the eligible mandatory sidestakes after dust elimination and
    //! coinstake-address filtering. Returns a fully-resolved spec — the single
    //! source of truth for which sidestakes apply and what their required
    //! amounts are.
    //!
    //! Both Construct and Validate consume this spec rather than independently
    //! re-querying the registry. If the eligible set computation or amount
    //! calculation changes, it changes once, and both modes inherit it.
    //!
    //! The returned vector includes ALL mandatory sidestakes (including
    //! suppressed ones) so that filtering decisions are visible to tests and
    //! debug logging. Use FilterEligible() to get only non-suppressed entries.
    //!
    //! \param coinstake_dest  The destination of the coinstake output (vout[1]).
    //! \param total_owed_to_staker  Net value of non-MRC outputs minus input.
    //!
    std::vector<MandatorySidestakeSpec> ComputeEligibleMandatorySidestakes(
        const CTxDestination& coinstake_dest,
        CAmount total_owed_to_staker) const;

    //! Overload accepting an explicit list of active mandatory sidestakes
    //! instead of querying the global registry. This enables direct unit
    //! testing of the spec computation with controlled inputs.
    //!
    std::vector<MandatorySidestakeSpec> ComputeEligibleMandatorySidestakes(
        const CTxDestination& coinstake_dest,
        CAmount total_owed_to_staker,
        const std::vector<SideStake_ptr>& active_sidestakes) const;

    //! Filter a spec vector to only non-suppressed (eligible) entries.
    static std::vector<MandatorySidestakeSpec> FilterEligible(
        const std::vector<MandatorySidestakeSpec>& specs);

    // --- Construct-mode methods (miner) --------------------------------------

    //! Select and create mandatory sidestake outputs on the coinstake.
    //!
    //! Calls ComputeEligibleMandatorySidestakes() to get the spec, then
    //! shuffles if over the output limit (non-deterministic — construct only).
    //! Appends outputs to mtx.vout.
    //!
    //! \param[in,out] mtx              The mutable coinstake transaction.
    //! \param[in]     coinstake_dest   Destination of the coinstake (vout[1]).
    //! \param[in]     total_owed_to_staker  Net staker reward for allocation.
    //! \param[out]    allocated_out    Total amount allocated to sidestakes.
    //! \return true on success.
    //!
    bool ConstructMandatorySidestakeOutputs(
        CMutableTransaction& mtx,
        const CTxDestination& coinstake_dest,
        CAmount total_owed_to_staker,
        CAmount& allocated_out) const;

    // --- Validate-mode methods (validator) ------------------------------------

    //! Verify mandatory sidestake outputs exist with correct amounts.
    //!
    //! Calls ComputeEligibleMandatorySidestakes() to get the spec, then
    //! enumerates actual coinstake outputs and matches against the eligible
    //! set using matched-flags to prevent double-counting.
    //!
    //! \param[in]  coinstake           The (immutable) coinstake transaction.
    //! \param[in]  coinstake_dest      Destination of the coinstake (vout[1]).
    //! \param[in]  total_owed_to_staker  Net staker reward for allocation.
    //! \param[in]  mrc_start_index     Index where MRC outputs begin.
    //! \param[out] error_out           Descriptive error on failure.
    //! \return true if all required mandatory sidestakes are present and valid.
    //!
    bool ValidateMandatorySidestakeOutputs(
        const CTransaction& coinstake,
        const CTxDestination& coinstake_dest,
        CAmount total_owed_to_staker,
        unsigned int mrc_start_index,
        std::string& error_out) const;

    //! Overload accepting an explicit list of active mandatory sidestakes
    //! instead of querying the global registry. Enables direct unit testing
    //! of the validation logic with controlled inputs.
    //!
    bool ValidateMandatorySidestakeOutputs(
        const CTransaction& coinstake,
        const CTxDestination& coinstake_dest,
        CAmount total_owed_to_staker,
        unsigned int mrc_start_index,
        std::string& error_out,
        const std::vector<SideStake_ptr>& active_sidestakes) const;

private:
    const CBlockIndex* m_pindex_prev;
    int m_block_version;
    int64_t m_block_time;

    // Full validation state (set by full validation constructor; null/zero otherwise).
    const CBlock* m_block;
    const CBlockIndex* m_pindex;
    CAmount m_stake_value_in;
    CAmount m_total_claimed;
    CAmount m_fees;
    uint64_t m_coin_age;

    // --- Full validation sub-checks ------------------------------------------

    bool CheckResearcherClaim(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    bool CheckNoncruncherClaim(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool CheckReward(CAmount research_owed, CAmount& out_stake_owed,
                     CAmount mrc_staker_fees_owed, CAmount mrc_fees,
                     CAmount mrc_rewards, unsigned int mrc_non_zero_outputs,
                     std::string& error_out) const;

    bool CheckMRCRewards(CAmount& mrc_rewards, CAmount& mrc_staker_fees,
                         CAmount& mrc_fees, unsigned int& non_zero_outputs,
                         std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool CheckResearchReward(std::string& error_out) const EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    bool CheckBeaconSignature(std::string& error_out) const;

    // Legacy v9-v10 checks
    bool CheckResearchRewardLimit(std::string& error_out) const;
    bool CheckResearchRewardDrift(std::string& error_out) const;
    bool CheckClaimMagnitude(std::string& error_out) const;
};

} // namespace GRC

#endif // GRIDCOIN_CONSENSUS_BLOCK_REWARDS_H
