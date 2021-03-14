// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
    , m_prev_beacon_hash()
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
        return now - g_v11_timestamp > 14 * 86400;
    }

    return false;
}

bool Beacon::Renewable(const int64_t now) const
{
    return Age(now) > RENEWAL_AGE;
}

bool Beacon::Renewed() const
{
    return (m_status == BeaconStatusForStorage::RENEWAL && m_prev_beacon_hash != uint256());
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
        + m_public_key.ToString());
}

bool Beacon::operator==(Beacon b)
{
    bool result = true;

    result &= (m_cpid == b.m_cpid);
    result &= (m_public_key == b.m_public_key);
    result &= (m_timestamp == b.m_timestamp);
    result &= (m_hash == b.m_hash);
    result &= (m_prev_beacon_hash == b.m_prev_beacon_hash);
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

std::vector<Beacon_ptr> BeaconRegistry::FindPending(const Cpid cpid) const
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
//! \brief This resets the in-memory maps of the registry. It does NOT
//! clear the leveldb storage.
//!
void BeaconRegistry::Reset()
{
    m_beacons.clear();
    m_pending.clear();
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
    renewal.m_prev_beacon_hash = current_beacon_ptr->m_hash;

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
    m_beacons[payload.m_cpid] = std::make_shared<Beacon>(renewal_iter->second);

    return true;
}

void BeaconRegistry::Add(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the leveldb database
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
        payload.m_beacon.m_prev_beacon_hash = current_beacon_ptr->m_hash;
    }
    else // Effectively Newbie.
    {
        payload.m_beacon.m_prev_beacon_hash = uint256 {};
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
        m_beacons[payload.m_cpid] = std::make_shared<Beacon>(m_beacon_db[ctx.m_tx.GetHash()]);
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
    m_pending[pending.GetId()] =  std::make_shared<Beacon>(m_beacon_db[ctx.m_tx.GetHash()]);
}

void BeaconRegistry::Delete(const ContractContext& ctx)
{
    // Poor man's mock. This is to prevent the tests from polluting the leveldb database
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
    deleted_beacon.m_prev_beacon_hash = last_active_ctx_hash;
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
            if (!renewal->m_prev_beacon_hash.IsNull())
            {
                Cpid cpid = iter->first;

                // Get the hash of the previous beacon. This normally could be a renewal, but
                // it could also be a gap advertisement or a force advertisement.
                uint256 renewal_hash = renewal->m_hash;
                uint256 resurrect_hash = renewal->m_prev_beacon_hash;

                // Erase the beacon that was ordered deleted.
                m_beacons.erase(iter);

                // Get an iterator from the beacon db (either in the historical table, or
                // will be loaded from leveldb and put in the historical table.
                auto resurrect_iter = m_beacon_db.find(resurrect_hash);

                if (resurrect_iter != m_beacon_db.end())
                {
                    // This is an element to the desired entry in the historical table.
                    Beacon& resurrected_beacon = resurrect_iter->second;

                    // Resurrect the prior beacon.
                    // ------------- cpid -------------- smart shared pointer to element in historical map.
                    m_beacons[cpid] = std::make_shared<Beacon>(resurrected_beacon);
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
    // deleted record in the beacon db. This could be an ACTIVE, RENWAL, or PENDING record. The EXPIRED_PENDING
    // records are handled in the Deactivate function, which is the block level Revert for the superblock reversion.
    if (ctx->m_action == ContractAction::REMOVE)
    {
        uint256 deletion_hash = ctx.m_tx.GetHash();

        auto deleted_beacon_record = m_beacon_db.find(deletion_hash);

        if (deleted_beacon_record != m_beacon_db.end())
        {
            auto record_to_restore = m_beacon_db.find(deleted_beacon_record->second.m_prev_beacon_hash);

            if (record_to_restore != m_beacon_db.end())
            {
                // Get a smart shared pointer to the beacon to restore
                Beacon_ptr beacon_to_restore_ptr = std::make_shared<Beacon>(record_to_restore->second);

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

bool BeaconRegistry::NeedsIsContractCorrection()
{
    return m_beacon_db.NeedsIsContractCorrection();
}

bool BeaconRegistry::SetNeedsIsContractCorrection(bool flag)
{
    return m_beacon_db.SetNeedsIsContractCorrection(flag);
}

bool BeaconRegistry::Validate(const Contract& contract, const CTransaction& tx) const
{
    if (contract.m_version <= 1) {
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
    const int64_t superblock_time, const uint256& block_hash, const int& height)
{
    LogPrint(LogFlags::BEACON, "INFO: %s: Called for superblock at height %i.", __func__, height);

    // Activate the pending beacons that are not expired with respect to pending age.
    for (const auto& id : beacon_ids) {
        auto iter_pair = m_pending.find(id);

        if (iter_pair != m_pending.end()) {

            Beacon_ptr found_pending_beacon = iter_pair->second;

            // Create a new beacon to activate from the found pending beacon.
            Beacon activated_beacon(*iter_pair->second);

            // Update the new beacon's prev hash to be the hash of the pending beacon that is being activated.
            activated_beacon.m_prev_beacon_hash = found_pending_beacon->m_hash;

            // We are going to have to use a composite hash for these because activation is not done as
            // individual transactions. Rather groups are done in each superblock under one hash. The
            // hash of the block hash, and the pending beacon that is being activated's hash is sufficient.
            activated_beacon.m_status = BeaconStatusForStorage::ACTIVE;

            activated_beacon.m_hash = Hash(block_hash.begin(),
                                           block_hash.end(),
                                           found_pending_beacon->m_hash.begin(),
                                           found_pending_beacon->m_hash.end());

            LogPrint(LogFlags::BEACON, "INFO: %s: Activating beacon for cpid %s, address %s, hash %s.",
                     __func__,
                     activated_beacon.m_cpid.ToString(),
                     activated_beacon.GetAddress().ToString(),
                     activated_beacon.m_hash.GetHex());

            // It is possible that more than one pending beacon with the same CPID can be attempted to be
            // activated in the same superblock. The behavior here to agree with the original implementation
            // is the last one. So get rid of any previous one activated/inserted.
            auto found_already_activated_beacon = m_beacon_db.find(activated_beacon.m_hash);
            if (found_already_activated_beacon != m_beacon_db.end())
            {
                m_beacon_db.erase(activated_beacon.m_hash);
            }

            m_beacon_db.insert(activated_beacon.m_hash, height, activated_beacon);

            // This is the subscript form of insert. Important here because an activated beacon should
            // overwrite any existing entry in the m_beacons map.
            m_beacons[activated_beacon.m_cpid] =
                    std::make_shared<Beacon>(m_beacon_db[activated_beacon.m_hash]);

            // Remove the pending beacon entry from the pending map. (Note this entry still exists in the historical
            // table and the db.
            m_pending.erase(iter_pair);
        }
    }

    // Discard pending beacons that are expired with respect to pending age.
    for (auto iter = m_pending.begin(); iter != m_pending.end(); /* no-op */) {
        PendingBeacon pending_beacon(*iter->second);

        if (pending_beacon.PendingExpired(superblock_time)) {
            // Set the expired pending beacon's previous beacon hash to the beacon entry's hash.
            pending_beacon.m_prev_beacon_hash = pending_beacon.m_hash;

            // Mark the status as EXPIRED_PENDING.
            pending_beacon.m_status = BeaconStatusForStorage::EXPIRED_PENDING;

            // Set the beacon entry's hash to a synthetic block hash similar to above.
            pending_beacon.m_hash = Hash(block_hash.begin(),
                                         block_hash.end(),
                                         pending_beacon.m_hash.begin(),
                                         pending_beacon.m_hash.end());

            LogPrint(LogFlags::BEACON, "INFO: %s: Marking pending beacon expired for cpid %s, address %s, hash %s.",
                     __func__,
                     pending_beacon.m_cpid.ToString(),
                     pending_beacon.GetAddress().ToString(),
                     pending_beacon.m_hash.GetHex());

            // Insert the expired pending beacon into the db.
            m_beacon_db.insert(pending_beacon.m_hash, height, static_cast<Beacon>(pending_beacon));
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
        Cpid cpid = iter->second->m_cpid;

        uint256 activation_hash = Hash(superblock_hash.begin(),
                                       superblock_hash.end(),
                                       iter->second->m_prev_beacon_hash.begin(),
                                       iter->second->m_prev_beacon_hash.end());

        // If we have an active beacon whose hash matches the composite hash assigned by ActivatePending...
        if (iter->second->m_hash == activation_hash) {
            // Find the pending beacon entry in the db before the activation. This is the previous state record.
            auto pending_beacon_entry = m_beacon_db.find(iter->second->m_prev_beacon_hash);

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
            m_pending[static_cast<PendingBeacon>(pending_beacon_entry->second).GetId()] =
                    std::make_shared<Beacon>(pending_beacon_entry->second);

            // Erase the entry from the active beacons map. This also increments the iterator.
            iter = m_beacons.erase(iter);

            // Erase the entry from the db. This removes the record from the underlying historical map and also leveldb.
            //  We do not need to retain this record because it is a reversion. The hash to use is the activation_hash,
            // because that was matched above.
            m_beacon_db.erase(activation_hash);
        } else {
            ++iter;
        }
    }

    // Find pending beacons that were removed from the pending beacon map and marked PENDING_EXPIRED and restore them
    // back to pending status. Unfortunately, the beacon_db has to be traversed for this, because it is the only entity
    // that has the records at this point. This will only be done very rarely, when a reorganization crosses a
    // superblock commit.
    auto iter = m_beacon_db.begin();

    while (iter != m_beacon_db.end())
    {
        // The cpid in the historical beacon record to be matched.
        Cpid cpid = iter->second.m_cpid;

        uint256 match_hash = Hash(superblock_hash.begin(),
                                       superblock_hash.end(),
                                       iter->second.m_prev_beacon_hash.begin(),
                                       iter->second.m_prev_beacon_hash.end());

        // If the calculated match_hash matches the key (hash) of the historical beacon record, then
        // restore the previous record pointed to by the historical beacon record to the pending map.
        if (match_hash == iter->first)
        {
            uint256 resurrect_pending_hash = iter->second.m_prev_beacon_hash;

            if (!resurrect_pending_hash.IsNull())
            {
                Beacon_ptr resurrected_pending = std::make_shared<Beacon>(m_beacon_db.find(resurrect_pending_hash)->second);

                // Check that the status of the beacon to resurrect is PENDING. If it is not log an error but continue
                // anyway.
                if (resurrected_pending->m_status != BeaconStatusForStorage::PENDING)
                {
                    error("%s: Superblock hash %s: The beacon for cpid %s pointed to by an EXPIRED_PENDING beacon to be "
                          "put back in PENDING status does not have the expected status of PENDING. The beacon hash is %s "
                          "and the status is %i",
                          __func__,
                          superblock_hash.GetHex(),
                          cpid.ToString(),
                          resurrected_pending->m_hash.GetHex(),
                          resurrected_pending->m_status.Raw());
                }

                // Put the record in m_pending.
                m_pending[resurrected_pending->GetId()] = resurrected_pending;
            }
            else
            {
                error("%s: Superblock hash %s: The beacon for cpid %s with an EXPIRED_PENDING status has no valid "
                      "previous beacon hash with which to restore the PENDING beacon.",
                      __func__,
                      superblock_hash.GetHex(),
                      cpid.ToString());
            }
        } //matched EXPIRED_PENDING record

        iter = m_beacon_db.advance(iter);
    } // m_beacon_db traversal
}

int BeaconRegistry::Initialize()
{
    int height = m_beacon_db.Initialize(m_pending, m_beacons);

    LogPrint(LogFlags::BEACON, "INFO %s: m_beacon_db size after load: %u", __func__, m_beacon_db.size());
    LogPrint(LogFlags::BEACON, "INFO %s: m_beacons size after load: %u", __func__, m_beacons.size());

    return height;
}

int BeaconRegistry::BeaconDB::Initialize(PendingBeaconMap& m_pending, BeaconMap& m_beacons)
{
    bool status = true;
    int height = 0;
    uint32_t version = 0;
    bool needs_IsContract_correction = false;

    // First load the beacon db version from leveldb and check it against the constant in the class.
    {
        CTxDB txdb("r");

        std::pair<std::string, std::string> key = std::make_pair("beacon_db", "version");

        bool status = txdb.ReadGenericSerializable(key, version);

        if (!status) version = 0;
    }

    if (version != CURRENT_VERSION)
    {
        LogPrint(LogFlags::BEACON, "WARNING: %s: Version level of the beacon db stored in leveldb, %u, does not "
                                   "match that required in this code level, version %u. Clearing the leveldb beacon "
                                   "storage and setting version level to match this code level.",
                 __func__,
                 version,
                 CURRENT_VERSION);

        // Version 1, which corresponds to the 5.2.1.0 release contained a bug in ApplyContracts which prevented the
        // application of multiple beacon contracts contained in a block under certain circumstances. In particular,
        // the case with superblock 2053368, which contained beacon activations AND two beacon contracts was affected
        // and once recorded in CBlockIndex, IsContract() returns the incorrect value. This flag will be used by
        // ApplyContracts to check every block during the ContractReplay to correct the situation.
        if (version == 1)
        {
            needs_IsContract_correction = true;
        }

        clear_leveldb();

        // After clearing the leveldb state, set the needs IsContract correction to the proper state. If the version that
        // was on disk was 1, then this will be set to true.
        SetNeedsIsContractCorrection(needs_IsContract_correction);

        LogPrint(LogFlags::BEACON, "INFO: %s: Leveldb beacon area cleared. Version level set to %u.",
                 __func__,
                 CURRENT_VERSION);
    }


    // If LoadDBHeight not successful or height is zero then leveldb has not been initialized before.
    // LoadDBHeight will also set the private member variable m_height_stored from leveldb for this first call.
    if (!LoadDBHeight(height) || !height)
    {
        return height;
    }
    else // Leveldb already initialized from a prior run.
    {
        // Set m_database_init to true. This will cause LoadDBHeight hereinafter to simply report
        // the value of m_height_stored rather than loading the stored height from leveldb.
        m_database_init = true;

        // We are in a restart where at least some of a rescan was completed during a prior run. It is possible
        // that the rescan may have not been completed before a shutdown was issued. In that case the
        // needs_IsContract_correction flag will be set to true in leveldb, so restore that state for this run
        // to ensure the correction finishes. When ReplayContracts finishes the corrections, it will mark the flag
        // false.
        CTxDB txdb("r");

        std::pair<std::string, std::string> key = std::make_pair("beacon_db", "needs_IsContract_correction");

        bool status = txdb.ReadGenericSerializable(key, needs_IsContract_correction);

        if (!status) needs_IsContract_correction = false;

        m_needs_IsContract_correction = needs_IsContract_correction;
    }

    LogPrint(LogFlags::BEACON, "INFO: %s: db stored height at block %i.",
             __func__,
             height);


    // Now load the beacons from leveldb.

    std::string key_type = "beacon";

    // This temporary map is keyed by record number, which insures the replay down below occurs in the right order.
    StorageBeaconMapByRecordNum storage_by_record_num;

    // Code block to scope the txdb object.
    {
        CTxDB txdb("r");

        uint256 hash_hint = uint256();

        // Load the temporary which is similar to m_historical, except the elements are of type BeaconStorage instead
        // of Beacon.
        status = txdb.ReadGenericSerializablesToMapWithForeignKey(key_type, storage_by_record_num, hash_hint);
    }

    if (!status)
    {
        if (height > 0)
        {
            // For the height be greater than zero from the height K-V, but the read into the map to fail
            // means the storage in leveldb must be messed up in the beacon area and not be in concordance with
            // the beacon_db K-V's. Therefore clear the whole thing.
            clear();
        }

        // Return height of zero.
        return 0;
    }

    uint64_t recnum_high_watermark = 0;

    // Replay the storage map. The iterator is ordered by record number, which ensures that the correct
    // elements end up in m_beacons and m_pending. Storage entries that are "mark deletions" are also inserted
    // and any entry in m_beacons (the active map) is removed.
    for (const auto& iter : storage_by_record_num)
    {
        const uint64_t& recnum = iter.first;
        Beacon beacon = static_cast<Beacon>(iter.second);

        recnum_high_watermark = std::max(recnum_high_watermark, recnum);

        LogPrint(LogFlags::BEACON, "INFO: %s: m_historical insert: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                  "prev_beacon_hash %s, beacon status = %u, recnum = %" PRId64 ".",
                  __func__,
                  beacon.m_cpid.ToString(), // cpid
                  beacon.GetAddress().ToString(), // address
                  beacon.m_timestamp, // timestamp
                  beacon.m_hash.GetHex(), // transaction hash
                  beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                  beacon.m_status.Raw(), // status
                  recnum
                  );

        // Insert the entry into the historical map. This includes ctx's where the beacon is marked deleted.
        // --------------- hash ---------- does NOT include the Cpid.
        m_historical[iter.second.m_hash] = beacon;
        Beacon& historical_beacon = m_historical[iter.second.m_hash];

        if (beacon.m_status == BeaconStatusForStorage::PENDING)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_pending insert: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u - (1) PENDING, recnum = %" PRId64 ".",
                      __func__,
                      beacon.m_cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw(), // status
                      recnum
                      );

            // Insert the pending beacon in the pending map.
            m_pending[beacon.GetId()] = std::make_shared<Beacon>(historical_beacon);
        }

        if (beacon.m_status == BeaconStatusForStorage::ACTIVE || beacon.m_status == BeaconStatusForStorage::RENEWAL)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_beacons insert: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u - (2) ACTIVE or (3) RENEWAL, recnum = %" PRId64 ".",
                      __func__,
                      beacon.m_cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw(), // status
                      recnum
                      );

            // Insert or replace the existing map entry for the cpid with the latest active or renewed for that CPID.
            m_beacons[beacon.m_cpid] = std::make_shared<Beacon>(historical_beacon);

            // Delete any entry in the pending map with THE SAME public key.
            auto pending_to_delete = m_pending.find(beacon.GetId());
            if (pending_to_delete != m_pending.end())
            {
                LogPrint(LogFlags::BEACON, "INFO: %s: m_pending delete after active insert: cpid %s, address %s, "
                         "timestamp %" PRId64 ", hash %s, "
                         "prev_beacon_hash %s, beacon status = %u - (2), recnum = %" PRId64 ".",
                          __func__,
                          pending_to_delete->second->m_cpid.ToString(), // cpid
                          pending_to_delete->second->GetAddress().ToString(), // address
                          pending_to_delete->second->m_timestamp, // timestamp
                          pending_to_delete->second->m_hash.GetHex(), // transaction hash
                          pending_to_delete->second->m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                          pending_to_delete->second->m_status.Raw(), // status
                          recnum
                          );

                m_pending.erase(pending_to_delete);
            }
        }

        if (beacon.m_status == BeaconStatusForStorage::EXPIRED_PENDING)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_pending delete: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u, recnum = %" PRId64 ".",
                      __func__,
                      beacon.m_cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw(), // status
                      recnum
                      );

            // Delete any entry in the pending map that is marked expired.
            m_pending.erase(beacon.GetId());
        }

        if (beacon.m_status == BeaconStatusForStorage::DELETED) // Erase any entry in m_beacons and m_pending for the CPID.
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_beacons delete: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u, recnum = %" PRId64 ".",
                      __func__,
                      beacon.m_cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw(), // status
                      recnum
                      );

            m_beacons.erase(beacon.m_cpid);
            m_pending.erase(beacon.m_public_key.GetID());
        }
    }

    // Set the in-memory record number stored variable to the highest recnum encountered during the replay above.
    m_recnum_stored = recnum_high_watermark;

    return height;
}

void BeaconRegistry::ResetInMemoryOnly()
{
    m_beacons.clear();
    m_pending.clear();
    m_beacon_db.clear_in_memory_only();
}

// Required to make the linker happy.
constexpr uint32_t BeaconRegistry::BeaconDB::CURRENT_VERSION;

void BeaconRegistry::BeaconDB::clear_in_memory_only()
{
    m_historical.clear();
    m_database_init = false;
    m_height_stored = 0;
    m_recnum_stored = 0;
}

bool BeaconRegistry::BeaconDB::clear_leveldb()
{
    bool status = true;

    CTxDB txdb("rw");

    std::string key_type = "beacon";
    uint256 start_key_hint_beacon = uint256();

    status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint_beacon);

    key_type = "beacon_db";
    std::string start_key_hint_beacon_db {};

    status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint_beacon_db);

    // We want to write back into leveldb the revision level of the db in the running code.
    std::pair<std::string, std::string> key = std::make_pair(key_type, "version");
    status &= txdb.WriteGenericSerializable(key, CURRENT_VERSION);

    m_height_stored = 0;
    m_recnum_stored = 0;
    m_database_init = false;
    m_needs_IsContract_correction = false;

    return status;
}

