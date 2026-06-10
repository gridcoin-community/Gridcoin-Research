// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "pool.h"

#include "chainparams.h"
#include "hash.h"
#include "logging.h"
#include "main.h"
#include "node/ui_interface.h"
#include "sync.h"

#include <cassert>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

namespace {

PoolRegistry g_pool_registry;

//!
//! \brief Hash the serialized payload bytes with the contract action neutralised,
//! matching the BeaconPayload convention. The signature field is excluded from
//! the hash via SER_GETHASH (see PoolRegisterPayload::SerializationOp), so the
//! operator can sign after constructing the payload without changing its hash.
//!
uint256 HashPoolRegisterPayload(const PoolRegisterPayload& payload)
{
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    payload.Serialize(hasher, ContractAction::UNKNOWN);
    return hasher.GetHash();
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

PoolRegistry& GRC::GetPoolRegistry()
{
    return g_pool_registry;
}

// -----------------------------------------------------------------------------
// Grandfathered builtin pools (issue #1783 / V15)
// -----------------------------------------------------------------------------
//
// The five entries below were the hardcoded MiningPools list (researcher.h:88-95)
// that drove pool-mode detection and the voting AVW calculation before the
// on-chain registry existed. PoolRegistry's constructor seeds them at boot so
// pre-V15 queries match g_mining_pools bit-for-bit. Keep this list in lockstep
// with researcher.h.
//
const std::vector<PoolRegistry::BuiltinSeed>& PoolRegistry::BuiltinPoolSeeds()
{
    static const std::vector<BuiltinSeed> seeds = {
        { "7d0d73fe026d66fd4ab8d5d8da32a611", "grcpool.com",       "https://grcpool.com/"     },
        { "a914eba952be5dfcf73d926b508fd5fa", "grcpool.com-2",     "https://grcpool.com/"     },
        { "163f049997e8a2dee054d69a7720bf05", "grcpool.com-3",     "https://grcpool.com/"     },
        { "f1f4d4e93b5b319b0a54b09dd47f1486", "grcpool.com-5",     "https://grcpool.com/"     },
        { "326bb50c0dd0ba9d46e15fae3484af35", "grc.arikado.pool",  "https://gridcoinpool.ru/" },
    };
    return seeds;
}

uint256 PoolRegistry::BuiltinSeedHash(const Cpid& cpid)
{
    const std::string preimage = "POOL_BUILTIN_SEED:" + cpid.ToString();
    return Hash(preimage);
}

bool PoolRegistry::IsBuiltin(const Cpid& cpid) const
{
    LOCK(cs_lock);
    return m_builtin_seeds.find(cpid) != m_builtin_seeds.end();
}

void PoolRegistry::SeedBuiltinPools()
{
    // The constructor call site runs single-threaded before any other
    // accessor can race, but cs_lock is a member (fully constructed by the
    // time this body runs), so taking it is safe there as well as on the
    // ClearForTests re-seed path — and it keeps the GUARDED_BY(cs_lock)
    // member accesses below analyzer-clean.
    LOCK(cs_lock);

    for (const BuiltinSeed& seed : BuiltinPoolSeeds()) {
        Cpid cpid = Cpid::Parse(seed.cpid_hex);

        // CPubKey{} is intentionally invalid: it marks the seed as "no
        // operator has claimed this yet." Combined with the IsBuiltin()
        // guard in BlockValidate, this prevents opportunistic POOL_REGISTER
        // claims; the foundation must POOL_APPROVE REMOVE first to open
        // the slot. See plan §3.5.
        Pool entry(cpid, seed.name, seed.url, CPubKey{});
        entry.m_timestamp = 0;
        entry.m_height = 0;
        entry.m_hash = BuiltinSeedHash(cpid);
        entry.m_previous_hash = uint256{};
        entry.m_status = PoolStatus::ACTIVE;

        // InstallInMemorySeed bypasses LevelDB so Initialize() won't
        // re-load these on the next startup (the constructor re-seeds
        // every boot). Returns the shared_ptr so we can pin it against
        // passivate() reclaiming the historical entry.
        Pool_ptr seed_ptr = m_pool_db.InstallInMemorySeed(entry.m_hash, entry);

        m_builtin_seeds[cpid] = seed_ptr;
        m_pool_entries[cpid] = seed_ptr;
    }
}

// -----------------------------------------------------------------------------
// Class: Pool
// -----------------------------------------------------------------------------

Pool::Pool()
    : m_cpid()
    , m_name()
    , m_url()
    , m_operator_key()
    , m_timestamp(0)
    , m_hash()
    , m_previous_hash()
    , m_height(0)
    , m_status(PoolStatus::UNKNOWN)
    , m_authorized_operator_key()
    , m_authorization_height(-1)
{
}

Pool::Pool(Cpid cpid, std::string name, std::string url, CPubKey operator_key)
    : m_cpid(cpid)
    , m_name(std::move(name))
    , m_url(std::move(url))
    , m_operator_key(std::move(operator_key))
    , m_timestamp(0)
    , m_hash()
    , m_previous_hash()
    , m_height(0)
    , m_status(PoolStatus::UNKNOWN)
    , m_authorized_operator_key()
    , m_authorization_height(-1)
{
}

bool Pool::WellFormed() const
{
    return !m_name.empty() && !m_url.empty() && m_operator_key.IsValid();
}

std::pair<std::string, std::string> Pool::KeyValueToString() const
{
    return std::make_pair(m_cpid.ToString(), m_name + " (" + m_url + ")");
}

std::string Pool::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string Pool::StatusToString(const PoolStatus& status, const bool& translated) const
{
    if (translated) {
        switch (status) {
        case PoolStatus::UNKNOWN:      return _("Unknown");
        case PoolStatus::PENDING:      return _("Pending");
        case PoolStatus::ACTIVE:       return _("Active");
        case PoolStatus::DELETED:      return _("Deleted");
        case PoolStatus::OUT_OF_BOUND: break;
        }
    } else {
        switch (status) {
        case PoolStatus::UNKNOWN:      return "Unknown";
        case PoolStatus::PENDING:      return "Pending";
        case PoolStatus::ACTIVE:       return "Active";
        case PoolStatus::DELETED:      return "Deleted";
        case PoolStatus::OUT_OF_BOUND: break;
        }
    }

    assert(false); // Suppress warning
    return std::string{};
}

// -----------------------------------------------------------------------------
// Class: PoolRegisterPayload
// -----------------------------------------------------------------------------

PoolRegisterPayload::PoolRegisterPayload()
    : m_cpid()
    , m_name()
    , m_url()
    , m_operator_key()
    , m_signature()
{
}

PoolRegisterPayload::PoolRegisterPayload(Cpid cpid, std::string name, std::string url, CPubKey operator_key)
    : m_cpid(cpid)
    , m_name(std::move(name))
    , m_url(std::move(url))
    , m_operator_key(std::move(operator_key))
    , m_signature()
{
}

bool PoolRegisterPayload::WellFormed(const ContractAction action) const
{
    if (m_version <= 0 || m_version > CURRENT_VERSION) {
        return false;
    }

    if (m_name.empty() || m_name.size() > MAX_NAME_SIZE) {
        return false;
    }

    // Operator must always supply a usable signing key, even on REMOVE (which
    // is an operator-self-withdrawal).
    if (!m_operator_key.IsFullyValid()) {
        return false;
    }

    if (action == ContractAction::ADD) {
        if (m_url.empty() || m_url.size() > MAX_URL_SIZE) {
            return false;
        }
    }

    // DER-encoded ECDSA signatures are typically 70-71 bytes but the framework
    // accepts the same 64-73 byte band that BeaconPayload allows.
    if (m_signature.size() < 64 || m_signature.size() > 73) {
        return false;
    }

    return true;
}

std::string PoolRegisterPayload::LegacyKeyString() const
{
    return m_cpid.ToString();
}

std::string PoolRegisterPayload::LegacyValueString() const
{
    return m_name + "|" + m_url;
}

bool PoolRegisterPayload::Sign(CKey& operator_private_key)
{
    if (!operator_private_key.Sign(HashPoolRegisterPayload(*this), m_signature)) {
        m_signature.clear();
        return false;
    }
    return true;
}

bool PoolRegisterPayload::VerifySignature(const CPubKey& expected_key) const
{
    if (!expected_key.IsValid()) {
        return false;
    }
    return expected_key.Verify(HashPoolRegisterPayload(*this), m_signature);
}

// -----------------------------------------------------------------------------
// Class: PoolApprovePayload
// -----------------------------------------------------------------------------

PoolApprovePayload::PoolApprovePayload()
    : m_cpid()
    , m_authorized_operator_key()
{
}

PoolApprovePayload::PoolApprovePayload(Cpid cpid)
    : m_cpid(cpid)
    , m_authorized_operator_key()
{
}

PoolApprovePayload::PoolApprovePayload(Cpid cpid, CPubKey authorized_operator_key)
    : m_cpid(cpid)
    , m_authorized_operator_key(std::move(authorized_operator_key))
{
}

bool PoolApprovePayload::WellFormed(const ContractAction action) const
{
    if (m_version <= 0 || m_version > CURRENT_VERSION) {
        return false;
    }

    switch (action) {
    case ContractAction::ADD:
    case ContractAction::REMOVE:
        // The authorized-operator-key field is only meaningful for OPEN;
        // ADD/REMOVE wire-shape is (version, cpid) and the field is not
        // serialized for those actions (see SerializationOp in pool.h).
        return true;

    case ContractAction::OPEN:
        // OPEN pre-authorizes a specific operator pubkey to claim a
        // builtin pool slot. The auth key MUST be valid — a default-
        // constructed/invalid key would silently do nothing (the
        // IsBuiltin guard's auth.IsValid() check would reject every
        // subsequent claim attempt), so reject at the WellFormed gate
        // for fail-fast clarity (finding N).
        return m_authorized_operator_key.IsValid();

    default:
        return false;
    }
}

std::string PoolApprovePayload::LegacyKeyString() const
{
    return m_cpid.ToString();
}

// -----------------------------------------------------------------------------
// Class: PoolRegistry
// -----------------------------------------------------------------------------

std::vector<Pool> PoolRegistry::Entries() const
{
    LOCK(cs_lock);

    std::vector<Pool> out;
    out.reserve(m_pool_entries.size());

    for (const auto& iter : m_pool_entries) {
        out.push_back(*iter.second);
    }

    return out;
}

std::vector<Pool> PoolRegistry::ActivePools() const
{
    LOCK(cs_lock);

    std::vector<Pool> out;
    out.reserve(m_pool_entries.size());

    for (const auto& iter : m_pool_entries) {
        if (iter.second->m_status == PoolStatus::ACTIVE) {
            out.push_back(*iter.second);
        }
    }

    return out;
}

std::vector<Pool> PoolRegistry::ActivePoolsAtHeight(int height) const
{
    LOCK(cs_lock);

    std::vector<Pool> out;
    out.reserve(m_pool_entries.size());

    // For each CPID currently tracked, walk back through the m_previous_hash
    // chain in the historical DB until reaching an entry produced at or
    // before `height`. That entry is the registry's view of this CPID as of
    // `height` — include it if it was ACTIVE at the time.
    //
    // We need a non-const reference to m_pool_db because find() may
    // LevelDB-fallback-load a passivated entry, mutating m_historical.
    // The const_cast is local to this read-only API (the registry's
    // logical state is unchanged — the load just rehydrates a cache).
    auto& pool_db = const_cast<PoolDB&>(m_pool_db);

    for (const auto& iter : m_pool_entries) {
        Pool_ptr current = iter.second;

        while (current && current->m_height > height) {
            if (current->m_previous_hash.IsNull()) {
                // Walked back past genesis for this CPID with no entry
                // satisfying m_height <= height — this CPID didn't exist
                // in the registry as of `height`.
                current.reset();
                break;
            }
            auto prior = pool_db.find(current->m_previous_hash);
            if (prior == pool_db.end()) {
                LogPrint(LogFlags::CONTRACT, "WARN: %s: prior pool entry %s missing during "
                                             "height walk for cpid %s",
                         __func__, current->m_previous_hash.GetHex(),
                         current->m_cpid.ToString());
                current.reset();
                break;
            }
            current = prior->second;
        }

        if (current && current->m_status == PoolStatus::ACTIVE) {
            out.push_back(*current);
        }
    }

    return out;
}

Pool_ptr PoolRegistry::Try(const Cpid& cpid) const
{
    LOCK(cs_lock);

    auto iter = m_pool_entries.find(cpid);
    if (iter == m_pool_entries.end()) {
        return nullptr;
    }
    return iter->second;
}

bool PoolRegistry::IsActivePool(const Cpid& cpid) const
{
    LOCK(cs_lock);

    auto iter = m_pool_entries.find(cpid);
    if (iter == m_pool_entries.end()) {
        return false;
    }
    return iter->second->m_status == PoolStatus::ACTIVE;
}

bool PoolRegistry::IsActivePoolName(const std::string& name) const
{
    LOCK(cs_lock);

    for (const auto& iter : m_pool_entries) {
        if (iter.second->m_status == PoolStatus::ACTIVE && iter.second->m_name == name) {
            return true;
        }
    }
    return false;
}

void PoolRegistry::Reset() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    LOCK(cs_lock);
    m_pool_entries.clear();
    m_pool_db.clear();
}

bool PoolRegistry::IsPendingExpired(const Pool& entry, int at_height)
{
    if (entry.m_status != PoolStatus::PENDING) {
        return false;
    }
    if (entry.m_height < 0) {
        return false;
    }
    return at_height > entry.m_height + GetPendingPoolRetention();
}

bool PoolRegistry::IsAuthorizationExpired(const Pool& entry, int at_height)
{
    if (!entry.m_authorized_operator_key.IsValid()) {
        return false;
    }
    if (entry.m_authorization_height < 0) {
        return false;
    }
    return at_height > entry.m_authorization_height + GetPendingPoolRetention();
}

bool PoolRegistry::VerifyRegisterAuth(const PoolRegisterPayload& payload,
                                      int at_height,
                                      int& DoS) const
{
    Pool_ptr existing;
    bool existing_is_builtin = false;
    {
        LOCK(cs_lock);
        auto iter = m_pool_entries.find(payload.m_cpid);
        if (iter != m_pool_entries.end()) {
            existing = iter->second;
            existing_is_builtin = (m_builtin_seeds.find(payload.m_cpid) != m_builtin_seeds.end());
        }
    }

    // Findings B + K + M: IsBuiltin guard runs first, with status-driven
    // Path 1 / Path 2 split. Every builtin always has a chained entry
    // (constructor-seeded), so `existing` is non-null whenever
    // `existing_is_builtin` is true.
    if (existing && existing_is_builtin) {
        const bool path1_claimed_operational =
            existing->m_status != PoolStatus::DELETED
            && existing->m_operator_key.IsValid();

        if (!path1_claimed_operational) {
            // Path 2: builtin is unclaimed (fresh seed), post-operator-
            // self-REMOVE, or post-Foundation-REMOVE-of-claimed-builtin.
            // Builtin-ness is permanent — the slot always requires a
            // fresh Foundation OPEN authorization, regardless of whether
            // it was ever claimed before (finding K).
            if (!existing->m_authorized_operator_key.IsValid()) {
                DoS = 25;
                LogPrint(LogFlags::CONTRACT, "ERROR: %s: POOL_REGISTER on builtin cpid %s rejected; "
                                             "Foundation must POOL_APPROVE OPEN authorizing the "
                                             "operator pubkey first",
                         __func__, payload.m_cpid.ToString());
                return false;
            }
            if (IsAuthorizationExpired(*existing, at_height)) {
                DoS = 25;
                LogPrint(LogFlags::CONTRACT, "ERROR: %s: POOL_REGISTER on builtin cpid %s rejected; "
                                             "OPEN authorization expired (issued at height %d, "
                                             "retention %d blocks)",
                         __func__, payload.m_cpid.ToString(),
                         existing->m_authorization_height, GetPendingPoolRetention());
                return false;
            }
            if (existing->m_authorized_operator_key != payload.m_operator_key) {
                DoS = 25;
                LogPrint(LogFlags::CONTRACT, "ERROR: %s: POOL_REGISTER on builtin cpid %s rejected; "
                                             "payload operator key does not match Foundation "
                                             "pre-authorization",
                         __func__, payload.m_cpid.ToString());
                return false;
            }
            // Auth matches. Authenticate against the payload's own key —
            // builtin claim is structurally a first-claim against an
            // entry whose m_operator_key is empty/invalid.
            if (!payload.VerifySignature(payload.m_operator_key)) {
                DoS = 25;
                LogPrint(LogFlags::CONTRACT, "ERROR: %s: bad operator signature on POOL_REGISTER "
                                             "for builtin cpid %s (Path 2 claim)",
                         __func__, payload.m_cpid.ToString());
                return false;
            }
            return true;
        }
        // Path 1: fall through to the standard takeover check below. Note
        // that for builtin Path 1 we deliberately do NOT apply the
        // expired-PENDING transparency below — once a builtin is claimed,
        // only the operator can rotate/withdraw, regardless of retention.
    }

    // Standard takeover check. Filter `existing` by the != DELETED
    // status (rotation / withdrawal targets a live entry). For
    // non-builtins additionally treat expired-PENDING as absent
    // (finding J), so a stale PENDING that Foundation never ratified
    // doesn't permanently lock the slot.
    Pool_ptr existing_for_takeover = existing;
    if (existing_for_takeover && existing_for_takeover->m_status == PoolStatus::DELETED) {
        existing_for_takeover.reset();
    }
    if (existing_for_takeover
        && !existing_is_builtin
        && IsPendingExpired(*existing_for_takeover, at_height))
    {
        existing_for_takeover.reset();
    }

    if (existing_for_takeover && existing_for_takeover->m_operator_key.IsValid()) {
        if (!payload.VerifySignature(existing_for_takeover->m_operator_key)) {
            DoS = 25;
            LogPrint(LogFlags::CONTRACT, "ERROR: %s: POOL_REGISTER on existing cpid %s "
                                         "did not match the prior operator key",
                     __func__, payload.m_cpid.ToString());
            return false;
        }
        return true;
    }

    // First-claim path: no existing valid key (or expired PENDING /
    // DELETED treated as absent above). Authenticate against the
    // payload's own key.
    if (!payload.VerifySignature(payload.m_operator_key)) {
        DoS = 25;
        LogPrint(LogFlags::CONTRACT, "ERROR: %s: bad operator signature on POOL_REGISTER",
                 __func__);
        return false;
    }
    return true;
}

bool PoolRegistry::ValidateAtHeight(const Contract& contract, int at_height, int& DoS) const
{
    // Finding P: V15 activation gate. Pre-V15 POOL contracts can never
    // pass BlockValidate, so reject at this shared validation entry too
    // (saves mempool admission / propagation cost). Same shape as the
    // existing gate at BlockValidate's top.
    if (!IsV15Enabled(at_height)) {
        DoS = 0;
        return false;
    }

    const auto type = contract.m_type.Value();

    if (type == ContractType::POOL_REGISTER) {
        const auto payload = contract.SharePayloadAs<PoolRegisterPayload>();

        if (!payload->WellFormed(contract.m_action.Value())) {
            DoS = 25;
            LogPrint(LogFlags::CONTRACT, "ERROR: %s: malformed POOL_REGISTER payload", __func__);
            return false;
        }

        return VerifyRegisterAuth(*payload, at_height, DoS);
    }

    if (type == ContractType::POOL_APPROVE) {
        const auto payload = contract.SharePayloadAs<PoolApprovePayload>();

        if (!payload->WellFormed(contract.m_action.Value())) {
            DoS = 25;
            LogPrint(LogFlags::CONTRACT, "ERROR: %s: malformed POOL_APPROVE payload", __func__);
            return false;
        }

        // Master-key authority is enforced upstream in CheckContracts via
        // HasMasterKeyInput; nothing to verify here beyond well-formedness.
        return true;
    }

    DoS = 100;
    LogPrint(LogFlags::CONTRACT, "ERROR: %s: PoolRegistry received unexpected contract type",
             __func__);
    return false;
}

bool PoolRegistry::Validate(const Contract& contract, const CTransaction& tx, int& DoS) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // The contract dispatch path (GRC::ValidateContracts -> Dispatcher::
    // Validate) is annotated EXCLUSIVE_LOCKS_REQUIRED(cs_main), so the
    // chain-tip height read is already under the canonical lock; cs_lock
    // acquisition inside ValidateAtHeight then honours the cs_main ->
    // cs_lock lock order.
    return ValidateAtHeight(contract, nBestHeight, DoS);
}

