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
    , m_status(BeaconStatusForStorage::INIT)
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
    m_beacon_db.update(renewal.m_hash, height, renewal);

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

        m_beacon_db.insert(ctx.m_tx.GetHash(), height, historical);
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
    m_beacon_db.insert(ctx.m_tx.GetHash(), height, static_cast<Beacon>(pending));

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

    if (ctx->m_version >= 2) {
        m_pending.erase(payload->m_beacon.GetId());
    }

    // Reverting a beacon in the registry is made tricky by the renewals. Note that reversion is NOT
    // the same as deletion. Reversion is the return to the state PRIOR to the application of the
    // supplied ctx argument.

    auto iter = m_beacons.find(payload->m_cpid);

    // The beacon exists and has a valid prev beacon hash...
    if (iter != m_beacons.end())
    {
        if (!iter->second->m_prev_beacon_hash.IsNull())
        {
            Cpid cpid = iter->first;

            // Get the hash of the previous beacon. This normally could be a renewal, but
            // it could also be a gap advertisement or a force advertisement.
            uint256 resurrect_hash = iter->second->m_prev_beacon_hash;

            // Erase the beacon that was ordered deleted.
            m_beacons.erase(payload->m_cpid);

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
        }
        else // Just erase the beacon ordered deleted.
        {
            m_beacons.erase(payload->m_cpid);
        }
    }

    // Erase the entry in the db. (Remember if this was a renewal, the prior entry was resurrected
    // above from leveldb.)
    m_beacon_db.erase(ctx.m_tx.GetHash());
}

void BeaconRegistry::SetDBHeight(int& height)
{
    if (m_beacon_db.StoreDBHeight(height))
    {
        m_beacon_db.m_height_stored = height;
    }
}

int BeaconRegistry::GetDBHeight()
{
    return m_beacon_db.m_height_stored;
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
            Cpid& cpid = found_pending_beacon->m_cpid;

            LogPrint(LogFlags::BEACON, "INFO: %s: Activating beacon for cpid %s.", __func__, cpid.ToString());

            // Update the new beacon's prev hash to be the hash of the pending beacon that is being activated.
            activated_beacon.m_prev_beacon_hash = found_pending_beacon->m_hash;

            // We are going to have to use a "synthetic hash" for these because activation is not done as
            // individual transactions. Rather groups are done in each superblock under one hash. The
            // hash of the block hash and the cpid raw array should be sufficient.
            activated_beacon.m_hash = Hash(block_hash.begin(), block_hash.end(),
                                           cpid.Raw().begin(), cpid.Raw().end());

            activated_beacon.m_status = BeaconStatusForStorage::ACTIVE;

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
        // TODO: the static cast here is to ensure to use the correct Expired method (the pending beacon one rather
        // than the active beacon one.

        //PendingBeacon pending_beacon = static_cast<PendingBeacon>(*iter->second);
        if (static_cast<PendingBeacon>(*iter->second).Expired(superblock_time)) {
            Beacon expired_pending_beacon = static_cast<Beacon>(*iter->second);

            // Mark the status as EXPIRED_PENDING.
            expired_pending_beacon.m_status = BeaconStatusForStorage::EXPIRED_PENDING;
            // Set the beacon entry's hash to a synthetic block hash similar to above.
            expired_pending_beacon.m_hash = Hash(block_hash.begin(),
                                                 block_hash.end(),
                                                 expired_pending_beacon.m_cpid.Raw().begin(),
                                                 expired_pending_beacon.m_cpid.Raw().end());
            // Set the storage beacon's previous beacon hash to the current (expired) pending beacon entry's hash.
            expired_pending_beacon.m_prev_beacon_hash = iter->second->m_hash;
            // Insert the expired_pending_beacon into the db.
            m_beacon_db.insert(block_hash, height, expired_pending_beacon);
            // Remove the pending beacon entry from the m_pending map.
            iter = m_pending.erase(iter);
        } else {
            ++iter;
        }
    }
}

