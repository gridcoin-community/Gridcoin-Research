/* scraper_net.cpp */

/* Define this if you want to show pubkey as address, otherwise hex id */
#define SCRAPER_NET_PK_AS_ADDRESS

#include <memory>
#include <atomic>
#include "net.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#ifdef SCRAPER_NET_PK_AS_ADDRESS
#include "base58.h"
#endif
#include "scraper_net.h"
#include "appcache.h"
#include "scraper/fwd.h"
#include "neuralnet/superblock.h"

//Globals
std::map<uint256,CSplitBlob::CPart> CSplitBlob::mapParts;
CCriticalSection CSplitBlob::cs_mapParts;
std::map< uint256, std::unique_ptr<CScraperManifest> > CScraperManifest::mapManifest;
std::map<uint256, std::pair<int64_t, std::unique_ptr<CScraperManifest>>> CScraperManifest::mapPendingDeletedManifest;
CCriticalSection CScraperManifest::cs_mapManifest;
extern unsigned int SCRAPER_MISBEHAVING_NODE_BANSCORE;
extern int64_t SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD;
extern int64_t SCRAPER_CMANIFEST_RETENTION_TIME;
extern unsigned int nScraperSleep;
extern AppCacheSectionExt mScrapersExt;
extern std::atomic<int64_t> nSyncTime;
extern ConvergedScraperStats ConvergedScraperStatsCache;
extern CCriticalSection cs_mScrapersExt;
extern CCriticalSection cs_ConvergedScraperStatsCache;

// A lock needs to be taken on cs_mapParts before calling this function.
bool CSplitBlob::RecvPart(CNode* pfrom, CDataStream& vRecv)
{
  /* Part of larger hashed blob. Currently only used for scraper data sharing.
   * retrive parent object from mapBlobParts
   * notify object or ignore if no object found
   * erase from mapAlreadyAskedFor
   */
    auto& ss= vRecv;
    uint256 hash(Hash(ss.begin(), ss.end()));
    mapAlreadyAskedFor.erase(CInv(MSG_PART,hash));

    auto ipart= mapParts.find(hash);

    if(ipart!=mapParts.end())
    {
        CPart& part= ipart->second;
        assert(vRecv.size()>0);
        if(!part.present())
        {
            LogPrint("manifest", "received part %s %u refs", hash.GetHex(),(unsigned)part.refs.size());
            part.data= CSerializeData(vRecv.begin(),vRecv.end()); //TODO: replace with move constructor
            for( const auto& ref : part.refs )
            {
                CSplitBlob& split= *ref.first;
                ++split.cntPartsRcvd;
                assert(split.cntPartsRcvd <= split.vParts.size());
                if( split.isComplete() )
                {
                    split.Complete();
                }
            }
            return true;
        } else {
            LogPrint("manifest", "received duplicate part %s", hash.GetHex());
            return false;
        }
    } else {
        if(pfrom)
        {
            pfrom->Misbehaving(SCRAPER_MISBEHAVING_NODE_BANSCORE / 5);
            LogPrintf("WARNING: CSplitBlob::RecvPart: Spurious part received from %s. Adding %u banscore.",
                     pfrom->addr.ToString(), SCRAPER_MISBEHAVING_NODE_BANSCORE / 5);
        }
        return error("Spurious part received!");
    }
}

void CSplitBlob::addPart(const uint256& ihash)
{
    assert( ihash != Hash(vParts.end(),vParts.end()) );
    unsigned n= vParts.size();
    auto rc= mapParts.emplace(ihash,CPart(ihash));
    CPart& part= rc.first->second;
    /* add to local vector */
    vParts.push_back(&part);
    if(part.present())
        cntPartsRcvd++;
    /* nature of set ensures no duplicates */
    part.refs.emplace(this, n);
}

int CSplitBlob::addPartData(CDataStream&& vData)
{
    uint256 hash(Hash(vData.begin(), vData.end()));

    //maybe? mapAlreadyAskedFor.erase(CInv(MSG_PART,hash));

    auto it= mapParts.emplace(hash,CPart(hash));

    /* common part */
    CPart& part= it.first->second;
    unsigned n= vParts.size();
    vParts.push_back(&part);
    part.refs.emplace(this, n);

    /* check if the part already has data */
    if(!part.present())
    {
        /* missing data; use the supplied data */
        /* prevent calling the Complete callback FIXME: make this look better */
        cntPartsRcvd--;
        CSplitBlob::RecvPart(0, vData);
        cntPartsRcvd++;
    }
    return n;
}

