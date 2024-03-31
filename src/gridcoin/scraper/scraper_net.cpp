// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

/* Define this if you want to show pubkey as address, otherwise hex id */
#define SCRAPER_NET_PK_AS_ADDRESS

#include <memory>
#include <atomic>
#include <stdexcept>
#include "main.h"
#include "net.h"
#include "rpc/server.h"
#include "rpc/protocol.h"
#ifdef SCRAPER_NET_PK_AS_ADDRESS
#include "base58.h"
#endif
#include "gridcoin/appcache.h"
#include "gridcoin/project.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/scraper/scraper_net.h"
#include "gridcoin/superblock.h"

//Globals
std::map<uint256, std::pair<int64_t, std::shared_ptr<CScraperManifest>>> CScraperManifest::mapPendingDeletedManifest;
extern unsigned int SCRAPER_MISBEHAVING_NODE_BANSCORE;
extern int64_t SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD;
extern int64_t SCRAPER_CMANIFEST_RETENTION_TIME;
extern double CONVERGENCE_BY_PROJECT_RATIO;
extern CCriticalSection cs_ScraperGlobals;
extern unsigned int nScraperSleep;
extern std::atomic<int64_t> g_nTimeBestReceived;
extern ConvergedScraperStats ConvergedScraperStatsCache;
extern CCriticalSection cs_ConvergedScraperStatsCache;
extern AppCacheSectionExt GetExtendedScrapersCache();
extern bool IsScraperMaximumManifestPublishingRateExceeded(int64_t& nTime, CPubKey& PubKey);

bool CSplitBlob::RecvPart(CNode* pfrom, CDataStream& vRecv)
{
   /* Part of larger hashed blob. Currently only used for scraper data sharing.
    * retrieve parent object from mapBlobParts
    * notify object or ignore if no object found
    * erase from mapAlreadyAskedFor
    */
    auto& ss = vRecv;
    uint256 hash(Hash(ss));
    mapAlreadyAskedFor.erase(CInv(MSG_PART, hash));

    LOCK(cs_mapParts);

    auto ipart = mapParts.find(hash);

    if (ipart != mapParts.end())
    {
        CPart& part = ipart->second;
        assert(vRecv.size() > 0);

        if (!part.present())
        {
            LogPrint(BCLog::LogFlags::MANIFEST, "received part %s %u refs", hash.GetHex(), (unsigned) part.refs.size());

            part.data = SerializeData(vRecv.begin(), vRecv.end());
            for (const auto& ref : part.refs)
            {
                CSplitBlob& split = *ref.first;

                LOCK(split.cs_manifest);

                ++split.cntPartsRcvd;
                assert(split.cntPartsRcvd <= split.vParts.size());
                if (split.isComplete())
                {
                    split.Complete();
                }
            }
            return true;
        }
        else
        {
            LogPrint(BCLog::LogFlags::MANIFEST, "received duplicate part %s", hash.GetHex());
            return false;
        }
    }
    else
    {
        if (pfrom)
        {
            LOCK(cs_ScraperGlobals);

            pfrom->Misbehaving(SCRAPER_MISBEHAVING_NODE_BANSCORE / 5);
            LogPrintf("WARNING: CSplitBlob::RecvPart: Spurious part received from %s. Adding %u banscore.",
                     pfrom->addr.ToString(), SCRAPER_MISBEHAVING_NODE_BANSCORE / 5);
        }
        return error("Spurious part received!");
    }
}

bool CSplitBlob::isComplete() const EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest)
{
    return (!m_publish_in_progress && cntPartsRcvd == vParts.size());
}

void CSplitBlob::addPart(const uint256& ihash)
{
    LOCK2(cs_mapParts, cs_manifest);

    assert(ihash != Hash(MakeByteSpan(vParts)));

    unsigned n = vParts.size();
    auto rc = mapParts.emplace(ihash,CPart(ihash));
    CPart& part = rc.first->second;

    /* add to local vector */
    vParts.push_back(&part);

    if (part.present()) cntPartsRcvd++;

    /* nature of set ensures no duplicates */
    part.refs.emplace(this, n);
}

