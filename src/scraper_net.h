/* scraper_net.h */

/* Maybe the parts system will be usefull for other things so let's abstract
 * that to parent class. Sice it will be all in one file there will not be any
 * polymorfism.
*/

#include <key.h>

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
  { return cntPartsRcvd == (long)vParts.size(); }

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
  long addPartData(CDataStream&& vData);

  /** Unref all parts referenced by this. Removes parts with no references */
  ~CSplitBlob();

  /* We could store the parts in mapRelay and have getdata service for free. */
  /** map from part hash to scraper Index, so we can attach incoming Part in Index */
  static std::map<uint256,CPart> mapParts;
  long cntPartsRcvd =0;

};

/** A objects holding info about the scraper data file we have or are downloading. */
class CScraperManifest
  : public CSplitBlob
{
  public: /* static methods */

  /** map from index hash to scraper Index, so we can process Inv messages */
  static std::map< uint256, std::unique_ptr<CScraperManifest> > mapManifest;

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

  public: /*==== fields ====*/

  const uint256* phash;
  std::string testName;
  CPubKey pubkey;
  std::vector<unsigned char> signature;

  struct dentry {
    std::string project;
    std::string ETag;
    unsigned int LastModified =0;
    long part1 =-1;
    unsigned partc =0;
    long GridcoinTeamID =-1;
    bool current =0;
    bool last =0;

    void Serialize(CDataStream& s, int nType, int nVersion) const;
    void Unserialize(CReaderStream& s, int nType, int nVersion);
    UniValue ToJson() const;
  };

  std::vector<dentry> projects;

  long BeaconList =-1;
  unsigned BeaconList_c =0;
  uint256 ConsensusBlock;
  unsigned int nTime =0;

  public: /* public methods */

  /** Hook called when all parts are available */
  void Complete() override;

  /** Serialize this object for seding over the network. */
  void Serialize(CDataStream& s, int nType, int nVersion) const;
  void UnserializeCheck(CReaderStream& s);
  UniValue ToJson() const;

};
