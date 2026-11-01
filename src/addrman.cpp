// Copyright (c) 2012 Pieter Wuille
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "addrman.h"
#include "addrman_impl.h"

#include "hash.h"
#include "random.h"
#include "streams.h"

#include <memory>

using namespace std;

int AddrInfo::GetTriedBucket(const uint256& nKey) const
{
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << GetKey()).GetCheapHash();
    uint64_t hash2 = (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup() << (hash1 % ADDRMAN_TRIED_BUCKETS_PER_GROUP)).GetCheapHash();
    return hash2 % ADDRMAN_TRIED_BUCKET_COUNT;
}

int AddrInfo::GetNewBucket(const uint256& nKey, const CNetAddr& src) const
{
    std::vector<unsigned char> vchSourceGroupKey = src.GetGroup();
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup() << vchSourceGroupKey).GetCheapHash();
    uint64_t hash2 = (CHashWriter(SER_GETHASH, 0) << nKey << vchSourceGroupKey << (hash1 % ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP)).GetCheapHash();
    return hash2 % ADDRMAN_NEW_BUCKET_COUNT;
}

int AddrInfo::GetBucketPosition(const uint256 &nKey, bool fNew, int nBucket) const
{
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << (fNew ? 'N' : 'K') << nBucket << GetKey()).GetCheapHash();
    return hash1 % ADDRMAN_BUCKET_SIZE;
}

bool AddrInfo::IsTerrible(int64_t nNow) const
{
    if (nLastTry && nLastTry >= nNow-60) // never remove things tried the last minute
        return false;

    if (nTime > nNow + 10*60) // came in a flying DeLorean
        return true;

    if (nTime==0 || nNow-nTime > ADDRMAN_HORIZON_DAYS*86400) // not seen in over a month
        return true;

    if (nLastSuccess==0 && nAttempts>=ADDRMAN_RETRIES) // tried three times and never a success
        return true;

    if (nNow-nLastSuccess > ADDRMAN_MIN_FAIL_DAYS*86400 && nAttempts>=ADDRMAN_MAX_FAILURES) // 10 successive failures in the last week
        return true;

    return false;
}

double AddrInfo::GetChance(int64_t nNow) const
{
    double fChance = 1.0;

    int64_t nSinceLastSeen = nNow - nTime;
    int64_t nSinceLastTry = nNow - nLastTry;

    if (nSinceLastSeen < 0) nSinceLastSeen = 0;
    if (nSinceLastTry < 0) nSinceLastTry = 0;

    // deprioritize very recent attempts away
    if (nSinceLastTry < 60*10)
        fChance *= 0.01;

    // deprioritize 50% after each failed attempt
    for (int n=0; n<nAttempts; n++)
        fChance /= 1.5;

    return fChance;
}

AddrManImpl::AddrManImpl(bool deterministic)
    : m_deterministic(deterministic)
    , m_det_rand_state(1)
{
    Clear();
    if (m_deterministic) {
        // Reproduce the pre-split AddrManTest's MakeDeterministic(): a null key
        // and a deterministic insecure_rand, paired with a deterministic
        // RandomInt (see RandomInt() below). This keeps the unit-test bucket
        // placements and selections byte-identical.
        nKey.SetNull();
        insecure_rand = FastRandomContext(true);
    }
}

AddrManImpl::~AddrManImpl()
{
    nKey.SetNull();
}

AddrInfo* AddrManImpl::Find(const CNetAddr& addr, int *pnId)
{
    std::map<CNetAddr, int>::iterator it = mapAddr.find(addr);
    if (it == mapAddr.end())
        return nullptr;
    if (pnId)
        *pnId = it->second;
    std::map<int, AddrInfo>::iterator it2 = mapInfo.find(it->second);
    if (it2 != mapInfo.end())
        return &it2->second;
    return nullptr;
}