bool PoolRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const
    EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // The V15 activation gate and registry-aware authentication both live
    // in the shared ValidateAtHeight helper. BlockValidate is the
    // authoritative caller — it passes the actual block height
    // (ctx.m_pindex->nHeight), which is deterministic for consensus. The
    // mempool path (Validate) calls the same helper with a chain-tip
    // height snapshot as a best-effort approximation.
    //
    // Authentication shape (issue #1783, plan §3 / §3.5):
    //   * POOL_REGISTER on an unclaimed builtin → requires fresh
    //     POOL_APPROVE OPEN authorization (sticky IsBuiltin Path 2).
    //   * POOL_REGISTER on a claimed-operational builtin or on a
    //     non-builtin with a valid existing operator key → signature
    //     must match the existing key (rotation / withdrawal /
    //     legitimate takeover defense; applies to BOTH ADD and REMOVE).
    //   * POOL_REGISTER on a non-builtin with no existing key (or with
    //     an expired-PENDING entry, treated as absent) → first-claim
    //     path; signature against the payload's own key.
    return ValidateAtHeight(ctx.m_contract, ctx.m_pindex->nHeight, DoS);
}

void PoolRegistry::Add(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    switch (ctx->m_type.Value()) {
        case ContractType::POOL_REGISTER:
            ApplyRegister(ctx);
            return;
        case ContractType::POOL_APPROVE:
            ApplyApprove(ctx);
            return;
        default:
            LogPrint(LogFlags::CONTRACT, "ERROR: %s: PoolRegistry::Add called with non-pool type",
                     __func__);
            return;
    }
}

