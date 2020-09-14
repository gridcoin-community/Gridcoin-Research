#include "base58.h"
#include "logging.h"
#include "main.h"
#include "neuralnet/beacon.h"
#include "neuralnet/contract/contract.h"
#include "util.h"
#include "wallet/wallet.h"

#include <algorithm>

using namespace NN;
using LogFlags = BCLog::LogFlags;

extern int64_t g_v11_timestamp;
extern int64_t g_v11_legacy_beacon_days;

namespace {
BeaconRegistry g_beacons;

//!
//! \brief Compute the hash of a beacon payload object.
//!
//! \param payload The beacon payload object to hash.
//!
uint256 HashBeaconPayload(const BeaconPayload& payload)
{
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);

    // Ignore the contract action and hash the whole object:
    payload.Serialize(hasher, NN::ContractAction::UNKNOWN);

    return hasher.GetHash();
}

//!
//! \brief Attempt to renew an existing beacon from a contract.
//!
//! \param beacons The set of active beacons to find a match from.
//! \param payload Beacon contract message from a transaction.
//!
//! \return \c true if the supplied beacon contract matches an active beacon.
//! This updates the matched beacon with a new timestamp.
//!
bool TryRenewal(BeaconRegistry::BeaconMap& beacons, const BeaconPayload& payload)
{
    auto beacon_pair_iter = beacons.find(payload.m_cpid);

    if (beacon_pair_iter == beacons.end()) {
        return false;
    }

    Beacon& current_beacon = beacon_pair_iter->second;

    if (current_beacon.Expired(payload.m_beacon.m_timestamp)) {
        return false;
    }

    if (current_beacon.m_public_key != payload.m_beacon.m_public_key) {
        return false;
    }

    current_beacon.m_timestamp = payload.m_beacon.m_timestamp;

    return true;
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

BeaconRegistry& NN::GetBeaconRegistry()
{
    return g_beacons;
}

// -----------------------------------------------------------------------------
// Class: Beacon
// -----------------------------------------------------------------------------

Beacon::Beacon() : m_public_key(), m_timestamp(0)
{
}

Beacon::Beacon(CPubKey public_key)
    : Beacon(std::move(public_key), 0)
{
}

Beacon::Beacon(CPubKey public_key, int64_t timestamp)
    : m_public_key(std::move(public_key))
    , m_timestamp(timestamp)
{
}

Beacon Beacon::Parse(const std::string& value)
{
    // The legacy beacon string format looks like this:
    //
    //     BASE64(HEX(CPIDV2);HEX(RANDOM_HASH);BASE58(ADDRESS);HEX(PUBLIC_KEY))
    //
    // We ignore the first three legacy fields (CPID "v2" followed by 32 random
    // bytes followed by the beacon owner's default wallet address) and extract
    // the just beacon's public key.
    //
    const std::string decoded = DecodeBase64(value);

    // If the beacon data does not contain a valid public key, the beacon is
    // invalid:
    //
    const std::size_t key_start = decoded.rfind(';');

    if (key_start == std::string::npos) {
        return Beacon();
    }

    CPubKey public_key(ParseHex(decoded.substr(key_start + 1)));

    if (!public_key.IsValid()) {
        return Beacon();
    }

    return Beacon(std::move(public_key));
}

bool Beacon::WellFormed() const
{
    return m_public_key.IsValid();
}

int64_t Beacon::Age(const int64_t now) const
{
    return now - m_timestamp;
}

bool Beacon::Expired(const int64_t now) const
{
    if (Age(now) > MAX_AGE) {
        return true;
    }

    // Temporary transition to version 2 beacons after the block version 11
    // hard-fork:
    //
    if (m_timestamp <= g_v11_timestamp) {
        return now - g_v11_timestamp > g_v11_legacy_beacon_days * 86400;
    }

    return false;
}

bool Beacon::Renewable(const int64_t now) const
{
    return Age(now) > RENEWAL_AGE;
}

CKeyID Beacon::GetId() const
{
    return m_public_key.GetID();
}

CBitcoinAddress Beacon::GetAddress() const
{
    return CBitcoinAddress(CTxDestination(m_public_key.GetID()));
}

std::string Beacon::GetVerificationCode() const
{
    const CKeyID key_id = GetId();

    return EncodeBase58(key_id.begin(), key_id.end());
}

bool Beacon::WalletHasPrivateKey(const CWallet* const wallet) const
{
    return wallet->HaveKey(m_public_key.GetID());
}

std::string Beacon::ToString() const
{
    return EncodeBase64(
        "0;0;"  // Unused: [CPIDv2];[nonce];
        + GetAddress().ToString()
        + ";"
        + m_public_key.ToString());
}

// -----------------------------------------------------------------------------
// Class: BeaconPayload
// -----------------------------------------------------------------------------

constexpr uint32_t BeaconPayload::CURRENT_VERSION; // For clang

BeaconPayload::BeaconPayload()
{
}

BeaconPayload::BeaconPayload(const uint32_t version, const Cpid cpid, Beacon beacon)
    : m_version(version)
    , m_cpid(cpid)
    , m_beacon(std::move(beacon))
{
}

BeaconPayload::BeaconPayload(const Cpid cpid, Beacon beacon)
    : BeaconPayload(CURRENT_VERSION, cpid, std::move(beacon))
{
}

BeaconPayload BeaconPayload::Parse(const std::string& key, const std::string& value)
{
    const CpidOption cpid = MiningId::Parse(key).TryCpid();

    if (!cpid) {
        return BeaconPayload();
    }

    // Legacy beacon payloads always parse to version 1:
    return BeaconPayload(1, *cpid, Beacon::Parse(value));
}

bool BeaconPayload::Sign(CKey& private_key)
{
    if (!private_key.Sign(HashBeaconPayload(*this), m_signature)) {
        m_signature.clear();
        return false;
    }

    return true;
}

bool BeaconPayload::VerifySignature() const
{
    CKey key;

    if (!key.SetPubKey(m_beacon.m_public_key)) {
        return false;
    }

    return key.Verify(HashBeaconPayload(*this), m_signature);
}

// -----------------------------------------------------------------------------
// Class: PendingBeacon
// -----------------------------------------------------------------------------

PendingBeacon::PendingBeacon(const Cpid cpid, Beacon beacon)
    : Beacon(std::move(beacon))
    , m_cpid(cpid)
{
}

bool PendingBeacon::Expired(const int64_t now) const
{
    return Age(now) > RETENTION_AGE;
}

// -----------------------------------------------------------------------------
// Class: BeaconRegistry
// -----------------------------------------------------------------------------

const BeaconRegistry::BeaconMap& BeaconRegistry::Beacons() const
{
    return m_beacons;
}

const BeaconRegistry::PendingBeaconMap& BeaconRegistry::PendingBeacons() const
{
    return m_pending;
}

BeaconOption BeaconRegistry::Try(const Cpid& cpid) const
{
    const auto iter = m_beacons.find(cpid);

    if (iter == m_beacons.end()) {
        return nullptr;
    }

    return &iter->second;
}

BeaconOption BeaconRegistry::TryActive(const Cpid& cpid, const int64_t now) const
{
    if (const BeaconOption beacon = Try(cpid)) {
        if (!beacon->Expired(now)) {
            return beacon;
        }
    }

    return nullptr;
}

std::vector<const PendingBeacon*> BeaconRegistry::FindPending(const Cpid cpid) const
{
    // TODO: consider adding a lookup table for pending beacons keyed by CPID.
    // Since the protocol just needs to look up pending beacons by public key,
    // we just do a search here. Informational RPCs or local beacon management
    // only need to call this occasionally.

    std::vector<const PendingBeacon*> found;

    for (const auto& pending_beacon_pair : m_pending) {
        const PendingBeacon& beacon = pending_beacon_pair.second;

        if (beacon.m_cpid == cpid) {
            found.emplace_back(&beacon);
        }
    }

    return found;
}

bool BeaconRegistry::ContainsActive(const Cpid& cpid, const int64_t now) const
{
    if (const BeaconOption beacon = Try(cpid)) {
        return !beacon->Expired(now);
    }

    return false;
}

bool BeaconRegistry::ContainsActive(const Cpid& cpid) const
{
    return ContainsActive(cpid, GetAdjustedTime());
}

void BeaconRegistry::Reset()
{
    m_beacons.clear();
    m_pending.clear();
}

void BeaconRegistry::Add(const ContractContext& ctx)
{
    BeaconPayload payload = ctx->CopyPayloadAs<BeaconPayload>();
    payload.m_beacon.m_timestamp = ctx.m_tx.nTime;

    // Legacy beacon contracts before block version 11--just load the beacon:
    //
    if (ctx->m_version == 1) {
        m_beacons[payload.m_cpid] = std::move(payload.m_beacon);
        return;
    }

    // For beacon renewals, check that the new beacon contains the same public
    // key. If it matches, we don't need to verify it again:
    //
    if (TryRenewal(m_beacons, payload)) {
        return;
    }

    // Otherwise, set the new beacon aside for scraper verification. The next
    // superblock will activate it if it matches a BOINC account:
    //
    PendingBeacon pending(payload.m_cpid, std::move(payload.m_beacon));

    m_pending.emplace(pending.GetId(), std::move(pending));
}

void BeaconRegistry::Delete(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<BeaconPayload>();

    if (ctx->m_version >= 2) {
        m_pending.erase(payload->m_beacon.GetId());
    }

    m_beacons.erase(payload->m_cpid);
}

bool BeaconRegistry::Validate(const Contract& contract, const CTransaction& tx) const
{
    // For legacy beacons, check that the unused parts contain non-empty values
    // for compatibility with the existing protocol to prevent a fork.
    //
    if (contract.m_version <= 1) {
        // Only administrative contracts can delete legacy beacons. This is
        // verified by master public key when checking the contract.
        //
        if (contract.m_action == ContractAction::REMOVE) {
            return true;
        }

        if (!contract.m_body.WellFormed(contract.m_action.Value())) {
            return false;
        }

        const ContractPayload payload = contract.m_body.AssumeLegacy();
        const std::string value = DecodeBase64(payload->LegacyValueString());
        const std::vector<std::string> parts = split(value, ";");

        if (parts.size() < 4) return false;
        if (parts[0].empty()) return false;
        if (parts[2].empty()) return false;
        if (parts[3].empty()) return false;

        return true;
    }

    const auto payload = contract.SharePayloadAs<BeaconPayload>();

    if (payload->m_version < 2) {
        LogPrint(LogFlags::CONTRACT, "%s: Legacy beacon contract", __func__);
        return false;
    }

    if (!payload->WellFormed(contract.m_action.Value())) {
        LogPrint(LogFlags::CONTRACT, "%s: Malformed beacon contract", __func__);
        return false;
    }

    if (!payload->VerifySignature()) {
        LogPrint(LogFlags::CONTRACT, "%s: Invalid beacon signature", __func__);
        return false;
    }

    const BeaconOption current_beacon = Try(payload->m_cpid);

    if (!current_beacon || current_beacon->Expired(tx.nTime)) {
        return true;
    }

    // Self-service beacon removal allowed when the signature matches the key
    // of the original beacon:
    if (contract.m_action == ContractAction::REMOVE) {
        if (current_beacon->m_public_key != payload->m_beacon.m_public_key) {
            LogPrint(LogFlags::CONTRACT, "%s: Beacon key mismatch", __func__);
            return false;
        }

        return true;
    }

    // Self-service beacon replacement will be authenticated by the scrapers:
    if (current_beacon->m_public_key != payload->m_beacon.m_public_key) {
        return true;
    }

    // Transition to version 2 beacons after the block version 11 threshold.
    // Legacy beacons are not renewable:
    if (current_beacon->m_timestamp <= g_v11_timestamp) {
        LogPrint(LogFlags::CONTRACT, "%s: Can't renew legacy beacon", __func__);
        return false;
    }

    if (!current_beacon->Renewable(tx.nTime)) {
        LogPrint(LogFlags::CONTRACT,
            "%s: Beacon for CPID %s is not renewable. Age: %" PRId64,
            __func__,
            payload->m_cpid.ToString(),
            current_beacon->Age(tx.nTime));

        return false;
    }

    return true;
}

void BeaconRegistry::ActivatePending(
    const std::vector<uint160>& beacon_ids,
    const int64_t superblock_time)
{
    for (const auto& id : beacon_ids) {
        auto iter_pair = m_pending.find(id);

        if (iter_pair != m_pending.end()) {
            m_beacons[iter_pair->second.m_cpid] = std::move(iter_pair->second);
            m_pending.erase(iter_pair);
        }
    }

    for (auto iter = m_pending.begin(); iter != m_pending.end(); /* no-op */) {
        if (iter->second.Expired(superblock_time)) {
            iter = m_pending.erase(iter);
        } else {
            ++iter;
        }
    }
}

void BeaconRegistry::Deactivate(const int64_t superblock_time)
{
    for (auto iter = m_beacons.begin(); iter != m_beacons.end(); /* no-op */) {
        if (iter->second.m_timestamp >= superblock_time) {
            PendingBeacon pending(iter->first, std::move(iter->second));
            m_pending.emplace(pending.GetId(), std::move(pending));

            iter = m_beacons.erase(iter);
        } else {
            ++iter;
        }
    }
}
