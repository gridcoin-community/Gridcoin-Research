// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_SCRAPER_SCRAPER_NET_H
#define GRIDCOIN_SCRAPER_SCRAPER_NET_H

/* Maybe the parts system will be useful for other things so let's abstract
 * that to parent class. Since it will be all in one file there will not be any
 * polymorphism.
*/

#include <key.h>
#include "net.h"
#include "streams.h"
#include "sync.h"
#include "gridcoin/appcache.h"

#include <univalue.h>


/** Abstract class for blobs that are split into parts. */
class CSplitBlob
{
public:
    /** Parts of the Split object */
    struct CPart {
        std::set<std::pair<CSplitBlob*,unsigned>> refs;
        CSerializeData data;
        uint256 hash;
        CPart(const uint256& ihash)
            :hash(ihash)
        {}
        CDataStream getReader() const { return CDataStream(data.begin(), data.end(), SER_NETWORK, PROTOCOL_VERSION); }
        bool present() const {return !this->data.empty();}
    };

    // static methods
    /** Process a message containing Part of Blob.
     * @return whether the data was useful
     */
    static bool RecvPart(CNode* pfrom, CDataStream& vRecv);

    /** Forward requested Part of Blob.
     * @returns whether something was sent
     */
    static bool SendPartTo(CNode* pto, const uint256& hash);

    // public methods
    /** Boolean that returns whether all parts for the split object have been received. **/
    bool isComplete() const;

    /** Notification that this Split object is fully received. */
    virtual void Complete() = 0;

    /** Use the node as download source for this Split object. */
    void UseAsSource(CNode* pnode);

    /** Add a part reference to vParts. Creates a CPart if necessary. */
    void addPart(const uint256& ihash);

    /** Create a part from specified data and add reference to it into vParts. */
    int addPartData(CDataStream&& vData);

    /** Unref all parts referenced by this. Removes parts with no references */
    virtual ~CSplitBlob();

    // static variables
    /** Mutex for mapParts **/
    static CCriticalSection cs_mapParts;

    /* We could store the parts in mapRelay and have getdata service for free. */
    /** map from part hash to scraper Index, so we can attach incoming Part in Index */
    static std::map<uint256, CPart> mapParts GUARDED_BY(cs_mapParts);

    // member variables
    /** Guards vParts and other manifest fields of the manifest (derived) class.
     * Note that this needs to be mutable so that a lock can be taken internally on cs_manifest on an
     * otherwise const qualified member function.
     **/
    mutable CCriticalSection cs_manifest;

    std::vector<CPart*> vParts GUARDED_BY(cs_manifest);
    size_t cntPartsRcvd GUARDED_BY(cs_manifest) = 0;
};

/** An objects holding info about the scraper data file we have or are downloading. */
class CScraperManifest
        : public CSplitBlob
{
public: /* constructors */
    CScraperManifest();

    CScraperManifest(CScraperManifest& manifest);

public: /* static methods */
    /** Mutex protects both mapManifest and MapPendingDeletedManifest **/
    static CCriticalSection cs_mapManifest;

    /** map from index hash to scraper Index, so we can process Inv messages */
    static std::map<uint256, std::shared_ptr<CScraperManifest>> mapManifest GUARDED_BY(cs_mapManifest);

    /** map of manifests that are pending deletion */
    // ------------ hash -------------- nTime ------- pointer to CScraperManifest
    static std::map<uint256, std::pair<int64_t, std::shared_ptr<CScraperManifest>>> mapPendingDeletedManifest GUARDED_BY(cs_mapManifest);

    /** Process a message containing Index of Scraper Data.
     * @returns whether the data was useful and valid
     */
    static bool RecvManifest(CNode* pfrom, CDataStream& vRecv);

    /** Check if we already have this object.
     * @returns false only if we need this object
     * Additionally sender node is used as fetch source if needed
     */
    static bool AlreadyHave(CNode* pfrom, const CInv& inv);

    /** Send Inv to that node about data files we have.
     * Called when a node connects.
     */
    static void PushInvTo(CNode* pto);

    /** Send a manifest of requested hash to node (from mapManifest).
     * @returns whether something was sent
     */
     static bool SendManifestTo(CNode* pfrom, const uint256& hash);

    /** Add new manifest object into list of known manifests */
    static bool addManifest(std::shared_ptr<CScraperManifest>&& m, CKey& keySign);

    /** Validate whether received manifest is authorized */
    static bool IsManifestAuthorized(int64_t& nTime, CPubKey& PubKey, unsigned int& banscore_out);

    /** Delete Manifest (key version) **/
    static bool DeleteManifest(const uint256& nHash, const bool& fImmediate = false);

    /** Delete Manifest (iterator version) **/
    static std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator
        DeleteManifest(std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator& iter, const bool& fImmediate = false);

    /** Delete PendingDeletedManifests **/
    static unsigned int DeletePendingDeletedManifests();


public: /*==== fields ====*/
    /** Local only (not serialized) pointer to hash (index) field of mapManifest **/
    const uint256* phash GUARDED_BY(cs_manifest) = nullptr;

    std::string sCManifestName GUARDED_BY(cs_manifest);
    CPubKey pubkey GUARDED_BY(cs_manifest);
    std::vector<unsigned char> signature GUARDED_BY(cs_manifest);

    struct dentry {
        std::string project;
        std::string ETag;
        unsigned int LastModified = 0;
        int part1 = -1;
        unsigned partc = 0;
        int GridcoinTeamID = -1;
        bool current = 0;
        bool last = 0;

        void Serialize(CDataStream& s) const;
        void Unserialize(CDataStream& s);
        UniValue ToJson() const;
    };

    std::vector<dentry> projects GUARDED_BY(cs_manifest);

    int BeaconList GUARDED_BY(cs_manifest) = -1 ;
    unsigned BeaconList_c GUARDED_BY(cs_manifest) = 0;
    uint256 ConsensusBlock GUARDED_BY(cs_manifest);
    int64_t nTime GUARDED_BY(cs_manifest) = 0;

    uint256 nContentHash GUARDED_BY(cs_manifest);

    // The bCheckedAuthorized flag is LOCAL only. It is not serialized/deserialized. This
    // is set during Unserializecheck to false if wallet not in sync, and true if in sync
    // and scraper ID matches authorized list (i.e. IsManifestAuthorized is true.
    // The node will walk the mapManifest from
    bool bCheckedAuthorized GUARDED_BY(cs_manifest);

public: /* public methods */

    /** Hook called when all parts are available */
    void Complete() override;

    /** Serialize this object for seding over the network. */
    void Serialize(CDataStream& s) const;
    void SerializeWithoutSignature(CDataStream& s) const;
    void SerializeForManifestCompare(CDataStream& ss) const;
    void UnserializeCheck(CDataStream& s, unsigned int& banscore_out);

    bool IsManifestCurrent() const;

    UniValue ToJson() const;
};

#endif // GRIDCOIN_SCRAPER_SCRAPER_NET_H
