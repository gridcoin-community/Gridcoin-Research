#include "appcache.h"
#include "../beacon.h" // temporary
#include "neuralnet/beacon.h"
#include "util.h"

using namespace NN;

namespace {
    BeaconRegistry beacons;
}

BeaconRegistry& NN::GetBeaconRegistry()
{
    return beacons;
}

// -----------------------------------------------------------------------------
// Class: BeaconRegistry
//
// TODO: This placeholder contract handler just writes messages into the
// AppCache. Refactor the implementation to use strongly-typed structures and
// organize the beacon logic behind this API.
// -----------------------------------------------------------------------------

void BeaconRegistry::Add(const Contract& contract)
{
    if (Contains(contract.m_value, "INVESTOR")) {
        return;
    }

    std::string out_cpid = "";
    std::string out_address = "";
    std::string out_publickey = "";

    GetBeaconElements(contract.m_value, out_cpid, out_address, out_publickey);

    WriteCache(
        Section::BEACONALT,
        contract.m_key + "." + ToString(contract.m_tx_timestamp),
        out_publickey,
        contract.m_tx_timestamp);

    WriteCache(
        Section::BEACON,
        contract.m_key,
        contract.m_value,
        contract.m_tx_timestamp);

    if (fDebug10) {
        LogPrintf(
            "BEACON add %s %s %s",
            contract.m_key,
            DecodeBase64(contract.m_value),
            TimestampToHRDate(contract.m_tx_timestamp));
    }
}

void BeaconRegistry::Delete(const Contract& contract)
{
    if (Contains(contract.m_value, "INVESTOR")) {
        return;
    }

    // We use beacons to verify blocks, so we cannot also delete it from the
    // BEACONALT AppCache section.
    DeleteCache(Section::BEACON, contract.m_key);

    if (fDebug10) {
        LogPrintf("BEACON DEL %s - %s", contract.m_key, TimestampToHRDate(contract.m_tx_timestamp));
    }
}

void BeaconRegistry::Revert(const Contract& contract)
{
    if (Contains(contract.m_value, "INVESTOR")) {
        return;
    }

    IContractHandler::Revert(contract);

    // Revert the entry used to verify blocks with beacon keys. Reorganizing
    // the chain will add this entry back:
    if (contract.m_action == ContractAction::ADD) {
        DeleteCache(
            Section::BEACONALT,
            contract.m_key + "." + ToString(contract.m_tx_timestamp));
    }

    // Ignore ContractAction::REMOVE -- we don't need to add the beacon back
    // to the BEACONALT section because we won't delete it from this section
    // in the first place.
}