int CSplitBlob::addPartData(CDataStream&& vData, const bool& publish_in_progress)
{
    LOCK2(cs_mapParts, cs_manifest);

    m_publish_in_progress = publish_in_progress;

    uint256 hash(Hash(vData));

    auto it = mapParts.emplace(hash, CPart(hash));

    /* common part */
    CPart& part = it.first->second;
    unsigned n = vParts.size();
    vParts.push_back(&part);

    part.refs.emplace(this, n);

    /* check if the part already has data */
    if (!part.present())
    {
        /* missing data; use the supplied data */
        CSplitBlob::RecvPart(nullptr, vData);
    }

    return n;
}

// Takes a lock on cs_mapParts.
CSplitBlob::~CSplitBlob()
{
    LOCK(cs_mapParts);

    for (unsigned int n = 0; n < vParts.size(); ++n)
    {
        CPart& part = *vParts[n];

        part.refs.erase(std::pair<CSplitBlob*, unsigned int>(this, n));

        if (part.refs.empty()) mapParts.erase(part.hash);
    }
}

void CSplitBlob::UseAsSource(CNode* pfrom) EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest, CSplitBlob::cs_mapParts)
{
    if (pfrom)
    {
        for (const CPart* part : vParts)
        {
            if (!part->present())
            {
                /*Actually request the part. Inventory system will prevent redundant requests.*/
                pfrom->AskFor(CInv(MSG_PART, part->hash));
            }
        }
    }
}

bool CSplitBlob::SendPartTo(CNode* pto, const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_mapParts)
{
    auto ipart = mapParts.find(hash);

    if (ipart != mapParts.end())
    {
        if (ipart->second.present())
        {
            pto->PushMessage(NetMsgType::PART,ipart->second.getReader());
            return true;
        }
    }
    return false;
}

CScraperManifest::CScraperManifest() {}

CScraperManifest::CScraperManifest(CScraperManifest& manifest)
{
    // The custom copy constructor here is to copy everything except the mutex cs_manifest, which is actually taken during
    // the copy.
    LOCK(manifest.cs_manifest);

    phash = manifest.phash;
    sCManifestName = manifest.sCManifestName;

    pubkey = manifest.pubkey;
    signature = manifest.signature;

    projects = manifest.projects;

    BeaconList = manifest.BeaconList;
    BeaconList_c = manifest.BeaconList_c;
    ConsensusBlock = manifest.ConsensusBlock;
    nTime = manifest.nTime;

    nContentHash = manifest.nContentHash;

    bCheckedAuthorized = manifest.bCheckedAuthorized;

    vParts = manifest.vParts;
    cntPartsRcvd = manifest.cntPartsRcvd;
}

bool CScraperManifest::AlreadyHave(CNode* pfrom, const CInv& inv) EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest)
{
    if (MSG_PART == inv.type)
    {
        return false;
    }

    if (MSG_SCRAPERINDEX != inv.type)
    {
        // For any other objects, just say that we do not need it:
        return true;
    }

   // Inventory notification about scraper data index--see if we already have it.
   // If yes, relay pfrom to Parts system as a fetch source and return true
   // else return false.
    auto found = mapManifest.find(inv.hash);
    if (found != mapManifest.end())
    {
        // Only record UseAsSource if manifest is current to avoid spurious parts.
        {
            // The lock order is important here
            LOCK2(cs_mapParts, found->second->cs_manifest);

            if (found->second->IsManifestCurrent()) found->second->UseAsSource(pfrom);
        }

        return true;
    }
    else
    {
        if (pfrom) LogPrint(BCLog::LogFlags::MANIFEST, "new manifest %s from %s", inv.hash.GetHex(), pfrom->addrName);
        return false;
    }
}

void CScraperManifest::PushInvTo(CNode* pto) EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest,
                                                                      CSplitBlob::cs_mapParts)
{
    /* send all keys from the index map as inventory */
    /* FIXME: advertise only completed manifests */
    for (auto const& obj : mapManifest)
    {
        pto->PushInventory(CInv(MSG_SCRAPERINDEX, obj.first));
    }
}

