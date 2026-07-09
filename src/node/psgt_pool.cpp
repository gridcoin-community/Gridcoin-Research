// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <node/psgt_pool.h>

#include <consensus/validation.h>
#include <dbwrapper.h>
#include <hash.h>
#include <index/txindex.h>
#include <init.h>
#include <logging.h>
#include <net_processing.h>
#include <policy/fees.h>
#include <tinyformat.h>
#include <txmempool.h>
#include <util.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <cassert>

PSGTPool g_psgt_pool;

std::string PSGTRemovalReasonToString(PSGTRemovalReason reason)
{
    switch (reason) {
    case PSGTRemovalReason::EXPIRED:          return "expired";
    case PSGTRemovalReason::REPLACED:         return "replaced";
    case PSGTRemovalReason::COMPLETED:        return "completed";
    case PSGTRemovalReason::CONFLICT_MEMPOOL: return "conflict-mempool";
    case PSGTRemovalReason::CONFLICT_BLOCK:   return "conflict-block";
    case PSGTRemovalReason::LOCAL_REMOVE:     return "removed";
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return "";
}

std::string PSGTPoolRejectToString(PSGTPoolReject reject)
{
    switch (reject) {
    case PSGTPoolReject::NONE:               return "none";
    case PSGTPoolReject::TOO_LARGE:          return "too-large";
    case PSGTPoolReject::MALFORMED:          return "malformed";
    case PSGTPoolReject::HAS_UNKNOWN_FIELDS: return "unknown-fields";
    case PSGTPoolReject::DUPLICATE_REVISION: return "duplicate-revision";
    case PSGTPoolReject::STRUCTURAL:   return "structural";
    case PSGTPoolReject::INVALID_SIG:  return "invalid-signature";
    case PSGTPoolReject::NO_VALID_SIG: return "no-valid-signature";
    case PSGTPoolReject::COMPLETE:     return "complete";
    case PSGTPoolReject::UTXO_MISSING: return "utxo-missing";
    case PSGTPoolReject::UTXO_SPENT:   return "utxo-spent";
    case PSGTPoolReject::FEE_TOO_LOW:  return "fee-too-low";
    case PSGTPoolReject::FEE_ABSURD:   return "fee-absurd";
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return "";
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

//! Reject fees above this multiple of the relay minimum as user error.
static constexpr int MAX_POOL_FEE_MULTIPLIER = 100;

//! Is the funding output available from this node's point of view? The
//! embedded funding transaction is already hash-authenticated against
//! prevout.hash, so existence and unspentness are the only chain facts left
//! to establish: unspent in the transaction index (confirmed funding) or
//! present in the mempool (unconfirmed funding), and in either case not
//! spent by a mempool transaction.
static PSGTPoolReject CheckFundingOutput(const COutPoint& prevout, CTxDB& txdb, std::string& error)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    {
        LOCK(mempool.cs);

        if (mempool.mapNextTx.count(prevout)) {
            error = strprintf("funding output %s:%u is spent by a mempool transaction",
                              prevout.hash.ToString(), prevout.n);
            return PSGTPoolReject::UTXO_SPENT;
        }

        if (mempool.exists(prevout.hash)) {
            return PSGTPoolReject::NONE; // unconfirmed funding, unspent
        }
    }

    CTxIndex txindex;
    if (!txdb.ReadTxIndex(prevout.hash, txindex)) {
        error = strprintf("funding transaction %s is unknown (not in chain or mempool)",
                          prevout.hash.ToString());
        return PSGTPoolReject::UTXO_MISSING;
    }

    if (prevout.n >= txindex.vSpent.size()) {
        error = strprintf("funding output %s:%u does not exist on chain",
                          prevout.hash.ToString(), prevout.n);
        return PSGTPoolReject::UTXO_MISSING;
    }

    if (!txindex.vSpent[prevout.n].IsNull()) {
        error = strprintf("funding output %s:%u is already spent on chain",
                          prevout.hash.ToString(), prevout.n);
        return PSGTPoolReject::UTXO_SPENT;
    }

    return PSGTPoolReject::NONE;
}

//! True if the PSGT carries any unknown (extension) key-value pairs in its
//! global, per-input, or per-output maps. The relay pool understands only the
//! fields it needs and rejects extension fields (see ValidatePSGTForPool):
//! they are a free revision-hash malleability vector and serve no pooling
//! purpose. Cheap -- iterates the already-decoded maps, no allocation.
static bool PSGTHasUnknownFields(const PartiallySignedTransaction& psgt)
{
    if (!psgt.unknown.empty()) return true;
    for (const PSGTInput& input : psgt.inputs) {
        if (!input.unknown.empty()) return true;
    }
    for (const PSGTOutput& output : psgt.outputs) {
        if (!output.unknown.empty()) return true;
    }
    return false;
}

PSGTPoolReject ValidatePSGTForPool(const PSGTPool& pool,
                                   const std::vector<unsigned char>& wire_bytes,
                                   int64_t now,
                                   PSGTPoolEntry& out_entry,
                                   std::string& error)
{
    AssertLockHeld(cs_main);

    // Decode into a pristine entry: DecodePSGTBytes fills psgt incrementally,
    // so state left over from a previous validation of a reused out_entry
    // (e.g. partial_sigs surviving an inputs.resize to the same count) would
    // otherwise leak into this one.
    out_entry = PSGTPoolEntry{};

    if (wire_bytes.size() > MAX_PSGT_WIRE_SIZE) {
        error = strprintf("PSGT of %zu bytes exceeds the %zu byte limit",
                          wire_bytes.size(), static_cast<size_t>(MAX_PSGT_WIRE_SIZE));
        return PSGTPoolReject::TOO_LARGE;
    }

    PartiallySignedTransaction& psgt = out_entry.psgt;
    if (!DecodePSGTBytes(psgt, wire_bytes, error)) {
        return PSGTPoolReject::MALFORMED;
    }

    // Unknown (extension) fields serve no purpose for a relay pool and are a
    // free malleability vector: appending one mints a fresh wire encoding for
    // otherwise-identical content. Reject them so the canonical revision hash
    // computed below is stable. Cheap check, done before any signature work.
    if (PSGTHasUnknownFields(psgt)) {
        error = "PSGT carries unknown extension fields; not eligible for the pool";
        return PSGTPoolReject::HAS_UNKNOWN_FIELDS;
    }

    // Canonical revision identity. Hash a re-serialization (SerializePSGT emits
    // sorted maps) rather than the received wire bytes, so map reordering cannot
    // mint a fresh identity for the same content either. Combined with #3113's
    // strict DER / low-S encoding -- exactly one valid signature per (keyid,
    // sighash) -- this makes the revision hash canonical for a given (unsigned
    // tx, valid-signer-set). Storing the canonical bytes means getdata serves,
    // and peers converge on, that single canonical form.
    out_entry.serialized = SerializePSGT(psgt);
    out_entry.revision_hash = Hash(out_entry.serialized);

    // Duplicate short-circuit BEFORE the expensive per-signature ECDSA
    // verification below: if we already hold (or recently held) this exact
    // canonical revision, drop it cheaply. This is what stops a peer replaying
    // one valid PSGT under an ever-changing hash to burn signature checks under
    // cs_main (the deferred #3115/#3116 DoS). Correctness does not depend on it
    // -- Add() re-checks under cs_psgt_pool -- so the read here is a pure
    // optimization; a race merely means we do work we would have done anyway.
    if (pool.HaveRevision(out_entry.revision_hash)) {
        error = "revision already present in or recently removed from the pool";
        return PSGTPoolReject::DUPLICATE_REVISION;
    }

    const CTransaction tx(psgt.tx);

    if (tx.nVersion < 2) {
        error = "legacy transaction version";
        return PSGTPoolReject::STRUCTURAL;
    }

    CValidationState state;
    if (!CheckTransaction(tx, state)) {
        error = "unsigned transaction fails CheckTransaction";
        return PSGTPoolReject::STRUCTURAL;
    }

    if (psgt.inputs.size() != tx.vin.size() || psgt.outputs.size() != tx.vout.size()) {
        error = "PSGT input/output count does not match unsigned transaction";
        return PSGTPoolReject::STRUCTURAL;
    }

    for (const PSGTInput& input : psgt.inputs) {
        if (!input.final_script_sig.empty()) {
            error = "pool holds partially signed material only; finalized "
                    "inputs belong in a broadcast transaction";
            return PSGTPoolReject::STRUCTURAL;
        }
    }

    const std::optional<CScriptID> image = GetPSGTImage(psgt);
    if (!image) {
        error = "not a single-arrangement P2SH multisig spend (every input "
                "must carry its matching funding transaction and a redeem "
                "script committed to by the funded output, all for the same "
                "multisig arrangement)";
        return PSGTPoolReject::STRUCTURAL;
    }

    int sigs_required = 0;
    int sigs_total = 0;
    if (!GetPSGTMultisigParams(psgt, sigs_required, sigs_total)) {
        error = "cannot recover m-of-n multisig parameters";
        return PSGTPoolReject::STRUCTURAL;
    }

    if (!VerifyPSGTPartialSigs(psgt, out_entry.valid_keys_per_input, error)) {
        return PSGTPoolReject::INVALID_SIG;
    }

    int min_valid_sigs = sigs_total;
    for (const std::set<CKeyID>& keys : out_entry.valid_keys_per_input) {
        min_valid_sigs = std::min<int>(min_valid_sigs, keys.size());
    }

    // The anti-spam floor of #2910: nobody puts a PSGT on the network without
    // proving they are a party to it (the ValidateMRC principle).
    if (min_valid_sigs < 1) {
        error = "every input must carry at least one valid partial signature";
        return PSGTPoolReject::NO_VALID_SIG;
    }

    // A PSGT with m valid signatures everywhere needs no co-signers: it should
    // be finalized and broadcast as a transaction, and nodes must not race to
    // finalize other people's PSGTs from the pool.
    if (min_valid_sigs >= sigs_required) {
        error = "already fully signed; finalize and broadcast as a transaction";
        return PSGTPoolReject::COMPLETE;
    }

    // One CTxDB for the whole PSGT rather than constructing one per input.
    CTxDB txdb("r");
    for (const CTxIn& txin : tx.vin) {
        const PSGTPoolReject utxo_result = CheckFundingOutput(txin.prevout, txdb, error);
        if (utxo_result != PSGTPoolReject::NONE) {
            return utxo_result;
        }
    }

    // Fee sanity. The funding amounts come from the hash-verified embedded
    // funding transactions, so AnalyzePSGT's fee is trustworthy here; its
    // estimated final size prices the fully signed transaction.
    const PSGTAnalysis analysis = AnalyzePSGT(psgt);
    if (!analysis.fee || !analysis.estimated_final_size) {
        error = "fee or final size not computable";
        return PSGTPoolReject::STRUCTURAL;
    }

    const CAmount min_fee = GetMinFee(tx, 1000, GMF_RELAY, *analysis.estimated_final_size);
    if (*analysis.fee < min_fee) {
        error = strprintf("fee %s below the relay minimum %s for an estimated "
                          "final size of %u bytes",
                          FormatMoney(*analysis.fee), FormatMoney(min_fee),
                          *analysis.estimated_final_size);
        return PSGTPoolReject::FEE_TOO_LOW;
    }

    if (*analysis.fee > MAX_POOL_FEE_MULTIPLIER * min_fee) {
        error = strprintf("fee %s is more than %d times the relay minimum %s; "
                          "rejecting as probable user error",
                          FormatMoney(*analysis.fee), MAX_POOL_FEE_MULTIPLIER,
                          FormatMoney(min_fee));
        return PSGTPoolReject::FEE_ABSURD;
    }

    // out_entry.serialized / revision_hash were set from the canonical
    // re-serialization above (before the duplicate short-circuit).
    out_entry.image = *image;
    out_entry.tx_hash = tx.GetHash();
    out_entry.time_received = now;
    out_entry.valid_sigs = min_valid_sigs;
    out_entry.sigs_required = sigs_required;
    out_entry.sigs_total = sigs_total;
    out_entry.fee = *analysis.fee;

    error.clear();
    return PSGTPoolReject::NONE;
}

// ---------------------------------------------------------------------------
// Pool container
// ---------------------------------------------------------------------------

PSGTPoolAddResult PSGTPool::Add(PSGTPoolEntry&& entry, std::string& reject_reason)
{
    std::optional<PSGTPoolEntry> accepted;
    PSGTPoolChangeType change = PSGTPoolChangeType::ADDED;
    PSGTPoolAddResult result;

    {
        LOCK(cs_psgt_pool);

        const int64_t now = entry.time_received;

        if (m_by_revision.count(entry.revision_hash)
            || m_recently_removed.count(entry.revision_hash)) {
            reject_reason = "already have this revision";
            return PSGTPoolAddResult::DUPLICATE;
        }

        const auto existing_it = m_by_image.find(entry.image);

        if (existing_it == m_by_image.end()) {
            if (m_by_image.size() >= MAX_ENTRIES) {
                // Reject, never evict: slots open as PSGTs complete or
                // expire, and the submitter gets a clear try-again-later
                // signal instead of silently killing someone else's
                // in-progress signing session.
                reject_reason = "PSGT pool is full; try again later";
                return PSGTPoolAddResult::REJECTED_POOL_FULL;
            }

            // First entry for this image: whoever holds a valid signature on
            // it is recorded as the initiator for the image's lifetime.
            entry.initiator_keys.clear();
            for (const std::set<CKeyID>& keys : entry.valid_keys_per_input) {
                entry.initiator_keys.insert(keys.begin(), keys.end());
            }

            accepted = entry;
            InsertInternal(std::move(entry));
            result = PSGTPoolAddResult::ACCEPTED_NEW;
            change = PSGTPoolChangeType::ADDED;
        } else {
            const PSGTPoolEntry& existing = existing_it->second;

            if (entry.tx_hash == existing.tx_hash) {
                // Same unsigned transaction: accept only signature progress.
                // Compare valid-signer SETS, not signature bytes -- ECDSA
                // signatures are malleable and re-signing produces different
                // bytes for the same authorization.
                // Same unsigned tx => same input count, so the per-input signer
                // vectors must match in length. Guard defensively before indexing
                // existing[i] by entry's size, so a future call-site bug or a
                // malformed synthetic entry rejects rather than reading OOB.
                if (existing.valid_keys_per_input.size() != entry.valid_keys_per_input.size()) {
                    reject_reason = "per-input signer vector size mismatch for same transaction";
                    return PSGTPoolAddResult::REJECTED_NOT_BETTER;
                }
                bool superset = true;
                bool strictly_larger = false;
                for (size_t i = 0; i < entry.valid_keys_per_input.size(); ++i) {
                    const std::set<CKeyID>& have = existing.valid_keys_per_input[i];
                    const std::set<CKeyID>& offered = entry.valid_keys_per_input[i];
                    if (!std::includes(offered.begin(), offered.end(),
                                       have.begin(), have.end())) {
                        superset = false;
                        break;
                    }
                    if (offered.size() > have.size()) {
                        strictly_larger = true;
                    }
                }

                if (!superset) {
                    reject_reason = "does not carry every signature of the pooled revision";
                    return PSGTPoolAddResult::REJECTED_NOT_BETTER;
                }
                if (!strictly_larger) {
                    reject_reason = "no new signatures over the pooled revision";
                    return PSGTPoolAddResult::DUPLICATE;
                }
            } else {
                // Different unsigned transaction: only the original initiator
                // may supersede (change destination/amount/fee). Their valid
                // signature over the NEW transaction on every input is the
                // authorization; co-signatures legitimately drop off.
                bool initiator_signed = false;
                for (const CKeyID& keyid : existing.initiator_keys) {
                    bool on_every_input = true;
                    for (const std::set<CKeyID>& keys : entry.valid_keys_per_input) {
                        if (!keys.count(keyid)) {
                            on_every_input = false;
                            break;
                        }
                    }
                    if (on_every_input) {
                        initiator_signed = true;
                        break;
                    }
                }

                if (!initiator_signed) {
                    reject_reason = "supersedes the pooled transaction without a "
                                    "valid signature from its initiator";
                    return PSGTPoolAddResult::REJECTED_NOT_INITIATOR;
                }
            }

            entry.initiator_keys = existing.initiator_keys;
            EraseInternal(entry.image, now);
            accepted = entry;
            InsertInternal(std::move(entry));
            result = PSGTPoolAddResult::ACCEPTED_REPLACEMENT;
            change = PSGTPoolChangeType::UPDATED;
        }
    }

    if (accepted) {
        LogPrint(BCLog::LogFlags::MEMPOOL,
                 "psgtpool: %s image %s revision %s (%d/%d signatures)",
                 change == PSGTPoolChangeType::ADDED ? "accepted" : "replaced",
                 accepted->image.ToString(), accepted->revision_hash.ToString(),
                 accepted->valid_sigs, accepted->sigs_required);
        Notify(*accepted, change, std::nullopt);
    }

    reject_reason.clear();
    return result;
}

bool PSGTPool::Remove(const CScriptID& image, PSGTRemovalReason reason)
{
    std::optional<PSGTPoolEntry> removed;

    {
        LOCK(cs_psgt_pool);
        removed = EraseInternal(image, GetAdjustedTime());
    }

    if (!removed) {
        return false;
    }

    LogPrint(BCLog::LogFlags::MEMPOOL, "psgtpool: removed image %s (%s)",
             image.ToString(), PSGTRemovalReasonToString(reason));
    Notify(*removed, PSGTPoolChangeType::REMOVED, reason);
    return true;
}

std::optional<PSGTPoolEntry> PSGTPool::Get(const CScriptID& image) const
{
    LOCK(cs_psgt_pool);

    const auto it = m_by_image.find(image);
    if (it == m_by_image.end()) return std::nullopt;
    return it->second;
}

std::optional<PSGTPoolEntry> PSGTPool::GetByRevision(const uint256& revision_hash) const
{
    LOCK(cs_psgt_pool);

    const auto it = m_by_revision.find(revision_hash);
    if (it == m_by_revision.end()) return std::nullopt;
    const auto img = m_by_image.find(it->second);
    if (img == m_by_image.end()) return std::nullopt;
    return img->second;
}

std::optional<PSGTPoolEntry> PSGTPool::GetByTxHash(const uint256& tx_hash) const
{
    LOCK(cs_psgt_pool);

    const auto it = m_by_txhash.find(tx_hash);
    if (it == m_by_txhash.end()) return std::nullopt;
    const auto img = m_by_image.find(it->second);
    if (img == m_by_image.end()) return std::nullopt;
    return img->second;
}

bool PSGTPool::HaveRevision(const uint256& revision_hash) const
{
    LOCK(cs_psgt_pool);
    return m_by_revision.count(revision_hash) || m_recently_removed.count(revision_hash);
}

std::vector<PSGTPoolEntry> PSGTPool::GetAll() const
{
    LOCK(cs_psgt_pool);

    std::vector<PSGTPoolEntry> entries;
    entries.reserve(m_by_image.size());
    for (const auto& [image, entry] : m_by_image) {
        entries.push_back(entry);
    }
    return entries;
}

std::vector<uint256> PSGTPool::GetAllRevisionHashes() const
{
    LOCK(cs_psgt_pool);

    std::vector<uint256> hashes;
    hashes.reserve(m_by_revision.size());
    for (const auto& [revision_hash, image] : m_by_revision) {
        hashes.push_back(revision_hash);
    }
    return hashes;
}

std::optional<std::vector<unsigned char>>
PSGTPool::GetSerializedByRevision(const uint256& revision_hash) const
{
    LOCK(cs_psgt_pool);

    const auto it = m_by_revision.find(revision_hash);
    if (it == m_by_revision.end()) return std::nullopt;
    const auto img = m_by_image.find(it->second);
    if (img == m_by_image.end()) return std::nullopt;
    return img->second.serialized;
}

size_t PSGTPool::EraseExpired(int64_t now)
{
    std::vector<PSGTPoolEntry> expired;

    {
        LOCK(cs_psgt_pool);

        std::vector<CScriptID> images;
        for (const auto& [image, entry] : m_by_image) {
            if (now - entry.time_received > EXPIRY_SECONDS) {
                images.push_back(image);
            }
        }
        for (const CScriptID& image : images) {
            if (auto entry = EraseInternal(image, now)) {
                expired.push_back(std::move(*entry));
            }
        }

        EraseExpiredOrphans(now);
    }

    for (const PSGTPoolEntry& entry : expired) {
        LogPrint(BCLog::LogFlags::MEMPOOL, "psgtpool: expired image %s",
                 entry.image.ToString());
        Notify(entry, PSGTPoolChangeType::REMOVED, PSGTRemovalReason::EXPIRED);
    }

    return expired.size();
}

size_t PSGTPool::Size() const
{
    LOCK(cs_psgt_pool);
    return m_by_image.size();
}

void PSGTPool::Clear()
{
    LOCK(cs_psgt_pool);
    m_by_image.clear();
    m_by_revision.clear();
    m_by_txhash.clear();
    m_by_prevout.clear();
    m_recently_removed.clear();
    m_orphans.clear();
    m_orphans_by_prevout.clear();
}

void PSGTPool::TransactionAddedToMempool(const CTransactionRef& tx)
{
    EvictConflicts(*tx, PSGTRemovalReason::CONFLICT_MEMPOOL);
    // The tx may also be the funding an orphan PSGT was waiting on.
    PromoteOrphans(*tx);
}

void PSGTPool::BlockConnected(const CBlock& block, int /*height*/)
{
    for (const CTransaction& tx : block.vtx) {
        EvictConflicts(tx, PSGTRemovalReason::CONFLICT_BLOCK);
        PromoteOrphans(tx);
    }
}

void PSGTPool::EvictConflicts(const CTransaction& tx, PSGTRemovalReason reason)
{
    std::vector<PSGTPoolEntry> evicted;

    {
        LOCK(cs_psgt_pool);

        const int64_t now = GetAdjustedTime();
        std::set<CScriptID> images;
        for (const CTxIn& txin : tx.vin) {
            const auto it = m_by_prevout.find(txin.prevout);
            if (it != m_by_prevout.end()) {
                images.insert(it->second);
            }
        }
        for (const CScriptID& image : images) {
            if (auto entry = EraseInternal(image, now)) {
                evicted.push_back(std::move(*entry));
            }
        }
    }

    for (const PSGTPoolEntry& entry : evicted) {
        LogPrint(BCLog::LogFlags::MEMPOOL,
                 "psgtpool: evicted image %s (%s, input spent by %s)",
                 entry.image.ToString(), PSGTRemovalReasonToString(reason),
                 tx.GetHash().ToString());
        Notify(entry, PSGTPoolChangeType::REMOVED, reason);
    }
}

void PSGTPool::AddOrphan(std::vector<unsigned char> wire, const uint256& revision_hash,
                         const std::vector<COutPoint>& prevouts, int64_t now)
{
    LOCK(cs_psgt_pool);

    // Already pooled, recently removed, or already held: nothing to do.
    if (m_by_revision.count(revision_hash) || m_recently_removed.count(revision_hash)
        || m_orphans.count(revision_hash)) {
        return;
    }

    // Bounded: drop the oldest held orphan when at the cap so a peer cannot grow
    // the map without bound.
    while (m_orphans.size() >= MAX_ORPHAN_PSGTS) {
        auto oldest = m_orphans.begin();
        for (auto it = m_orphans.begin(); it != m_orphans.end(); ++it) {
            if (it->second.time_received < oldest->second.time_received) oldest = it;
        }
        EraseOrphanInternal(oldest->first);
    }

    m_orphans.emplace(revision_hash, OrphanPSGT{std::move(wire), now});
    for (const COutPoint& prevout : prevouts) {
        m_orphans_by_prevout.emplace(prevout, revision_hash);
    }
    LogPrint(BCLog::LogFlags::MEMPOOL, "psgtpool: holding orphan revision %s (%zu held)",
             revision_hash.ToString(), m_orphans.size());
}

size_t PSGTPool::OrphanCount() const
{
    LOCK(cs_psgt_pool);
    return m_orphans.size();
}

void PSGTPool::EraseOrphanInternal(const uint256& revision_hash)
{
    // Copy first: a caller may pass a reference INTO m_orphans (AddOrphan's
    // oldest-eviction does), and erasing that node would dangle the argument
    // before the prevout-index sweep below reads it again.
    const uint256 rev = revision_hash;
    m_orphans.erase(rev);
    for (auto it = m_orphans_by_prevout.begin(); it != m_orphans_by_prevout.end();) {
        it = (it->second == rev) ? m_orphans_by_prevout.erase(it) : std::next(it);
    }
}

size_t PSGTPool::EraseExpiredOrphans(int64_t now)
{
    std::vector<uint256> stale;
    for (const auto& [rev, orphan] : m_orphans) {
        if (now - orphan.time_received > ORPHAN_EXPIRY_SECONDS) stale.push_back(rev);
    }
    for (const uint256& rev : stale) EraseOrphanInternal(rev);
    return stale.size();
}

void PSGTPool::PromoteOrphans(const CTransaction& tx)
{
    AssertLockHeld(cs_main);

    // Snapshot (revision, wire) of every orphan waiting on an output of tx under
    // the pool lock, then re-validate with the pool lock RELEASED -- as in
    // EvictConflicts. ValidatePSGTForPool needs cs_main only (held throughout by
    // our caller, satisfying Add's funding-unspent invariant); Add re-takes
    // cs_psgt_pool as a leaf.
    std::vector<std::pair<uint256, std::vector<unsigned char>>> candidates;
    {
        LOCK(cs_psgt_pool);
        if (m_orphans.empty()) return;

        const uint256 txid = tx.GetHash();
        std::set<uint256> seen;
        for (size_t n = 0; n < tx.vout.size(); ++n) {
            const auto range = m_orphans_by_prevout.equal_range(COutPoint(txid, n));
            for (auto it = range.first; it != range.second; ++it) {
                const uint256& rev = it->second;
                if (!seen.insert(rev).second) continue;
                const auto orphan = m_orphans.find(rev);
                if (orphan != m_orphans.end()) candidates.emplace_back(rev, orphan->second.wire);
            }
        }
    }
    if (candidates.empty()) return;

    const int64_t now = GetAdjustedTime();
    for (auto& [rev, wire] : candidates) {
        PSGTPoolEntry entry;
        std::string error;
        const PSGTPoolReject reject = ValidatePSGTForPool(*this, wire, now, entry, error);

        // Still waiting on another funding input -- keep holding it.
        if (reject == PSGTPoolReject::UTXO_MISSING) continue;

        // Otherwise we stop holding it: either it now validates (promote) or it
        // hard-rejects (e.g. this tx spent the funding output it needed).
        {
            LOCK(cs_psgt_pool);
            EraseOrphanInternal(rev);
        }

        if (reject != PSGTPoolReject::NONE) {
            LogPrint(BCLog::LogFlags::MEMPOOL, "psgtpool: orphan %s dropped on promotion: %s (%s)",
                     rev.ToString(), PSGTPoolRejectToString(reject), error);
            continue;
        }

        // Relay the freshly re-validated canonical revision (== rev by
        // construction, but do not depend on the stash path for it).
        const uint256 revision = entry.revision_hash;
        std::string reject_reason;
        const PSGTPoolAddResult result = Add(std::move(entry), reject_reason);
        if (result == PSGTPoolAddResult::ACCEPTED_NEW
            || result == PSGTPoolAddResult::ACCEPTED_REPLACEMENT) {
            LogPrint(BCLog::LogFlags::MEMPOOL, "psgtpool: promoted orphan %s (funding arrived)",
                     revision.ToString());
            RelayPSGT(revision);
        }
    }
}

std::optional<PSGTPoolEntry> PSGTPool::EraseInternal(const CScriptID& image, int64_t now)
{
    const auto it = m_by_image.find(image);
    if (it == m_by_image.end()) {
        return std::nullopt;
    }

    PSGTPoolEntry entry = std::move(it->second);
    m_by_image.erase(it);
    m_by_revision.erase(entry.revision_hash);
    m_by_txhash.erase(entry.tx_hash);
    for (const CTxIn& txin : entry.psgt.tx.vin) {
        m_by_prevout.erase(txin.prevout);
    }
    RecordRemoved(entry.revision_hash, now);
    return entry;
}

void PSGTPool::InsertInternal(PSGTPoolEntry&& entry)
{
    m_by_revision[entry.revision_hash] = entry.image;
    m_by_txhash[entry.tx_hash] = entry.image;
    for (const CTxIn& txin : entry.psgt.tx.vin) {
        m_by_prevout[txin.prevout] = entry.image;
    }
    const CScriptID image = entry.image;
    m_by_image.insert_or_assign(image, std::move(entry));
}

void PSGTPool::RecordRemoved(const uint256& revision_hash, int64_t now)
{
    // TTL prune, then a hard cap as a safety net (evict oldest).
    for (auto it = m_recently_removed.begin(); it != m_recently_removed.end();) {
        if (now - it->second > RECENTLY_REMOVED_TTL) {
            it = m_recently_removed.erase(it);
        } else {
            ++it;
        }
    }

    while (m_recently_removed.size() >= MAX_RECENTLY_REMOVED) {
        auto oldest = m_recently_removed.begin();
        for (auto it = m_recently_removed.begin(); it != m_recently_removed.end(); ++it) {
            if (it->second < oldest->second) oldest = it;
        }
        m_recently_removed.erase(oldest);
    }

    m_recently_removed[revision_hash] = now;
}

void PSGTPool::Notify(const PSGTPoolEntry& entry, PSGTPoolChangeType change,
                      std::optional<PSGTRemovalReason> reason) const
{
    if (m_notify_hook) {
        m_notify_hook(entry, change, reason);
    }
}

// ---------------------------------------------------------------------------
// Signing workflow
// ---------------------------------------------------------------------------

PSGTSignResult SignAndAdvancePSGT(const CScriptID& image, std::string& error,
                                  uint256* txid_out)
{
    if (!pwalletMain) {
        error = "wallet is not loaded";
        return PSGTSignResult::FAILED;
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const std::optional<PSGTPoolEntry> pooled = g_psgt_pool.Get(image);
    if (!pooled) {
        error = "PSGT not found in the pool";
        return PSGTSignResult::NOT_FOUND;
    }

    PartiallySignedTransaction psgt = pooled->psgt;

    // SignPSGTInput's multisig return value means "any signature present",
    // not "this wallet signed", so count partial_sigs growth instead.
    bool added_any = false;
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i) {
        const size_t before = psgt.inputs[i].partial_sigs.size();
        SignPSGTInput(*pwalletMain, psgt, i);
        if (psgt.inputs[i].partial_sigs.size() > before) {
            added_any = true;
        }
    }

    if (!added_any) {
        error = "this wallet holds no key that can add a signature";
        return PSGTSignResult::NO_NEW_SIGNATURES;
    }

    // Revalidate the enriched PSGT exactly as the network would; COMPLETE
    // means our signature(s) finished the job.
    const std::vector<unsigned char> wire = SerializePSGT(psgt);
    PSGTPoolEntry replacement;
    std::string validate_error;
    const PSGTPoolReject reject =
        ValidatePSGTForPool(g_psgt_pool, wire, GetAdjustedTime(), replacement, validate_error);

    if (reject == PSGTPoolReject::COMPLETE) {
        PartiallySignedTransaction to_extract = psgt;
        CMutableTransaction final_mtx;
        if (!FinalizeAndExtractPSGT(to_extract, final_mtx)) {
            error = "PSGT has enough signatures but could not be finalized";
            return PSGTSignResult::FAILED;
        }

        // Free the slot as COMPLETED before broadcasting: mempool admission
        // fires this pool's own conflict eviction, which would otherwise get
        // there first and report the completion as a conflict.
        g_psgt_pool.Remove(image, PSGTRemovalReason::COMPLETED);

        CTransaction final_tx(final_mtx);
        CValidationState state;
        if (!AcceptToMemoryPool(mempool, final_tx, state, nullptr)) {
            // The local slot is already freed, but peers still carry the
            // PSGT and the initiator can always resubmit.
            error = "finalized transaction was rejected by the mempool";
            return PSGTSignResult::FAILED;
        }

        RelayTransaction(final_tx, final_tx.GetHash());

        if (txid_out) *txid_out = final_tx.GetHash();

        LogPrint(BCLog::LogFlags::MEMPOOL,
                 "psgtpool: completed image %s -> broadcast tx %s",
                 image.ToString(), final_tx.GetHash().ToString());
        return PSGTSignResult::COMPLETED_AND_BROADCAST;
    }

    if (reject == PSGTPoolReject::DUPLICATE_REVISION) {
        // The wallet's signature reproduced a revision the pool already holds or
        // recently held: another co-signer holding the same key signed
        // identically first (deterministic low-S ECDSA yields identical bytes),
        // or this exact revision was pooled before. The contribution is already
        // known to the network -- there is nothing to add or relay, and it is
        // not a failure. Report the unsigned-tx id like the SIGNED path does.
        if (txid_out) *txid_out = pooled->tx_hash;
        return PSGTSignResult::ALREADY_KNOWN;
    }

    if (reject != PSGTPoolReject::NONE) {
        error = strprintf("signed PSGT failed revalidation: %s (%s)",
                          PSGTPoolRejectToString(reject), validate_error);
        return PSGTSignResult::FAILED;
    }

    const uint256 revision_hash = replacement.revision_hash;
    std::string reject_reason;
    const PSGTPoolAddResult result = g_psgt_pool.Add(std::move(replacement), reject_reason);
    if (result != PSGTPoolAddResult::ACCEPTED_REPLACEMENT
        && result != PSGTPoolAddResult::ACCEPTED_NEW) {
        error = strprintf("signed PSGT was not pooled: %s", reject_reason);
        return PSGTSignResult::FAILED;
    }

    if (txid_out) *txid_out = pooled->tx_hash;

    RelayPSGT(revision_hash);
    return PSGTSignResult::SIGNED_AND_RELAYED;
}