void BeaconRegistry::Deactivate(const int64_t superblock_time)
{
    for (auto iter = m_beacons.begin(); iter != m_beacons.end(); /* no-op */) {
        // If we have an active beacon that is equal to or greater than the superblock_time...
        if (iter->second->m_timestamp >= superblock_time) {

            //PendingBeacon pending(iter->first, *iter->second);

            // Find the pending beacon entry in the db before the activation.
            auto pending_beacon_entry = m_beacon_db.find(iter->second->m_prev_beacon_hash);

            // If not found for some reason, move on.
            if (pending_beacon_entry == m_beacon_db.end()) continue;

            // Resurrect the pending record prior to the activation. This points to the pending record still in the db.
            m_pending[static_cast<PendingBeacon>(pending_beacon_entry->second).GetId()] =
                    std::make_shared<Beacon>(pending_beacon_entry->second);

            // Get the hash to erase in the db. (The active beacon record to delete.
            uint256 hash_to_erase = iter->second->m_hash;

            // Erase the entry from the active beacons map.
            iter = m_beacons.erase(iter);

            // Erase the entry from the db. This removes the record from the underlying historical map and also leveldb.
            m_beacon_db.erase(hash_to_erase);
        } else {
            ++iter;
        }
    }
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

    // if load DB height not successful or height is zero
    // then leveldb has not been initialized before
    if (!LoadDBHeight(height) || !height)
    {
        // Return. No point in trying to load beacons that don't exist in the database.
        m_height_stored = height;
        return height;
    }
    else // Already initialized from a prior run.
    {
        // Set m_height_stored to height returned from LoadDBheight and m_database_init to true.
        m_height_stored = height;
        m_database_init = true;
    }

    // Find the time equivalent of the block height that the beacon db was up to in storage. (This is to
    // cull expired (from active) beacons and expired pending beacons.)
    CBlockIndex* pblock_index = mapBlockIndex[hashBestChain];
    while (pblock_index->nHeight > height)
    {
        pblock_index = pblock_index->pprev;
    }

    int64_t db_time = pblock_index->nTime;
    LogPrint(LogFlags::BEACON, "INFO: %s: db stored height timestamp = %" PRId64 ".", __func__, db_time);

    // Now load the beacons from leveldb.

    std::string key_type = "beacon";

    // Hideous that we need two temporary maps here.
    // The first one supports the loading of the leveldb elements into a map keyed by hash, but without
    // destroying the cpid information.
    // The second supports the keying of the beaconstorage elements by the pair (cpid, timestamp)
    // to ensure ordering by m_timestamp within the same CPID for the replay.
    StorageBeaconMapByCpidTime storage_by_cpid_time;
    // Use a code block to get rid of the storage temporary map ASAP.
    {
       StorageBeaconMap storage;

       // Code block to scope the txdb object.
       {
           CTxDB txdb("r");

           // Load the temporary which is similar to m_historical, except the elements are of type BeaconStorage instead
           // of Beacon.
           status = txdb.ReadGenericSerializablesToMap(key_type, storage, m_beacon_init_hash_hint);
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

           height = 0;
           m_height_stored = 0;
           m_database_init = false;
           return height;
       }

       // Load the storage_by_cpid multimap from the temporary storage map.
       for (const auto& iter : storage)
       {
           storage_by_cpid_time.insert(std::make_pair(std::make_pair(iter.second.m_cpid, iter.second.m_timestamp),
                                                      iter.second));
       }
    }

    // Replay the storage map. The iterator is ordered by cpid and then by timestamp, which ensures that the correct
    // elements end up in m_beacons. Storage entries that are "mark deletions" are also inserted and any entry in
    // m_beacons (the active map) is removed.
    for (const auto& iter : storage_by_cpid_time)
    {
        const Cpid& cpid = iter.first.first;
        Beacon beacon = static_cast<Beacon>(iter.second);

        LogPrint(LogFlags::BEACON, "INFO: %s: m_historical insert: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                  "prev_beacon_hash %s, beacon status = %u.",
                  __func__,
                  cpid.ToString(), // cpid
                  beacon.GetAddress().ToString(), // address
                  beacon.m_timestamp, // timestamp
                  beacon.m_hash.GetHex(), // transaction hash
                  beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                  beacon.m_status.Raw() // status
                  );

        // Insert the entry into the historical map. This includes ctx's where the beacon is marked deleted.
        // --------------- hash ---------- does NOT include the Cpid.
        m_historical[iter.second.m_hash] = beacon;
        Beacon& historical_beacon = m_historical[iter.second.m_hash];

        if (beacon.m_status == BeaconStatusForStorage::PENDING)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_pending insert: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u - (1) PENDING.",
                      __func__,
                      cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw() // status
                      );

            // Insert the pending beacon in the pending map.
            m_pending[beacon.GetId()] = std::make_shared<Beacon>(historical_beacon);
        }

        if (beacon.m_status == BeaconStatusForStorage::ACTIVE || beacon.m_status == BeaconStatusForStorage::RENEWAL)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_beacons insert: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u - (2) ACTIVE or (3) RENEWAL.",
                      __func__,
                      cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw() // status
                      );

            // Insert or replace the existing map entry for the cpid with the latest active or renewed for that CPID.
            m_beacons[cpid] = std::make_shared<Beacon>(historical_beacon);

            // Delete any entry in the pending map with THE SAME public key.
            auto pending_to_delete = m_pending.find(beacon.GetId());
            if (pending_to_delete != m_pending.end())
            {
                LogPrint(LogFlags::BEACON, "INFO: %s: m_pending delete after active insert: cpid %s, address %s, "
                         "timestamp %" PRId64 ", hash %s, "
                         "prev_beacon_hash %s, beacon status = %u - (2).",
                          __func__,
                          pending_to_delete->second->m_cpid.ToString(), // cpid
                          pending_to_delete->second->GetAddress().ToString(), // address
                          pending_to_delete->second->m_timestamp, // timestamp
                          pending_to_delete->second->m_hash.GetHex(), // transaction hash
                          pending_to_delete->second->m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                          pending_to_delete->second->m_status.Raw() // status
                          );

                m_pending.erase(pending_to_delete);
            }
        }

        if (beacon.m_status == BeaconStatusForStorage::EXPIRED_PENDING)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_pending delete: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u.",
                      __func__,
                      cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw() // status
                      );

            // Delete any entry in the pending map that is marked expired.
            m_pending.erase(beacon.GetId());
        }

        if (beacon.m_status == BeaconStatusForStorage::DELETED) // Erase any entry in m_beacons for the CPID.
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_beacons delete: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u.",
                      __func__,
                      cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw() // status
                      );

            m_beacons.erase(cpid);
        }
    }

    /*
    // Erase any remaining pending beacons that are expired as of the height / time at height of the db.
    for (auto iter = m_pending.begin(); iter != m_pending.end();)
    {
        const PendingBeacon& beacon = static_cast<PendingBeacon>(*iter->second);

        // TODO: Fix PendingBeacon and Beacon class nonsense and use Expired() function.
        if (db_time - beacon.m_timestamp > 60 * 60 * 24 * 30 * 5)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_pending delete: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u.",
                      __func__,
                      beacon.m_cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw() // status
                      );

            iter = m_pending.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    // Erase any active beacons that are expired as of the height / time at height of the db.
    for (auto iter = m_beacons.begin(); iter != m_beacons.end();)
    {
        const Beacon& beacon = static_cast<Beacon>(*iter->second);

        // TODO: Fix PendingBeacon and Beacon class nonsense and use Expired() function.
        bool expired = false;

        if (db_time - beacon.m_timestamp > 0 * 60 * 24 * 30 * 6) {
            expired = true;
        }
        // Temporary transition to version 2 beacons after the block version 11
        // hard-fork:
        else if (beacon.m_timestamp <= g_v11_timestamp && db_time - g_v11_timestamp > 14 * 86400) {
            expired = true;
        }

        if (expired)
        {
            LogPrint(LogFlags::BEACON, "INFO: %s: m_beacons delete: cpid %s, address %s, timestamp %" PRId64 ", hash %s, "
                      "prev_beacon_hash %s, beacon status = %u.",
                      __func__,
                      beacon.m_cpid.ToString(), // cpid
                      beacon.GetAddress().ToString(), // address
                      beacon.m_timestamp, // timestamp
                      beacon.m_hash.GetHex(), // transaction hash
                      beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
                      beacon.m_status.Raw() // status
                      );
            iter = m_beacons.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    */

    return height;
}