// The exclusive lock on cs_mapParts is required because the part hashes are serialized as part of the manifest
// serialization. These hashes are contained in the part objects in the mapParts, which is POINTED TO by the manifest
// vParts vector. We need to ensure that the mapParts is not changing while the vParts vector is traversed.
bool CScraperManifest::SendManifestTo(CNode* pto, std::shared_ptr<CScraperManifest> manifest)
EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_mapParts)
{
    LOCK(manifest->cs_manifest);

    pto->PushMessage(NetMsgType::SCRAPERINDEX, *manifest);

    return true;
}

void CScraperManifest::dentry::Serialize(CDataStream& ss) const
{
    ss << project;
    ss << ETag;
    ss << LastModified;
    ss << part1 << partc;
    ss << GridcoinTeamID;
    ss << current;
    ss << last;
}

void CScraperManifest::dentry::Unserialize(CDataStream& ss)
{
    ss >> project;
    ss >> ETag;
    ss >> LastModified;
    ss >> part1 >> partc;
    ss >> GridcoinTeamID;
    ss >> current;
    ss >> last;
}

void CScraperManifest::SerializeWithoutSignature(CDataStream& ss) const
EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest, CSplitBlob::cs_mapParts)
{
    WriteCompactSize(ss, vParts.size());
    for (const CPart* part : vParts)
    {
        ss << part->hash;
    }

    ss << pubkey;
    ss << sCManifestName;
    ss << nTime;
    ss << ConsensusBlock;
    ss << BeaconList << BeaconList_c;
    ss << projects;
    ss << nContentHash;
}

// This is to compare manifest content quickly. We just need the parts and the consensus block.
void CScraperManifest::SerializeForManifestCompare(CDataStream& ss) const
EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest, CSplitBlob::cs_mapParts)
{
    WriteCompactSize(ss, vParts.size());
    for (const CPart* part : vParts)
    {
        ss << part->hash;
    }

    ss << ConsensusBlock;
}

void CScraperManifest::Serialize(CDataStream& ss) const
EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest, CSplitBlob::cs_mapParts)
{
    SerializeWithoutSignature(ss);
    ss << signature;
}


// This is the complement to IsScraperAuthorizedToBroadcastManifests in the scraper.
// It is used to determine whether received manifests are authorized.
bool CScraperManifest::IsManifestAuthorized(int64_t& nTime, CPubKey& PubKey, unsigned int& banscore_out)
EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest)
{
    bool bIsValid = PubKey.IsValid();

    if (!bIsValid) return false;

    CKeyID ManifestKeyID = PubKey.GetID();

    CBitcoinAddress ManifestAddress;
    ManifestAddress.Set(ManifestKeyID);

    // This is the address corresponding to the manifest public key.
    std::string sManifestAddress = ManifestAddress.ToString();

    AppCacheSectionExt mScrapersExtended = GetExtendedScrapersCache();

    // Now mScrapersExtended is up to date. Walk and see if there is an entry with a value of true that matches
    // manifest address. If so the manifest is authorized. Note that no grace period has to be considered
    // for the authorized case. To prevent islanding in the unauthorized case, we must allow a grace period
    // before we return a banscore > 0. The grace period must extend SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD
    // from the timestamp of the last updated entry in mScraperExt or the time the wallet went in sync, whichever is later.
    bool bAuthorized = false;
    int64_t nLastFalseEntryTime = 0;
    int64_t nGracePeriodEnd = 0;

    for (auto const& entry : mScrapersExtended)
    {
        if (entry.second.value == "true" || entry.second.value == "1")
        {
            // If the entry key (address) matches the manifest address, then the scraper is authorized, so
            // set banscore_out equal to 0 and bAuthorized = true and break.
            if (sManifestAddress == entry.first)
            {
                banscore_out = 0;
                bAuthorized = true;
                break;
            }
        }
        else
            // Track the latest timestamp of the false/deleted entries.
            nLastFalseEntryTime = std::max(nLastFalseEntryTime, entry.second.timestamp);
    }

    // Check for excessive manifest publishing rate by the associated scraper. If the maximum rate is exceeded
    // Then return false. This is exempt from the grace period below.
    if (IsScraperMaximumManifestPublishingRateExceeded(nTime, PubKey))
    {
        // Immediate ban
        banscore_out = gArgs.GetArg("-banscore", 100);
        return false;
    }

    if (bAuthorized)
    {
        return true;
    }
    else
    {
        LOCK(cs_ScraperGlobals);

        nGracePeriodEnd = std::max<int64_t>(g_nTimeBestReceived, nLastFalseEntryTime)
                + SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD;

        // If the current time is past the grace period end then set SCRAPER_MISBEHAVING_NODE_BANSCORE, otherwise 0.
        if (nGracePeriodEnd < GetAdjustedTime())
        {
            banscore_out = SCRAPER_MISBEHAVING_NODE_BANSCORE;
        }
        else
        {
            banscore_out = 0;
        }

        LogPrintf("WARNING: CScraperManifest::IsManifestAuthorized: Manifest from %s is not authorized.", sManifestAddress);

        return false;
    }
}

