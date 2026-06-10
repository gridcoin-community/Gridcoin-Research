// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_POOL_H
#define GRIDCOIN_POOL_H

#include "amount.h"
#include "key.h"
#include "pubkey.h"
#include "serialize.h"
#include "uint256.h"

#include "gridcoin/contract/handler.h"
#include "gridcoin/contract/payload.h"
#include "gridcoin/contract/registry_db.h"
#include "gridcoin/cpid.h"
#include "gridcoin/support/enumbytes.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace GRC {

//!
//! \brief Status values for an on-chain pool registration entry.
//!
//! The lifecycle is:
//!
//!   POOL_REGISTER ADD (operator-signed)  -->  PENDING
//!   POOL_APPROVE ADD  (master-key signed) -->  ACTIVE   (must reference a known PENDING/ACTIVE CPID)
//!   POOL_APPROVE REMOVE (master-key)      -->  DELETED  (foundation de-list)
//!   POOL_REGISTER REMOVE (operator-signed) -->  DELETED  (operator self-withdrawal)
//!   POOL_APPROVE OPEN  (master-key)       -->  status unchanged; sets
//!                                              Pool::m_authorized_operator_key
//!                                              authorizing a specific
//!                                              operator pubkey to claim a
//!                                              builtin slot (see §11 of
//!                                              doc/consensus.md).
//!
//! Note: PENDING entries and OPEN authorizations are query-time expired
//! after consensus.PendingPoolRetention blocks via IsPendingExpired /
//! IsAuthorizationExpired — pure functions, no state mutation, reorg-safe.
//!
enum class PoolStatus : uint8_t
{
    UNKNOWN,
    PENDING,    //!< Operator submitted, awaiting foundation approval.
    ACTIVE,     //!< Approved by foundation; consulted by IsPoolCpid/IsPoolUsername.
    DELETED,    //!< De-listed (foundation) or withdrawn (operator).
    OUT_OF_BOUND
};

//!
//! \brief Runtime entry for an on-chain pool. Carries the data persisted in the
//! pool RegistryDB (history-tracked via m_previous_hash).
//!
class Pool
{
public:
    using Status = EnumByte<PoolStatus>;

    Cpid m_cpid;                //!< The pool's external CPID (registry key).
    std::string m_name;         //!< Display name (e.g. "grcpool.com").
    std::string m_url;          //!< Pool website.
    CPubKey m_operator_key;     //!< Operator key for self-sovereign updates / withdrawal.
    int64_t m_timestamp;        //!< Tx time of the contract that produced this entry.
    uint256 m_hash;             //!< Tx hash of the contract that produced this entry.
    uint256 m_previous_hash;    //!< Tx hash of the prior entry for this CPID (history chain).
    int m_height;               //!< Block height at which this entry was produced. Grandfathered builtins use 0.
    Status m_status;            //!< Current lifecycle status.

    //!
    //! \brief Foundation-pre-authorized operator pubkey allowed to claim a
    //! builtin pool slot. Set by POOL_APPROVE OPEN, cleared on successful
    //! POOL_REGISTER ADD that consumes the authorization, propagated
    //! unchanged through other chained actions (so Foundation can perform
    //! unrelated admin actions between OPEN and the operator's claim
    //! without invalidating the authorization). See doc/consensus.md §11.
    //!
    CPubKey m_authorized_operator_key;

    //!
    //! \brief Block height at which the OPEN authorization was set. Used by
    //! the IsAuthorizationExpired check; tracked separately from m_height
    //! so retention does NOT drift forward when an inheriting action (e.g.
    //! Foundation POOL_APPROVE ADD/REMOVE) chains a new entry between the
    //! OPEN and the operator's claim. Sentinel -1 == no authorization set.
    //!
    int m_authorization_height;

    Pool();

    Pool(Cpid cpid, std::string name, std::string url, CPubKey operator_key);

    //!
    //! \brief Sanity check. Does not validate any signature.
    //!
    bool WellFormed() const;

    //!
    //! \brief Registry-template key accessor.
    //!
    Cpid Key() const { return m_cpid; }

    //!
    //! \brief Used by RegistryDB logging.
    //!
    std::pair<std::string, std::string> KeyValueToString() const;

    //!
    //! \brief Returns the (translated) status string of the current m_status.
    //!
    std::string StatusToString() const;

