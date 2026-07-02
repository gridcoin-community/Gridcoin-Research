// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_NODE_PSGT_POOL_H
#define GRIDCOIN_NODE_PSGT_POOL_H

#include <amount.h>
#include <main.h>
#include <psgt.h>
#include <pubkey.h>
#include <script/script.h>
#include <sync.h>
#include <uint256.h>
#include <validationinterface.h>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

//!
//! \brief The in-band PSGT pool (issue #2910): pending partially signed
//! multisig transactions relayed through the network so co-signers can find,
//! review, sign and rebroadcast them without out-of-band coordination.
//!
//! Identity model:
//!  - Every pooled PSGT has an \b image = CScriptID(redeem_script) -- the
//!    multisig arrangement alone, NOT destinations or amounts. There is at
//!    most ONE active PSGT per image, so pool capacity is bounded by the
//!    number of distinct multisig arrangements actually funded on-chain,
//!    which an attacker cannot inflate for free.
//!  - The relay identity of a specific revision is its \b revision hash =
//!    Hash(serialized wire bytes). Adding a co-signature does not change the
//!    unsigned transaction's hash, so tx-hash inventory would never
//!    propagate signature progress; every mutation produces a fresh
//!    revision hash and therefore a fresh inv.
//!

//! Why a pooled PSGT was removed.
enum class PSGTRemovalReason
{
    EXPIRED,          //!< Older than EXPIRY_SECONDS.
    REPLACED,         //!< Superseded by a better revision of the same image.
    COMPLETED,        //!< Finalized locally into a broadcast transaction.
    CONFLICT_MEMPOOL, //!< An input was spent by a transaction accepted to the mempool.
    CONFLICT_BLOCK,   //!< An input was confirmed spent by a connected block.
    LOCAL_REMOVE,     //!< Explicitly removed by the local user (RPC/GUI).
};

//! Human-readable removal reason (for logs, RPC and notifications).
std::string PSGTRemovalReasonToString(PSGTRemovalReason reason);

//! How the pool changed, for the notification hook.
enum class PSGTPoolChangeType
{
    ADDED,   //!< New image accepted.
    UPDATED, //!< Existing image replaced by a better/initiator revision.
    REMOVED, //!< Entry left the pool (see PSGTRemovalReason).
};

//! Result of PSGTPool::Add.
enum class PSGTPoolAddResult
{
    ACCEPTED_NEW,          //!< First entry for this image.
    ACCEPTED_REPLACEMENT,  //!< Replaced the existing entry for this image.
    DUPLICATE,             //!< Already have this revision (or it was just removed); no-op.
    REJECTED_POOL_FULL,    //!< Pool at capacity and this is a new image.
    REJECTED_NOT_BETTER,   //!< Same unsigned tx but not a strict signature superset.
    REJECTED_NOT_INITIATOR,//!< Different unsigned tx without a valid initiator signature.
};

//! A pooled PSGT together with everything validation established about it.
//! Produced by ValidatePSGTForPool in production; outside of unit tests that
//! exercise container semantics with synthetic entries, do not hand-build one
//! for Add() (its invariants are otherwise unchecked).
struct PSGTPoolEntry
{
    PartiallySignedTransaction psgt;
    std::vector<unsigned char> serialized; //!< Exact wire bytes (getdata serving; hashed for revision).
    uint256 revision_hash;                 //!< Hash(serialized) -- the inv/relay identity.
    CScriptID image;                       //!< CScriptID(redeem script) -- the pool key.
    uint256 tx_hash;                       //!< Hash of the unsigned transaction.
    int64_t time_received = 0;

    //! Key ids holding a cryptographically valid partial signature, per input
    //! (VerifyPSGTPartialSigs output). Basis of the replacement rules.
    std::vector<std::set<CKeyID>> valid_keys_per_input;

    //! Key ids that signed the FIRST entry ever accepted for this image.
    //! Carried forward verbatim across replacements; a valid signature by one
    //! of these keys over a *different* unsigned tx authorizes the initiator
    //! to supersede the pending PSGT (change destination/amount/fee) even
    //! though co-signatures drop off.
    std::set<CKeyID> initiator_keys;

    int valid_sigs = 0;     //!< Minimum per-input count of valid signatures.
    int sigs_required = 0;  //!< m of the m-of-n arrangement.
    int sigs_total = 0;     //!< n of the m-of-n arrangement.
    CAmount fee = 0;        //!< sum(inputs) - sum(outputs), from the hash-verified funding txs.
};

