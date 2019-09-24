#pragma once

/* scraper_net.h */

/* Maybe the parts system will be useful for other things so let's abstract
 * that to parent class. Since it will be all in one file there will not be any
 * polymorphism.
*/

#include "net.h"
#include "sync.h"

#include <key.h>
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
        CReaderStream getReader() const { return CReaderStream(&data); }
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

    static CCriticalSection cs_mapParts;

};

/** A objects holding info about the scraper data file we have or are downloading. */
class CScraperManifest
        : public CSplitBlob
{
public: /* static methods */

    /** map from index hash to scraper Index, so we can process Inv messages */
    static std::map<uint256, std::unique_ptr<CScraperManifest>> mapManifest;

    // ------------ hash -------------- nTime ------- pointer to CScraperManifest
    static std::map<uint256, std::pair<int64_t, std::unique_ptr<CScraperManifest>>> mapPendingDeletedManifest;

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
    static bool addManifest(std::unique_ptr<CScraperManifest>&& m, CKey& keySign);

    /** Validate whether recieved manifest is authorized */
    static bool IsManifestAuthorized(CPubKey& PubKey, unsigned int& banscore_out);

    /** Delete Manifest (key version) **/
    static bool DeleteManifest(const uint256& nHash, const bool& fImmediate = false);

    /** Delete Manifest (iterator version) **/
    static std::map<uint256, std::unique_ptr<CScraperManifest>>::iterator
        DeleteManifest(std::map<uint256, std::unique_ptr<CScraperManifest>>::iterator& iter, const bool& fImmediate = false);

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

        void Serialize(CDataStream& s, int nType, int nVersion) const;
        void Unserialize(CReaderStream& s, int nType, int nVersion);
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
    void Serialize(CDataStream& s, int nType, int nVersion) const;
    void SerializeWithoutSignature(CDataStream& s, int nType, int nVersion) const;
    void SerializeForManifestCompare(CDataStream& ss, int nType, int nVersion) const;
    void UnserializeCheck(CReaderStream& s, unsigned int& banscore_out);

    bool IsManifestCurrent() const;

    UniValue ToJson() const;

};