    //!
    //! \brief Returns the (translated or raw) status string for the given enum.
    //!
    std::string StatusToString(const PoolStatus& status, const bool& translated = true) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_cpid);
        READWRITE(m_name);
        READWRITE(m_url);
        READWRITE(m_operator_key);
        READWRITE(m_timestamp);
        READWRITE(m_hash);
        READWRITE(m_previous_hash);
        READWRITE(m_height);
        READWRITE(m_status);
        READWRITE(m_authorized_operator_key);
        READWRITE(m_authorization_height);
    }
};

typedef std::shared_ptr<Pool> Pool_ptr;

//!
//! \brief The body of a POOL_REGISTER contract. Operator-self-signed.
//!
//! Operator submits this to register a pool. Lands in PENDING in the registry
//! and does NOT affect IsPoolCpid until a follow-up POOL_APPROVE flips it to
//! ACTIVE. RequiresMasterKey returns false for POOL_REGISTER, so a non-master
//! wallet can broadcast this without rejection by CheckContracts; the
//! operator-key signature in m_signature is what authenticates the submission.
//!
class PoolRegisterPayload : public IContractPayload
{
public:
    //!
    //! \brief Bound for the serialized display name. Discourages spam.
    //!
    static constexpr size_t MAX_NAME_SIZE = 100;

    //!
    //! \brief Bound for the serialized pool URL.
    //!
    static constexpr size_t MAX_URL_SIZE = 500;

    //!
    //! \brief GRC burned by every POOL_REGISTER tx, both ADD and REMOVE.
    //!
    //! Set higher than the standard 0.5 GRC anti-spam floor because pool
    //! registration is a heavier on-chain commitment than a beacon — a
    //! pool operator is asking the network and the researchers staking
    //! through their pool to trust them with honest BOINC aggregation,
    //! infrastructure, and reported policy, sustained over time. 100 GRC
    //! puts the cost above the 50 GRC poll-creation burn (poll creation
    //! also gates on a 100K GRC balance attestation; pools don't but
    //! deserve at least the same ordering). Symmetric for ADD and REMOVE
    //! to keep the framework's per-payload-constant pattern — REMOVE's
    //! spam control is the existing-key signature check
    //! (PoolRegistry::VerifyRegisterAuth), not the burn. Reviewed and
    //! endorsed by James Cowens on #2955.
    //!
    //! Combined with the PENDING + OPEN retention window (issue #1783
    //! findings J + L), this turns a builtin-slot race attack from
    //! "indefinite war of attrition at 0.5 GRC per round" into
    //! "bounded cost, bounded duration."
    //!
    static constexpr CAmount REGISTRATION_BURN = 100 * COIN;

    static constexpr uint32_t CURRENT_VERSION = 1;

    uint32_t m_version = CURRENT_VERSION;

    Cpid m_cpid;
    std::string m_name;
    std::string m_url;
    CPubKey m_operator_key;

    //!
    //! \brief Operator-signed bytes proving the operator owns m_operator_key.
    //!
    //! For renewals / withdrawal on a CPID already registered, the signature is
    //! verified against the *existing* entry's operator key, not this payload's
    //! key. This prevents takeover by a third party submitting a new operator
    //! key for someone else's CPID. (See PoolRegistry::Add.)
    //!
    //! The signature covers the serialized payload bytes EXCLUDING this field
    //! (SER_GETHASH exclusion below), so re-signing the same payload doesn't
    //! change its contract hash.
    //!
    std::vector<uint8_t> m_signature;

    PoolRegisterPayload();

    PoolRegisterPayload(Cpid cpid, std::string name, std::string url, CPubKey operator_key);

    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::POOL_REGISTER;
    }

    bool WellFormed(const ContractAction action) const override;

    std::string LegacyKeyString() const override;

    std::string LegacyValueString() const override;

    CAmount RequiredBurnAmount() const override
    {
        return REGISTRATION_BURN;
    }

    //!
    //! \brief Sign the payload bytes with the operator's private key.
    //!
    //! \return false if signing failed.
    //!
    bool Sign(CKey& operator_private_key);

    //!
    //! \brief Verify m_signature against the supplied pubkey.
    //!
    //! Callers pass either this payload's m_operator_key (new CPID) or the
    //! existing entry's m_operator_key (CPID renewal / withdrawal).
    //!
    bool VerifySignature(const CPubKey& expected_key) const;

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_cpid);
        READWRITE(LIMITED_STRING(m_name, MAX_NAME_SIZE));
        READWRITE(LIMITED_STRING(m_url, MAX_URL_SIZE));
        READWRITE(m_operator_key);

        // Exclude the signature from the hash so the operator can sign the
        // payload after constructing it. The CPubKey IS in the hash, so the
        // signature is bound to a specific operator key.
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(m_signature);
        }
    }
};