AddrInfo* AddrManImpl::Create(const CAddress &addr, const CNetAddr &addrSource, int *pnId)
{
    int nId = nIdCount++;
    mapInfo[nId] = AddrInfo(addr, addrSource);
    mapAddr[addr] = nId;
    mapInfo[nId].nRandomPos = vRandom.size();
    vRandom.push_back(nId);
    if (pnId)
        *pnId = nId;
    return &mapInfo[nId];
}

void AddrManImpl::SwapRandom(unsigned int nRndPos1, unsigned int nRndPos2)
{
    if (nRndPos1 == nRndPos2)
        return;

    assert(nRndPos1 < vRandom.size() && nRndPos2 < vRandom.size());

    int nId1 = vRandom[nRndPos1];
    int nId2 = vRandom[nRndPos2];

    assert(mapInfo.count(nId1) == 1);
    assert(mapInfo.count(nId2) == 1);

    mapInfo[nId1].nRandomPos = nRndPos2;
    mapInfo[nId2].nRandomPos = nRndPos1;

    vRandom[nRndPos1] = nId2;
    vRandom[nRndPos2] = nId1;
}

void AddrManImpl::Delete(int nId)
{
    assert(mapInfo.count(nId) != 0);
    AddrInfo& info = mapInfo[nId];
    assert(!info.fInTried);
    assert(info.nRefCount == 0);

    SwapRandom(info.nRandomPos, vRandom.size() - 1);
    vRandom.pop_back();
    mapAddr.erase(info);
    mapInfo.erase(nId);
    nNew--;
}

void AddrManImpl::ClearNew(int nUBucket, int nUBucketPos)
{
    // if there is an entry in the specified bucket, delete it.
    if (vvNew[nUBucket][nUBucketPos] != -1) {
        int nIdDelete = vvNew[nUBucket][nUBucketPos];
        AddrInfo& infoDelete = mapInfo[nIdDelete];
        assert(infoDelete.nRefCount > 0);
        infoDelete.nRefCount--;
        vvNew[nUBucket][nUBucketPos] = -1;
        if (infoDelete.nRefCount == 0) {
            Delete(nIdDelete);
        }
    }
}

void AddrManImpl::MakeTried(AddrInfo& info, int nId)
{
    // remove the entry from all new buckets
    for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
        int pos = info.GetBucketPosition(nKey, true, bucket);
        if (vvNew[bucket][pos] == nId) {
            vvNew[bucket][pos] = -1;
            info.nRefCount--;
        }
    }
    nNew--;

    assert(info.nRefCount == 0);

    // which tried bucket to move the entry to
    int nKBucket = info.GetTriedBucket(nKey);
    int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);

    // first make space to add it (the existing tried entry there is moved to new, deleting whatever is there).
    if (vvTried[nKBucket][nKBucketPos] != -1) {
        // find an item to evict
        int nIdEvict = vvTried[nKBucket][nKBucketPos];
        assert(mapInfo.count(nIdEvict) == 1);
        AddrInfo& infoOld = mapInfo[nIdEvict];

        // Remove the to-be-evicted item from the tried set.
        infoOld.fInTried = false;
        vvTried[nKBucket][nKBucketPos] = -1;
        nTried--;

        // find which new bucket it belongs to
        int nUBucket = infoOld.GetNewBucket(nKey);
        int nUBucketPos = infoOld.GetBucketPosition(nKey, true, nUBucket);
        ClearNew(nUBucket, nUBucketPos);
        assert(vvNew[nUBucket][nUBucketPos] == -1);

        // Enter it into the new set again.
        infoOld.nRefCount = 1;
        vvNew[nUBucket][nUBucketPos] = nIdEvict;
        nNew++;
    }
    assert(vvTried[nKBucket][nKBucketPos] == -1);

    vvTried[nKBucket][nKBucketPos] = nId;
    nTried++;
    info.fInTried = true;
}