bool BeaconRegistry::BeaconDB::clear()
{
    clear_in_memory_only();

    return clear_leveldb();
}

size_t BeaconRegistry::BeaconDB::size()
{
    return m_historical.size();
}

bool BeaconRegistry::BeaconDB::NeedsIsContractCorrection()
{
    return m_needs_IsContract_correction;
}

bool BeaconRegistry::BeaconDB::SetNeedsIsContractCorrection(bool flag)
{
    // Update the in-memory flag.
    m_needs_IsContract_correction = flag;

    // Update leveldb
    CTxDB txdb("rw");

    std::pair<std::string, std::string> key = std::make_pair("beacon_db", "needs_IsContract_correction");

    return txdb.WriteGenericSerializable(key, m_needs_IsContract_correction);
}

bool BeaconRegistry::BeaconDB::StoreDBHeight(const int& height_stored)
{
    // Update the in-memory bookmark variable.
    m_height_stored = height_stored;

    // Update leveldb.
    CTxDB txdb("rw");

    std::pair<std::string, std::string> key = std::make_pair("beacon_db", "height_stored");

    return txdb.WriteGenericSerializable(key, height_stored);
}

bool BeaconRegistry::BeaconDB::LoadDBHeight(int& height_stored)
{
    bool status = true;

    // If the database has already been initialized (which includes loading the height to what the
    // beacon storage was updated), then just report the valud of m_height_stored, otherwise
    // pull the value from leveldb.
    if (m_database_init)
    {
        height_stored = m_height_stored;
    }
    else
    {
        CTxDB txdb("r");

        std::pair<std::string, std::string> key = std::make_pair("beacon_db", "height_stored");

        bool status = txdb.ReadGenericSerializable(key, height_stored);

        if (!status) height_stored = 0;

        m_height_stored = height_stored;
    }

    return status;
}

