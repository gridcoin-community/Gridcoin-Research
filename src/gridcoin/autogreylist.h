// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_AUTOGREYLIST_H
#define GRIDCOIN_AUTOGREYLIST_H

#include <gridcoin/autogreylist_v2.h>
#include <gridcoin/project.h>
#include <gridcoin/superblock.h>

#include <optional>
#include <set>

namespace GRC {

//!
//! \brief Identifies which walker implementation produced a computed greylist result.
//!
//! The version is a property of each computed result, derived from the anchor height that
//! produced it at write time. There is deliberately NO global "V2 mode" flag: a mode flag is a
//! fourth piece of state that can disagree with the data it describes, which is the defect
//! class the V1/V2 fork exists to remove.
//!
enum class GreylistVersion : uint8_t {
    V1, //!< The frozen pre-AutoGreylistRedesignHeight AutoGreylist class.
    V2, //!< The redesigned walker (pending/authoritative state separation).
};

//!
//! \brief An immutable computed greylist result that carries its own identity and validity.
//!
//! This is the unit the AutoGreylistService caches. Carrying the key and the producing version
//! on the result itself is what lets a consumer determine locally what it is reading -- the
//! property the previous single-cache design lacked.
//!
struct GreylistComputation
{
    //! \brief Which walker produced this result.
    GreylistVersion m_version;

    //! \brief Identity of the state this result was computed against.
    //!
    //! For the authoritative state this is the committed superblock's (quorum) hash, per
    //! DESIGN.md section 10.2; for the pending state it is the convergence content hash
    //! (ConvergedManifest::nContentHash). The key is an identity label for consumers and
    //! diagnostics -- FRESHNESS is guaranteed by the push-model write discipline (every
    //! superblock push/pop/load replaces the authoritative slot; every convergence install
    //! replaces the pending slot), never by a reader comparing keys, which would require the
    //! read path to reach into chain or convergence state. Null for V1-backed views: V1's
    //! single cache exposes no key, and below the redesign gate no consumer needs one.
    uint256 m_key;

    //! \brief True when this result was derived by reading the superblock's serialized
    //! m_project_status record rather than running the walker. A record-derived result
    //! carries membership only (no per-project ZCD/WAS detail).
    bool m_from_record;

    //! \brief Names of the projects that meet the auto greylist criteria.
    std::set<std::string> m_auto_greylisted;

    //! \brief Per-project candidate detail (ZCD, WAS, history). Empty when m_from_record --
    //! the serialized record carries membership only.
    std::map<std::string, GreylistCandidateV2> m_candidates;
};

//!
//! \brief Facade over the V1 and V2 auto greylist implementations.
//!
//! This class is the single owner of auto greylist state and the only type consumers see. It
//! separates the two dispatches that the previous design conflated (see
//! ~/GridcoinDev/autogreylist_state_separation/DESIGN.md section 10.7):
//!
//! - VERSION dispatch (V1 vs V2) belongs to the PRODUCERS at write time, decided from the
//!   anchor height of the refresh. The invariant that replaces a mode flag: a V1-producer
//!   write clears both V2 slots; a V2-producer write fills them; a read serves a filled V2
//!   slot and otherwise falls through to the untouched V1 instance. Below the redesign gate
//!   the V2 slots are therefore always empty and behavior is bit-identical to V1.
//!
//! - STATE dispatch (pending vs authoritative) belongs to the CONSUMERS at read time, via the
//!   required GreylistState selector. Below the gate both selectors resolve to V1's single
//!   cache and are degenerate by construction.
//!
//! Lock ordering: m_service_lock occupies the same slot as the V1 class's internal
//! autogreylist_lock -- a leaf under Whitelist's cs_lock. The canonical chain is
//! cs_lock -> m_service_lock -> autogreylist_lock (consumer reads hold m_service_lock across
//! the V1 fall-through, whose methods take only autogreylist_lock). Producer paths must NOT
//! hold m_service_lock while calling into the V1 refresh methods, because those re-enter
//! Whitelist::Snapshot and take cs_lock -- doing so would invert the chain. The producer and
//! reporting paths REQUIRE cs_main from their callers (they read the current superblock, the
//! chain tip and, where they walk, the chain) but never acquire it themselves; the consumer
//! read paths (Contains/IsDeepCopyActive/Get) touch no cs_main-guarded data at all -- version
//! and state are properties of the already-computed results, never re-derived from a height
//! on the read side.
//!
class AutoGreylistService
{
public:
    AutoGreylistService();