void AddrManImpl::Good_(const CService& addr, int64_t nTime)
{
    int nId;
    AddrInfo* pinfo = Find(addr, &nId);

    // if not found, bail out
    if (!pinfo)
        return;

    AddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastSuccess = nTime;
    info.nLastTry = nTime;
    info.nAttempts = 0;
    // nTime is not updated here, to avoid leaking information about
    // currently-connected peers.

    // if it is already in the tried set, don't do anything else
    if (info.fInTried)
        return;

    // find a bucket it is in now
    int nRnd = GetRand<int>(ADDRMAN_NEW_BUCKET_COUNT);
    int nUBucket = -1;
    for (unsigned int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        int nB = (n + nRnd) % ADDRMAN_NEW_BUCKET_COUNT;
        int nBpos = info.GetBucketPosition(nKey, true, nB);
        if (vvNew[nB][nBpos] == nId) {
            nUBucket = nB;
            break;
        }
    }

    // if no bucket is found, something bad happened;
    // TODO: maybe re-add the node, but for now, just bail out
    if (nUBucket == -1)
        return;

    LogPrint(BCLog::LogFlags::ADDRMAN, "Moving %s to tried", addr.ToString());

    // move nId to the tried tables
    MakeTried(info, nId);
}

bool AddrManImpl::Add_(const CAddress& addr, const CNetAddr& source, int64_t nTimePenalty)
{
    if (!addr.IsRoutable())
        return false;

    bool fNew = false;
    int nId;
    AddrInfo* pinfo = Find(addr, &nId);

    if (pinfo) {
        // periodically update nTime
        bool fCurrentlyOnline = (GetAdjustedTime() - addr.nTime < 24 * 60 * 60);
        int64_t nUpdateInterval = (fCurrentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (addr.nTime && (!pinfo->nTime || pinfo->nTime < addr.nTime - nUpdateInterval - nTimePenalty))
            pinfo->nTime = max((int64_t)0, addr.nTime - nTimePenalty);

        // add services
        pinfo->nServices = ServiceFlags(pinfo->nServices | addr.nServices);

        // do not update if no new information is present
        if (!addr.nTime || (pinfo->nTime && addr.nTime <= pinfo->nTime))
            return false;

        // do not update if the entry was already in the "tried" table
        if (pinfo->fInTried)
            return false;

        // do not update if the max reference count is reached
        if (pinfo->nRefCount == ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
            return false;

        // stochastic test: previous nRefCount == N: 2^N times harder to increase it
        int nFactor = 1;
        for (int n = 0; n < pinfo->nRefCount; n++)
            nFactor *= 2;
        if (nFactor > 1 && (GetRand<int>(nFactor) != 0))
            return false;
    } else {
        pinfo = Create(addr, source, &nId);
        pinfo->nTime = max((int64_t)0, (int64_t)pinfo->nTime - nTimePenalty);
        nNew++;
        fNew = true;
    }

    int nUBucket = pinfo->GetNewBucket(nKey, source);
    int nUBucketPos = pinfo->GetBucketPosition(nKey, true, nUBucket);
    if (vvNew[nUBucket][nUBucketPos] != nId) {
        bool fInsert = vvNew[nUBucket][nUBucketPos] == -1;
        if (!fInsert) {
            AddrInfo& infoExisting = mapInfo[vvNew[nUBucket][nUBucketPos]];
            if (infoExisting.IsTerrible() || (infoExisting.nRefCount > 1 && pinfo->nRefCount == 0)) {
                // Overwrite the existing new table entry.
                fInsert = true;
            }
        }
        if (fInsert) {
            ClearNew(nUBucket, nUBucketPos);
            pinfo->nRefCount++;
            vvNew[nUBucket][nUBucketPos] = nId;
        } else {
            if (pinfo->nRefCount == 0) {
                Delete(nId);
            }
        }
    }
    return fNew;
}

void AddrManImpl::Attempt_(const CService &addr, int64_t nTime)
{
    AddrInfo *pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    AddrInfo &info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastTry = nTime;
    info.nAttempts++;
}

AddrInfo AddrManImpl::Select_(bool newOnly)
{
    if (size() == 0)
        return AddrInfo();

    if (newOnly && nNew == 0)
        return AddrInfo();

    // Use a 50% chance for choosing between tried and new table entries.
    if (!newOnly &&
       (nTried > 0 && (nNew == 0 || RandomInt(2) == 0))) {
        // use a tried node
        double fChanceFactor = 1.0;
        while (1) {
            int nKBucket = RandomInt(ADDRMAN_TRIED_BUCKET_COUNT);
            int nKBucketPos = RandomInt(ADDRMAN_BUCKET_SIZE);
            while (vvTried[nKBucket][nKBucketPos] == -1) {
                nKBucket = (nKBucket + insecure_rand()) % ADDRMAN_TRIED_BUCKET_COUNT;
                nKBucketPos = (nKBucketPos + insecure_rand()) % ADDRMAN_BUCKET_SIZE;
            }
            int nId = vvTried[nKBucket][nKBucketPos];
            assert(mapInfo.count(nId) == 1);
            AddrInfo& info = mapInfo[nId];
            if (RandomInt(1 << 30) < fChanceFactor * info.GetChance() * (1 << 30))
                return info;
            fChanceFactor *= 1.2;
        }
    } else {
        // use a new node
        double fChanceFactor = 1.0;
        while (1) {
            int nUBucket = RandomInt(ADDRMAN_NEW_BUCKET_COUNT);
            int nUBucketPos = RandomInt(ADDRMAN_BUCKET_SIZE);
            while (vvNew[nUBucket][nUBucketPos] == -1) {
                nUBucket = (nUBucket + insecure_rand()) % ADDRMAN_NEW_BUCKET_COUNT;
                nUBucketPos = (nUBucketPos + insecure_rand()) % ADDRMAN_BUCKET_SIZE;
            }
            int nId = vvNew[nUBucket][nUBucketPos];
            assert(mapInfo.count(nId) == 1);
            AddrInfo& info = mapInfo[nId];
            if (RandomInt(1 << 30) < fChanceFactor * info.GetChance() * (1 << 30))
                return info;
            fChanceFactor *= 1.2;
        }
    }
}

#ifdef DEBUG_ADDRMAN
int AddrManImpl::Check_()
{
    std::set<int> setTried;
    std::map<int, int> mapNew;

    if (vRandom.size() != nTried + nNew)
        return -7;

    for (std::map<int, AddrInfo>::iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
        int n = (*it).first;
        AddrInfo& info = (*it).second;
        if (info.fInTried) {
            if (!info.nLastSuccess)
                return -1;
            if (info.nRefCount)
                return -2;
            setTried.insert(n);
        } else {
            if (info.nRefCount < 0 || info.nRefCount > ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
                return -3;
            if (!info.nRefCount)
                return -4;
            mapNew[n] = info.nRefCount;
        }
        if (mapAddr[info] != n)
            return -5;
        if (info.nRandomPos < 0 || info.nRandomPos >= vRandom.size() || vRandom[info.nRandomPos] != n)
            return -14;
        if (info.nLastTry < 0)
            return -6;
        if (info.nLastSuccess < 0)
            return -8;
    }

    if (setTried.size() != nTried)
        return -9;
    if (mapNew.size() != nNew)
        return -10;

    for (int n = 0; n < ADDRMAN_TRIED_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
             if (vvTried[n][i] != -1) {
                 if (!setTried.count(vvTried[n][i]))
                     return -11;
                 if (mapInfo[vvTried[n][i]].GetTriedBucket(nKey) != n)
                     return -17;
                 if (mapInfo[vvTried[n][i]].GetBucketPosition(nKey, false, n) != i)
                     return -18;
                 setTried.erase(vvTried[n][i]);
             }
        }
    }

    for (int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
            if (vvNew[n][i] != -1) {
                if (!mapNew.count(vvNew[n][i]))
                    return -12;
                if (mapInfo[vvNew[n][i]].GetBucketPosition(nKey, true, n) != i)
                    return -19;
                if (--mapNew[vvNew[n][i]] == 0)
                    mapNew.erase(vvNew[n][i]);
            }
        }
    }

    if (setTried.size())
        return -13;
    if (mapNew.size())
        return -15;
    if (nKey.IsNull())
        return -16;

    return 0;
}
#endif

void AddrManImpl::GetAddr_(std::vector<CAddress> &vAddr)
{
    unsigned int nNodes = ADDRMAN_GETADDR_MAX_PCT * vRandom.size() / 100;
    if (nNodes > ADDRMAN_GETADDR_MAX)
        nNodes = ADDRMAN_GETADDR_MAX;

    // gather a list of random nodes, skipping those of low quality
    for (unsigned int n = 0; n < vRandom.size(); n++)
    {
        if (vAddr.size() >= nNodes)
            break;

        int nRndPos = GetRand<int>(vRandom.size() - n) + n;
        SwapRandom(n, nRndPos);
        assert(mapInfo.count(vRandom[n]) == 1);

        const AddrInfo& ai = mapInfo[vRandom[n]];
        if(!ai.IsTerrible())
            vAddr.push_back(ai);
    }
}

void AddrManImpl::Connected_(const CService &addr, int64_t nTime)
{
    AddrInfo *pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    AddrInfo &info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    int64_t nUpdateInterval = 20 * 60;
    if (nTime - info.nTime > nUpdateInterval)
        info.nTime = nTime;
}

void AddrManImpl::SetServices_(const CService& addr, ServiceFlags nServices)
{
    AddrInfo* pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    AddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nServices = nServices;
}

int AddrManImpl::RandomInt(int nMax)
{
    if (m_deterministic) {
        // Deterministic PRNG identical to the pre-split AddrManTest::RandomInt,
        // so test bucket placements/selections are byte-preserved.
        m_det_rand_state = (CHashWriter(SER_GETHASH, 0) << m_det_rand_state).GetCheapHash();
        return (unsigned int)(m_det_rand_state % nMax);
    }
    return GetRand<int>(nMax);
}

void AddrManImpl::Clear()
{
    std::vector<int>().swap(vRandom);
    nKey = GetRandHash();
    for (size_t bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
        for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
            vvNew[bucket][entry] = -1;
        }
    }
    for (size_t bucket = 0; bucket < ADDRMAN_TRIED_BUCKET_COUNT; bucket++) {
        for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
            vvTried[bucket][entry] = -1;
        }
    }

    nIdCount = 0;
    nTried = 0;
    nNew = 0;
}

