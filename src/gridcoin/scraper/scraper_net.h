// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/* Maybe the parts system will be useful for other things so let's abstract
 * that to parent class. Since it will be all in one file there will not be any
 * polymorphism.
*/

#include <key.h>
#include "net.h"
#include "streams.h"
#include "sync.h"

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

    /** Process a message containing Part of Blob.
   * @return whether the data was useful
  */
    static bool RecvPart(CNode* pfrom, CDataStream& vRecv);

    bool isComplete() const
    { return cntPartsRcvd == vParts.size(); }

    /** Notification that this Split object is fully received. */
    virtual void Complete() = 0;

    /** Use the node as download source for this Split object. */
    void UseAsSource(CNode* pnode);

    /** Forward requested Part of Blob.
   * @returns whether something was sent
  */
    static bool SendPartTo(CNode* pto, const uint256& hash);

    std::vector<CPart*> vParts;

    /** Add a part reference to vParts. Creates a CPart if necessary. */
    void addPart(const uint256& ihash);

    /** Create a part from specified data and add reference to it into vParts. */
    int addPartData(CDataStream&& vData);

    /** Unref all parts referenced by this. Removes parts with no references */
    ~CSplitBlob();

    /* We could store the parts in mapRelay and have getdata service for free. */
    /** map from part hash to scraper Index, so we can attach incoming Part in Index */
    static std::map<uint256,CPart> mapParts;
    size_t cntPartsRcvd =0;

    static CCriticalSection cs_mapParts; // also protects vParts.

};

/** A objects holding info about the scraper data file we have or are downloading. */
class CScraperManifest
        : public CSplitBlob
{
public: /* static methods */

    /** map from index hash to scraper Index, so we can process Inv messages */
    static std::map<uint256, std::shared_ptr<CScraperManifest>> mapManifest;

    // ------------ hash -------------- nTime ------- pointer to CScraperManifest
    static std::map<uint256, std::pair<int64_t, std::shared_ptr<CScraperManifest>>> mapPendingDeletedManifest;

    // Protects both mapManifest and MapPendingDeletedManifest
    static CCriticalSection cs_mapManifest;

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

    const uint256* phash;
    std::string sCManifestName;
    CPubKey pubkey;
    std::vector<unsigned char> signature;

    struct dentry {
        std::string project;
        std::string ETag;
        unsigned int LastModified =0;
        int part1 =-1;
        unsigned partc =0;
        int GridcoinTeamID =-1;
        bool current =0;
        bool last =0;

        void Serialize(CDataStream& s) const;
        void Unserialize(CDataStream& s);
        UniValue ToJson() const;
    };

    std::vector<dentry> projects;

    int BeaconList =-1;
    unsigned BeaconList_c =0;
    uint256 ConsensusBlock;
    int64_t nTime = 0;

    uint256 nContentHash;

    // The bCheckedAuthorized flag is LOCAL only. It is not serialized/deserialized. This
    // is set during Unserializecheck to false if wallet not in sync, and true if in sync
    // and scraper ID matches authorized list (i.e. IsManifestAuthorized is true.
    // The node will walk the mapManifest from
    bool bCheckedAuthorized;

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