void BeaconRegistry::ResetMapsOnly()
{
    m_beacons.clear();
    m_pending.clear();
    m_beacon_db.clear_map();
}

void BeaconRegistry::BeaconDB::clear_map()
{
    m_historical.clear();
}

bool BeaconRegistry::BeaconDB::clear_leveldb()
{
    bool status = true;

    CTxDB txdb("rw");

    std::string key_type = "beacon_db";
    std::string start_key_hint_beacon_db {};

    status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint_beacon_db);

    key_type = "beacon";
    uint256 start_key_hint_beacon = uint256();

    status &= txdb.EraseGenericSerializablesByKeyType(key_type, start_key_hint_beacon);

    return status;
}

bool BeaconRegistry::BeaconDB::clear()
{
    clear_map();

    return clear_leveldb();
}

size_t BeaconRegistry::BeaconDB::size()
{
    return m_historical.size();
}

bool BeaconRegistry::BeaconDB::StoreDBHeight(const int& height_stored)
{
    CTxDB txdb("rw");

    std::pair<std::string, std::string> key = std::make_pair("beacon_db", "height_stored");

    return txdb.WriteGenericSerializable(key, height_stored);
}

bool BeaconRegistry::BeaconDB::LoadDBHeight(int& height_stored)
{
    CTxDB txdb("r");

    std::pair<std::string, std::string> key = std::make_pair("beacon_db", "height_stored");

    bool status = txdb.ReadGenericSerializable(key, height_stored);

    if (!status) height_stored = 0;

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
        LogPrint(LogFlags::BEACON, "INFO %s - store beacon: cpid %s, address %s, timestamp %" PRId64
                  ", hash %s, prev_beacon_hash %s, status = %u.",
                  __func__,
                  beacon.m_cpid.ToString(), // cpid
                  beacon.GetAddress().ToString(), // address
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

bool BeaconRegistry::BeaconDB::update(const uint256 &hash, const int& height, const Beacon &beacon)
{
    bool status = false;

    LogPrint(LogFlags::BEACON, "INFO %s - store beacon: cpid %s, address %s, timestamp %" PRId64
              ", hash %s, prev_beacon_hash %s, status = %u.",
              __func__,
              beacon.m_cpid.ToString(), // cpid
              beacon.GetAddress().ToString(), // address
              beacon.m_timestamp, // timestamp
              beacon.m_hash.GetHex(), // transaction hash
              beacon.m_prev_beacon_hash.GetHex(), // prev beacon transaction hash
              beacon.m_status.Raw() // status
              );

    // update m_historical entry
    m_historical[hash] = beacon;

    // update leveldb
    status = Store(hash, static_cast<StorageBeacon>(beacon));

    if (height) status &= StoreDBHeight(height);

    return status;
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

bool BeaconRegistry::BeaconDB::Store(const uint256 &hash, const StorageBeacon& beacon)
{
    CTxDB txdb("rw");

    std::pair<std::string, uint256> key = std::make_pair("beacon", hash);

    return txdb.WriteGenericSerializable(key, beacon);
}

bool BeaconRegistry::BeaconDB::Load(uint256& hash, StorageBeacon& beacon)
{
    CTxDB txdb("r");

    std::pair<std::string, uint256> key = std::make_pair("beacon", hash);

    return txdb.ReadGenericSerializable(key, beacon);
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
