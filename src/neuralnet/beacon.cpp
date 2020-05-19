#include "base58.h"
#include "logging.h"
#include "neuralnet/beacon.h"
#include "neuralnet/contract/contract.h"
#include "util.h"

#include <algorithm>

using namespace NN;
using LogFlags = BCLog::LogFlags;

namespace {
BeaconRegistry g_beacons;
}

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
    return Age(now) > MAX_AGE;
}

bool Beacon::Renewable(const int64_t now) const
{
    return Age(now) > RENEWAL_AGE;
}

CBitcoinAddress Beacon::GetAddress() const
{
    return CBitcoinAddress(CTxDestination(m_public_key.GetID()));
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

BeaconPayload::BeaconPayload(const Cpid cpid, Beacon beacon)
    : m_cpid(cpid)
    , m_beacon(std::move(beacon))
{
}

BeaconPayload BeaconPayload::Parse(const std::string& key, const std::string& value)
{
    const CpidOption cpid = MiningId::Parse(key).TryCpid();

    if (!cpid) {
        return BeaconPayload();
    }

    Beacon beacon = Beacon::Parse(value);

    if (!beacon.WellFormed()) {
        return BeaconPayload();
    }

    return BeaconPayload(*cpid, std::move(beacon));
}

// -----------------------------------------------------------------------------
// Class: BeaconRegistry
// -----------------------------------------------------------------------------

const BeaconRegistry::BeaconMap& BeaconRegistry::Beacons() const
{
    return m_beacons;
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

void BeaconRegistry::Add(Contract contract)
{
    BeaconPayload payload = contract.PullPayloadAs<BeaconPayload>();

    payload.m_beacon.m_timestamp = contract.m_tx_timestamp;

    m_beacons[payload.m_cpid] = std::move(payload.m_beacon);
}

void BeaconRegistry::Delete(const Contract& contract)
{
    const auto payload = contract.SharePayloadAs<BeaconPayload>();

    m_beacons.erase(payload->m_cpid);
}

void BeaconRegistry::Revert(const Contract& contract)
{
    IContractHandler::Revert(contract);
}

bool BeaconRegistry::Validate(const Contract& contract) const
{
    // Only administrative contracts can delete beacons. This is verified
    // by master public key when checking the contract.
    //
    if (contract.m_action == ContractAction::REMOVE) {
        return true;
    }

    // For legacy beacons, check that the unused parts contain non-empty values
    // for compatibility with the existing protocol to prevent a fork.
    //
    if (contract.m_version <= 1) {
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

    if (!payload->WellFormed(contract.m_action.Value())) {
        LogPrint(LogFlags::CONTRACT, "%s: Malformed beacon contract", __func__);

        return false;
    }

    const BeaconOption current_beacon = Try(payload->m_cpid);

    if (!current_beacon || current_beacon->Expired(contract.m_tx_timestamp)) {
        return true;
    }

    if (!current_beacon->Renewable(contract.m_tx_timestamp)) {
        LogPrint(LogFlags::CONTRACT,
            "%s: Beacon for CPID %s is not renewable. Age: %" PRId64,
            __func__,
            payload->m_cpid.ToString(),
            current_beacon->Age(contract.m_tx_timestamp));

        return false;
    }

    if (payload->m_beacon.m_public_key != current_beacon->m_public_key) {
        LogPrint(LogFlags::CONTRACT,
            "%s: Beacon renewal for CPID %s does not match existing public key",
            __func__,
            payload->m_cpid.ToString());

        return false;
    }

    return true;
}
