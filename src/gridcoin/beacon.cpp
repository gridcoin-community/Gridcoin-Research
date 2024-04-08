// Copyright (c) 2014-2024 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "logging.h"
#include "main.h"
#include "gridcoin/beacon.h"
#include "gridcoin/contract/contract.h"
#include "util.h"
#include "wallet/wallet.h"

#include <algorithm>

using namespace GRC;
using LogFlags = BCLog::LogFlags;

extern int64_t g_v11_timestamp;

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
    payload.Serialize(hasher, GRC::ContractAction::UNKNOWN);

    return hasher.GetHash();
}
} // Anonymous namespace

// -----------------------------------------------------------------------------
// Global Functions
// -----------------------------------------------------------------------------

BeaconRegistry& GRC::GetBeaconRegistry()
{
    return g_beacons;
}

// -----------------------------------------------------------------------------
// Class: Beacon
// -----------------------------------------------------------------------------

Beacon::Beacon() : m_public_key(), m_timestamp(0), m_hash()
{
}

Beacon::Beacon(CPubKey public_key)
    : Beacon(std::move(public_key), 0, uint256 {})
{
}

Beacon::Beacon(CPubKey public_key, int64_t timestamp, uint256 hash)
    : m_cpid()
    , m_public_key(std::move(public_key))
    , m_timestamp(timestamp)
    , m_hash(hash)
    , m_previous_hash()
    , m_status(BeaconStatusForStorage::UNKNOWN)
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

std::pair<std::string, std::string> Beacon::KeyValueToString() const
{
    return std::make_pair(m_cpid.ToString(), GetAddress().ToString());
}

std::string Beacon::StatusToString() const
{
    return StatusToString(m_status.Value());
}

std::string Beacon::StatusToString(const BeaconStatusForStorage& status, const bool& translated) const
{
    if (translated) {
        switch(status) {
        case BeaconStatusForStorage::UNKNOWN:         return _("Unknown");
        case BeaconStatusForStorage::PENDING:         return _("Pending");
        case BeaconStatusForStorage::ACTIVE:          return _("Active");
        case BeaconStatusForStorage::RENEWAL:         return _("Renewal");
        case BeaconStatusForStorage::EXPIRED_PENDING: return _("Expired while pending");
        case BeaconStatusForStorage::DELETED:         return _("Deleted");
        case BeaconStatusForStorage::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    } else {
        // The untranslated versions are really meant to serve as the string equivalent of the enum values.
        switch(status) {
        case BeaconStatusForStorage::UNKNOWN:         return "Unknown";
        case BeaconStatusForStorage::PENDING:         return "Pending";
        case BeaconStatusForStorage::ACTIVE:          return "Active";
        case BeaconStatusForStorage::RENEWAL:         return "Renewal";
        case BeaconStatusForStorage::EXPIRED_PENDING: return "Expired while pending";
        case BeaconStatusForStorage::DELETED:         return "Deleted";
        case BeaconStatusForStorage::OUT_OF_BOUND:    break;
        }

        assert(false); // Suppress warning
    }

    // This will never be reached. Put it in anyway to prevent control reaches end of non-void function warning
    // from some compiler versions.
    return std::string{};
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
    // if (m_timestamp <= g_v11_timestamp) {
    //    return now - g_v11_timestamp > 14 * 86400;
    // }

    return false;
}

bool Beacon::Renewable(const int64_t now) const
{
    return (!Expired(now) && Age(now) > RENEWAL_AGE);
}

bool Beacon::Renewed() const
{
    return (m_status == BeaconStatusForStorage::RENEWAL && m_previous_hash != uint256());
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
    // We need this lock here because this function is being called from
    // the researcher model for the GUI beacon status.
    LOCK(wallet->cs_wallet);

    return wallet->HaveKey(m_public_key.GetID());
}

std::string Beacon::ToString() const
{
    return EncodeBase64(
        "0;0;"  // Unused: [CPIDv2];[nonce];
        + GetAddress().ToString()
        + ";"
        + HexStr(m_public_key));
}

bool Beacon::operator==(Beacon b)
{
    bool result = true;

    result &= (m_cpid == b.m_cpid);
    result &= (m_public_key == b.m_public_key);
    result &= (m_timestamp == b.m_timestamp);
    result &= (m_hash == b.m_hash);
    result &= (m_previous_hash == b.m_previous_hash);
    result &= (m_status == b.m_status);

    return result;
}

bool Beacon::operator!=(Beacon b)
{
    return !(*this == b);
}


// -----------------------------------------------------------------------------
// Class: BeaconPayload
// -----------------------------------------------------------------------------

constexpr uint32_t BeaconPayload::CURRENT_VERSION; // For clang

BeaconPayload::BeaconPayload()
{
}

BeaconPayload::BeaconPayload(const uint32_t version, const Cpid& cpid, Beacon beacon)
    : m_version(version)
    , m_cpid(cpid)
    , m_beacon(std::move(beacon))
{
}

BeaconPayload::BeaconPayload(const Cpid& cpid, Beacon beacon)
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
    return m_beacon.m_public_key.Verify(HashBeaconPayload(*this), m_signature);
}

// -----------------------------------------------------------------------------
// Class: PendingBeacon
// -----------------------------------------------------------------------------

PendingBeacon::PendingBeacon(const Cpid& cpid, Beacon beacon)
    : Beacon(std::move(beacon))
{
    m_cpid = cpid;
}

bool PendingBeacon::PendingExpired(const int64_t now) const
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

const std::set<Beacon_ptr>& BeaconRegistry::ExpiredBeacons() const
{
    return m_expired_pending;
}