//!
//! \brief The body of a POOL_APPROVE contract. Master-key authenticated.
//!
//! Foundation submits this to flip a PENDING entry to ACTIVE (action=ADD),
//! to de-list an active pool (action=REMOVE), or to pre-authorize a
//! specific operator pubkey to claim a builtin pool slot (action=OPEN).
//! The transaction is authenticated by Contract::RequiresMasterKey()
//! returning true for POOL_APPROVE; the framework's existing
//! HasMasterKeyInput check enforces master-key authority at the validation
//! boundary (see src/validation.cpp:175) for all three actions.
//!
//! For ADD/REMOVE the payload is minimal — version + cpid — because the
//! authority is in the tx inputs and the target is identified by CPID.
//! For OPEN it additionally carries m_authorized_operator_key naming the
//! operator pubkey the Foundation is authorizing to claim the slot.
//!
//! CURRENT_VERSION stays at 1 because POOL_APPROVE was introduced by this
//! PR and has no on-chain v1 payload to be backward-compatible with —
//! conditional serialization on contract_action is sufficient.
//!
class PoolApprovePayload : public IContractPayload
{
public:
    static constexpr uint32_t CURRENT_VERSION = 1;

    uint32_t m_version = CURRENT_VERSION;

    Cpid m_cpid;

    //!
    //! \brief Operator pubkey authorized to claim the slot. Populated only
    //! for action=OPEN; default-constructed (invalid) for ADD/REMOVE.
    //! WellFormed(OPEN) rejects payloads where this key isn't valid.
    //!
    CPubKey m_authorized_operator_key;

    PoolApprovePayload();

    explicit PoolApprovePayload(Cpid cpid);

    //!
    //! \brief Construct an OPEN payload pre-authorizing the supplied
    //! operator pubkey to claim the given builtin slot.
    //!
    PoolApprovePayload(Cpid cpid, CPubKey authorized_operator_key);

    GRC::ContractType ContractType() const override
    {
        return GRC::ContractType::POOL_APPROVE;
    }

    bool WellFormed(const ContractAction action) const override;

    std::string LegacyKeyString() const override;

    std::string LegacyValueString() const override
    {
        return std::string{};
    }

    CAmount RequiredBurnAmount() const override
    {
        return Contract::STANDARD_BURN_AMOUNT;
    }

    ADD_CONTRACT_PAYLOAD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(
        Stream& s,
        Operation ser_action,
        const ContractAction contract_action)
    {
        READWRITE(m_version);
        READWRITE(m_cpid);

        // m_authorized_operator_key is serialized only for OPEN. Pre-OPEN
        // POOL_APPROVE contracts (ADD/REMOVE) keep the original v1 wire
        // shape of (version, cpid) = 20 bytes; an OPEN payload extends to
        // (version, cpid, pubkey) = 20 + 33 = 53 bytes.
        if (contract_action == ContractAction::OPEN) {
            READWRITE(m_authorized_operator_key);
        }
    }
};

//!
//! \brief Handles POOL_REGISTER and POOL_APPROVE contracts.
//!
//! Persisted via the standard RegistryDB template (history-tracked, reorg-safe).
//! All on-chain pool state lives in m_pools_active / m_pools_pending / m_pool_db.
//!
//! Consumed by researcher.cpp (IsPoolCpid / IsPoolUsername) and by the Qt
//! researcher wizard pool page.
//!
class PoolRegistry : public IContractHandler
{
public:
    PoolRegistry()
        : m_pool_db(3)
    {
        SeedBuiltinPools();
    }

    typedef std::map<Cpid, Pool_ptr> PoolMap;

    //!
    //! \brief One row of the static builtin-pool seed list. Mirrors
    //! MiningPools (researcher.h) so the two stay in sync; see
    //! BuiltinPoolSeeds() below.
    //!
    struct BuiltinSeed
    {
        const char* cpid_hex;
        const char* name;
        const char* url;
    };

    //!
    //! \brief The 5 pre-V15 hardcoded pools, grandfathered into the
    //! registry at construction so pre-activation queries match the legacy
    //! g_mining_pools list bit-for-bit. Must stay in sync with the
    //! MiningPools constructor in researcher.h:88-95.
    //!
    static const std::vector<BuiltinSeed>& BuiltinPoolSeeds();

    //!
    //! \brief Deterministic synthetic tx-hash sentinel for the seed entry
    //! of a builtin CPID. Used as m_previous_hash when the first real
    //! contract lands on a builtin so Revert's chain-walk terminates on
    //! the seed (which is installed in m_pool_db.m_historical at boot
    //! via RegistryDB::InstallInMemorySeed).
    //!
    static uint256 BuiltinSeedHash(const Cpid& cpid);