void PoolRegistry::Delete(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    // Symmetric to Add; the action on the contract is REMOVE and the per-type
    // helpers branch on it internally.
    switch (ctx->m_type.Value()) {
        case ContractType::POOL_REGISTER:
            ApplyRegister(ctx);
            return;
        case ContractType::POOL_APPROVE:
            ApplyApprove(ctx);
            return;
        default:
            LogPrint(LogFlags::CONTRACT, "ERROR: %s: PoolRegistry::Delete called with non-pool type",
                     __func__);
            return;
    }
}

void PoolRegistry::ApplyRegister(const ContractContext& ctx)
{
    int height = -1;
    if (ctx.m_pindex) {
        height = ctx.m_pindex->nHeight;
    }

    PoolRegisterPayload payload = ctx->CopyPayloadAs<PoolRegisterPayload>();

    LOCK(cs_lock);

    Pool entry(payload.m_cpid, payload.m_name, payload.m_url, payload.m_operator_key);
    entry.m_timestamp = ctx.m_tx.nTime;
    entry.m_hash = ctx.m_tx.GetHash();
    entry.m_height = height;

    auto existing_iter = m_pool_entries.find(payload.m_cpid);
    bool existing_present = (existing_iter != m_pool_entries.end());

    if (existing_present) {
        entry.m_previous_hash = existing_iter->second->m_hash;
    }

    if (ctx->m_action == ContractAction::REMOVE) {
        entry.m_status = PoolStatus::DELETED;
    } else {
        entry.m_status = PoolStatus::PENDING;
    }

    LogPrint(LogFlags::CONTRACT, "INFO: %s: POOL_REGISTER %s on cpid %s -> %s, hash %s, previous_hash %s",
             __func__,
             ctx->m_action == ContractAction::REMOVE ? "REMOVE" : "ADD",
             payload.m_cpid.ToString(),
             entry.StatusToString(),
             entry.m_hash.GetHex(),
             entry.m_previous_hash.GetHex());

    if (!m_pool_db.insert(ctx.m_tx.GetHash(), height, entry)) {
        LogPrint(LogFlags::CONTRACT, "WARN: %s: pool db insert reported duplicate for hash %s",
                 __func__, ctx.m_tx.GetHash().GetHex());
    }

    m_pool_entries[payload.m_cpid] = m_pool_db.find(ctx.m_tx.GetHash())->second;
}

