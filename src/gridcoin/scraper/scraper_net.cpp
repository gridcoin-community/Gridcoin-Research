// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Define this if you want to show pubkey as address, otherwise hex id */
#define SCRAPER_NET_PK_AS_ADDRESS

#include <memory>
#include <atomic>
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
extern unsigned int nScraperSleep;
extern AppCacheSectionExt mScrapersExt;
extern std::atomic<int64_t> g_nTimeBestReceived;
extern ConvergedScraperStats ConvergedScraperStatsCache;
extern CCriticalSection cs_mScrapersExt;
extern CCriticalSection cs_ConvergedScraperStatsCache;
extern bool IsScraperMaximumManifestPublishingRateExceeded(int64_t& nTime, CPubKey& PubKey);

// A lock needs to be taken on cs_mapParts before calling this function.
bool CSplitBlob::RecvPart(CNode* pfrom, CDataStream& vRecv)
{
  /* Part of larger hashed blob. Currently only used for scraper data sharing.
   * retrieve parent object from mapBlobParts
   * notify object or ignore if no object found
   * erase from mapAlreadyAskedFor
   */
    auto& ss = vRecv;
    uint256 hash(Hash(ss.begin(), ss.end()));
    mapAlreadyAskedFor.erase(CInv(MSG_PART, hash));

    auto ipart = mapParts.find(hash);

    if (ipart != mapParts.end())
    {
        CPart& part = ipart->second;
        assert(vRecv.size() > 0);

        if (!part.present())
        {
            LogPrint(BCLog::LogFlags::MANIFEST, "received part %s %u refs", hash.GetHex(), (unsigned) part.refs.size());

            part.data = CSerializeData(vRecv.begin(),vRecv.end()); //TODO: replace with move constructor
            for (const auto& ref : part.refs)
            {
                CSplitBlob& split = *ref.first;
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
            pfrom->Misbehaving(SCRAPER_MISBEHAVING_NODE_BANSCORE / 5);
            LogPrintf("WARNING: CSplitBlob::RecvPart: Spurious part received from %s. Adding %u banscore.",
                     pfrom->addr.ToString(), SCRAPER_MISBEHAVING_NODE_BANSCORE / 5);
        }
        return error("Spurious part received!");
    }
}

// A lock needs to be taken on cs_mapParts before calling this function.
void CSplitBlob::addPart(const uint256& ihash)
{
    assert(ihash != Hash(vParts.end(),vParts.end()));

    unsigned n = vParts.size();
    auto rc = mapParts.emplace(ihash,CPart(ihash));
    CPart& part = rc.first->second;

    /* add to local vector */
    vParts.push_back(&part);

    if (part.present()) cntPartsRcvd++;

    /* nature of set ensures no duplicates */
    part.refs.emplace(this, n);
}

// A lock needs to be taken on cs_mapParts before calling this function.
int CSplitBlob::addPartData(CDataStream&& vData)
{
    uint256 hash(Hash(vData.begin(), vData.end()));

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
        /* prevent calling the Complete callback FIXME: make this look better */
        cntPartsRcvd--;
        CSplitBlob::RecvPart(0, vData);
        cntPartsRcvd++;
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

// A lock needs to be taken on cs_mapParts before calling this function.
void CSplitBlob::UseAsSource(CNode* pfrom)
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

// A lock needs to be taken on cs_mapParts before calling this function.
bool CSplitBlob::SendPartTo(CNode* pto, const uint256& hash)
{
    auto ipart = mapParts.find(hash);

    if (ipart != mapParts.end())
    {
        if (ipart->second.present())
        {
            pto->PushMessage("part",ipart->second.getReader());
            return true;
        }
    }
    return false;
}

// A lock needs to be taken on cs_mapManifest before calling this function.
bool CScraperManifest::AlreadyHave(CNode* pfrom, const CInv& inv)
{
    if (MSG_PART == inv.type)
    {
        //TODO: move
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
            LOCK(cs_mapParts);

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

// A lock needs to be taken on cs_mapManifest before calling this.
void CScraperManifest::PushInvTo(CNode* pto)
{
    /* send all keys from the index map as inventory */
    /* FIXME: advertise only completed manifests */
    for (auto const& obj : mapManifest)
    {
        pto->PushInventory(CInv(MSG_SCRAPERINDEX, obj.first));
    }
}

// A lock needs to be taken on cs_mapManifest before calling this.
bool CScraperManifest::SendManifestTo(CNode* pto, const uint256& hash)
{
    auto it = mapManifest.find(hash);

    if (it == mapManifest.end()) return false;

    pto->PushMessage("scraperindex", *it->second);

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

// A lock needs to be taken on cs_mapManifest and cs_mapParts before calling this.
void CScraperManifest::SerializeWithoutSignature(CDataStream& ss) const
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
// A lock needs to be taken on cs_mapManifest and cs_mapParts before calling this.
void CScraperManifest::SerializeForManifestCompare(CDataStream& ss) const
{
    WriteCompactSize(ss, vParts.size());
    for (const CPart* part : vParts)
    {
        ss << part->hash;
    }

    ss << ConsensusBlock;
}

// A lock needs to be taken on cs_mapManifest and cs_mapParts before calling this.
void CScraperManifest::Serialize(CDataStream& ss) const
{
    SerializeWithoutSignature(ss);
    ss << signature;
}


// This is the complement to IsScraperAuthorizedToBroadcastManifests in the scraper.
// It is used to determine whether received manifests are authorized.
bool CScraperManifest::IsManifestAuthorized(int64_t& nTime, CPubKey& PubKey, unsigned int& banscore_out)
{
    bool bIsValid = PubKey.IsValid();

    if (!bIsValid) return false;

    CKeyID ManifestKeyID = PubKey.GetID();

    CBitcoinAddress ManifestAddress;
    ManifestAddress.Set(ManifestKeyID);

    // This is the address corresponding to the manifest public key.
    std::string sManifestAddress = ManifestAddress.ToString();

    // Now check and see if that address is in the authorized scraper list.
    AppCacheSection mScrapers = ReadCacheSection(Section::SCRAPER);

    /* We cannot use the AppCacheSection mScrapers in the raw, because there are two ways to deauthorize scrapers.
     * The first way is to change the value of an existing entry to false. This works fine with mScrapers. The second way is to
     * issue an addkey delete key. This will remove the key entirely, therefore deauthorizing the scraper. We need to preserve
     * the key entry of the deleted record and when it was deleted to calculate a grace period. Why? To ensure that
     * we do not generate islanding in the network in the case of a scraper deauthorization, we must apply a grace period
     * after the timestamp of the marking of false/deletion, or from the time when the wallet came in sync, whichever is greater, before
     * we start assigning a banscore to nodes that send/forward unauthorized manifests. This is because not all nodes
     * may receive and accept the block that contains the transaction that modifies or deletes the scraper appcache entry
     * at the same time, so there is a chance a node could send/forward an unauthorized manifest between when the scraper
     * is deauthorized and the block containing that deauthorization is received by the sending node.
     */

    // So we are going to make use of AppCacheEntryExt and mScrapersExt, which are just like the normal AppCache structure, except they
    // have an explicit deleted boolean.

    // First, walk the mScrapersExt map and see if it contains an entry that does not exist in mScrapers. If so,
    // update the entry's value and timestamp and mark deleted.
    LOCK(cs_mScrapersExt);

    for (auto const& entry : mScrapersExt)
    {
        const auto& iter = mScrapers.find(entry.first);

        if (iter == mScrapers.end())
        {
            // Mark entry in mScrapersExt as deleted at the current adjusted time. The value is changed
            // to false, because if it is deleted, it is also not authorized.
            mScrapersExt[entry.first] = AppCacheEntryExt {"false", GetAdjustedTime(), true};
        }

    }

    // Now insert/update entries from mScrapers into mScrapersExt.
    for (auto const& entry : mScrapers)
    {
        mScrapersExt[entry.first] = AppCacheEntryExt {entry.second.value, entry.second.timestamp, false};
    }

    // Now mScrapersExt is up to date. Walk and see if there is an entry with a value of true that matches
    // manifest address. If so the manifest is authorized. Note that no grace period has to be considered
    // for the authorized case. To prevent islanding in the unauthorized case, we must allow a grace period
    // before we return a banscore > 0. The grace period must extend SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD
    // from the timestamp of the last updated entry in mScraperExt or the time the wallet went in sync, whichever is later.
    bool bAuthorized = false;
    int64_t nLastFalseEntryTime = 0;
    int64_t nGracePeriodEnd = 0;

    for (auto const& entry : mScrapersExt)
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
        banscore_out = GetArg("-banscore", 100);
        return false;
    }

    if (bAuthorized)
    {
        return true;
    }
    else
    {
        nGracePeriodEnd = std::max<int64_t>(g_nTimeBestReceived, nLastFalseEntryTime) + SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD;

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

// A lock must be taken on cs_mapManifest before calling this function.
void CScraperManifest::UnserializeCheck(CDataStream& ss, unsigned int& banscore_out)
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
    // otherwise terminate the unserializecheck and throw an error,
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
        throw error("CScraperManifest::UnserializeCheck: Unapproved scraper ID");
    }

    // We need to do an additional check here for non-current manifests, because the sending node may not
    // be filtering it on the send side, depending on the version on the sending node. Also, the
    // AlreadyHave only checks for whether the manifest is current if it is already in the map.
    if (!IsManifestCurrent())
    {
        throw error("CScraperManifest::UnserializeCheck: Received non-current manifest.");
    }

    ss >> ConsensusBlock;
    ss >> BeaconList >> BeaconList_c;
    ss >> projects;

    if (BeaconList + BeaconList_c > vph.size())
    {
        throw error("CScraperManifest::UnserializeCheck: beacon part out of range");
    }

    for (const dentry& prj : projects)
    {
        if (prj.part1 + prj.partc > vph.size())
        {
            throw error("CScraperManifest::UnserializeCheck: project part out of range");
        }
    }

    // It is not reasonable for a manifest to contain more than nMaxProjects, where this is
    // calculated by dividing the current whitelist size by the CONVERGENCE_BY_PROJECT_RATIO,
    // taking the ceiling and then adding 2. The motivation behind this is the corner case where
    // the whitelist has been reduced in size by that ratio as a corrective action in the situation
    // where suddenly a number of projects are not available, and a convergence was not able to be formed.
    // Then existing manifests on the network would have the reciprocol of that ratio projects. I
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

    unsigned int nMaxProjects = static_cast<unsigned int>(std::ceil(static_cast<double>(GRC::GetWhitelist().Snapshot().size()) /
                                                                    std::max(0.5, CONVERGENCE_BY_PROJECT_RATIO)) + 2);

    if (!OutOfSyncByAge() && projects.size() > nMaxProjects)
    {
        // Immediately ban the node from which the manifest was received.
        banscore_out = GetArg("-banscore", 100);

        throw error("CScraperManifest::UnserializeCheck: Too many projects in the manifest.");
    }

    ss >> nContentHash;

    uint256 hash = Hash(pbegin, ss.begin());

    ss >> signature;
    LogPrint(BCLog::LogFlags::MANIFEST, "CScraperManifest::UnserializeCheck: hash of signature = %s", Hash(signature.begin(), signature.end()).GetHex());

    CKey mkey;
    if (!mkey.SetPubKey(pubkey)) throw error("CScraperManifest: Invalid manifest key");
    if (!mkey.Verify(hash, signature)) throw error("CScraperManifest: Invalid manifest signature");

    {
        LOCK(cs_mapParts);

        for (const uint256& ph : vph)
        {
            addPart(ph);
        }
    }
}

bool CScraperManifest::IsManifestCurrent() const
{
    // This checks to see if the manifest is current, i.e. not about to be deleted.
    return (nTime >= GetAdjustedTime() - SCRAPER_CMANIFEST_RETENTION_TIME + (int64_t) nScraperSleep / 1000);
}


// A lock must be taken on cs_mapManifest before calling this function.
bool CScraperManifest::DeleteManifest(const uint256& nHash, const bool& fImmediate)
{
    bool fDeleted = false;

    auto iter = mapManifest.find(nHash);

    if(iter != mapManifest.end())
    {
        if (!fImmediate) mapPendingDeletedManifest[nHash] = std::make_pair(GetAdjustedTime(), std::move(iter->second));

        mapManifest.erase(nHash);

        // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a manifest has been deleted
        // that could have been used in the cached convergence, so the convergence may change.
        {
            LOCK(cs_ConvergedScraperStatsCache);

            ConvergedScraperStatsCache.bClean = false;
        }

        fDeleted = true;
    }

    return fDeleted;
}

// A lock must be taken on cs_mapManifest before calling this function.
std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator CScraperManifest::DeleteManifest(std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator& iter,
                                                                                                const bool& fImmediate)
{
    if (!fImmediate) mapPendingDeletedManifest[iter->first] = std::make_pair(GetAdjustedTime(), std::move(iter->second));

    iter = mapManifest.erase(iter);

    // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a manifest has been deleted
    // that could have been used in the cached convergence, so the convergence may change. This is not conditional, because the
    // iterator must be valid.
    {
        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache.bClean = false;
    }
    return iter;
}

// A lock must be taken on cs_mapManifest before calling this function.
unsigned int CScraperManifest::DeletePendingDeletedManifests()
{
    unsigned int nDeleted = 0;
    int64_t nDeleteThresholdTime = GetAdjustedTime() - nScraperSleep / 1000;

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

// A lock must be taken on cs_mapManifest before calling this function.
bool CScraperManifest::RecvManifest(CNode* pfrom, CDataStream& vRecv)
{
    /* Index object for scraper data.
   * deserialize message
   * hash
   * see if we do not already have it
   * validate the message
   * populate the maps
   * request parts
  */
    unsigned int banscore = 0;

    /* hash the object */
    uint256 hash(Hash(vRecv.begin(), vRecv.end()));

    /* see if we do not already have it */
    if (AlreadyHave(pfrom, CInv(MSG_SCRAPERINDEX, hash)))
    {
        LogPrint(BCLog::LogFlags::SCRAPER, "INFO: ScraperManifest::RecvManifest: Already have CScraperManifest %s from node %s.", hash.GetHex(), pfrom->addrName);
        return false;
    }

    const auto it = mapManifest.emplace(hash, std::shared_ptr<CScraperManifest>(new CScraperManifest()));
    CScraperManifest& manifest = *it.first->second;
    manifest.phash = &it.first->first;

    try
    {
        manifest.UnserializeCheck(vRecv, banscore);
    } catch (bool& e)
    {
        mapManifest.erase(hash);
        LogPrint(BCLog::LogFlags::MANIFEST, "invalid manifest %s received", hash.GetHex());

        if (pfrom)
        {
            LogPrintf("WARNING: CScraperManifest::RecvManifest: Invalid manifest %s received from %s. Increasing banscore by %u.",
                     hash.GetHex(), pfrom->addr.ToString(), banscore);
            pfrom->Misbehaving(banscore);
        }
        return false;
    } catch(std::ios_base::failure& e)
    {
        mapManifest.erase(hash);
        LogPrint(BCLog::LogFlags::MANIFEST, "invalid manifest %s received", hash.GetHex());

        if (pfrom)
        {
            LogPrintf("WARNING: CScraperManifest::RecvManifest: Invalid manifest %s received from %s. Increasing banscore by %u.",
                     hash.GetHex(), pfrom->addr.ToString(), banscore);
            pfrom->Misbehaving(banscore);
        }
        return false;
    }

    // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a new manifest is present,
    // so the convergence may change.
    {
        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache.bClean = false;
    }

    LOCK(cs_mapParts);

    LogPrint(BCLog::LogFlags::MANIFEST, "received manifest %s with %u / %u parts", hash.GetHex(),(unsigned)manifest.cntPartsRcvd,(unsigned)manifest.vParts.size());
    if (manifest.isComplete())
    {
        /* If we already got all the parts in memory, signal completion */
        manifest.Complete();
    }
    else
    {
        /* else request missing parts from the sender */
        // Note: As an additional buffer to prevent spurious part receipts, if the manifest timestamp is within nScraperSleep of expiration (i.e.
        // about to go on the pending delete list, then do not request missing parts, as it is possible that the manifest will be deleted
        // by the housekeeping loop in between the receipt of the manifest, request for parts, and receipt of parts otherwise.
        if (manifest.IsManifestCurrent()) manifest.UseAsSource(pfrom);
    }
    return true;
}

// A lock needs to be taken on cs_mapManifest and cs_mapParts before calling this function.
bool CScraperManifest::addManifest(std::shared_ptr<CScraperManifest>&& m, CKey& keySign)
{
    m->pubkey = keySign.GetPubKey();

    CDataStream sscomp(SER_NETWORK, 1);
    CDataStream ss(SER_NETWORK, 1);

    // serialize the content for comparison purposes and put in manifest.
    m->SerializeForManifestCompare(sscomp);
    m->nContentHash = Hash(sscomp.begin(), sscomp.end());

    /* serialize and hash the object */
    m->SerializeWithoutSignature(ss);

    /* sign the serialized manifest and append the signature */
    uint256 hash(Hash(ss.begin(), ss.end()));
    keySign.Sign(hash, m->signature);

    LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: hash of manifest contents = %s", m->nContentHash.ToString());
    LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: hash of manifest = %s", hash.ToString());
    LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: hash of signature = %s", Hash(m->signature.begin(), m->signature.end()).GetHex());
    LogPrint(BCLog::LogFlags::MANIFEST, "INFO: CScraperManifest::addManifest: datetime = %s", DateTimeStrFormat("%x %H:%M:%S", m->nTime));

    LogPrint(BCLog::LogFlags::MANIFEST, "adding new local manifest");

    /* try inserting into map */
    const auto it = mapManifest.emplace(hash, std::move(m));
    /* Already exists, do nothing */
    if (it.second == false)
        return false;

    CScraperManifest& manifest = *it.first->second;
    /* set the hash pointer inside */
    manifest.phash= &it.first->first;

    // We do not need to do a deserialize check here, because the
    // manifest originates from THIS node, and the scraper's authorization
    // to send has already been checked before the call.
    // We also do not need to do a manifest.isComplete to see if all
    // parts are available, because they have to be - this manifest was constructed
    // on THIS node.

    // Call manifest complete to notify peers of new manifest.
    manifest.Complete();

    // lock cs_ConvergedScraperStatsCache and mark ConvergedScraperStatsCache dirty because a new manifest is present,
    // so the convergence may change.
    {
        LOCK(cs_ConvergedScraperStatsCache);

        ConvergedScraperStatsCache.bClean = false;
    }

    return true;
}

// A lock needs to be taken on cs_mapManifest and cs_mapParts before calling this function.
void CScraperManifest::Complete()
{
    /* Notify peers that we have a new manifest */
    LogPrint(BCLog::LogFlags::MANIFEST, "manifest %s complete with %u parts", phash->GetHex(),(unsigned)vParts.size());
    {
        LOCK(cs_vNodes);
        for (auto const& pnode : vNodes)
        {
            pnode->PushInventory(CInv{MSG_SCRAPERINDEX, *phash});
        }
    }

    LogPrint(BCLog::LogFlags::SCRAPER, "INFO: CScraperManifest::Complete(): from %s with hash %s", sCManifestName, phash->GetHex());
}

/* how?
 * Should we only request objects that we need?
 * Because nodes should only have valid data, download anything they send.
 * They should only send what we requested, but we do not know what it is,
 * until we have it, let it pass.
 * There is 32MiB message size limit. There is a chance we could hit it, so
 * splitting is necesssary. Index object with list of parts is needed.
 *
 * If inv about index is received, and we do not know about it yet, just
 * getdata it. If it turns out useless, just ban the node. Then getdata the
 * parts from the node.
*/

UniValue CScraperManifest::ToJson() const
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
UniValue CScraperManifest::dentry::ToJson() const
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

    LOCK(CScraperManifest::cs_mapManifest);

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

    return UniValue(HexStr(ipart->second.data.begin(), ipart->second.data.end()));
}
