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

/** Abstract class for blobs that are split into parts. This more complex approach using a parent vanilla parts class will
 *  allow the parts system to be used for other purposes besides the scrapers if needed in the future.
 * polymorphism.
 */
class CSplitBlob
{
public:
    /** Parts of the Split object. For right now in the current implementation for the scraper system, all objects are
     *  represented by one part. This provides the future capability to have large objects greater than the message size
     *  limit (currently 32 MiB), but this is not necessary now.
     */
    struct CPart {
        std::set<std::pair<CSplitBlob*, unsigned int>> refs;
        SerializeData data;
        uint256 hash;
        CPart(const uint256& ihash)
            :hash(ihash)
        {}
        CDataStream getReader() const { return CDataStream(data, SER_NETWORK, PROTOCOL_VERSION); }
        bool present() const { return !this->data.empty(); }
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
    /** Boolean that returns whether all parts for the split object have been received. */
    bool isComplete() const;

    /** Notification that this Split object is fully received. */
    virtual void Complete() = 0;

    /** Use the node as download source for this Split object. */
    void UseAsSource(CNode* pnode);

    /** Add a part reference to vParts. Creates a CPart if necessary. */
    void addPart(const uint256& ihash);

    /** Create a part from specified data and add reference to it into vParts. */
    int addPartData(CDataStream&& vData, const bool &publish_in_progress = false);

    /** Unref all parts referenced by this. Removes parts with no references */
    virtual ~CSplitBlob();

    // static variables
    /** Mutex for mapParts */
    static CCriticalSection cs_mapParts;

    /** map from part hash to scraper Index, so we can attach incoming Part in Index */
    static std::map<uint256, CPart> mapParts GUARDED_BY(cs_mapParts);

    // member variables
    /** Guards vParts and other manifest fields of the manifest (derived) class.
     * Note that this needs to be mutable so that a lock can be taken internally on cs_manifest on an
     * otherwise const qualified member function.
     */
    mutable CCriticalSection cs_manifest;

    std::vector<CPart*> vParts GUARDED_BY(cs_manifest);
    size_t cntPartsRcvd GUARDED_BY(cs_manifest) = 0;

    //!
    //! \brief Used by the scraper when building manifests part by part to prevent triggering Complete() prematurely.
    //!
    bool m_publish_in_progress GUARDED_BY(cs_manifest) = false;
};

/** An objects holding info about the scraper data file we have or are downloading. */
class CScraperManifest
        : public CSplitBlob
{
public: /* constructors */
    CScraperManifest();

    CScraperManifest(CScraperManifest& manifest);

public: /* static methods */
    /** Mutex protects both mapManifest and MapPendingDeletedManifest */
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

    /** Send manifest pointed to by the provided smart pointer to node.
     * @returns whether something was sent
     */
    static bool SendManifestTo(CNode* pfrom, std::shared_ptr<CScraperManifest> manifest);

    /** Add new manifest object into list of known manifests */
    static bool addManifest(std::shared_ptr<CScraperManifest> m, CKey& keySign);

    /** Validate whether received manifest is authorized */
    static bool IsManifestAuthorized(int64_t& nTime, CPubKey& PubKey, unsigned int& banscore_out);

    /** Delete Manifest (key version) */
    static bool DeleteManifest(const uint256& nHash, const bool& fImmediate = false);

    /** Delete Manifest (iterator version) */
    static std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator
        DeleteManifest(std::map<uint256, std::shared_ptr<CScraperManifest>>::iterator& iter, const bool& fImmediate = false);

    /** Delete PendingDeletedManifests */
    static unsigned int DeletePendingDeletedManifests();


public: /*==== fields ====*/
    /** LOCAL only (not serialized) pointer to hash (index) field of mapManifest */
    const uint256* phash GUARDED_BY(cs_manifest) = nullptr;

    /** By convention the string version of the public key on the scraper used to publish the manifest */
    std::string sCManifestName GUARDED_BY(cs_manifest);
    /** The public key of the private key used by the publishing scraper to sign the manifest */
    CPubKey pubkey GUARDED_BY(cs_manifest);
    /** The signature on the manifest from the publishing scraper */
    std::vector<unsigned char> signature GUARDED_BY(cs_manifest);

    /** Project "directory" entry in the manifest. The GridcoinTeamID is not used. */
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

        /** Outputs content of dentry in JSON format. Helper function to CScraperManifest::ToJson(). */
        UniValue ToJson() const;
    };

    /** Vector of project entries */
    std::vector<dentry> projects GUARDED_BY(cs_manifest);

    /** Part index in the vParts vector for the BeaconList. This should always be zero once populated (the first element). */
    int BeaconList GUARDED_BY(cs_manifest) = -1 ;
    unsigned BeaconList_c GUARDED_BY(cs_manifest) = 0;
    /** The block on which the convergence will be formed if this manifest is part of a convergence. */
    uint256 ConsensusBlock GUARDED_BY(cs_manifest);
    /** The time the manifest was published */
    int64_t nTime GUARDED_BY(cs_manifest) = 0;

    /** The hash of the manifest's contents (the vparts vector). This hash is used for matching purposes in a convergence. */
    uint256 nContentHash GUARDED_BY(cs_manifest);

    /** The bCheckedAuthorized flag is LOCAL only. It is not serialized/deserialized. This
     * is set during Unserializecheck to false if wallet not in sync, and true if in sync
     * and scraper ID matches authorized list (i.e. IsManifestAuthorized is true.
     * The node will walk the mapManifest from
     */
    bool bCheckedAuthorized GUARDED_BY(cs_manifest);

public: /* public methods */

    /** Hook called when all parts are available */
    void Complete() override;

    /** Serialize this object for sending over the network. This includes the signature as well as the payload. */
    void Serialize(CDataStream& s) const;
    /** Serialize without the signature. We need this to generate the (inner) content for the hash to sign with the key. */
    void SerializeWithoutSignature(CDataStream& s) const;
    /** Serialize the contents (vParts vector) for purposes of content comparison. This is used to fill out the nContentHash,
     *  which is then included in SerializeWithoutSignature.
     */
    void SerializeForManifestCompare(CDataStream& ss) const;
    /** A combination of unserialization and integrity checking, which includes hash checks, authorization checks, and
     *  signature checks.
     */
    [[nodiscard]] bool UnserializeCheck(CDataStream& s, unsigned int& banscore_out);

    /** Checks to see whether manifest age is current according to the SCRAPER_CMANIFEST_RETENTION_TIME network setting. */
    bool IsManifestCurrent() const;

    /** Outputs manifest in JSON format. */
    UniValue ToJson() const;
};

#endif // GRIDCOIN_SCRAPER_SCRAPER_NET_H