    // ---------- producers (version dispatch at write time; called from cs_main contexts) ----

    //!
    //! \brief Refresh against the current committed superblock (the authoritative anchor).
    //! Called from the chain handler points: Quorum::PushSuperblock, Quorum::PopSuperblock and
    //! Quorum::LoadSuperblockIndex.
    //!
    void Refresh();

    //!
    //! \brief Refresh against a specific superblock. Test seam; forwards to the walker
    //! selected by the anchor height.
    //!
    void RefreshWithSuperblock(SuperblockPtr superblock_ptr_in,
                               std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>>
                                   unit_test_blocks = nullptr);

    //!
    //! \brief Refresh against a candidate superblock built from a convergence (the pending
    //! anchor, pindexBest) and stamp the candidate's m_project_status.
    //!
    //! \param superblock The candidate superblock. Total credits must already be populated.
    //! \param convergence_id Content hash of the convergence the candidate was built from
    //! (ConvergedManifest::nContentHash). Keys the pending cache above the redesign gate.
    //! \param update_pending_cache True when the candidate is built from the convergence at
    //! the head (superblock construction and validation); false for re-derivations of PAST
    //! convergences (e.g. the convergencereport RPC), which must not clobber live pending
    //! state. Deliberately not defaulted: a new call site must decide.
    //!
    void RefreshWithAndUpdateSuperblock(Superblock& superblock, const uint256& convergence_id,
                                        bool update_pending_cache,
                                        std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>>
                                            unit_test_blocks = nullptr);

    //!
    //! \brief Re-stamp a candidate superblock's m_project_status anchored at the block being
    //! created (miner bind time). No-op below the redesign gate.
    //!
    //! This is the single pending-to-authoritative transition point (DESIGN.md 10.3), made
    //! mechanical: the cached superblock contract can be built at one tip and staked several
    //! blocks later, so a record stamped only at convergence time carries an anchor a
    //! validator cannot recover. Re-stamping at bind time gives producer, validator and any
    //! later recompute ONE anchor -- the containing block -- and, because the previous
    //! superblock is roughly a day old at bind time, the record is naturally free of the
    //! phantom-head shape. The quorum hash excludes m_project_status, so re-stamping cannot
    //! break contract matching against other nodes' convergences.
    //!
    //! \param superblock The candidate about to be bound into the block claim.
    //! \param anchor_height Height of the block being created (tip height + 1).
    //! \param anchor_time nTime of the block being created.
    //! \param walk_start Block index entry the backward walk starts from (the current tip).
    //!
    void StampProjectStatus(Superblock& superblock, int anchor_height, int64_t anchor_time,
                            CBlockIndex* walk_start,
                            std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>>
                                unit_test_blocks = nullptr);

    //!
    //! \brief Validate a received superblock's m_project_status record at acceptance.
    //!
    //! The field is serialized but deliberately excluded from the quorum hash, and nothing
    //! else compares it -- so without this check a staker could ship arbitrary record
    //! content that every node would consistently adopt once the record is read back as the
    //! authoritative state. The validator runs the SAME single walker the producer used,
    //! anchored at the received superblock's containing block, derives the expected record
    //! through the same DeriveProjectStatusRecord rule, and compares byte-for-byte.
    //! Deterministic across nodes: the inputs are the hashed candidate total credits, the
    //! committed chain behind the block, and the contract-driven registry state -- and the
    //! caller (TryLoadSuperblock) runs BEFORE ApplyContracts, so the validator sees the same
    //! parent-block registry the producer stamped against.
    //!
    //! \param superblock_ptr The received superblock, bound to its containing block.
    //! \param walk_start The containing block's pprev (reorg-safe walk start).
    //!
    //! \return true when the record matches (or the check does not apply: below the gate,
    //! or a pre-v3 superblock).
    //!
    bool ValidateProjectStatus(const SuperblockPtr& superblock_ptr, CBlockIndex* walk_start,
                               std::shared_ptr<std::map<int, std::pair<CBlockIndex*, SuperblockPtr>>>
                                   unit_test_blocks = nullptr) const;

    //!
    //! \brief Reset all cached state (both V2 slots and the V1 instance).
    //!
    void Reset();

    // ---------- consumers (leaf locks only; never derives a height, never takes cs_main) ----