[[nodiscard]] bool CScraperManifest::UnserializeCheck(CDataStream& ss, unsigned int& banscore_out)
EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest, CSplitBlob::cs_manifest)
{
    const auto pbegin = ss.begin();

    std::vector<uint256> vph;
    ss >> vph;
    ss >> pubkey;
    ss >> sCManifestName;
    ss >> nTime;

    // This will set the bCheckAuthorized flag to false if a message
    // is received while the wallet is not in sync. If in sync and
    // the manifest is authorized, then set the checked flag to true,
    // otherwise terminate the unserializecheck and return false,
    // which will also result in an increase in banscore, if past the grace period.
    if (OutOfSyncByAge())
    {
        bCheckedAuthorized = false;
    }
    else if (IsManifestAuthorized(nTime, pubkey, banscore_out))
    {
        bCheckedAuthorized = true;
    }
    else
    {
        return error("CScraperManifest::UnserializeCheck: Unapproved scraper ID");
    }

    // We need to do an additional check here for non-current manifests, because the sending node may not
    // be filtering it on the send side, depending on the version on the sending node. Also, the
    // AlreadyHave only checks for whether the manifest is current if it is already in the map.
    if (!IsManifestCurrent())
    {
        return error("CScraperManifest::UnserializeCheck: Received non-current manifest.");
    }

    ss >> ConsensusBlock;
    ss >> BeaconList >> BeaconList_c;
    ss >> projects;

    if (BeaconList + BeaconList_c > vph.size())
    {
        return error("CScraperManifest::UnserializeCheck: beacon part out of range");
    }

    for (const dentry& prj : projects)
    {
        if (prj.part1 + prj.partc > vph.size())
        {
            return error("CScraperManifest::UnserializeCheck: project part out of range");
        }
    }

    // It is not reasonable for a manifest to contain more than nMaxProjects, where this is
    // calculated by dividing the current whitelist size by the CONVERGENCE_BY_PROJECT_RATIO,
    // taking the ceiling and then adding 2. The motivation behind this is the corner case where
    // the whitelist has been reduced in size by that ratio as a corrective action in the situation
    // where suddenly a number of projects are not available, and a convergence was not able to be formed.
    // Then existing manifests on the network would have the reciprocal of that ratio projects. I
    // take the ceiling and add 2 for a safety measure. For a CONVERGENCE_BY_PROJECT_RATIO of 0.75, which
    // is the network default, and a whitelist count of 20, this would come out to ceil(20.0/0.75)+2 = 29.
    // Or if the whitelist were suddenly reduced from 20 to 15, then it would be ceil(15.0/0.75)+2 = 22.
    // Note that this places limits on the change of the whitelist without causing nodes to ban the scrapers
    // But it is exceedingly unlikely to need to change the whitelist by more than this ratio.
    // Let's look at several scenarios to see the actual constraints using a
    // CONVERGENCE_BY_PROJECT_RATIO of 0.75 ...

    // Whitelist = 0 .... nMaxProjects = 2. (So whitelist could be reduced by 2 from 2 to 0 without tripping.)
    // Whitelist = 1 .... nMaxProjects = 4. (So whitelist could be reduced by 3 from 4 to 1 without tripping.)
    // Whitelist = 5 .... nMaxProjects = 9. (So whitelist could be reduced by 4 from 9 to 5 without tripping.)
    // Whitelist = 10 ... nMaxProjects = 16. (So whitelist could be reduced by 6 from 16 to 10 without tripping.)
    // Whitelist = 15 ... nMaxProjects = 22. (So whitelist could be reduced by 7 from 22 to 15 without tripping.)
    // Whitelist = 20 ... nMaxProjects = 29. (So whitelist could be reduced by 9 from 29 to 20 without tripping.)

    // There is also a clamp on the divisor to ensure it is never less than 0.5, even if the CONVERGENCE_BY_PROJECT_RATIO
    // is set to below 0.5, both to prevent a divide by zero exception, and also prevent unreasonably lose limits. So this
    // means the loosest limit that is allowed is essentially 2 * whitelist + 2.

    unsigned int nMaxProjects = 0;

    {
        LOCK(cs_ScraperGlobals);

        nMaxProjects = static_cast<unsigned int>(std::ceil(static_cast<double>(GRC::GetWhitelist().Snapshot().size()) /
                                                                    std::max(0.5, CONVERGENCE_BY_PROJECT_RATIO)) + 2);
    }

    if (!OutOfSyncByAge() && projects.size() > nMaxProjects)
    {
        // Immediately ban the node from which the manifest was received.
        banscore_out = gArgs.GetArg("-banscore", 100);

        return error("CScraperManifest::UnserializeCheck: Too many projects in the manifest.");
    }

    ss >> nContentHash;

    uint256 hash = Hash(Span<const std::byte>{(std::byte*)&pbegin[0], (std::byte*)&ss.begin()[0]});

    ss >> signature;
    LogPrint(BCLog::LogFlags::MANIFEST, "CScraperManifest::UnserializeCheck: hash of signature = %s",
             Hash(signature).GetHex());

    if (!pubkey.Verify(hash, signature)) return error("CScraperManifest: Invalid manifest signature");

    for (const uint256& ph : vph)
    {
        addPart(ph);
    }

    return true;
}

