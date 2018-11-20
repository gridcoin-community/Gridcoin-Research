/* scraper_net.cpp */

/* Define this if you want to show pubkey as address, otherwise hex id */
#define SCRAPER_NET_PK_AS_ADDRESS

#include <memory>
#include "net.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#ifdef SCRAPER_NET_PK_AS_ADDRESS
#include "base58.h"
#endif
#include "scraper_net.h"

//Globals
std::map<uint256,CSplitBlob::CPart> CSplitBlob::mapParts;
std::map< uint256, std::unique_ptr<CScraperManifest> > CScraperManifest::mapManifest;

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
        if( split.cntPartsRcvd == split.vParts.size() )
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
    if(pfrom)  pfrom->Misbehaving(10);
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

bool CSplitBlob::addPartData(CDataStream&& vData)
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
    bool rc= CSplitBlob::RecvPart(0, vData);
    cntPartsRcvd++;
    return rc;
  }
  else return false;
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

bool CScraperManifest::AlreadyHave(CNode* pfrom, const CInv& inv)
{
  if( MSG_PART ==inv.type )
  {
    //TODO: move
    return false;
  }
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
    found->second->UseAsSource(pfrom);
    return true;
  }
  else
  {
    if(pfrom)  LogPrint("manifest", "new manifest %s from %s", inv.hash.GetHex(), pfrom->addrName);
    return false;
  }
}

void CScraperManifest::PushInvTo(CNode* pto)
{
  /* send all keys from the index map as inventory */
  /* FIXME: advertise only completed manifests */
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
  pto->PushMessage("scraperindex", *it->second);
  return true;
}


void CScraperManifest::Serialize(CDataStream& ss, int nType, int nVersion) const
{
  ss << testName;
  ss << pubkey;
  for( const CPart* part : vParts )
    ss << part->hash;
}

void CScraperManifest::UnserializeCheck(CReaderStream& ss)
{
  uint256 rh;
  const auto pbegin = ss.begin();
  ss >> testName;
  ss >> pubkey;
  ss >> rh;
  addPart(rh);
  if(0==1)
    throw error("kek");

  uint256 hash(Hash(pbegin,ss.begin()));
  ss >> signature;
  CKey mkey;
  if(!mkey.SetPubKey(pubkey))
    throw error("CScraperManifest: Invalid manifest key");
  if(!mkey.Verify(hash, signature))
    throw error("CScraperManifest: Invalid manifest signature");
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
  const auto it = mapManifest.emplace(hash,std::unique_ptr<CScraperManifest>(new CScraperManifest()));
  CScraperManifest& manifest = *it.first->second;
  manifest.phash= &it.first->first;
  try {
    //void Unserialize(Stream& s, int nType, int nVersion)
    manifest.UnserializeCheck(vRecv);
  } catch(bool& e) {
    mapManifest.erase(hash);
    LogPrint("manifest", "invalid manifest %s received", hash.GetHex());
    if(pfrom)  pfrom->Misbehaving(50);
    return false;
  } catch(std::ios_base::failure& e) {
    mapManifest.erase(hash);
    LogPrint("manifest", "invalid manifest %s received", hash.GetHex());
    if(pfrom)  pfrom->Misbehaving(50);
    return false;
  }
  LogPrint("manifest", "received manifest %s with %u / %u parts", hash.GetHex(),(unsigned)manifest.cntPartsRcvd,(unsigned)manifest.vParts.size());
  if( manifest.cntPartsRcvd == manifest.vParts.size() )
  {
    /* If we already got all the parts in memory, signal completition */
    manifest.Complete();
  } else {
    /* else request missing parts from the sender */
    manifest.UseAsSource(pfrom);
  }
  return true;
}

bool CScraperManifest::addManifest(std::unique_ptr<CScraperManifest>&& m, CKey& keySign)
{
  m->pubkey= keySign.GetPubKey();

  /* serialize and hash the object */
  CDataStream ss(SER_NETWORK,1);
  ss << *m;

  /* sign the serialized manifest and append the signature */
  uint256 hash(Hash(ss.begin(),ss.end()));
  keySign.Sign(hash, m->signature);
  ss << m->signature;

#if 1
  LogPrint("manifest", "adding new local manifest");
  /* at this point it is easier to pretent like it was received from network */
  return CScraperManifest::RecvManifest(0, ss);
#else
  uint256 hash(Hash(ss.begin(),ss.end()));
  /* try inserting into map */
  const auto it = mapManifest.emplace(hash,m);
  /* Already exists, do nothing */
  if(it.second==false)
    return false;

  CScraperManifest& manifest = *it.first->second;
  /* set the hash pointer inside */
  manifest.phash= &it.first->first;

  /* TODO: call Complete or PushInventory, which is better? */
  return true;
#endif
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
  printf("CScraperManifest::Complete(): %s %s\n",testName.c_str(),bodystr.c_str());
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
  UniValue result(UniValue::VOBJ);
  result.pushKV("testName",testName);
  #ifdef SCRAPER_NET_PK_AS_ADDRESS
  result.pushKV("pubkey",CBitcoinAddress(pubkey.GetID()).ToString());
  #else
  result.pushKV("pubkey",pubkey.GetID().ToString());
  #endif
  UniValue parts(UniValue::VARR);
  for( const CPart* part : vParts )
    parts.push_back(part->hash.GetHex());
  result.pushKV("parts",parts);
  return result;
}

UniValue listmanifests(const UniValue& params, bool fHelp)
{
  if(fHelp || params.size() != 0 )
    throw std::runtime_error(
        "listmanifests\n"
        "Show detailed list of known ScraperManifest objects.\n"
    );
  UniValue result1(UniValue::VOBJ);
  for(const auto& pair : CScraperManifest::mapManifest)
  {
    const uint256& hash= pair.first;
    const CScraperManifest& manifest= *pair.second;
    result1.pushKV(hash.GetHex(),manifest.ToJson());
  }
  return result1;
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
  return UniValue(std::string(ipart->second.data.begin(),ipart->second.data.end()));
}

UniValue sendmanifest(const UniValue& params, bool fHelp)
{
  if(fHelp || params.size() != 1 )
    throw std::runtime_error(
        "sendmanifest <test>\n"
        "Send a new CScraperManifest object.\n"
    );
  auto manifest=  std::unique_ptr<CScraperManifest>(new CScraperManifest());
  manifest->testName= params[0].get_str();
  CDataStream part(SER_NETWORK,1);
  part << std::string("SampleText") << rand();
  manifest->addPartData(std::move(part));

  CKey key;
  std::vector<unsigned char> vchPrivKey = ParseHex(msMasterMessagePrivateKey);
  key.SetPrivKey(CPrivKey(vchPrivKey.begin(),vchPrivKey.end()));
  CScraperManifest::addManifest(std::move(manifest), key);
  return UniValue(true);
}