    //!
    //! \brief True if the supplied CPID is one of the grandfathered
    //! builtins. Used by BlockValidate to block opportunistic claims of
    //! builtin CPIDs (an attacker submitting POOL_REGISTER ADD before the
    //! legitimate operator notices V15 activation).
    //!
    bool IsBuiltin(const Cpid& cpid) const;

    //!
    //! \brief Satisfies the RegistryDB pending-map template parameter. Pool
    //! entries enter the active map at PENDING status; we don't keep a
    //! separate pending sub-map because the lookup is keyed by CPID and the
    //! status field distinguishes ACTIVE vs PENDING vs DELETED. This typedef
    //! exists only because RegistryDB requires the third template parameter.
    //!
    typedef PoolMap PendingPoolMap;

    typedef std::map<uint256, Pool_ptr> HistoricalPoolMap;

    //!
    //! \brief All entries (active + pending + deleted) as a value snapshot
    //! taken under cs_lock. Caller filters by status. Returns by value
    //! rather than const-ref to avoid lifetime issues with concurrent
    //! mutation — the lock is released before the caller iterates.
    //!
    std::vector<Pool> Entries() const;

    //!
    //! \brief Get all pools currently in the ACTIVE state. Used by the wizard
    //! and the listpools RPC.
    //!
    std::vector<Pool> ActivePools() const;

    //!
    //! \brief Get all pools that were ACTIVE as of the supplied block
    //! height — the consensus-critical view used by voting AVW
    //! (voting/registry.cpp and voting/result.cpp).
    //!
    //! For each CPID currently in m_pool_entries, walks back through
    //! m_previous_hash via m_pool_db.find() (which falls back to LevelDB
    //! for passivated entries) until reaching an entry with
    //! m_height <= height. If that entry's m_status == ACTIVE, includes
    //! it in the result. Grandfathered builtin seeds have m_height == 0
    //! so they appear in every query for height >= 0, exactly matching
    //! the pre-V15 hardcoded list bit-for-bit.
    //!
    std::vector<Pool> ActivePoolsAtHeight(int height) const;

    //!
    //! \brief Get the entry for the supplied CPID, or nullptr.
    //!
    Pool_ptr Try(const Cpid& cpid) const;

    //!
    //! \brief Cheap query for researcher.cpp::IsPoolCpid.
    //!
    bool IsActivePool(const Cpid& cpid) const;

    //!
    //! \brief Cheap query for researcher.cpp::IsPoolUsername (matches display name).
    //!
    bool IsActivePoolName(const std::string& name) const;

    //!
    //! \brief True if the supplied entry is in PENDING status AND its
    //! registration height + consensus.PendingPoolRetention is below the
    //! query height. Pure function, no state mutation — expired-PENDING
    //! entries remain in storage but are treated as absent by queries and
    //! by the takeover-defense paths in Validate / BlockValidate. Reorg
    //! back across the boundary naturally surfaces the entry as PENDING
    //! again. See doc/consensus.md §11.
    //!
    static bool IsPendingExpired(const Pool& entry, int at_height);

    //!
    //! \brief True if the supplied entry has a valid
    //! m_authorized_operator_key AND its m_authorization_height plus
    //! consensus.PendingPoolRetention is below the query height. Pure
    //! function, same reorg-safety properties as IsPendingExpired. Used by
    //! the IsBuiltin sticky-claim guard so an unused OPEN goes stale on
    //! the same budget as PENDING.
    //!
    static bool IsAuthorizationExpired(const Pool& entry, int at_height);