bool CScraperManifest::IsManifestCurrent() const EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest)
{
    LOCK(cs_ScraperGlobals);

    // This checks to see if the manifest is current, i.e. not about to be deleted.
    return (nTime >= GetAdjustedTime() - SCRAPER_CMANIFEST_RETENTION_TIME + (int64_t) nScraperSleep / 1000);
}


bool CScraperManifest::DeleteManifest(const uint256& nHash, const bool& fImmediate)
EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest)
{
    bool fDeleted = false;

    auto iter = mapManifest.find(nHash);

    if (iter != mapManifest.end())
    {
        if (!fImmediate)
        {
            auto iter2 = mapPendingDeletedManifest.insert(std::make_pair(nHash, std::make_pair(GetAdjustedTime(),
                                                                                               iter->second)));
            if (!iter2.second)
            {
                LogPrintf("WARN: %s: Manifest insertion attempt into pending deleted map failed because an entry with the same "
                          "hash = %s, already exists. This should not happen.", __func__, nHash.GetHex());
            }
            else
            {
                // Since phash in the manifest is actually a pointer, we need to change it to point to the key of the
                // mapPendingDeletedManifest key entry, since the old pointer is now invalid.
                CScraperManifest_shared_ptr manifest = iter2.first->second.second;

                LOCK(manifest->cs_manifest);

                manifest->phash = &iter2.first->first;
            }
        }

        mapManifest.erase(nHash);

        // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a manifest has been deleted
        // that could have been used in the cached convergence, so the convergence may change.
        {
            LOCK(cs_ConvergedScraperStatsCache);

            ConvergedScraperStatsCache.bClean = false;
        }

        fDeleted = true;
    }

    // Note that this will be false if an entry was not found and deleted in the mapManifest.
    return fDeleted;
}

std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator
CScraperManifest::DeleteManifest(std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator& iter,
                                 const bool& fImmediate) EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest)
{
    if (!fImmediate)
    {
        auto iter2 = mapPendingDeletedManifest.insert(std::make_pair(iter->first, std::make_pair(GetAdjustedTime(),
                                                                                           iter->second)));
        if (!iter2.second)
        {
            LogPrintf("WARN: %s: Manifest insertion attempt into pending deleted map failed because an entry with the same "
                      "hash = %s, already exists. This should not happen.", __func__, iter->first.GetHex());
        }
        else
        {
            // Since phash in the manifest is actually a pointer, we need to change it to point to the key of the
            // mapPendingDeletedManifest key entry, since the old pointer is now invalid.
            CScraperManifest_shared_ptr manifest = iter2.first->second.second;

            LOCK(manifest->cs_manifest);

            manifest->phash = &iter2.first->first;
        }
    }

    iter = mapManifest.erase(iter);

    // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a manifest has been deleted
    // that could have been used in the cached convergence, so the convergence may change. This is not conditional, because
    // the iterator must be valid.
    {
        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache.bClean = false;
    }
    return iter;
}