CSplitBlob::~CSplitBlob()
{
    for(unsigned n= 0; n<vParts.size(); ++n)
    {
        CPart& part= *vParts[n];
        part.refs.erase(std::pair<CSplitBlob*,unsigned>(this,n));
        if(part.refs.empty())
            mapParts.erase(part.hash);
    }
}

void CSplitBlob::UseAsSource(CNode* pfrom)
{
    if(pfrom)
    {
        for ( const CPart* part : vParts )
        {
            if(!part->present())
            {
                /*Actually request the part. Inventory system will prevent redundant requests.*/
                pfrom->AskFor(CInv(MSG_PART, part->hash));
            }
        }
    }
}

bool CSplitBlob::SendPartTo(CNode* pto, const uint256& hash)
{
    auto ipart= mapParts.find(hash);

    if(ipart!=mapParts.end())
    {
        if(ipart->second.present())
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
    if( MSG_PART ==inv.type )
    {
        //TODO: move
        return false;
    }
    if( MSG_SCRAPERINDEX !=inv.type )
    {
        // For any other objects, just say that we do not need it:
        return true;
    }

   // Inventory notification about scraper data index--see if we already have it.
   // If yes, relay pfrom to Parts system as a fetch source and return true
   // else return false.
    auto found = mapManifest.find(inv.hash);
    if( found!=mapManifest.end() )
    {
        // Only record UseAsSource if manifest is current to avoid spurious parts.
        if (found->second->IsManifestCurrent()) found->second->UseAsSource(pfrom);

        return true;
    }
    else
    {
        if(pfrom)  LogPrint("manifest", "new manifest %s from %s", inv.hash.GetHex(), pfrom->addrName);
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

    if (it == mapManifest.end())
        return false;
    pto->PushMessage("scraperindex", *it->second);
    return true;
}


void CScraperManifest::dentry::Serialize(CDataStream& ss, int nType, int nVersion) const
{ /* TODO: remove this redundant code */
    ss<< project;
    ss<< ETag;
    ss<< LastModified;
    ss<< part1 << partc;
    ss<< GridcoinTeamID;
    ss<< current;
    ss<< last;
}
void CScraperManifest::dentry::Unserialize(CReaderStream& ss, int nType, int nVersion)
{
    ss>> project;
    ss>> ETag;
    ss>> LastModified;
    ss>> part1 >> partc;
    ss>> GridcoinTeamID;
    ss>> current;
    ss>> last;
}


void CScraperManifest::SerializeWithoutSignature(CDataStream& ss, int nType, int nVersion) const
{
    WriteCompactSize(ss, vParts.size());
    for( const CPart* part : vParts )
        ss << part->hash;
    ss<< pubkey;
    ss<< sCManifestName;
    ss<< nTime;
    ss<< ConsensusBlock;
    ss<< BeaconList << BeaconList_c;
    ss<< projects;
    ss<< nContentHash;
}

// This is to compare manifest content quickly. We just need the parts and the consensus block.
void CScraperManifest::SerializeForManifestCompare(CDataStream& ss, int nType, int nVersion) const
{
    WriteCompactSize(ss, vParts.size());
    for( const CPart* part : vParts )
        ss << part->hash;
    ss<< ConsensusBlock;
}


void CScraperManifest::Serialize(CDataStream& ss, int nType, int nVersion) const
{
    SerializeWithoutSignature(ss, nType, nVersion);
    ss << signature;
}


// This is the complement to IsScraperAuthorizedToBroadcastManifests in the scraper.
// It is used to determine whether received manifests are authorized.
bool CScraperManifest::IsManifestAuthorized(CPubKey& PubKey, unsigned int& banscore_out)
{
    bool bIsValid = PubKey.IsValid();
    if (!bIsValid)
        return false;

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
            // Mark entry in mScrapersExt as deleted at the current adjusted time. The value is changed
            // to false, because if it is deleted, it is also not authorized.
            mScrapersExt[entry.first] = AppCacheEntryExt {"false", GetAdjustedTime(), true};

    }

    // Now insert/update entries from mScrapers into mScrapersExt.
    for (auto const& entry : mScrapers)
        mScrapersExt[entry.first] = AppCacheEntryExt {entry.second.value, entry.second.timestamp, false};

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

    if (bAuthorized)
        return true;
    else
    {
        nGracePeriodEnd = std::max((int64_t)nSyncTime, nLastFalseEntryTime) + SCRAPER_DEAUTHORIZED_BANSCORE_GRACE_PERIOD;

        // If the current time is past the grace period end then set SCRAPER_MISBEHAVING_NODE_BANSCORE, otherwise 0.
        if (nGracePeriodEnd < GetAdjustedTime())
            banscore_out = SCRAPER_MISBEHAVING_NODE_BANSCORE;
        else
            banscore_out = 0;

        LogPrintf("WARNING: CScraperManifest::IsManifestAuthorized: Manifest from %s is not authorized.", sManifestAddress);

        return false;
    }
}


void CScraperManifest::UnserializeCheck(CReaderStream& ss, unsigned int& banscore_out)
{
    const auto pbegin = ss.begin();

    vector<uint256> vph;
    ss>>vph;
    ss>> pubkey;

    // This will set the bCheckAuthorized flag to false if a message
    // is received while the wallet is not in sync. If in sync and
    // the manifest is authorized, then set the checked flag to true,
    // otherwise terminate the unserializecheck and throw an error,
    // which will also result in an increase in banscore, if past the grace period.
    if (OutOfSyncByAge())
        bCheckedAuthorized = false;
    else if (IsManifestAuthorized(pubkey, banscore_out))
        bCheckedAuthorized = true;
    else
        throw error("CScraperManifest::UnserializeCheck: Unapproved scraper ID");

    ss>> sCManifestName;
    ss>> nTime;
    ss>> ConsensusBlock;
    ss>> BeaconList >> BeaconList_c;
    ss>> projects;

    if(BeaconList+BeaconList_c>vph.size())
        throw error("CScraperManifest::UnserializeCheck: beacon part out of range");
    for(const dentry& prj : projects)
        if(prj.part1+prj.partc>vph.size())
            throw error("CScraperManifest::UnserializeCheck: project part out of range");

    ss >> nContentHash;

    uint256 hash = Hash(pbegin, ss.begin());

    ss >> signature;
    LogPrint("Manifest", "CScraperManifest::UnserializeCheck: hash of signature = %s", Hash(signature.begin(), signature.end()).GetHex());

    CKey mkey;
    if(!mkey.SetPubKey(pubkey))
        throw error("CScraperManifest: Invalid manifest key");
    if(!mkey.Verify(hash, signature))
        throw error("CScraperManifest: Invalid manifest signature");
    for( const uint256& ph : vph )
        addPart(ph);
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
std::map<uint256, std::unique_ptr<CScraperManifest>>::iterator CScraperManifest::DeleteManifest(std::map<uint256, std::unique_ptr<CScraperManifest>>::iterator& iter,
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

    std::map<uint256, std::pair<int64_t, std::unique_ptr<CScraperManifest>>>::iterator iter;
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
    if( AlreadyHave(pfrom,CInv(MSG_SCRAPERINDEX, hash)) )
    {
        if (fDebug3) LogPrintf("INFO: ScraperManifest::RecvManifest: Already have CScraperManifest %s from node %s.", hash.GetHex(), pfrom->addrName);
        return false;
    }
    const auto it = mapManifest.emplace(hash,std::unique_ptr<CScraperManifest>(new CScraperManifest()));
    CScraperManifest& manifest = *it.first->second;
    manifest.phash= &it.first->first;
    try {
        //void Unserialize(Stream& s, int nType, int nVersion)
        manifest.UnserializeCheck(vRecv, banscore);
    } catch(bool& e) {
        mapManifest.erase(hash);
        LogPrint("manifest", "invalid manifest %s received", hash.GetHex());
        if(pfrom)
        {
            LogPrintf("WARNING: CScraperManifest::RecvManifest): Invalid manifest %s received from %s. Increasing banscore by %u.",
                     hash.GetHex(), pfrom->addr.ToString(), banscore);
            pfrom->Misbehaving(banscore);
        }
        return false;
    } catch(std::ios_base::failure& e) {
        mapManifest.erase(hash);
        LogPrint("manifest", "invalid manifest %s received", hash.GetHex());
        if(pfrom)
        {
            LogPrintf("WARNING: CScraperManifest::RecvManifest): Invalid manifest %s received from %s. Increasing banscore by %u.",
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

    LogPrint("manifest", "received manifest %s with %u / %u parts", hash.GetHex(),(unsigned)manifest.cntPartsRcvd,(unsigned)manifest.vParts.size());
    if( manifest.isComplete() )
    {
        /* If we already got all the parts in memory, signal completion */
        manifest.Complete();
    } else {
        /* else request missing parts from the sender */
        // Note: As an additional buffer to prevent spurious part receipts, if the manifest timestamp is within nScraperSleep of expiration (i.e.
        // about to go on the pending delete list, then do not request missing parts, as it is possible that the manifest will be deleted
        // by the housekeeping loop in between the receipt of the manifest, request for parts, and receipt of parts otherwise.
        if (manifest.IsManifestCurrent()) manifest.UseAsSource(pfrom);
    }
    return true;
}

// A lock needs to be taken on cs_mapManifest before calling this function.
bool CScraperManifest::addManifest(std::unique_ptr<CScraperManifest>&& m, CKey& keySign)
{
    m->pubkey= keySign.GetPubKey();

    // serialize the content for comparison purposes and put in manifest.
    CDataStream sscomp(SER_NETWORK,1);
    m->SerializeForManifestCompare(sscomp, SER_NETWORK, 1);
    m->nContentHash = Hash(sscomp.begin(), sscomp.end());

    /* serialize and hash the object */
    CDataStream ss(SER_NETWORK,1);
    m->SerializeWithoutSignature(ss, SER_NETWORK, 1);
    //ss << *m;

    /* sign the serialized manifest and append the signature */
    uint256 hash(Hash(ss.begin(),ss.end()));
    keySign.Sign(hash, m->signature);
    //ss << m->signature;
    if (fDebug3) LogPrintf("INFO: CScraperManifest::addManifest: hash of signature = %s", Hash(m->signature.begin(), m->signature.end()).GetHex());

    LogPrint("manifest", "adding new local manifest");
    /* at this point it is easier to pretend like it was received from network */
    // ^ Yes, but your are creating a new object and pointer that way. It is better to do
    // a special insert routine below, which forwards the object (pointer).
    // return CScraperManifest::RecvManifest(0, ss);

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

void CScraperManifest::Complete()
{
    /* Notify peers that we have a new manifest */
    LogPrint("manifest", "manifest %s complete with %u parts", phash->GetHex(),(unsigned)vParts.size());
    {
        LOCK(cs_vNodes);
        for (auto const& pnode : vNodes)
            pnode->PushInventory(CInv{MSG_SCRAPERINDEX, *phash});
    }

    /* Do something with the complete manifest */
    std::string bodystr;
    vParts[0]->getReader() >> bodystr;
    if (fDebug3) LogPrintf("INFO: CScraperManifest::Complete(): from %s with hash %s", sCManifestName, phash->GetHex());
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
    r.pushKV("pubkey",CBitcoinAddress(pubkey.GetID()).ToString());
#else
    r.pushKV("pubkey",pubkey.GetID().ToString());
#endif
    r.pushKV("sCManifestName",sCManifestName);

    r.pushKV("nTime",(int64_t)nTime);
    r.pushKV("nTime",DateTimeStrFormat(nTime));
    r.pushKV("ConsensusBlock",ConsensusBlock.GetHex());
    r.pushKV("nContentHash",nContentHash.GetHex());
    r.pushKV("BeaconList",(int64_t)BeaconList); r.pushKV("BeaconList_c",(int64_t)BeaconList_c);

    UniValue projects(UniValue::VARR);
    for( const dentry& part : this->projects )
        projects.push_back(part.ToJson());
    r.pushKV("projects",projects);

    UniValue parts(UniValue::VARR);
    for( const CPart* part : this->vParts )
        parts.push_back(part->hash.GetHex());
    r.pushKV("parts",parts);
    return r;
}
UniValue CScraperManifest::dentry::ToJson() const
{
    UniValue r(UniValue::VOBJ);
    r.pushKV("project",project);
    r.pushKV("ETag",ETag);
    r.pushKV("LastModified",DateTimeStrFormat(LastModified));
    r.pushKV("part1",(int64_t)part1); r.pushKV("partc",(int64_t)partc);
    r.pushKV("GridcoinTeamID",(int64_t)GridcoinTeamID);
    r.pushKV("current",current);
    r.pushKV("last",last);
    return r;
}

UniValue listmanifests(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() > 1 )
        throw std::runtime_error(
                "listmanifests [bool details]\n"
                "Show [detailed] list of known ScraperManifest objects.\n"
                );
    UniValue obj(UniValue::VOBJ);
    UniValue subset(UniValue::VOBJ);

    bool bShowDetails = false;

    if (params.size() > 0)
        bShowDetails = params[0].get_bool();

    LOCK(CScraperManifest::cs_mapManifest);

    for(const auto& pair : CScraperManifest::mapManifest)
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
    return obj;
}

UniValue getmpart(const UniValue& params, bool fHelp)
{
    if(fHelp || params.size() != 1 )
        throw std::runtime_error(
                "getmpart <hash>\n"
                "Show content of CPart object.\n"
                );
    auto ipart= CSplitBlob::mapParts.find(uint256(params[0].get_str()));
    if(ipart == CSplitBlob::mapParts.end())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Object not found");
    return UniValue(HexStr(ipart->second.data.begin(),ipart->second.data.end()));
}