void PoolRegistry::ApplyApprove(const ContractContext& ctx)
{
    int height = -1;
    if (ctx.m_pindex) {
        height = ctx.m_pindex->nHeight;
    }

    PoolApprovePayload payload = ctx->CopyPayloadAs<PoolApprovePayload>();
    const ContractAction action = ctx->m_action.Value();

    LOCK(cs_lock);

    auto existing_iter = m_pool_entries.find(payload.m_cpid);
    if (existing_iter == m_pool_entries.end()) {
        // POOL_APPROVE on a CPID with no prior POOL_REGISTER. We accept this
        // so the foundation can pre-stage the migration entries for issue
        // #1783 without forcing an operator submission first, but the entry's
        // descriptive fields will be empty until an operator follows up.
        //
        // OPEN on an unknown CPID is admissible too — it records the
        // authorized pubkey on a placeholder entry. For non-builtins this
        // is a no-op for the takeover flow (non-builtin claims go through
        // the standard first-claim path, not the IsBuiltin guard); the
        // OPEN authorization just sits unused. Documented as finding E.
        LogPrint(LogFlags::CONTRACT, "INFO: %s: POOL_APPROVE references unknown cpid %s; "
                                     "creating empty entry",
                 __func__, payload.m_cpid.ToString());

        Pool placeholder(payload.m_cpid, std::string{}, std::string{}, CPubKey{});
        placeholder.m_timestamp = ctx.m_tx.nTime;
        placeholder.m_hash = ctx.m_tx.GetHash();
        placeholder.m_height = height;

        switch (action) {
        case ContractAction::ADD:
            placeholder.m_status = PoolStatus::ACTIVE;
            break;
        case ContractAction::REMOVE:
            placeholder.m_status = PoolStatus::DELETED;
            break;
        case ContractAction::OPEN:
            placeholder.m_status = PoolStatus::DELETED;
            placeholder.m_authorized_operator_key = payload.m_authorized_operator_key;
            placeholder.m_authorization_height = height;
            break;
        default:
            return;
        }

        if (!m_pool_db.insert(ctx.m_tx.GetHash(), height, placeholder)) {
            LogPrint(LogFlags::CONTRACT, "WARN: %s: pool db insert reported duplicate for hash %s",
                     __func__, ctx.m_tx.GetHash().GetHex());
        }

        m_pool_entries[payload.m_cpid] = m_pool_db.find(ctx.m_tx.GetHash())->second;
        return;
    }

    // Copy from the existing chained entry so previous-state fields
    // propagate by default (m_authorized_operator_key, m_authorization_height,
    // m_operator_key, descriptive fields). OPEN overrides the auth fields;
    // ADD/REMOVE only touch status. Auth-field propagation is finding C.
    Pool flipped = *existing_iter->second;
    flipped.m_timestamp = ctx.m_tx.nTime;
    flipped.m_previous_hash = existing_iter->second->m_hash;
    flipped.m_hash = ctx.m_tx.GetHash();
    flipped.m_height = height;

    switch (action) {
    case ContractAction::ADD:
        flipped.m_status = PoolStatus::ACTIVE;
        break;
    case ContractAction::REMOVE:
        flipped.m_status = PoolStatus::DELETED;
        break;
    case ContractAction::OPEN:
        // Status preserved from prior entry — supports both the 3-step
        // (OPEN on still-ACTIVE seed) and 4-step (Foundation REMOVE then
        // OPEN, status stays DELETED) claim flows. See doc/consensus.md §11.
        flipped.m_authorized_operator_key = payload.m_authorized_operator_key;
        flipped.m_authorization_height = height;
        break;
    default:
        return;
    }

    LogPrint(LogFlags::CONTRACT, "INFO: %s: POOL_APPROVE %s on cpid %s -> %s",
             __func__,
             Contract::Action(action).ToString(),
             payload.m_cpid.ToString(),
             flipped.StatusToString());

    if (!m_pool_db.insert(ctx.m_tx.GetHash(), height, flipped)) {
        LogPrint(LogFlags::CONTRACT, "WARN: %s: pool db insert reported duplicate for hash %s",
                 __func__, ctx.m_tx.GetHash().GetHex());
    }

    m_pool_entries[payload.m_cpid] = m_pool_db.find(ctx.m_tx.GetHash())->second;
}