unsigned int CScraperManifest::DeletePendingDeletedManifests() EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest)
{
    unsigned int nDeleted = 0;

    int64_t nDeleteThresholdTime = 0;
    {
        LOCK(cs_ScraperGlobals);

        nDeleteThresholdTime = GetAdjustedTime() - nScraperSleep / 1000;
    }

    std::map<uint256, std::pair<int64_t, std::shared_ptr<CScraperManifest>>>::iterator iter;
    for (iter = mapPendingDeletedManifest.begin(); iter != mapPendingDeletedManifest.end();)
    {
        // Delete any entry more than nScraperSleep old.
        if (iter->second.first < nDeleteThresholdTime)
        {
            iter = mapPendingDeletedManifest.erase(iter);
            ++nDeleted;
        }
        else
        {
            ++iter;
        }
    }

    return nDeleted;
}

bool CScraperManifest::RecvManifest(CNode* pfrom, CDataStream& vRecv)
{
    // General procedure here:
    //
    // Index object for scraper data.
    // deserialize message
    // hash
    // see if we do not already have it
    // validate the message
    // populate the maps
    // request parts

    unsigned int banscore = 0;

    // hash the object
    uint256 hash(Hash(vRecv));

    LOCK(cs_mapManifest);

    // see if we do not already have it
    if (AlreadyHave(pfrom, CInv(MSG_SCRAPERINDEX, hash)))
    {
        LogPrint(BCLog::LogFlags::SCRAPER, "INFO: ScraperManifest::RecvManifest: Already have CScraperManifest %s from "
                                           "node %s.", hash.GetHex(), pfrom->addrName);
        return false;
    }

    CScraperManifest_shared_ptr manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());

    const auto it = mapManifest.emplace(hash, manifest);

    {
        LOCK(manifest->cs_manifest);

        // The phash in the manifest points to the actual hash which is the index to the element in the map.
        manifest->phash = &it.first->first;

        try
        {
            if (!manifest->UnserializeCheck(vRecv, banscore))
            {
                mapManifest.erase(hash);
                LogPrint(BCLog::LogFlags::MANIFEST, "invalid manifest %s received", hash.GetHex());

                if (pfrom)
                {
                    LogPrintf("WARNING: CScraperManifest::RecvManifest: Invalid manifest %s received from %s. Increasing banscore "
                              "by %u.", hash.GetHex(), pfrom->addr.ToString(), banscore);
                    pfrom->Misbehaving(banscore);
                }
                return false;
            }
        } catch(std::ios_base::failure& e)
        {
            mapManifest.erase(hash);
            LogPrint(BCLog::LogFlags::MANIFEST, "invalid manifest %s received", hash.GetHex());

            if (pfrom)
            {
                LogPrintf("WARNING: CScraperManifest::RecvManifest: Invalid manifest %s received from %s. Increasing banscore "
                          "by %u.", hash.GetHex(), pfrom->addr.ToString(), banscore);
                pfrom->Misbehaving(banscore);
            }
            return false;
        }
    }

    // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a new manifest is present,
    // so the convergence may change.
    {
        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache.bClean = false;
    }

    // Lock mapParts and relock manifest
    LOCK2(cs_mapParts, manifest->cs_manifest);

    LogPrint(BCLog::LogFlags::MANIFEST, "received manifest %s with %u / %u parts", hash.GetHex(),
             (unsigned) manifest->cntPartsRcvd, (unsigned) manifest->vParts.size());

    if (manifest->isComplete())
    {
        // If we already got all the parts in memory, signal completion...
        manifest->Complete();
    }
    else
    {
        // ... else request missing parts from the sender
        // Note: As an additional buffer to prevent spurious part receipts, if the manifest timestamp is within nScraperSleep
        // of expiration (i.e. about to go on the pending delete list, then do not request missing parts, as it is possible
        // that the manifest will be deleted by the housekeeping loop in between the receipt of the manifest, request for
        // parts, and receipt of parts otherwise.
        if (manifest->IsManifestCurrent()) manifest->UseAsSource(pfrom);
    }

    return true;
}