size_t AddrManImpl::size() const
{
    LOCK(cs); // TODO: Cache this in an atomic to avoid this overhead
    return vRandom.size();
}

void AddrManImpl::Check()
{
#ifdef DEBUG_ADDRMAN
    {
        LOCK(cs);
        int err;
        if ((err=Check_()))
            LogPrint(BCLog::LogFlags::ADDRMAN, "ADDRMAN CONSISTENCY CHECK FAILED!!! err=%i", err);
    }
#endif
}

bool AddrManImpl::Add(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty)
{
    LOCK(cs);
    bool fRet = false;
    Check();
    fRet |= Add_(addr, source, nTimePenalty);
    Check();
    if (fRet)
        LogPrint(BCLog::LogFlags::ADDRMAN,"Added %s from %s: %i tried, %i new", addr.ToStringIPPort(), source.ToString(), nTried, nNew);
    return fRet;
}

bool AddrManImpl::Add(const std::vector<CAddress> &vAddr, const CNetAddr& source, int64_t nTimePenalty)
{
    LOCK(cs);
    int nAdd = 0;
    Check();
    for (std::vector<CAddress>::const_iterator it = vAddr.begin(); it != vAddr.end(); it++)
        nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
    Check();
    if (nAdd)
        LogPrint(BCLog::LogFlags::ADDRMAN,"Added %i addresses from %s: %i tried, %i new", nAdd, source.ToString(), nTried, nNew);
    return nAdd > 0;
}