void PoolRegistry::Revert(const ContractContext& ctx) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    LOCK(cs_lock);

    // Both POOL_REGISTER and POOL_APPROVE produce a single history record per
    // contract context, so reversion walks m_previous_hash and either restores
    // the prior entry or erases the CPID outright.
    Cpid cpid;
    switch (ctx->m_type.Value()) {
        case ContractType::POOL_REGISTER:
            cpid = ctx->SharePayloadAs<PoolRegisterPayload>()->m_cpid;
            break;
        case ContractType::POOL_APPROVE:
            cpid = ctx->SharePayloadAs<PoolApprovePayload>()->m_cpid;
            break;
        default:
            LogPrint(LogFlags::CONTRACT, "ERROR: %s: PoolRegistry::Revert called with non-pool type",
                     __func__);
            return;
    }

    auto iter_current = m_pool_entries.find(cpid);
    if (iter_current == m_pool_entries.end()) {
        LogPrint(LogFlags::CONTRACT, "ERROR: %s: pool entry for cpid %s not found during revert",
                 __func__, cpid.ToString());
        return;
    }

    uint256 prior_hash = iter_current->second->m_previous_hash;
    m_pool_entries.erase(iter_current);

    if (!m_pool_db.erase(ctx.m_tx.GetHash())) {
        LogPrint(LogFlags::CONTRACT, "WARN: %s: pool db erase reported missing record for hash %s",
                 __func__, ctx.m_tx.GetHash().GetHex());
    }

    if (prior_hash.IsNull()) {
        // First contract for this CPID; nothing to resurrect.
        return;
    }

    auto prior_iter = m_pool_db.find(prior_hash);
    if (prior_iter == m_pool_db.end()) {
        LogPrint(LogFlags::CONTRACT, "ERROR: %s: prior pool entry %s missing during revert of cpid %s",
                 __func__, prior_hash.GetHex(), cpid.ToString());
        return;
    }

    m_pool_entries[prior_iter->second->m_cpid] = prior_iter->second;
}