bool BeaconRegistry::BeaconDB::insert(const uint256 &hash, const int& height, const Beacon &beacon)
{
    bool status = false;


    if (m_historical.find(hash) != m_historical.end())
    {
        return status;
    }
    else
    {
        LogPrint(LogFlags::BEACON, "INFO %s - store beacon: cpid %s, address %s, height %i, timestamp %" PRId64
                  ", hash %s, prev_beacon_hash %s, status = %u.",
                  __func__,
                  beacon.m_cpid.ToString(), // cpid
                  beacon.GetAddress().ToString(), // address
                  height, // height
                  beacon.m_timestamp, // timestamp
                  beacon.m_hash.GetHex(), // transaction hash
                  beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                  beacon.m_status.Raw() // status
                  );

        m_historical.insert(std::make_pair(hash, beacon));

        status = Store(hash, static_cast<StorageBeacon>(beacon));

        if (height) status &= StoreDBHeight(height);

        return status;
    }
}

bool BeaconRegistry::BeaconDB::erase(uint256 hash)
{
    auto iter = m_historical.find(hash);

    if (iter != m_historical.end())
    {
        m_historical.erase(hash);
    }

    return Delete(hash);
}

BeaconRegistry::HistoricalBeaconMap::iterator BeaconRegistry::BeaconDB::begin()
{
    return m_historical.begin();
}