bool CScraperManifest::addManifest(std::shared_ptr<CScraperManifest> m, CKey& keySign)
EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest, cs_mapParts)
{
    uint256 hash;

    {
        LOCK(m->cs_manifest);

        m->pubkey = keySign.GetPubKey();

        CDataStream sscomp(SER_NETWORK, 1);
        CDataStream ss(SER_NETWORK, 1);

        // serialize the content for comparison purposes and put in manifest.
        m->SerializeForManifestCompare(sscomp);
        m->nContentHash = Hash(sscomp);

        // serialize and hash the object
        m->SerializeWithoutSignature(ss);

        // sign the serialized manifest and append the signature
        hash = Hash(ss);
        keySign.Sign(hash, m->signature);

        LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: hash of manifest contents = %s",
                 m->nContentHash.ToString());
        LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: hash of manifest = %s",
                 hash.ToString());
        LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: hash of signature = %s",
                 Hash(m->signature).GetHex());
        LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: datetime = %s",
                 DateTimeStrFormat("%x %H:%M:%S", m->nTime));

        LogPrint(BCLog::LogFlags::MANIFEST, "adding new local manifest");
    }

    // try inserting into map
    const auto it = mapManifest.emplace(hash, m);

    // Already exists, do nothing
    if (it.second == false)
        return false;

    // Release lock on cs_manifest before taking a lock on cs_ConvergedScraperStatsCache to avoid potential deadlocks.
    {
        CScraperManifest& manifest = *it.first->second;

        // Relock the manifest pointed to by the iterator.
        LOCK(manifest.cs_manifest);

        // set the hash pointer inside
        manifest.phash = &it.first->first;

        // We do not need to do a deserialize check here, because the
        // manifest originates from THIS node, and the scraper's authorization
        // to send has already been checked before the call.
        // We also do not need to do a manifest.isComplete to see if all
        // parts are available, because they have to be - this manifest was constructed
        // on THIS node.

        // Call manifest complete to notify peers of new manifest.
        manifest.Complete();
    }

    // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a new manifest is present,
    // so the convergence may change.
    {
        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache.bClean = false;
    }

    return true;
}

void CScraperManifest::Complete() EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest, CSplitBlob::cs_mapParts)
{
    m_publish_in_progress = false;

    // Notify peers that we have a new manifest
    LogPrint(BCLog::LogFlags::MANIFEST, "manifest %s complete with %u parts", phash->GetHex(), (unsigned)vParts.size());
    {
        LOCK(cs_vNodes);
        for (auto const& pnode : vNodes)
        {
            pnode->PushInventory(CInv{MSG_SCRAPERINDEX, *phash});
        }
    }

    LogPrint(BCLog::LogFlags::SCRAPER, "INFO: CScraperManifest::Complete(): from %s with hash %s",
             sCManifestName, phash->GetHex());
}

UniValue CScraperManifest::ToJson() const EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest, CSplitBlob::cs_mapParts)
{
    UniValue r(UniValue::VOBJ);

#ifdef SCRAPER_NET_PK_AS_ADDRESS
    r.pushKV("pubkey", CBitcoinAddress(pubkey.GetID()).ToString());
#else
    r.pushKV("pubkey", pubkey.GetID().ToString());
#endif
    r.pushKV("sCManifestName", sCManifestName);

    r.pushKV("nTime", (int64_t) nTime);
    r.pushKV("nTime", DateTimeStrFormat(nTime));
    r.pushKV("ConsensusBlock", ConsensusBlock.GetHex());
    r.pushKV("nContentHash", nContentHash.GetHex());
    r.pushKV("BeaconList", (int64_t) BeaconList);
    r.pushKV("BeaconList_c", (int64_t) BeaconList_c);

    UniValue projects(UniValue::VARR);
    for (const dentry& part : this->projects)
    {
        projects.push_back(part.ToJson());
    }

    r.pushKV("projects", projects);

    UniValue parts(UniValue::VARR);
    for (const CPart* part : this->vParts)
    {
        parts.push_back(part->hash.GetHex());
    }

    r.pushKV("parts", parts);

    return r;
}