void AddrManImpl::Good(const CService &addr, int64_t nTime)
{
    LOCK(cs);
    Check();
    Good_(addr, nTime);
    Check();
}

void AddrManImpl::Attempt(const CService &addr, int64_t nTime)
{
    LOCK(cs);
    Check();
    Attempt_(addr, nTime);
    Check();
}

CAddress AddrManImpl::Select(bool newOnly)
{
    AddrInfo addrRet;
    {
        LOCK(cs);
        Check();
        addrRet = Select_(newOnly);
        Check();
    }
    return addrRet;
}

std::vector<CAddress> AddrManImpl::GetAddr()
{
    Check();
    std::vector<CAddress> vAddr;
    {
        LOCK(cs);
        GetAddr_(vAddr);
    }
    Check();
    return vAddr;
}

void AddrManImpl::Connected(const CService &addr, int64_t nTime)
{
    LOCK(cs);
    Check();
    Connected_(addr, nTime);
    Check();
}

void AddrManImpl::SetServices(const CService &addr, ServiceFlags nServices)
{
    LOCK(cs);
    Check();
    SetServices_(addr, nServices);
    Check();
}

// ---------------------------------------------------------------------------
// AddrMan: thin pimpl wrapper forwarding to AddrManImpl (issue #2558 PR 6b).
// ---------------------------------------------------------------------------