    //!
    //! \brief Whether the project is in the selected greylist state's computed set.
    //!
    //! \param state Required state selector (see GreylistState).
    //! \param name Project name (the whitelist key).
    //! \param only_auto_greylisted If true (default), only projects meeting the greylist
    //! criteria match; if false, any project with a candidate entry matches (V1 semantics).
    //!
    bool Contains(GreylistState state, const std::string& name,
                  const bool& only_auto_greylisted = true) const;

    //!
    //! \brief Whether Whitelist::Snapshot must deep-copy project entries before applying the
    //! greylist overlay for the selected state. V2-backed results hard-code true (the
    //! deep-copy gate is at or below the redesign gate by the enforced ordering constraint);
    //! V1-backed reads forward to the V1 instance.
    //!
    bool IsDeepCopyActive(GreylistState state) const;

    //!
    //! \brief The computed result for the selected state, or std::nullopt when it is not
    //! primed. A consumer that cannot tolerate an unprimed state must refuse rather than
    //! treat absence as an empty greylist.
    //!
    std::optional<GreylistComputation> Get(GreylistState state) const;

    //!
    //! \brief Compute a fresh greylist report against the current committed superblock.
    //!
    //! Value-returning: mutates no cached state. Above the redesign gate this runs the V2
    //! walker against the committed head -- the same computation a validator runs to check
    //! the record, so the report doubles as an operator-visible record cross-check. Below
    //! the gate it refreshes the V1 cache (preserving the pre-redesign getautogreylist
    //! behavior) and returns a V1-tagged membership summary; the RPC reads V1 candidate
    //! detail through the transitional iterators below.
    //!
    //! Requires cs_main (reads the current superblock and, above the gate, walks the chain).
    //!
    GreylistComputation ComputeReport() const;

    // ---------- transitional V1 pass-throughs -------------------------------------------

    //! \brief Iteration over the V1 candidate map (getautogreylist and tests). These forward
    //! to the V1 instance and are retired when reporting moves to a value-returning API in
    //! the state-separation stage.
    AutoGreylist::const_iterator begin() const;
    AutoGreylist::const_iterator end() const;
    AutoGreylist::size_type size() const;

private:
    //!
    //! \brief Clear both V2 slots. Every V1-producer write path calls this first, which is
    //! what makes "a filled V2 slot" a trustworthy signal on the read side.
    //!
    void ClearV2Slots();

    //!
    //! \brief Derive the authoritative computation from the stored superblock record if a
    //! source is present and the slot is not yet derived. Must be called with m_service_lock
    //! held. A pure map read of the owning SuperblockPtr -- no chain access, no cs_main, so
    //! lazy priming cannot pull heavyweight work onto a reader thread (the objection that
    //! killed lazy population in the refresh redesign was source-dependent, not
    //! laziness-dependent).
    //!
    void PrimeAuthoritativeLocked() const EXCLUSIVE_LOCKS_REQUIRED(m_service_lock);

    std::shared_ptr<AutoGreylist> m_v1; //!< The frozen V1 instance. Never edited; fronted only.

    mutable CCriticalSection m_service_lock; //!< Leaf lock guarding the V2 slots below.

    //! \brief Owning pointer to the committed superblock the authoritative state derives
    //! from. Held so lazy priming is a pure map read with no chain access. Mutable: primed
    //! lazily from const readers under m_service_lock.
    mutable SuperblockPtr m_authoritative_source GUARDED_BY(m_service_lock);

    //! \brief Whether m_authoritative_source holds a V2-producer write. An explicit flag
    //! rather than SuperblockPtr emptiness: a default-constructed SuperblockPtr wraps a
    //! non-null empty superblock, so pointer emptiness cannot distinguish "cleared" from
    //! "stored" -- exactly the empty-vs-unpopulated ambiguity this design exists to remove.
    mutable bool m_have_authoritative_source GUARDED_BY(m_service_lock);

    //! \brief Lazily derived authoritative result (from m_authoritative_source's record).
    mutable std::optional<GreylistComputation> m_authoritative GUARDED_BY(m_service_lock);

    //! \brief Pending result, keyed by convergence content hash.
    mutable std::optional<GreylistComputation> m_pending GUARDED_BY(m_service_lock);
};

} // namespace GRC

#endif // GRIDCOIN_AUTOGREYLIST_H