//! Why ValidatePSGTForPool rejected a candidate. Ordered roughly by the
//! validation pipeline. The P2P layer maps these to misbehavior scores; the
//! RPC layer maps them to error strings.
enum class PSGTPoolReject
{
    NONE,         //!< Valid; out_entry is filled.
    TOO_LARGE,    //!< Wire size above MAX_PSGT_WIRE_SIZE.
    MALFORMED,    //!< Does not decode as a PSGT.
    STRUCTURAL,   //!< Decodes, but is not a well-formed single-image P2SH multisig spend.
    INVALID_SIG,  //!< Carries a cryptographically invalid or foreign partial signature.
    NO_VALID_SIG, //!< Some input has no valid partial signature (anti-spam floor).
    COMPLETE,     //!< Already has m valid signatures on every input; belongs in a tx, not the pool.
    UTXO_MISSING, //!< A funding transaction is unknown to this node (chain + mempool).
    UTXO_SPENT,   //!< A funding output is already spent (chain or mempool).
    FEE_TOO_LOW,  //!< Below the relay minimum for the estimated final size.
    FEE_ABSURD,   //!< Implausibly high; almost certainly user error.
};

//! Human-readable reject reason (for logs and RPC errors).
std::string PSGTPoolRejectToString(PSGTPoolReject reject);

//!
//! \brief Validate a PSGT received from an untrusted source (network relay or
//! local submission) and fill a pool entry from it.
//!
//! Checks, cheap to expensive: wire size; decode; basic transaction sanity
//! (CheckTransaction, version >= 2, no finalized inputs); single-image P2SH
//! multisig structure across ALL inputs (GetPSGTImage's trustworthiness
//! rules); cryptographic verification of EVERY partial signature with at
//! least one valid signature on every input, and fewer than m so the pool
//! only holds material that still needs co-signers; funding outputs exist
//! unspent in the chain or mempool (the embedded funding transactions are
//! hash-authenticated by prevout, so existence and unspentness are the only
//! chain facts to establish); and fee within [relay minimum, 100x relay
//! minimum] for the estimated final size.
//!
//! Requires cs_main (chain/mempool lookups). Does NOT check IsV15Enabled or
//! sync state -- those are the caller's (P2P handler / RPC) admission gates.
//!
PSGTPoolReject ValidatePSGTForPool(const std::vector<unsigned char>& wire_bytes,
                                   int64_t now,
                                   PSGTPoolEntry& out_entry,
                                   std::string& error) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

//!
//! \brief Bounded, expiring container of pending PSGTs, one per image.
//!
//! Locking: a dedicated leaf mutex. Lock order is cs_main -> cs_psgt_pool;
//! nothing that takes cs_main, the connection manager's node mutex, or
//! cs_wallet may be called while holding it. The notification hook and any
//! relay always fire AFTER the lock is released (though possibly still under
//! the caller's cs_main, as the eviction callbacks run under it).
//!
//! The two CValidationInterface overrides implement the UTXO-conflict
//! eviction triggers of #2910: mempool admission (fast, near-simultaneous
//! network-wide) and block connection (authoritative). They are public so
//! tests can drive them directly; in production they are invoked through
//! CMainSignals after RegisterValidationInterface in init.
//!
class PSGTPool final : public CValidationInterface
{
public:
    //! Maximum entries. Reached only if this many distinct funded multisig
    //! arrangements have a PSGT in flight at once; new images are REJECTED
    //! when full (no eviction of someone else's in-progress signing session);
    //! replacements of existing images are not growth and still accepted.
    static constexpr size_t MAX_ENTRIES = 100;

    //! Time-based expiry: covers timezone gaps, weekends, travel. The
    //! initiator can always resubmit (they retain the PSGT locally).
    static constexpr int64_t EXPIRY_SECONDS = 7 * 24 * 60 * 60;

    //! How long removed revision hashes keep answering "already have" so a
    //! completed/evicted/replaced revision is neither re-fetched from lagging
    //! peers nor re-admitted by gossip. A NEW revision of the same image is
    //! unaffected (keyed by revision, not image).
    static constexpr int64_t RECENTLY_REMOVED_TTL = 60 * 60;

    //! Hard cap on the recently-removed set (safety net; TTL is the normal bound).
    static constexpr size_t MAX_RECENTLY_REMOVED = 1000;