int PoolRegistry::Initialize()
{
    LOCK(cs_lock);

    int height = m_pool_db.Initialize(m_pool_entries,
                                      m_pending_pool_entries,
                                      m_expired_pool_entries,
                                      m_pool_first_entries,
                                      false);

    LogPrint(LogFlags::CONTRACT, "INFO: %s: m_pool_db size after load: %u; m_pool_entries size: %u",
             __func__, m_pool_db.size(), m_pool_entries.size());

    return height;
}

int PoolRegistry::GetDBHeight()
{
    int height = 0;
    LOCK(cs_lock);
    m_pool_db.LoadDBHeight(height);
    return height;
}

void PoolRegistry::SetDBHeight(int& height)
{
    LOCK(cs_lock);
    m_pool_db.StoreDBHeight(height);
}

uint64_t PoolRegistry::PassivateDB()
{
    LOCK(cs_lock);
    return m_pool_db.passivate_db();
}

void PoolRegistry::SeedForTests(const Pool& entry)
{
    LOCK(cs_lock);
    m_pool_entries[entry.m_cpid] = std::make_shared<Pool>(entry);
}

void PoolRegistry::ClearForTests()
{
    {
        LOCK(cs_lock);
        m_pool_entries.clear();
        m_builtin_seeds.clear();
    }

    // Restore the default boot state — the grandfathered builtins must be
    // present after a "reset" so that subsequent non-fixture tests see the
    // same registry shape they'd see in production.
    SeedBuiltinPools();
}

// -----------------------------------------------------------------------------
// RegistryDB template KeyType() specialization
// -----------------------------------------------------------------------------

template<> const std::string PoolRegistry::PoolDB::KeyType()
{
    return std::string("Pool");
}