UniValue CScraperManifest::dentry::ToJson() const EXCLUSIVE_LOCKS_REQUIRED(CSplitBlob::cs_manifest)
{
    UniValue r(UniValue::VOBJ);

    r.pushKV("project", project);
    r.pushKV("ETag", ETag);
    r.pushKV("LastModified", DateTimeStrFormat(LastModified));
    r.pushKV("part1", (int64_t) part1);
    r.pushKV("partc", (int64_t) partc);
    r.pushKV("GridcoinTeamID", (int64_t) GridcoinTeamID);
    r.pushKV("current", current);
    r.pushKV("last", last);

    return r;
}

/** RPC function to list manifests and optionally provide their contents in JSON form. */
UniValue listmanifests(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
    {
        throw std::runtime_error(
                "listmanifests [bool details] [manifest hash]\n"
                "\n"
                "details: boolean to show details of manifests\n"
                "manifest hash: hash of specific manifest (Not provided returns all.)"
                "\n"
                "Show list of known ScraperManifest objects.\n"
                );
    }
    UniValue obj(UniValue::VOBJ);
    UniValue subset(UniValue::VOBJ);

    bool bShowDetails = false;

    if (params.size() > 0)
        bShowDetails = params[0].get_bool();

    LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

    if (params.size() > 1)
    {
        uint256 manifest_hash = uint256S(params[1].get_str());

        auto pair = CScraperManifest::mapManifest.find(manifest_hash);

        if (pair == CScraperManifest::mapManifest.end())
        {
            throw JSONRPCError(RPC_MISC_ERROR, "Manifest with specified hash not found.");
        }

        const uint256& hash = pair->first;
        const CScraperManifest& manifest = *pair->second;

        LOCK(manifest.cs_manifest);

        if (bShowDetails)
            obj.pushKV(hash.GetHex(), manifest.ToJson());
        else
        {
#ifdef SCRAPER_NET_PK_AS_ADDRESS
            subset.pushKV("scraper (manifest) address", CBitcoinAddress(manifest.pubkey.GetID()).ToString());
#else
            subset.pushKV("scraper (manifest) pubkey", manifest.pubkey.GetID().ToString());
#endif
            subset.pushKV("manifest datetime", DateTimeStrFormat(manifest.nTime));
            subset.pushKV("manifest content hash", manifest.nContentHash.GetHex());
            obj.pushKV(hash.GetHex(), subset);
        }
    }
    else
    {
        for (const auto& pair : CScraperManifest::mapManifest)
        {
            const uint256& hash = pair.first;
            const CScraperManifest& manifest = *pair.second;

            LOCK(manifest.cs_manifest);

            if (bShowDetails)
                obj.pushKV(hash.GetHex(), manifest.ToJson());
            else
            {
    #ifdef SCRAPER_NET_PK_AS_ADDRESS
                subset.pushKV("scraper (manifest) address", CBitcoinAddress(manifest.pubkey.GetID()).ToString());
    #else
                subset.pushKV("scraper (manifest) pubkey", manifest.pubkey.GetID().ToString());
    #endif
                subset.pushKV("manifest datetime", DateTimeStrFormat(manifest.nTime));
                subset.pushKV("manifest content hash", manifest.nContentHash.GetHex());
                obj.pushKV(hash.GetHex(), subset);
            }
        }
    }

    return obj;
}

/** Provides hex string output of part object contents. */
UniValue getmpart(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
    {
        throw std::runtime_error(
                "getmpart <hash>\n"
                "Show content of CPart object.\n"
                );
    }

    LOCK(CSplitBlob::cs_mapParts);

    auto ipart = CSplitBlob::mapParts.find(uint256S(params[0].get_str()));

    if (ipart == CSplitBlob::mapParts.end())
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Object not found");
    }

    return UniValue(HexStr(ipart->second.data));
}