BeaconOption BeaconRegistry::Try(const Cpid& cpid) const
{
    const auto iter = m_beacons.find(cpid);

    if (iter == m_beacons.end()) {
        return nullptr;
    }

    return iter->second;
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

std::vector<Beacon_ptr> BeaconRegistry::FindPending(const Cpid& cpid) const
{
    // TODO: consider adding a lookup table for pending beacons keyed by CPID.
    // Since the protocol just needs to look up pending beacons by public key,
    // we just do a search here. Informational RPCs or local beacon management
    // only need to call this occasionally.

    std::vector<Beacon_ptr> found;

    for (const auto& pending_beacon_pair : m_pending) {
        const Beacon_ptr& beacon_ptr = pending_beacon_pair.second;

        if (beacon_ptr->m_cpid == cpid) {
            found.emplace_back(beacon_ptr);
        }
    }

    return found;
}

const BeaconOption BeaconRegistry::FindHistorical(const uint256& hash)
{
    auto beacon_iter = m_beacon_db.find(hash);

    if (beacon_iter != m_beacon_db.end()) {
        return beacon_iter->second;
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

//!
//! \brief This resets the in-memory maps of the registry and the LevelDB backing storage.
//!
void BeaconRegistry::Reset()
{
    m_beacons.clear();
    m_pending.clear();
    m_expired_pending.clear();
    m_beacon_db.clear();
}

//!
//! \brief Attempt to renew an existing beacon from a contract.
//!
//! \param payload Beacon contract message from a transaction.
//!
//! \return \c true if the supplied beacon contract matches an active beacon.
//! This updates the matched beacon with a new timestamp.
//!
bool BeaconRegistry::TryRenewal(Beacon_ptr& current_beacon_ptr, int& height, const BeaconPayload& payload)
{

    if (current_beacon_ptr->Expired(payload.m_beacon.m_timestamp)) {
        return false;
    }

    if (current_beacon_ptr->m_public_key != payload.m_beacon.m_public_key) {
        return false;
    }

    // Public key matches incoming. Process the renewal.

    PendingBeacon renewal(payload.m_cpid, payload.m_beacon);

    // Set the status to RENEWAL.
    renewal.m_status = BeaconStatusForStorage::RENEWAL;
    renewal.m_previous_hash = current_beacon_ptr->m_hash;

    // Put the renewal beacon into the db.
    if (!m_beacon_db.insert(renewal.m_hash, height, renewal))
    {
        LogPrint(LogFlags::BEACON, "INFO: %s: In renewal of beacon for cpid %s, address %s, hash %s, beacon db record "
                                   "already exists. This can be expected on a restart of the wallet to ensure multiple "
                                   "contracts in the same block get stored/replayed.",
                 __func__,
                 renewal.m_cpid.ToString(),
                 renewal.GetAddress().ToString(),
                 renewal.m_hash.GetHex());
    }

    // Get the iterator to the renewal beacon.
    auto renewal_iter = m_beacon_db.find(renewal.m_hash);

    // Place a smart shared pointer to the renewed beacon in the active beacons map. Note that the
    // subscript form of the insert with the same key replaces the current beacon entry with the
    //renewal.
    m_beacons[payload.m_cpid] = renewal_iter->second;

    return true;
}

void BeaconRegistry::Add(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the LevelDB database
    int height = -1;

    if (ctx.m_pindex)
    {
        height = ctx.m_pindex->nHeight;
    }

    BeaconPayload payload = ctx->CopyPayloadAs<BeaconPayload>();

    // Get an iterator to any existing beacon with the same cpid already in the
    // m_beacons map. This could be a expired gap add or a forced advertisement.
    auto beacon_pair_iter = m_beacons.find(payload.m_cpid);

    Beacon_ptr current_beacon_ptr = nullptr;

    // Make sure the payload m_beacon has the correct time and transaction hash.
    // TODO: See if these are initialized correctly in the CopyPayloadAs.
    payload.m_beacon.m_timestamp = ctx.m_tx.nTime;
    payload.m_beacon.m_hash = ctx.m_tx.GetHash();

    // Is there an existing beacon in the map (renewal, expired new advertisement, or force advertisement)?
    bool current_beacon_present = (beacon_pair_iter != m_beacons.end());

    // If so, then get a smart pointer to it.
    if (current_beacon_present)
    {
        current_beacon_ptr = beacon_pair_iter->second;

        // Set the payload m_beacon's prev beacon ctx hash = to the existing beacon's hash.
        payload.m_beacon.m_previous_hash = current_beacon_ptr->m_hash;
    }
    else // Effectively Newbie.
    {
        payload.m_beacon.m_previous_hash = uint256 {};
    }

    // Legacy beacon contracts before block version 11--just load the beacon:
    //
    if (ctx->m_version == 1) {
        Beacon historical(payload.m_beacon);

        historical.m_cpid = payload.m_cpid;
        historical.m_status = BeaconStatusForStorage::ACTIVE;

        if (!m_beacon_db.insert(ctx.m_tx.GetHash(), height, historical))
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: In activation of v1 beacon for cpid %s, address %s, hash %s, beacon db record "
                                       "already exists. This can be expected on a restart of the wallet to ensure multiple "
                                       "contracts in the same block get stored/replayed.",
                     __func__,
                     historical.m_cpid.ToString(),
                     historical.GetAddress().ToString(),
                     historical.m_hash.GetHex());
        }
        m_beacons[payload.m_cpid] = m_beacon_db.find(ctx.m_tx.GetHash())->second;
        return;
    }

    // For beacon renewals, check that the new beacon contains the same public
    // key. If it matches, we don't need to verify it again:
    //
    if (current_beacon_present && TryRenewal(current_beacon_ptr, height, payload)) {
        return;
    }

    // Otherwise, set the new beacon aside for scraper verification. The next
    // superblock will activate it if it matches a BOINC account:
    //
    // Make a pending beacon out of the payload.
    PendingBeacon pending(payload.m_cpid, std::move(payload.m_beacon));

    // Mark the status as PENDING.
    // TODO: Put this in the constructor?
    pending.m_status = BeaconStatusForStorage::PENDING;

    // Insert the entry into the db.
    if (!m_beacon_db.insert(ctx.m_tx.GetHash(), height, static_cast<Beacon>(pending)))
    {
        LogPrint(LogFlags::BEACON, "INFO: %s: In advertisement of beacon for cpid %s, address %s, hash %s, beacon db record "
                                   "already exists. This can be expected on a restart of the wallet to ensure multiple "
                                   "contracts in the same block get stored/replayed.",
                 __func__,
                 pending.m_cpid.ToString(),
                 pending.GetAddress().ToString(),
                 pending.m_hash.GetHex());
    }

    // Insert a pointer to the entry in the m_pending map.
    m_pending[pending.GetId()] =  m_beacon_db.find(ctx.m_tx.GetHash())->second;
}

void BeaconRegistry::Delete(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the LevelDB database
    int height = -1;

    if (ctx.m_pindex)
    {
        height = ctx.m_pindex->nHeight;
    }

    const auto payload = ctx->SharePayloadAs<BeaconPayload>();

    if (ctx->m_version >= 2) {
        m_pending.erase(payload->m_beacon.GetId());
    }

    auto iter = m_beacons.find(payload->m_cpid);

    uint256 last_active_ctx_hash;

    // If the beacon exists in the active map, delete the entry.
    if (iter != m_beacons.end())
    {
        last_active_ctx_hash = iter->second->m_hash;

        m_beacons.erase(payload->m_cpid);
    }

    // If the beacon exists in the pending map, delete the entry.
    auto iter2 = m_pending.find(payload->m_beacon.m_public_key.GetID());

    if (iter2 != m_pending.end())
    {
        m_pending.erase(payload->m_beacon.m_public_key.GetID());
    }

    Beacon deleted_beacon(payload->m_beacon);

    deleted_beacon.m_cpid = payload->m_cpid;
    deleted_beacon.m_hash = ctx.m_tx.GetHash();
    deleted_beacon.m_previous_hash = last_active_ctx_hash;
    deleted_beacon.m_status = BeaconStatusForStorage::DELETED;

    // Insert the deleted beacon entry in the storage db.
    m_beacon_db.insert(deleted_beacon.m_hash, height, deleted_beacon);
}

void BeaconRegistry::Revert(const ContractContext& ctx)
{
    const auto payload = ctx->SharePayloadAs<BeaconPayload>();

    // Note that the contract actions are ADD and REMOVE.
    //
    // ADD results in pending beacons UNTIL they are activated OR renewals. So the result state recorded in the historical
    // table that is associated with the TRANSACTION HASH is PENDING or RENEWAL. The activations are done as part of the
    // superblock commit and are associated with the SB hash.
    //
    // REMOVE is the revoke beacon order.
    //
    // Note that Deactivate, which occurs at the block level, will be called on a superblock revert BEFORE the first
    // transaction level beacon revert is called. It could very well be that there are only activations to revert at the
    // superblock revert (Deactivate), and there are no beacon advertisement transactions (contracts) to revert here.
    // Any records activated in the superblock itself to be reverted will already be reverted to the PENDING state.
    // There is no danger here though of an overly aggressive deletion of those PENDING records, because those pending
    // records would have to be advertised IN THE SUPERBLOCK itself if that were the case, for those to be included in
    // the transaction based reversion here where the transaction resides in a superblock. That is extremely unlikely
    // because of the process required in the scrapers and the convergence required to get activated, and in the unlikely
    // event that does happen, the record would be properly found here and reverted, otherwise not.
    //
    // Revert the ADD action:
    if (ctx->m_action == ContractAction::ADD)
    {
        // If the ctx to revert was an ADD, and it was a version 1 contract, then just delete the record. There is no
        // pending state in version 1 beacons.
        if (ctx->m_version == 1)
        {
            // Erase the record from m_beacons.
            m_beacons.erase(payload->m_cpid);

            // Erase the record from m_beacon_db.
            m_beacon_db.erase(ctx.m_tx.GetHash());
        }

        // PENDING beacons:
        //
        bool pending_to_revert_found = false;

        // If the ctx to revert was an ADD, and it was a version 2+ contract, then we need to look for pending records
        // to revert. Remember the reversion of activations are not handled here, but rather in Deactivate, calleed
        // at the block disconnect loop level. (See DisconnectBlocksBatch.)
        if (ctx->m_version >= 2) {
            // Note that the GetId() is essentially the public key of the beacon advertisement. The same key will
            // NOT be readvertised. (That rather is the special case of the renewal for which there is never a pending
            // state.) So the GetId finds the correct pending entry to delete, if it is not a renewal, because there HAS
            // to be a pending beacon if we are reverting the transaction for the advertisement itself. If there is more
            // than one pending record for the CPID, then it will have a different public key and be on a different
            // transaction and be handled appropriately by the revert of that transaction, if required.
            auto pending_to_revert = m_pending.find(payload->m_beacon.GetId());

            pending_to_revert_found = (pending_to_revert != m_pending.end());

            if (pending_to_revert_found)
            {
                if (pending_to_revert->second->m_status != BeaconStatusForStorage::PENDING)
                {
                    error("%s: Transaction hash %s: Pending beacon to revert for cpid %s found in historical table "
                          "but status is not PENDING. Status is %i.",
                          __func__,
                          ctx.m_tx.GetHash().GetHex(),
                          pending_to_revert->second->m_cpid.ToString(),
                          pending_to_revert->second->m_status.Raw());
                }

                // Remove the found pending entry.
                m_pending.erase(pending_to_revert);

                // Also remove this historical record, because in a revert it should not be retained.
                m_beacon_db.erase(ctx.m_tx.GetHash());
            }
        }

        // RENEWAL beacons:
        //

        // If the beacon is a renewal, it will have been put directly into the active beacons map with a status
        // of RENEWAL. m_beacons is a map keyed by CPID, so therefore there can be only one record in m_beacons that
        // corresponds to the renewed beacon to be deleted. It also must be in a status of renewal to be reverted as a
        // renewal. Direct add v1 contracts are directly reverted when they are gotten to above.
        auto iter = m_beacons.find(payload->m_cpid);

        // The beacon exists, has a valid prev beacon hash, and is a renewal...
        if (iter != m_beacons.end() && iter->second->m_status == BeaconStatusForStorage::RENEWAL)
        {
            Beacon_ptr renewal = iter->second;

            // Check that the identified beacon hash corresponds to the beacon in the transaction context.
            if (renewal->m_hash != ctx.m_tx.GetHash())
            {
                error("%s: The hash of the renewal beacon for cpid %s to revert does not equal the hash of the provided "
                      "transaction context. The transaction context hash is %s; the identified beacon renewal "
                      "to revert hash is %s.",
                      __func__,
                      payload->m_cpid.ToString(),
                      ctx.m_tx.GetHash().GetHex(),
                      renewal->m_hash.GetHex());
            }

            // Let's proceed anyway. A renewed beacon will have a non-null m_prev_beacon_hash, referring to the
            // prior beacon record, which could itself be a renewal, or the original advertisement. Regardless,
            // if found, resurrect that record.
            if (!renewal->m_previous_hash.IsNull())
            {
                Cpid cpid = iter->first;

                // Get the hash of the previous beacon. This normally could be a renewal, but
                // it could also be a gap advertisement or a force advertisement.
                uint256 renewal_hash = renewal->m_hash;
                uint256 resurrect_hash = renewal->m_previous_hash;

                // Erase the beacon that was ordered deleted.
                m_beacons.erase(iter);

                // Get an iterator from the beacon db (either in the historical table, or
                // will be loaded from LevelDB and put in the historical table.
                auto resurrect_iter = m_beacon_db.find(resurrect_hash);

                if (resurrect_iter != m_beacon_db.end())
                {
                    // This is an element to the desired entry in the historical table.
                    Beacon_ptr& resurrected_beacon = resurrect_iter->second;

                    // Resurrect the prior beacon.
                    // ------------- cpid -------------- smart shared pointer to element in historical map.
                    m_beacons[cpid] = resurrected_beacon;
                }

                // Erase the renewal record in the db that was reverted. No reason to keep it.
                m_beacon_db.erase(renewal_hash);
            }
            else
            {
                // This else case should NOT happen because version 1 contracts were handled above. Log an error.
                error("%s: Transaction hash %s: The identified renewal beacon for cpid %s to revert "
                      "does not have a m_prev_beacon_hash.",
                      __func__,
                      ctx.m_tx.GetHash().GetHex(),
                      payload->m_cpid.ToString());
            }
        }
    } // if (ctx->m_action == ContractAction::ADD)

    // Revert a REMOVE action:
    //
    // In beacons, the only use-case for contract actions of remove are the beacon revocation or the old style
    // direct deletes for v1 contracts. In this case we need to resurrect the previous record pointed to by the
    // deleted record in the beacon db. This could be an ACTIVE, RENEWAL, or PENDING record. The EXPIRED_PENDING
    // records are handled in the Deactivate function, which is the block level Revert for the superblock reversion.
    if (ctx->m_action == ContractAction::REMOVE)
    {
        uint256 deletion_hash = ctx.m_tx.GetHash();

        auto deleted_beacon_record = m_beacon_db.find(deletion_hash);

        if (deleted_beacon_record != m_beacon_db.end())
        {
            auto record_to_restore = m_beacon_db.find(deleted_beacon_record->second->m_previous_hash);

            if (record_to_restore != m_beacon_db.end())
            {
                // Get a smart shared pointer to the beacon to restore
                Beacon_ptr beacon_to_restore_ptr = record_to_restore->second;

                // Check the beacon's status. If it was ACTIVE or RENEWAL, put it back in the m_beacons map
                // under the cpid.
                if (beacon_to_restore_ptr->m_status == BeaconStatusForStorage::ACTIVE
                        || beacon_to_restore_ptr->m_status == BeaconStatusForStorage::RENEWAL)
                {
                    m_beacons[beacon_to_restore_ptr->m_cpid] = beacon_to_restore_ptr;
                }
                else if (beacon_to_restore_ptr->m_status == BeaconStatusForStorage::PENDING)
                {
                    m_pending[beacon_to_restore_ptr->GetId()] = beacon_to_restore_ptr;
                }
                else
                {
                    error("%s: Transaction hash %s: In a revert of a beacon deletion for cpid %s, the beacon pointed to by "
                          "the deletion entry does not contain a status of ACTIVE, RENEWAL, or PENDING. The status is %i.",
                          __func__,
                          ctx.m_tx.GetHash().GetHex(),
                          payload->m_cpid.ToString(),
                          beacon_to_restore_ptr->m_status.Raw());
                }
            }
            else
            {
                error("%s: Transaction hash %s: In a revert of a beacon deletion for cpid %s, no previous beacon record "
                      "to restore was found.",
                      __func__,
                      ctx.m_tx.GetHash().GetHex(),
                      payload->m_cpid.ToString());
            }
        }
    } // if (ctx->m_action == ContractAction::REMOVE)
}

void BeaconRegistry::SetDBHeight(int& height)
{
    m_beacon_db.StoreDBHeight(height);
}

int BeaconRegistry::GetDBHeight()
{
    int height = 0;

    m_beacon_db.LoadDBHeight(height);

    return height;
}

Beacon_ptr BeaconRegistry::GetBeaconChainletRoot(Beacon_ptr beacon,
                                                 std::shared_ptr<std::vector<std::pair<uint256, int64_t>>> beacon_chain_out)
{
    const auto ChainletErrorHandle = [this](unsigned int i, Beacon_ptr beacon, std::string error_message) {
        error("%s: Beacon chainlet is corrupted at link %u for cpid %s: timestamp = %s" PRId64 ", ctx_hash = %s,"
              "prev_beacon_ctx_hash = %s, status = %s: %s.",
              __func__,
              i,
              beacon->m_cpid.ToString(),
              beacon->m_timestamp,
              beacon->m_hash.GetHex(),
              beacon->m_previous_hash.GetHex(),
              beacon->StatusToString(),
              error_message);

        std::string str_error = strprintf("ERROR: %s: Beacon chainlet is corrupted at link %u for cpid %s: timestamp = %s"
                                          PRId64 ", ctx_hash = %s, prev_beacon_ctx_hash = %s, status = %s: %s.",
                                          __func__,
                                          i,
                                          beacon->m_cpid.ToString(),
                                          beacon->m_timestamp,
                                          beacon->m_hash.GetHex(),
                                          beacon->m_previous_hash.GetHex(),
                                          beacon->StatusToString(),
                                          error_message);

        Reset();

        uiInterface.ThreadSafeMessageBox(str_error, "Gridcoin", CClientUIInterface::MSG_ERROR);

        throw std::runtime_error(std::string {"The beacon registry is corrupted and will be rebuilt on the next start. "
                                             "Please restart."});
    };

    const auto ChainletLinkLog = [&beacon_chain_out](unsigned int i, Beacon_ptr beacon) {
        LogPrint(BCLog::LogFlags::ACCRUAL, "INFO %s: beacon chainlet link %u for cpid %s: timestamp = %" PRId64 ", ctx_hash = %s,"
                                           " prev_beacon_ctx_hash = %s, status = %s.",
                 __func__,
                 i,
                 beacon->m_cpid.ToString(),
                 beacon->m_timestamp,
                 beacon->m_hash.GetHex(),
                 beacon->m_previous_hash.GetHex(),
                 beacon->StatusToString());

        if (beacon_chain_out == nullptr) {
            return;
        }

        beacon_chain_out->push_back(std::make_pair(beacon->m_hash, beacon->m_timestamp));
    };

    unsigned int i = 0;

    // Given that we have had rare situations where somehow circularity has occurred in the beacon chainlet, which either
    // results in the current hash and previous hash being the same, or even suspected previous hash of another entry pointing
    // back to a beacon in a circular manner, this vector is used to detect the circularity.
    std::vector<uint256> encountered_hashes;

    // The chain head itself. (This uses a scope to separate beacon_iter.)
    {
        auto beacon_iter = m_beacon_db.find(beacon->m_hash);

        if (beacon_iter == m_beacon_db.end()) {
            // Beacon chainlet chainhead cannot be found. This is fatal.
            ChainletErrorHandle(i, beacon, "not found in the registry");
        }

        // Make sure status is renewed or active.
        if (beacon_iter->second->m_status != BeaconStatusForStorage::ACTIVE
            && beacon_iter->second->m_status != BeaconStatusForStorage::RENEWAL) {
            ChainletErrorHandle(i, beacon, "beacon status is not active or renewal");
        }

        encountered_hashes.push_back(beacon->m_hash);

        ChainletLinkLog(i, beacon);

        ++i;
    }

    // Walk back the entries in the historical beacon map linked by renewal prev tx hash until the first
    // beacon in the renewal chain is found (the original advertisement). The accrual starts no earlier
    // than here.
    while (beacon->Renewed())
    {
        // Select previous beacon in chainlet
        auto beacon_iter = m_beacon_db.find(beacon->m_previous_hash);

        if (beacon_iter == m_beacon_db.end()) {
            ChainletErrorHandle(i, beacon, "previous beacon not found in the registry");
        }

        if (std::find(encountered_hashes.begin(), encountered_hashes.end(), beacon->m_previous_hash) != encountered_hashes.end()) {
            // If circularity is found this is an indication of corruption of beacon state and is fatal.
            // Produce an error message, reset the beacon registry, and require a restart of the node.
            ChainletErrorHandle(i, beacon, "circularity encountered");
        }

        // Reassign previous beacon to beacon.
        beacon = beacon_iter->second;

        encountered_hashes.push_back(beacon->m_hash);

        if (beacon_chain_out != nullptr) {
            ChainletLinkLog(i, beacon);
        }

        ++i;
    }

    // Check of initial advertised beacon's previous hash. This should point to the pending beacon that was activated and not
    // anywhere else.
    {
        // Select previous beacon in chainlet
        auto beacon_iter = m_beacon_db.find(beacon->m_previous_hash);

        if (beacon_iter == m_beacon_db.end()) {
            ChainletErrorHandle(i, beacon, "previous beacon not found in the registry");
        }

        // Make sure status of previous beacon is pending.
        if (beacon_iter->second->m_status != BeaconStatusForStorage::PENDING) {
            ChainletErrorHandle(i, beacon, "previous beacon to the beacon marked active is not pending");
        }

        if (std::find(encountered_hashes.begin(), encountered_hashes.end(), beacon->m_previous_hash) != encountered_hashes.end()) {
            // If circularity is found this is an indication of corruption of beacon state and is fatal.
            // Produce an error message, reset the beacon registry, and require a restart of the node.
            ChainletErrorHandle(i, beacon, "circularity encountered");
        }

        // Note that we do not actually walk back to the pending beacon. The parameter beacon remains at the activated beacon.
    }

    return beacon;
}

bool BeaconRegistry::NeedsIsContractCorrection()
{
    return m_beacon_db.NeedsIsContractCorrection();
}

bool BeaconRegistry::SetNeedsIsContractCorrection(bool flag)
{
    return m_beacon_db.SetNeedsIsContractCorrection(flag);
}

bool BeaconRegistry::Validate(const Contract& contract, const CTransaction& tx, int& DoS) const
{
    if (contract.m_version <= 1) {
        return true;
    }

    const auto payload = contract.SharePayloadAs<BeaconPayload>();

    if (payload->m_version < 2) {
        DoS = 25;
        LogPrint(LogFlags::CONTRACT, "%s: Legacy beacon contract", __func__);
        return false;
    }

    if (!payload->WellFormed(contract.m_action.Value())) {
        DoS = 25;
        LogPrint(LogFlags::CONTRACT, "%s: Malformed beacon contract", __func__);
        return false;
    }

    if (!payload->VerifySignature()) {
        DoS = 25;
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
            DoS = 25;
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
        DoS = 25;
        LogPrint(LogFlags::CONTRACT, "%s: Can't renew legacy beacon", __func__);
        return false;
    }

    if (!current_beacon->Renewable(tx.nTime)) {
        DoS = 25;
        LogPrint(LogFlags::CONTRACT,
            "%s: Beacon for CPID %s is not renewable. Age: %" PRId64,
            __func__,
            payload->m_cpid.ToString(),
            current_beacon->Age(tx.nTime));

        return false;
    }

    return true;
}

bool BeaconRegistry::BlockValidate(const ContractContext& ctx, int& DoS) const
{
    return Validate(ctx.m_contract, ctx.m_tx, DoS);
}

void BeaconRegistry::ActivatePending(
    const std::vector<uint160>& beacon_ids,
    const int64_t superblock_time, const uint256& block_hash, const int& height)
{
    LogPrint(LogFlags::BEACON, "INFO: %s: Called for superblock at height %i.", __func__, height);

    // It is possible that more than one pending beacon with the same CPID can be attempted to be
    // activated in the same superblock. The behavior here to agree with the original implementation
    // is the last one. Here we are going to use a map keyed by the CPID with the array style insert
    // to ensure that the LAST pending beacon verified is the one activated.
    BeaconMap verified_beacons;

    for (const auto& id : beacon_ids) {
        auto iter_pair = m_pending.find(id);

        if (iter_pair != m_pending.end()) {
            bool already_found = (verified_beacons.find(iter_pair->second->m_cpid) != verified_beacons.end());

            if (already_found) {
                LogPrint(LogFlags::BEACON, "INFO: %s: More than one pending beacon verified for the same CPID %s. Overriding previous"
                                           "verified beacon.",
                         __func__,
                         iter_pair->second->m_cpid.ToString());
            }

            verified_beacons[iter_pair->second->m_cpid] = iter_pair->second;
        }
    }

    // Activate the pending beacons that are not expired with respect to pending age as of the time of verification (the
    // committing of the superblock).
    for (const auto& iter_pair : verified_beacons) {

        Beacon_ptr last_pending_beacon = iter_pair.second;

        // Create a new beacon to activate from the found pending beacon.
        Beacon activated_beacon(*iter_pair.second);

        // Update the new beacon's prev hash to be the hash of the pending beacon that is being activated.
        activated_beacon.m_previous_hash = last_pending_beacon->m_hash;

        // We are going to have to use a composite hash for these because activation is not done as
        // individual transactions. Rather groups are done in each superblock under one hash. The
        // hash of the block hash, and the pending beacon that is being activated's hash is sufficient.
        activated_beacon.m_status = BeaconStatusForStorage::ACTIVE;

        activated_beacon.m_hash = Hash(block_hash, last_pending_beacon->m_hash);

        LogPrint(LogFlags::BEACON, "INFO: %s: Activating beacon for cpid %s, address %s, hash %s.",
                 __func__,
                 activated_beacon.m_cpid.ToString(),
                 activated_beacon.GetAddress().ToString(),
                 activated_beacon.m_hash.GetHex());

        m_beacon_db.insert(activated_beacon.m_hash, height, activated_beacon);

        // This is the subscript form of insert. Important here because an activated beacon should
        // overwrite any existing entry in the m_beacons map.
        m_beacons[activated_beacon.m_cpid] = m_beacon_db.find(activated_beacon.m_hash)->second;

        // Remove the pending beacon entry from the pending map. (Note this entry still exists in the historical
        // table and the db.
        m_pending.erase(iter_pair.second->GetId());
    }

    // Clear the expired pending beacon set. There is no need to retain expired beacons beyond one SB boundary (which is when
    // this method is called) as this gives ~960 blocks of reorganization depth before running into the slight possibility that
    // a different SB could verify a different pending beacon that should be resurrected to be verified.
    m_expired_pending.clear();

    // Mark remaining pending beacons that are expired with respect to pending age as expired and move to the expired map.
    for (auto iter = m_pending.begin(); iter != m_pending.end(); /* no-op */) {
        PendingBeacon pending_beacon(*iter->second);

        // If the pending beacon has expired with no action remove the pending beacon.
        if (pending_beacon.PendingExpired(superblock_time)) {
            // Set the expired pending beacon's previous beacon hash to the beacon entry's hash.
            pending_beacon.m_previous_hash = pending_beacon.m_hash;

            // Mark the status as EXPIRED_PENDING.
            pending_beacon.m_status = BeaconStatusForStorage::EXPIRED_PENDING;

            // Set the beacon entry's hash to a synthetic block hash similar to above.
            pending_beacon.m_hash = Hash(block_hash, pending_beacon.m_hash);
            LogPrint(LogFlags::BEACON, "INFO: %s: Marking pending beacon expired for cpid %s, address %s, hash %s.",
                     __func__,
                     pending_beacon.m_cpid.ToString(),
                     pending_beacon.GetAddress().ToString(),
                     pending_beacon.m_hash.GetHex());

            // Insert the expired pending beacon into the db.
            if (!m_beacon_db.insert(pending_beacon.m_hash, height, static_cast<Beacon>(pending_beacon))) {
                LogPrintf("WARN: %s: Attempt to insert an expired pending beacon entry for cpid %s in the beacon registry where "
                          "one with that hash key (%s) already exists.",
                          __func__,
                          pending_beacon.m_cpid.ToString(),
                          pending_beacon.m_hash.GetHex());
            }

            // Insert the expired pending beacon into the m_expired_pending set. We do the find here because the insert above
            // created a shared pointer to the beacon object we want to hold a reference to. To save memory we do not want to
            // use a copy.
            m_expired_pending.insert(m_beacon_db.find(pending_beacon.m_hash)->second);

            // Remove the pending beacon entry from the m_pending map.
            iter = m_pending.erase(iter);
        } else {
            ++iter;
        }
    }
}

void BeaconRegistry::Deactivate(const uint256 superblock_hash)
{
    // Remember this function is a form of reversion, intended to be called during block disconnects as the inverse
    // of the ActivatePending, which is called when superblocks are committed.
    //
    // Find beacons that were activated by the superblock to be reverted and restore them to pending status. These come
    // from the beacon db.
    for (auto iter = m_beacons.begin(); iter != m_beacons.end();) {
        uint256 activation_hash = Hash(superblock_hash, iter->second->m_previous_hash);
        // If we have an active beacon whose hash matches the composite hash assigned by ActivatePending...
        if (iter->second->m_hash == activation_hash) {
            Cpid cpid = iter->second->m_cpid;

            // Find the pending beacon entry in the db before the activation. This is the previous state record. NOTE that this
            // find pulls the record from leveldb back into memory if the record had been passivated for memory savings before.
            auto pending_beacon_entry = m_beacon_db.find(iter->second->m_previous_hash);

            // If not found for some reason, move on.
            if (pending_beacon_entry == m_beacon_db.end())
            {
                error("%s: Superblock hash %s: No pending beacon for cpid %s found to restore in reversion of activated "
                      "beacon record.",
                      __func__,
                      superblock_hash.GetHex(),
                      cpid.ToString());
                continue;
            }

            // Resurrect the pending record prior to the activation. This points to the pending record still in the db.
            m_pending[static_cast<PendingBeacon>(*pending_beacon_entry->second).GetId()] =
                    pending_beacon_entry->second;

            // Erase the entry from the active beacons map. This also increments the iterator.
            iter = m_beacons.erase(iter);

            // Erase the entry from the db. This removes the record from the underlying historical map and also LevelDB.
            //  We do not need to retain this record because it is a reversion. The hash to use is the activation_hash,
            // because that was matched above.
            m_beacon_db.erase(activation_hash);
        } else {
            ++iter;
        }
    }

    // With the newer m_expired_pending set, the resurrection of expired pending beacons is relatively painless. We traverse
    // the m_expired_pending set and simply restore the pending beacon pointed to as the antecedent of each expired beacon in
    // the map. This is done by the m_beacon_db.find which will pull the beacon record from leveldb if it does not exist in
    // memory, which makes this passivation-proof up to a reorganization depth of the interval between two SB's (approximately
    // 960 blocks).
    for (const auto& iter : m_expired_pending) {
        // Get the pending beacon entry that is the antecedent of the expired entry.
        auto pending_beacon_entry = m_beacon_db.find(iter->m_previous_hash);

        // Resurrect pending beacon entry
        if (!m_pending.insert(std::make_pair(pending_beacon_entry->second->GetId(), pending_beacon_entry->second)).second) {
            LogPrintf("WARN: %s: Resurrected pending beacon entry, hash %s, from expired pending beacon for cpid %s during deactivation "
                      " of superblock hash %s already exists in the pending beacon map corresponding to beacon address %s.",
                      __func__,
                      pending_beacon_entry->second->m_hash.GetHex(),
                      pending_beacon_entry->second->m_cpid.ToString(),
                      superblock_hash.GetHex(),
                      pending_beacon_entry->second->GetAddress().ToString()
                      );
        }
    }

    // We clear the expired pending beacon map, as when the chain moves forward (perhaps on a different fork), the SB boundary will
    // (during the activation) repopulate the m_expired_pending map with a new set of expired_beacons. (This is very, very likely
    // to be the same set, BTW.)
    m_expired_pending.clear();

    // Note that making this foolproof in a reorganization across more than one SB boundary means we would have to repopulate the
    // expired pending beacon map from the PREVIOUS set of expired pending beacons. This would require a traversal of the entire
    // leveldb beacon structure for beacons, as it is keyed by beacon hash, not CPID or CKeyID. The expense is not worth it. In
    // artificial reorgs for testing purposes on testnet, where the chain is reorganized back thousands of blocks and then reorganized
    // forward along the same effective branch, the same superblocks will be restored using the same beacon activations as before,
    // which means in effect none of the expired beacons are ever used. In a real fork scenario, not repopulating the expired_pending
    // map limits the 100% foolproof reorg to the interval between SB's, which is approximately 960 blocks. This depth of reorg
    // in an operational network scenario is almost inconceivable, and if it actually happens we have other problems much worse
    // than the SLIGHT possibility of a different pending beacon being activated with the committed SB.

    // The original algorithm, which traversed m_beacon_db using an iterator, was actually broken, because passivation removes
    // elements from the m_beacon_db in memory map if there is only one remaining reference, which is the m_historical map that holds
    // references to all historical (non-current) entries. In the original algorithm, expired_pending entries were created in the
    // m_beacon_db, and the pending beacon pointer references were removed from m_pending, but no in memory map other than
    // m_historical kept a reference to the expired entry. This qualified the expired entry for passivation, so would
    // not necessarily be present to find in an iterator traversal of m_beacon_db. The iterator style traversal of m_beacon_db, unlike
    // the find, does NOT have the augmentation to pull passivated items from leveldb not in memory, because this would be
    // exceedingly expensive.
 }

//!
//! \brief BeaconRegistry::BeaconDB::HandleCurrentHistoricalEntries. This is a specialization of the RegistryDB template
//! HandleCurrentHistoricalEntries specific to Beacons. It handles the pending/active/renewal/expired pending/deleted
//! states and their interaction with the active entries map (m_beacons) and the pending entries map (m_pending) when loading
//! the beacon history from the beacon leveldb backing store during registry initialization. It is not intended to be used
//! for other specializations/overrides.
//!
//! \param entries
//! \param pending_entries
//! \param entry
//! \param historical_entry_ptr
//! \param recnum
//! \param key_type
//!
template<> void BeaconRegistry::BeaconDB::HandleCurrentHistoricalEntries(GRC::BeaconRegistry::BeaconMap& entries,
                                               GRC::BeaconRegistry::PendingBeaconMap& pending_entries,
                                                               std::set<Beacon_ptr>& expired_entries,
                                               const Beacon& entry,
                                               entry_ptr& historical_entry_ptr,
                                               const uint64_t& recnum,
                                               const std::string& key_type)
{
    // Note that in this specialization, entry.m_cpid and entry.GetId() are used for the map keys. In the general template,
    // entry.Key() is used (which here is the same as entry.m_cpid). No generalized method to implement entry.PendingKey()
    // has been implemented up to this point, because the pending map is actually only used here in the beacon
    // specialization.

    // If there is another registry class that arises that actually needs to use the "pending" state then it would be
    // necessary to implement the PendingKey() call in the template.

    if (entry.m_status == BeaconStatusForStorage::PENDING)
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: %ss: pending entry insert: cpid %s, address %s, timestamp %" PRId64 ", "
                 "hash %s, previous_hash %s, status %s, recnum %" PRId64 ".",
                 __func__,
                 key_type,
                 entry.KeyValueToString().first, // cpid
                 entry.KeyValueToString().second, // address
                 entry.m_timestamp, // timestamp
                 entry.m_hash.GetHex(), // transaction hash
                 entry.m_previous_hash.GetHex(), // prev beacon transaction hash
                 entry.StatusToString(), // status
                 recnum
                 );

        // Insert the pending beacon in the pending map.
        pending_entries[entry.GetId()] = historical_entry_ptr;
    }

    if (entry.m_status == BeaconStatusForStorage::ACTIVE || entry.m_status == BeaconStatusForStorage::RENEWAL)
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: %s: entry insert: cpid %s, address %s, timestamp %" PRId64 ", "
                "hash %s, previous_hash %s, status %s, recnum %" PRId64 ".",
                 __func__,
                 key_type,
                 entry.KeyValueToString().first, // cpid
                 entry.KeyValueToString().second, // address
                 entry.m_timestamp, // timestamp
                 entry.m_hash.GetHex(), // transaction hash
                 entry.m_previous_hash.GetHex(), // prev beacon transaction hash
                 entry.StatusToString(), // status
                 recnum
                 );

        // Insert or replace the existing map entry for the cpid with the latest active or renewed for that CPID.
        entries[entry.m_cpid] = historical_entry_ptr;

        // Delete any entry in the pending map with THE SAME public key.
        auto pending_to_delete = pending_entries.find(entry.GetId());
        if (pending_to_delete != pending_entries.end())
        {
            LogPrint(LogFlags::CONTRACT, "INFO: %s: %s: pending entry delete after active insert: cpid %s, address %s, "
                     "timestamp %" PRId64 ", hash %s, previous_hash %s, beacon status %s, recnum %" PRId64 ".",
                      __func__,
                     key_type,
                     pending_to_delete->second->KeyValueToString().first, // cpid
                     pending_to_delete->second->KeyValueToString().second, // address
                     pending_to_delete->second->m_timestamp, // timestamp
                     pending_to_delete->second->m_hash.GetHex(), // transaction hash
                     pending_to_delete->second->m_previous_hash.GetHex(), // prev beacon transaction hash
                     pending_to_delete->second->StatusToString(), // status
                     recnum
                     );

            pending_entries.erase(pending_to_delete);
        }
    }

    if (entry.m_status == BeaconStatusForStorage::ACTIVE) {
        // Note that in the original activation, all the activations happen for a superblock, and then the expired_entry set is
        // cleared and then new expired entries recorded from the just committed SB. This method operates at the record level, but
        // clearing the expired_entries for each ACTIVE record posting will achieve the same effect, because the entries are ordered
        // the proper way. It is a little bit of undesired work, but it is not worth the complexity of feeding the boundaries
        // of the group of verified beacons to activate.
        expired_entries.clear();
    }

    if (entry.m_status == BeaconStatusForStorage::EXPIRED_PENDING)
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: %s: expired pending entry delete: cpid %s, address %s, timestamp %" PRId64 ", "
                 "hash %s, previous_hash %s, beacon status %s, recnum %" PRId64 ".",
                  __func__,
                 key_type,
                 entry.KeyValueToString().first, // cpid
                 entry.KeyValueToString().second, // address
                 entry.m_timestamp, // timestamp
                 entry.m_hash.GetHex(), // transaction hash
                 entry.m_previous_hash.GetHex(), // prev beacon transaction hash
                 entry.StatusToString(), // status
                 recnum
                 );

        // Insert the expired pending entry into the expired entries set.
        expired_entries.insert(historical_entry_ptr);

        // Delete any entry in the pending map that is marked expired.
        pending_entries.erase(entry.GetId());
    }

    if (entry.m_status == BeaconStatusForStorage::DELETED) // Erase any entry in m_beacons and m_pending for the CPID.
    {
        LogPrint(LogFlags::CONTRACT, "INFO: %s: %s: entry delete: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                  "previous_hash %s, beacon status %s, recnum %" PRId64 ".",
                  __func__,
                 key_type,
                 entry.KeyValueToString().first, // cpid
                 entry.KeyValueToString().second, // address
                 entry.m_timestamp, // timestamp
                 entry.m_hash.GetHex(), // transaction hash
                 entry.m_previous_hash.GetHex(), // prev beacon transaction hash
                 entry.StatusToString(), // status
                 recnum
                 );

        entries.erase(entry.m_cpid);
        pending_entries.erase(entry.m_public_key.GetID());
    }
}

int BeaconRegistry::Initialize()
{
    int height = m_beacon_db.Initialize(m_beacons, m_pending, m_expired_pending);

    LogPrint(LogFlags::BEACON, "INFO: %s: m_beacon_db size after load: %u", __func__, m_beacon_db.size());
    LogPrint(LogFlags::BEACON, "INFO: %s: m_beacons size after load: %u", __func__, m_beacons.size());

    return height;
}

void BeaconRegistry::ResetInMemoryOnly()
{
    m_beacons.clear();
    m_pending.clear();
    m_expired_pending.clear();
    m_beacon_db.clear_in_memory_only();
}

uint64_t BeaconRegistry::PassivateDB()
{
    return m_beacon_db.passivate_db();
}

BeaconRegistry::BeaconDB &BeaconRegistry::GetBeaconDB()
{
    return m_beacon_db;
}

template<> const std::string BeaconRegistry::BeaconDB::KeyType()
{
    return std::string("beacon");
}
