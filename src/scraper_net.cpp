/* scraper_net.cpp */
#include "net.h"
#include "scraper_net.h"

//Globals
std::map<uint256,CSplitBlob::CPart> CSplitBlob::mapParts;
std::map< uint256, CScraperManifest > CScraperManifest::mapManifest;

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
    if(part.data.empty())
    {
      part.data= CSerializeData(vRecv.begin(),vRecv.end()); //TODO: replace with move constructor
      for( const auto& ref : part.refs )
      {
        CSplitBlob& split= *ref.first;
        ++split.cntPartsRcvd;
        assert(split.cntPartsRcvd <= split.vParts.size());
        if( split.cntPartsRcvd == split.vParts.size() )
        {
          split.Complete();
        }
      }
      return true;
    } else {
      return error("Duplicate part received!");
    }
  } else {
    pfrom->Misbehaving(10);
    return error("Unknown part received!");
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
  /* nature of set ensures no duplicates */
  part.refs.emplace(this, n);
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
  for ( const CPart* part : vParts )
  {
    /*Actually request the part. Inventory system will prevent redundant requests.*/
    pfrom->AskFor(CInv(MSG_PART, part->hash));
  }
}

bool CSplitBlob::SendPartTo(CNode* pto, const uint256& hash)
{
  auto ipart= mapParts.find(hash);

  if(ipart!=mapParts.end())
  {
    pto->PushMessage("part",ipart->second.data);
    return true;
  }
  else return false;
}

bool CScraperManifest::AlreadyHave(CNode* pfrom, const CInv& inv)
{
  if( MSG_SCRAPERINDEX !=inv.type )
  {
    /* For any other objects, just say that we do not need it: */
    return true;
  }

  /* Inv-entory notification about scraper data index
   * see if we already have it
   * if yes, relay pfrom to Parts system as a fetch source and return true
   * else return false
  */
  auto found = mapManifest.find(inv.hash);
  if( found!=mapManifest.end() )
  {
    found->second.UseAsSource(pfrom);
    return true;
  }
  else
  {
    return false;
  }
}

void CScraperManifest::PushInvTo(CNode* pto)
{
  /* send all keys from the index map as inventory */
  for (auto const& obj : mapManifest)
  {
    pto->PushInventory(CInv(MSG_SCRAPERINDEX, obj.first));
  }
}


bool CScraperManifest::SendManifestTo(CNode* pto, const uint256& hash)
{
  auto it= mapManifest.find(hash);
  if(it==mapManifest.end())
    return false;
  pto->PushMessage("scraperindex0", it->second);
  return true;
}


void CScraperManifest::Serialize(CDataStream& ss, int nType, int nVersion) const
{
  ss << testName;
  for( const auto part : vParts )
    ss << part->data;
}

void CScraperManifest::UnserializeCheck(CReaderStream& ss)
{
  uint256 rh;
  ss >> testName;
  ss >> rh;
  addPart(rh);
  if(0==1)
    throw error("kek");
}

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
  /* hash the object */
  uint256 hash(Hash(vRecv.begin(), vRecv.end()));
  /* see if we do not already have it */
  if( AlreadyHave(pfrom,CInv(MSG_SCRAPERINDEX, hash)) )
  {
    return error("Already have this ScraperManifest");
  }
  CScraperManifest& manifest = mapManifest[hash];
  try {
    //void Unserialize(Stream& s, int nType, int nVersion)
    manifest.UnserializeCheck(vRecv);
  } catch(bool& e) {
    mapManifest.erase(hash);
    return false;
  } catch(std::ios_base::failure& e) {
    mapManifest.erase(hash);
    return false;
  }
  manifest.UseAsSource(pfrom);
  return true;
}

void CScraperManifest::Complete()
{
  /* Do something */
  std::string bodystr;
  CReaderStream(vParts[0]->data) >> bodystr;
  printf("CScraperManifest::Complete(): %s %s\n",testName.c_str(),bodystr.c_str());
}

/* how?
 * Should we only request objects that we need?
 * Because nodes should only have valid data, download anuthing they send.
 * They should only send what we requested, but we do not know what it is,
 * until we have it, let it pass.
 * There is 32MiB message size limit. There is a chance we could hit it, so
 * splitting is necesssary. Index object with list of parts is needed.
 *
 * If inv about index is received, and we do not know about it yet, just
 * getdata it. If it turns out useless, just ban the node. Then getdata the
 * parts from the node.
*/