    void Reset() override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool Validate(const Contract& contract, const CTransaction& tx, int& DoS) const override
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool BlockValidate(const ContractContext& ctx, int& DoS) const override
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    void Add(const ContractContext& ctx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    void Delete(const ContractContext& ctx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    void Revert(const ContractContext& ctx) override EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    int Initialize() override;

    int GetDBHeight() override;

    void SetDBHeight(int& height) override;

    uint64_t PassivateDB() override;

    //!
    //! \brief RegistryDB instantiation. The third template param (PendingPoolMap)
    //! is typedef'd to PoolMap because we keep PENDING entries in the main map.
    //!
    typedef RegistryDB<Pool,
                       Pool,
                       PoolStatus,
                       PoolMap,
                       PendingPoolMap,
                       std::set<Pool>,
                       HistoricalPoolMap> PoolDB;

    //!
    //! \brief Test-only: directly insert a fully-formed Pool into the
    //! in-memory map without going through contract validation or LevelDB.
    //! Mirrors SideStakeRegistry::ResetInMemoryOnly's role as a unit-test
    //! escape hatch. Production code MUST NOT call this.
    //!
    void SeedForTests(const Pool& entry);

    //!
    //! \brief Test-only: clear the in-memory map without touching LevelDB.
    //!
    void ClearForTests();

private:
    //!
    //! \brief Protects the registry with multithreaded access. This is implemented INTERNAL to
    //! the registry class, following pattern (b) of doc/contract_registry_locking_design.md:
    //! no public method carries a cs_lock annotation; read accessors take cs_lock internally
    //! and return self-contained values; chain handlers additionally require cs_main from the
    //! caller (canonical order cs_main -> cs_lock).
    //!
    mutable CCriticalSection cs_lock;

    PoolMap m_pool_entries GUARDED_BY(cs_lock);                //!< CPID -> current entry (any status).
    PendingPoolMap m_pending_pool_entries GUARDED_BY(cs_lock) {}; //!< Not used; satisfies template.
    std::set<Pool> m_expired_pool_entries GUARDED_BY(cs_lock) {}; //!< Not used; satisfies template.
    PoolMap m_pool_first_entries GUARDED_BY(cs_lock) {};       //!< Not used; satisfies template.

    PoolDB m_pool_db GUARDED_BY(cs_lock);

    //!
    //! \brief Extra shared_ptrs to the grandfathered builtin seed entries.
    //! Pins them against RegistryDB::passivate() — without this extra
    //! reference, the seeds would be reclaimed from m_pool_db.m_historical
    //! once the active map's pointer is replaced by a real contract, and
    //! find() would then miss because the seeds have no LevelDB backing.
    //! Also doubles as the membership oracle for IsBuiltin().
    //!
    std::map<Cpid, Pool_ptr> m_builtin_seeds GUARDED_BY(cs_lock);

    //!
    //! \brief Construct seed Pool entries for each row of BuiltinPoolSeeds(),
    //! install them into m_pool_db.m_historical via InstallInMemorySeed,
    //! pin them in m_builtin_seeds, and publish them into m_pool_entries
    //! at status ACTIVE. Called once from the constructor.
    //!
    void SeedBuiltinPools();

    //!
    //! \brief Apply a POOL_REGISTER contract (operator-self-signed).
    //!
    //! ADD: lands as PENDING (or refreshes operator-signed re-registration).
    //! REMOVE: operator self-withdrawal (validated against existing operator key).
    //!
    void ApplyRegister(const ContractContext& ctx);

    //!
    //! \brief Apply a POOL_APPROVE contract (master-key authority).
    //!
    //! ADD: flip the named CPID's entry to ACTIVE (must already exist).
    //! REMOVE: flip to DELETED.
    //! OPEN: pre-authorize an operator pubkey to claim a builtin slot
    //! (sets m_authorized_operator_key / m_authorization_height; status
    //! preserved from the prior chained entry).
    //!
    void ApplyApprove(const ContractContext& ctx);

    //!
    //! \brief Shared core for Validate (mempool admission, chain-tip
    //! height) and BlockValidate (block validation, block height). Runs:
    //!   1. V15 activation gate (finding P).
    //!   2. WellFormed for the payload type.
    //!   3. Registry-aware authentication for POOL_REGISTER (Issue 2's
    //!      key-selection rule + the IsBuiltin Path 1 / Path 2 guard
    //!      from findings B, K, M, and the non-builtin expired-PENDING
    //!      transparency from finding J).
    //!
    //! Factoring this out lets Validate (no block context, uses chain
    //! tip height under cs_main) and BlockValidate (block context, uses
    //! ctx.m_pindex->nHeight) share identical logic. The mempool view
    //! is best-effort; BlockValidate is authoritative.
    //!
    bool ValidateAtHeight(const Contract& contract, int at_height, int& DoS) const;

    //!
    //! \brief Compute and verify the authorization for a POOL_REGISTER
    //! against the current registry state at the given height. Sets
    //! DoS on rejection. Implements the IsBuiltin Path 1 / Path 2 split
    //! and the non-builtin expired-PENDING transparency.
    //!
    bool VerifyRegisterAuth(const PoolRegisterPayload& payload,
                            int at_height,
                            int& DoS) const;
};

//!
//! \brief Global pool registry accessor.
//!
PoolRegistry& GetPoolRegistry();

} // namespace GRC

#endif // GRIDCOIN_POOL_H