BeaconRegistry::HistoricalBeaconMap::iterator BeaconRegistry::BeaconDB::end()
{
    return m_historical.end();
}

BeaconRegistry::HistoricalBeaconMap::iterator BeaconRegistry::BeaconDB::find(uint256& hash)
{
    // See if beacon from that ctx_hash is already in the historical map. If so, get iterator.
    auto iter = m_historical.find(hash);

    // If it isn't, attempt to load the beacon from leveldb into the map.
    if (iter == m_historical.end())
    {
        StorageBeacon beacon;

        // If the load from leveldb is successful, insert into the historical map and return the iterator.
        if (Load(hash, beacon))
        {
            iter = m_historical.insert(std::make_pair(hash, static_cast<Beacon>(beacon))).first;
        }
    }

    // Note that if there is no entry in m_historical, and also there is no K-V in leveldb, then an
    // iterator at end() will be returned.
    return iter;
}

// TODO: A poor man's forward iterator. Implement a full wrapper iterator. Maybe don't need it though.
BeaconRegistry::HistoricalBeaconMap::iterator BeaconRegistry::BeaconDB::advance(HistoricalBeaconMap::iterator iter)
{
    return ++iter;
}

Beacon& BeaconRegistry::BeaconDB::operator[](const uint256& hash)
{
    return m_historical[hash];
}

bool BeaconRegistry::BeaconDB::Store(const uint256& hash, const StorageBeacon& beacon)
{
    CTxDB txdb("rw");

    ++m_recnum_stored;

    std::pair<std::string, uint256> key = std::make_pair("beacon", hash);

    return txdb.WriteGenericSerializable(key, std::make_pair(m_recnum_stored, beacon));
}

bool BeaconRegistry::BeaconDB::Load(uint256& hash, StorageBeacon& beacon)
{
    CTxDB txdb("r");

    std::pair<std::string, uint256> key = std::make_pair("beacon", hash);

    std::pair<uint64_t, StorageBeacon> beacon_pair;

    bool status = txdb.ReadGenericSerializable(key, beacon_pair);

    beacon = beacon_pair.second;

    return status;
}

bool BeaconRegistry::BeaconDB::Delete(uint256& hash)
{
    CTxDB txdb("rw");

    std::pair<std::string, uint256> key = std::make_pair("beacon", hash);

    return txdb.EraseGenericSerializable(key);
}

BeaconRegistry::BeaconDB &BeaconRegistry::GetBeaconDB()
{
    return m_beacon_db;
}