AddrMan::AddrMan(bool deterministic)
    : m_impl(std::make_unique<AddrManImpl>(deterministic))
{
}

AddrMan::~AddrMan() = default;

template <typename Stream>
void AddrMan::Serialize(Stream& s) const
{
    m_impl->Serialize(s);
}

template <typename Stream>
void AddrMan::Unserialize(Stream& s)
{
    m_impl->Unserialize(s);
}

// Explicit instantiations for the concrete stream types addrman is (de)serialized
// with (peers.dat via CAutoFile + CHashWriter/CHashVerifier, and CDataStream in
// the unit tests). A missing instantiation here is a link error.
template void AddrMan::Serialize(CAutoFile&) const;
template void AddrMan::Serialize(CHashWriter&) const;
template void AddrMan::Serialize(CDataStream&) const;
template void AddrMan::Unserialize(CDataStream&);
template void AddrMan::Unserialize(CHashVerifier<CAutoFile>&);
template void AddrMan::Unserialize(CHashVerifier<CDataStream>&);

size_t AddrMan::size() const { return m_impl->size(); }

bool AddrMan::Add(const CAddress& addr, const CNetAddr& source, int64_t nTimePenalty)
{
    return m_impl->Add(addr, source, nTimePenalty);
}

bool AddrMan::Add(const std::vector<CAddress>& vAddr, const CNetAddr& source, int64_t nTimePenalty)
{
    return m_impl->Add(vAddr, source, nTimePenalty);
}

void AddrMan::Good(const CService& addr, int64_t nTime) { m_impl->Good(addr, nTime); }

void AddrMan::Attempt(const CService& addr, int64_t nTime) { m_impl->Attempt(addr, nTime); }

CAddress AddrMan::Select(bool newOnly) { return m_impl->Select(newOnly); }

std::vector<CAddress> AddrMan::GetAddr() { return m_impl->GetAddr(); }

void AddrMan::Connected(const CService& addr, int64_t nTime) { m_impl->Connected(addr, nTime); }

void AddrMan::SetServices(const CService& addr, ServiceFlags nServices) { m_impl->SetServices(addr, nServices); }

void AddrMan::Clear() { m_impl->Clear(); }

AddrInfo* AddrMan::Find(const CNetAddr& addr, int* pnId) { return m_impl->Find(addr, pnId); }

AddrInfo* AddrMan::Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId)
{
    return m_impl->Create(addr, addrSource, pnId);
}

void AddrMan::Delete(int nId) { m_impl->Delete(nId); }