    //! Notification hook, fired outside the pool lock for every mutation.
    //! For REMOVED changes the removal reason is provided. Wired to the
    //! uiInterface signal and -psgtnotify by the RPC/notification layer;
    //! unset (and skipped) until then.
    std::function<void(const PSGTPoolEntry&, PSGTPoolChangeType,
                       std::optional<PSGTRemovalReason>)> m_notify_hook;

    //! Add a validated entry, applying the one-per-image replacement rules:
    //!  - unknown image: accepted (unless the pool is full);
    //!  - same image, same unsigned tx: accepted iff the valid-signer sets
    //!    are a superset on every input and strictly larger on at least one
    //!    (signature progress). Signer SETS are compared, not signature
    //!    bytes: ECDSA signatures are malleable and re-signing produces
    //!    different bytes for the same authorization.
    //!  - same image, different unsigned tx: accepted iff some initiator key
    //!    has a valid signature on EVERY input of the replacement
    //!    (initiator-privileged supersede).
    //! initiator_keys are snapshotted from the first accepted entry and
    //! carried forward on every replacement.
    PSGTPoolAddResult Add(PSGTPoolEntry&& entry, std::string& reject_reason);

    //! Remove the entry for an image. Returns false if not present.
    bool Remove(const CScriptID& image, PSGTRemovalReason reason);

    std::optional<PSGTPoolEntry> Get(const CScriptID& image) const;
    std::optional<PSGTPoolEntry> GetByRevision(const uint256& revision_hash) const;
    std::optional<PSGTPoolEntry> GetByTxHash(const uint256& tx_hash) const;

    //! True if the revision is pooled OR was recently removed (relay
    //! damping: recently completed/evicted revisions read as "already have").
    bool HaveRevision(const uint256& revision_hash) const;

    //! Snapshot of all entries (RPC/GUI listing, connect-time advertisement).
    std::vector<PSGTPoolEntry> GetAll() const;

    //! Evict entries older than EXPIRY_SECONDS. Returns the number evicted.
    size_t EraseExpired(int64_t now);

    size_t Size() const;
    void Clear();

    //! Eviction trigger 1: a transaction entered the local mempool; any
    //! pooled PSGT spending one of its inputs is dead (locally observed
    //! double spend -- or, commonly, the completed PSGT itself arriving as a
    //! transaction, which is how completion propagates network-wide).
    void TransactionAddedToMempool(const CTransactionRef& tx) override;

    //! Eviction trigger 2 (authoritative): a connected block spent an input.
    void BlockConnected(const CBlock& block, int height) override;

private:
    mutable CCriticalSection cs_psgt_pool;

    std::map<CScriptID, PSGTPoolEntry> m_by_image GUARDED_BY(cs_psgt_pool);
    std::map<uint256, CScriptID> m_by_revision GUARDED_BY(cs_psgt_pool);
    std::map<uint256, CScriptID> m_by_txhash GUARDED_BY(cs_psgt_pool);
    //! One image per prevout: a P2SH output belongs to exactly one arrangement
    //! and each arrangement has at most one pooled PSGT.
    std::map<COutPoint, CScriptID> m_by_prevout GUARDED_BY(cs_psgt_pool);
    //! Removed revision hash -> removal time (TTL-pruned, hard-capped).
    std::map<uint256, int64_t> m_recently_removed GUARDED_BY(cs_psgt_pool);

    //! Erase an entry from every index and record its revision as recently
    //! removed. Returns the entry (for post-lock notification).
    std::optional<PSGTPoolEntry> EraseInternal(const CScriptID& image, int64_t now)
        EXCLUSIVE_LOCKS_REQUIRED(cs_psgt_pool);

    //! Insert an entry into every index (image slot must be free).
    void InsertInternal(PSGTPoolEntry&& entry) EXCLUSIVE_LOCKS_REQUIRED(cs_psgt_pool);

    //! TTL-prune and cap m_recently_removed, then record a removal.
    void RecordRemoved(const uint256& revision_hash, int64_t now)
        EXCLUSIVE_LOCKS_REQUIRED(cs_psgt_pool);

    //! Shared body of the two eviction triggers.
    void EvictConflicts(const CTransaction& tx, PSGTRemovalReason reason);

    //! Fire m_notify_hook if set (call with the lock RELEASED).
    void Notify(const PSGTPoolEntry& entry, PSGTPoolChangeType change,
                std::optional<PSGTRemovalReason> reason) const;
};

//! Global PSGT pool. Registered as a validation-interface subscriber in init.
extern PSGTPool g_psgt_pool;

#endif // GRIDCOIN_NODE_PSGT_POOL_H
