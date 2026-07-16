// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_MRC_H
#define GRIDCOIN_INTERFACES_MRC_H

#include "amount.h"

#include <functional>
#include <memory>
#include <string>

class CWallet;

namespace interfaces {

class Handler;

//! One atomic, node-side read of everything the MRC request page needs
//! (Phase 1d-i). Produced under a single cs_main hold so every field is
//! consistent with one chain tip — replacing MRCModel's former GUI-thread
//! reads of pindexBest / mempool / CreateMRC, which were annotated
//! EXCLUSIVE_LOCKS_REQUIRED(cs_main) but ran holding nothing (the annotation is
//! invisible across the Qt slot boundary) and raced. All fields are pointer-free
//! value types; the GUI classifies them into display status without touching any
//! core state.
struct MRCSnapshot
{
    //! Node is not yet synced (OutOfSyncByAge()); the rest of the snapshot below
    //! is unpopulated and the request page should show the out-of-sync state.
    bool out_of_sync = false;

    //! Best-chain height at the instant of the snapshot.
    int block_height = 0;

    //! The MRC block-version gate (IsV12Enabled(height)) is active. When false,
    //! MRC is not yet available at this height and the CreateMRC / queue fields
    //! are unpopulated.
    bool block_version_valid = false;

    //! Per-block MRC output cap (GetMRCOutputLimit), i.e. how many MRCs a block
    //! can pay; drives the "queue full / past pay limit" classification.
    int output_limit = 0;

    //! The minimum-fee trial CreateMRC (fee = 0 run) raised GRC::MRC_error —
    //! e.g. no positive research reward pending; \p create_error_msg carries its
    //! text. Mirrors MRCModel::refresh()'s first CreateMRC, so the GUI applies it
    //! before the zero-payout / excessive-fee checks (it may be overridden by
    //! them).
    bool create_error = false;
    std::string create_error_msg;

    //! The boosted trial CreateMRC (second run, only attempted when the caller
    //! passed a non-zero fee boost) raised GRC::MRC_error — e.g. the boosted fee
    //! exceeds the research reward. Mirrors refresh()'s rerun, so the GUI applies
    //! it after the excessive-fee check (overriding it, as the node's authority
    //! on fee validity).
    bool boosted_fee_error = false;
    std::string boosted_fee_error_msg;

    //! Trial CreateMRC results. \p min_fee is the required minimum; \p fee is the
    //! total the caller requested (min_fee + fee boost, or 0 when no boost) — the
    //! intended value, never the minimum CreateMRC may reset an out-of-range fee
    //! to. \p reward is the research reward the MRC would claim. min_fee == reward
    //! marks the zero-payout interval (too soon); fee > reward marks an excessive
    //! boost.
    CAmount reward = 0;
    CAmount min_fee = 0;
    CAmount fee = 0;

    //! Fee-ordered mempool MRC queue (mempool.GetMRCQueue()) evaluated against
    //! this request's fee. \p queue_length is the total pending MRC count; \p pos
    //! is this request's 0-based position among strictly-higher-fee entries; \p
    //! found_in_queue is true if this CPID's MRC is already pending. The fees
    //! bound the queue for the fee-boost UI: head (highest), tail (lowest), and
    //! pay-limit (the fee at output_limit-1, i.e. the last that would be paid).
    int queue_length = 0;
    int pos = 0;
    bool found_in_queue = false;
    CAmount queue_head_fee = 0;
    CAmount queue_tail_fee = 0;
    CAmount queue_pay_limit_fee = 0;
};

//! Result of an MRC submission (Phase 1d-i).
struct MRCSubmitResult
{
    //! The contract was created and broadcast.
    bool success = false;

    //! Failure text when \p success is false (empty otherwise).
    std::string error;

    //! Height at submission, and the submitted MRC's research subsidy — the GUI
    //! retains both to detect a later stale/unbound MRC (accrual >= subsidy after
    //! the block advanced) without any further core read.
    int submitted_height = 0;
    CAmount research_subsidy = 0;
};

//! Called when the node's pending-MRC set changes (uiInterface.MRCChanged).
using MRCChangedFn = std::function<void()>;

//! The Manual Research Claim boundary (Phase 1d-i). Gives the GUI an atomic
//! snapshot of MRC eligibility / queue state and a submit command, so no
//! GUI-side MRC code touches core state or carries cs_main annotations.
class MRC
{
public:
    virtual ~MRC() = default;

    //! Atomic node-side snapshot at the given fee boost. \p wallet_locked (the
    //! GUI's WalletModel encryption state) becomes CreateMRC's no_sign flag — a
    //! locked wallet cannot sign, so the trial claim is built unsigned. \p
    //! researcher_eligible is the GUI's active-beacon / not-noncruncher /
    //! not-pool determination (owned by ResearcherModel, migrated in 1d-iv); when
    //! false the snapshot skips the trial CreateMRC (which would otherwise throw
    //! MRC_error on every block for a non-researcher) and the queue scan, leaving
    //! those fields defaulted. Taken under one cs_main hold; see MRCSnapshot.
    virtual MRCSnapshot snapshot(CAmount fee_boost, bool wallet_locked,
                                 bool researcher_eligible) = 0;

    //! Create a fresh, signed MRC at the given fee boost and broadcast it, under
    //! cs_main + cs_wallet. Re-creating here (rather than resubmitting the
    //! snapshot's trial claim) keeps the whole submit atomic and avoids
    //! marshaling the contract payload. The caller must have unlocked the wallet
    //! first (\p wallet_locked false); a locked wallet yields an error result.
    virtual MRCSubmitResult submit(CAmount fee_boost, bool wallet_locked) = 0;

    //! Live out-of-sync (OutOfSyncByAge) reading. Separate from the snapshot's
    //! cached out_of_sync because this check is purely time-based
    //! (GetAdjustedTime() - g_previous_block_time) and can flip true with no new
    //! block or MRCChanged event: the GUI's status display queries it on demand
    //! so it never reports VALID while the node has aged out of sync.
    virtual bool isOutOfSync() = 0;

    //! Subscribe to the node's MRCChanged notification (the mempool MRC set
    //! changed) — the GUI re-snapshots on it.
    virtual std::unique_ptr<Handler> handleMRCChanged(MRCChangedFn fn) = 0;
};

//! Return an in-process MRC interface over the node's single wallet, which must
//! outlive the returned object.
std::unique_ptr<MRC> MakeMRC(CWallet* wallet);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_MRC_H
