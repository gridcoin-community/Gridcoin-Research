// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "cpid.h"
#include "kernel.h"
#include "init.h" // for pwalletMain
#include "block.h"
#include "txdb.h"
#include "beacon.h"
#include "util.h"
#include "neuralnet.h"
#include "grcrestarter.h"
#include "backup.h"
#include "appcache.h"
#include "tally.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <algorithm>


bool TallyResearchAverages_v9(CBlockIndex* index);
using namespace json_spirit;
using namespace std;
extern std::string YesNo(bool bin);
extern double DoubleFromAmount(int64_t amount);
std::string PubKeyToAddress(const CScript& scriptPubKey);
CBlockIndex* GetHistoricalMagnitude(std::string cpid);
extern std::string GetProvableVotingWeightXML();
extern double ReturnVerifiedVotingBalance(std::string sXML, bool bCreatedAfterSecurityUpgrade);
extern double ReturnVerifiedVotingMagnitude(std::string sXML, bool bCreatedAfterSecurityUpgrade);
bool AskForOutstandingBlocks(uint256 hashStart);
bool WriteKey(std::string sKey, std::string sValue);
bool ForceReorganizeToHash(uint256 NewHash);
extern std::string SendReward(std::string sAddress, int64_t nAmount);
extern double GetMagnitudeByCpidFromLastSuperblock(std::string sCPID);
extern std::string SuccessFail(bool f);
extern Array GetUpgradedBeaconReport();
extern Array MagnitudeReport(std::string cpid);
std::string ConvertBinToHex(std::string a);
std::string ConvertHexToBin(std::string a);
extern std::vector<unsigned char> readFileToVector(std::string filename);
bool bNetAveragesLoaded_retired;
extern bool SignBlockWithCPID(const std::string& sCPID, const std::string& sBlockHash, std::string& sSignature, std::string& sError, bool bAdvertising = false);
std::string BurnCoinsWithNewContract(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue, int64_t MinimumBalance, double dFees, std::string strPublicKey, std::string sBurnAddress);
extern std::string GetBurnAddress();
bool StrLessThanReferenceHash(std::string rh);
extern std::string AddMessage(bool bAdd, std::string sType, std::string sKey, std::string sValue, std::string sSig, int64_t MinimumBalance, double dFees, std::string sPublicKey);
extern std::string ExtractValue(std::string data, std::string delimiter, int pos);
extern Array SuperblockReport(std::string cpid);
MiningCPID GetBoincBlockByIndex(CBlockIndex* pblockindex);
extern double GetSuperblockMagnitudeByCPID(std::string data, std::string cpid);
extern bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature);
std::string GetQuorumHash(const std::string& data);
double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime, double& total_owed, double block_magnitude);
bool UpdateNeuralNetworkQuorumData();
extern Array LifetimeReport(std::string cpid);
extern std::string AddContract(std::string sType, std::string sName, std::string sContract);
StructCPID GetLifetimeCPID(const std::string& cpid, const std::string& sFrom);
int64_t GetEarliestWalletTransaction();
extern bool CheckMessageSignature(std::string sAction,std::string messagetype, std::string sMsg, std::string sSig, std::string opt_pubkey);
bool LoadAdminMessages(bool bFullTableScan,std::string& out_errors);
int64_t GetMaximumBoincSubsidy(int64_t nTime);
double GRCMagnitudeUnit(int64_t locktime);
std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);
std::string NeuralRequest(std::string MyNeuralRequest);
extern bool AdvertiseBeacon(std::string &sOutPrivKey, std::string &sOutPubKey, std::string &sError, std::string &sMessage);

double Round(double d, int place);
bool UnusualActivityReport();
extern double GetSuperblockAvgMag(std::string data,double& out_beacon_count,double& out_participant_count,double& out_average, bool bIgnoreBeacons,int nHeight);
extern bool CPIDAcidTest2(std::string bpk, std::string externalcpid);

bool AsyncNeuralRequest(std::string command_name,std::string cpid,int NodeLimit);
bool FullSyncWithDPORNodes();

std::string GetNeuralNetworkSupermajorityHash(double& out_popularity);
std::string GetCurrentNeuralNetworkSupermajorityHash(double& out_popularity);

std::string GetNeuralNetworkReport();
Array GetJSONNeuralNetworkReport();
Array GetJSONCurrentNeuralNetworkReport();

extern Array GetJSONVersionReport();
extern Array GetJsonUnspentReport();

extern bool PollExists(std::string pollname);
extern bool PollExpired(std::string pollname);

extern bool PollAcceptableAnswer(std::string pollname, std::string answer);
extern std::string PollAnswers(std::string pollname);

extern std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2);
extern std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5);
bool GetEarliestStakeTime(std::string grcaddress, std::string cpid);


extern Array GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool bIncludeExpired);


extern Array GetJsonVoteDetailsReport(std::string pollname);
extern std::string GetPollXMLElementByPollTitle(std::string pollname, std::string XMLElement1, std::string XMLElement2);


extern double PollDuration(std::string pollname);

extern Array GetJSONBeaconReport();


void GatherNeuralHashes();
void qtSyncWithDPORNodes(std::string data);

extern bool TallyMagnitudesInSuperblock();
double GetTotalBalance();

std::string strReplace(std::string& str, const std::string& oldStr, const std::string& newStr);
MiningCPID GetNextProject(bool bForce);
std::string SerializeBoincBlock(MiningCPID mcpid);
extern std::string TimestampToHRDate(double dtm);

double CoinToDouble(double surrogate);
int64_t GetRSAWeightByCPID(std::string cpid);
double GetUntrustedMagnitude(std::string cpid, double& out_owed);
extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);
std::string getfilecontents(std::string filename);
int CreateRestorePoint();
int DownloadBlocks();
double LederstrumpfMagnitude2(double mag,int64_t locktime);
bool IsCPIDValidv2(MiningCPID& mc, int height);
std::string RetrieveMd5(std::string s1);

std::string getfilecontents(std::string filename);

std::string ToOfficialName(std::string proj);

extern double GetNetworkAvgByProject(std::string projectname);
void HarvestCPIDs(bool cleardata);
static BlockFinder RPCBlockFinder;

double GetNetworkAvgByProject(std::string projectname)
{
        projectname = strReplace(projectname,"_"," ");
        if (mvNetwork.size() < 1)   return 0;
        StructCPID structcpid = mvNetwork[projectname];
        if (!structcpid.initialized) return 0;
        double networkavgrac = structcpid.AverageRAC;
        return networkavgrac;
}

double GetNetworkTotalByProject(std::string projectname)
{
        projectname = strReplace(projectname,"_"," ");
        if (mvNetwork.size() < 1)   return 0;
        StructCPID structcpid = mvNetwork[projectname];
        if (!structcpid.initialized) return 0;
        double networkavgrac = structcpid.rac;
        return networkavgrac;
}

std::string FileManifest()
{
    using namespace boost::filesystem;
    path dir_path = GetDataDir() / "nn2";
    std::string sMyManifest;
    for(directory_iterator it(dir_path); it != directory_iterator(); ++it)
    {
       if(boost::filesystem::is_regular_file(it->path()))
       {
           sMyManifest += it->path().string();
       }
    }
    return sMyManifest;
}

std::vector<unsigned char> readFileToVector(std::string filename)
{
    std::ifstream file(filename.c_str(), std::ios::binary);
    file.unsetf(std::ios::skipws);
    std::streampos fileSize;
    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> vec;
    vec.reserve(fileSize);
    vec.insert(vec.begin(), std::istream_iterator<unsigned char>(file), std::istream_iterator<unsigned char>());
    return vec;
}

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (pindexBest == NULL)
            return 1.0;
        else
            blockindex = GetLastBlockIndex(pindexBest, false);
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}




double GetBlockDifficulty(unsigned int nBits)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    int nShift = (nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);
    result.push_back(Pair("confirmations", txGen.GetDepthInMainChain()));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    double mint = CoinToDouble(blockindex->nMint);
    result.push_back(Pair("mint", mint));
    result.push_back(Pair("MoneySupply", blockindex->nMoneySupply));
    result.push_back(Pair("time", block.GetBlockTime()));
    result.push_back(Pair("nonce", (int)block.nNonce));
    result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("blocktrust", leftTrim(blockindex->GetBlockTrust().GetHex(), '0')));
    result.push_back(Pair("chaintrust", leftTrim(blockindex->nChainTrust.GetHex(), '0')));
    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    if (blockindex->pnext)
        result.push_back(Pair("nextblockhash", blockindex->pnext->GetBlockHash().GetHex()));
    MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc,block.nVersion);
    uint256 blockhash = block.GetPoWHash();
    std::string sblockhash = blockhash.GetHex();
    bool IsPoR = false;
    IsPoR = (bb.Magnitude > 0 && IsResearcher(bb.cpid) && blockindex->IsProofOfStake());
    std::string PoRNarr = "";
    if (IsPoR) PoRNarr = "proof-of-research";
    result.push_back(Pair("flags",
        strprintf("%s%s", blockindex->IsProofOfStake()? "proof-of-stake" : "proof-of-work", blockindex->GeneratedStakeModifier()? " stake-modifier": "") + " " + PoRNarr        )       );
    result.push_back(Pair("proofhash", blockindex->hashProof.GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016" PRIx64, blockindex->nStakeModifier)));
    result.push_back(Pair("modifierchecksum", strprintf("%08x", blockindex->nStakeModifierChecksum)));
    Array txinfo;
    for (auto const& tx : block.vtx)
    {
        if (fPrintTransactionDetail)
        {
            Object entry;

            entry.push_back(Pair("txid", tx.GetHash().GetHex()));
            TxToJSON(tx, 0, entry);

            txinfo.push_back(entry);
        }
        else
            txinfo.push_back(tx.GetHash().GetHex());
    }

    result.push_back(Pair("tx", txinfo));
    if (block.IsProofOfStake())
        result.push_back(Pair("signature", HexStr(block.vchBlockSig.begin(), block.vchBlockSig.end())));
    result.push_back(Pair("CPID", bb.cpid));
    if (!IsResearchAgeEnabled(blockindex->nHeight))
    {
        result.push_back(Pair("ProjectName", bb.projectname));
        result.push_back(Pair("RAC", bb.rac));
        result.push_back(Pair("NetworkRAC", bb.NetworkRAC));
        result.push_back(Pair("RSAWeight",bb.RSAWeight));
    }

    result.push_back(Pair("Magnitude", bb.Magnitude));
    if (fDebug3) result.push_back(Pair("BoincHash",block.vtx[0].hashBoinc));
    result.push_back(Pair("LastPaymentTime",TimestampToHRDate(bb.LastPaymentTime)));

    result.push_back(Pair("ResearchSubsidy",bb.ResearchSubsidy));
    result.push_back(Pair("ResearchAge",bb.ResearchAge));
    result.push_back(Pair("ResearchMagnitudeUnit",bb.ResearchMagnitudeUnit));
    result.push_back(Pair("ResearchAverageMagnitude",bb.ResearchAverageMagnitude));
    result.push_back(Pair("LastPORBlockHash",bb.LastPORBlockHash));
    result.push_back(Pair("Interest",bb.InterestSubsidy));
    result.push_back(Pair("GRCAddress",bb.GRCAddress));
    if (!bb.BoincPublicKey.empty())
    {
        result.push_back(Pair("BoincPublicKey",bb.BoincPublicKey));
        result.push_back(Pair("BoincSignature",bb.BoincSignature));
        bool fValidSig = VerifyCPIDSignature(bb.cpid, bb.lastblockhash, bb.BoincSignature);
        result.push_back(Pair("SignatureValid",fValidSig));
    }
    result.push_back(Pair("ClientVersion",bb.clientversion));

    if (!bb.cpidv2.empty())     result.push_back(Pair("CPIDv2",bb.cpidv2.substr(0,32)));
    bool IsCPIDValid2 = IsCPIDValidv2(bb,blockindex->nHeight);
    result.push_back(Pair("CPIDValid",IsCPIDValid2));

    result.push_back(Pair("NeuralHash",bb.NeuralHash));
    if (bb.superblock.length() > 20)
    {
        //12-20-2015 Support for Binary Superblocks
        std::string superblock=UnpackBinarySuperblock(bb.superblock);
        std::string neural_hash = GetQuorumHash(superblock);
        result.push_back(Pair("SuperblockHash", neural_hash));
        result.push_back(Pair("SuperblockUnpackedLength", (int)superblock.length()));
        result.push_back(Pair("SuperblockLength", (int)bb.superblock.length()));
        bool bIsBinary = Contains(bb.superblock,"<BINARY>");
        result.push_back(Pair("IsBinary",bIsBinary));
        if(fPrintTransactionDetail)
        {
            result.push_back(Pair("SuperblockContents", superblock));
        }
    }
    result.push_back(Pair("IsSuperBlock", (int)blockindex->nIsSuperBlock));
    result.push_back(Pair("IsContract", (int)blockindex->nIsContract));
    return result;
}


Value showblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "showblock <index>\n"
            "Returns all information about the block at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");
    CBlockIndex* pblockindex = RPCBlockFinder.FindByHeight(nHeight);

    if (pblockindex==NULL)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
    CBlock block;
    block.ReadFromDisk(pblockindex);
    return blockToJSON(block, pblockindex, false);
}





Value getbestblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "Returns the hash of the best block in the longest block chain.");

    return hashBestChain.GetHex();
}

Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "Returns the number of blocks in the longest block chain.");

    return nBestHeight;
}


Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "Returns the difficulty as a multiple of the minimum difficulty.");

    Object obj;
    obj.push_back(Pair("proof-of-work",        GetDifficulty()));
    obj.push_back(Pair("proof-of-stake",       GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    return obj;
}


Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1 || AmountFromValue(params[0]) < MIN_TX_FEE)
        throw runtime_error(
            "settxfee <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.01\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    nTransactionFee = AmountFromValue(params[0]);
    nTransactionFee = (nTransactionFee / CENT) * CENT;  // round to cent

    return true;
}

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getrawmempool\n"
            "Returns all transaction ids in memory pool.");

    vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    Array a;
    for (auto const& hash : vtxid)
        a.push_back(hash.ToString());

    return a;
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)       throw runtime_error("Block number out of range.");
    if (fDebug10)   LogPrintf("Getblockhash %f",(double)nHeight);
    CBlockIndex* RPCpblockindex = RPCBlockFinder.FindByHeight(nHeight);
    return RPCpblockindex->phashBlock->GetHex();
}

Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock <hash> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-hash.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

Value getblockbynumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblockbynumber <number> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-number.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;

    uint256 hash = *pblockindex->phashBlock;

    pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}




void filecopy(FILE *dest, FILE *src)
{
    const int size = 16384;
    char buffer[size];

    while (!feof(src))
    {
        int n = fread(buffer, 1, size, src);
        fwrite(buffer, 1, n, dest);
    }

    fflush(dest);
}


void fileopen_and_copy(std::string src, std::string dest)
{
    FILE * infile  = fopen(src.c_str(),  "rb");
    FILE * outfile = fopen(dest.c_str(), "wb");

    filecopy(outfile, infile);

    fclose(infile);
    fclose(outfile);
}

std::string SignMessage(std::string sMsg, std::string sPrivateKey)
{
     CKey key;
     std::vector<unsigned char> vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());
     std::vector<unsigned char> vchPrivKey = ParseHex(sPrivateKey);
     std::vector<unsigned char> vchSig;
     key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
     if (!key.Sign(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
     {
             return "Unable to sign message, check private key.";
     }

     const std::string sig(vchSig.begin(), vchSig.end());
     std::string SignedMessage = EncodeBase64(sig);
     return SignedMessage;
}

bool CheckMessageSignature(std::string sAction,std::string messagetype, std::string sMsg, std::string sSig, std::string strMessagePublicKey)
{
     std::string strMasterPubKey = "";
     if (messagetype=="project" || messagetype=="projectmapping")
     {
        strMasterPubKey= msMasterProjectPublicKey;
     }
     else
     {
         strMasterPubKey = msMasterMessagePublicKey;
     }

     if (!strMessagePublicKey.empty()) strMasterPubKey = strMessagePublicKey;
     if (sAction=="D" && messagetype=="beacon") strMasterPubKey = msMasterProjectPublicKey;
	 if (sAction=="D" && messagetype=="poll")   strMasterPubKey = msMasterProjectPublicKey;
	 if (sAction=="D" && messagetype=="vote")   strMasterPubKey = msMasterProjectPublicKey;

     std::string db64 = DecodeBase64(sSig);
     CKey key;
     if (!key.SetPubKey(ParseHex(strMasterPubKey))) return false;
     std::vector<unsigned char> vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());
     std::vector<unsigned char> vchSig = vector<unsigned char>(db64.begin(), db64.end());
     if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig)) return false;
     return true;

}


std::string ExtractValue(std::string data, std::string delimiter, int pos)
{
    std::vector<std::string> vKeys = split(data.c_str(),delimiter);
    std::string keyvalue = "";
    if (vKeys.size() > (unsigned int)pos)
    {
        keyvalue = vKeys[pos];
    }

    return keyvalue;
}



double GetAverageInList(std::string superblock,double& out_count)
{
    try
    {
        const std::vector<std::string>& vSuperblock = split(superblock.c_str(),";");
        if (vSuperblock.size() < 2)
            return 0;

        double rows_above_zero = 0;
        double rows_with_zero = 0;
        double total_mag = 0;
        for (const std::string& row : vSuperblock)
        {
            const std::vector<std::string>& fields = split(row, ",");
            if(fields.size() < 2)
                continue;

            const std::string& cpid = fields[0];
            double magnitude = std::atoi(fields[1].c_str());
            if (cpid.length() > 10)
            {
                total_mag += magnitude;
                rows_above_zero++;
            }
            else
            {
                // Non-compressed legacy block placeholder
                rows_with_zero++;
            }
        }
        out_count = rows_above_zero + rows_with_zero;
        double avg = total_mag/(rows_above_zero+.01);
        return avg;
    }
    catch(...)
    {
        LogPrintf("Error in GetAvgInList");
        out_count=0;
        return 0;
    }

}




double GetSuperblockMagnitudeByCPID(std::string data, std::string cpid)
{
        std::string mags = ExtractXML(data,"<MAGNITUDES>","</MAGNITUDES>");
        std::vector<std::string> vSuperblock = split(mags.c_str(),";");
        if  (vSuperblock.size() < 2) return -2;
        if  (cpid.length() < 31) return -3;
        for (unsigned int i = 0; i < vSuperblock.size(); i++)
        {
            // For each CPID in the contract
            LogPrintf(".");
            if (vSuperblock[i].length() > 1)
            {
                std::string sTempCPID = ExtractValue(vSuperblock[i],",",0);
                double magnitude = RoundFromString(ExtractValue("0"+vSuperblock[i],",",1),0);
                boost::to_lower(sTempCPID);
                boost::to_lower(cpid);
                // For each CPID in the contract
                if (sTempCPID.length() > 31 && cpid.length() > 31)
                {
                    if (sTempCPID.substr(0,31) == cpid.substr(0,31))
                    {
                        return magnitude;
                    }
                }
            }
        }
        return -1;
}


void GetSuperblockProjectCount(std::string data, double& out_project_count, double& out_whitelist_count)
{
	   // This is reserved in case we ever want to resync prematurely when the last superblock contains < .75% of whitelisted projects (remember we allow superblocks with up to .50% of the whitelisted projects, in case some project sites are being ddossed)
       std::string avgs = ExtractXML(data,"<AVERAGES>","</AVERAGES>");
       double avg_of_projects = GetAverageInList(avgs, out_project_count);
       out_whitelist_count = GetCountOf("project");
	   if (fDebug10) LogPrintf(" GSPC:CountOfProjInBlock %f vs WhitelistedCount %f  \n",(double)out_project_count,(double)out_whitelist_count);
}


double GetSuperblockAvgMag(std::string data,double& out_beacon_count,double& out_participant_count,double& out_average, bool bIgnoreBeacons,int nHeight)
{
    try
    {
        std::string mags = ExtractXML(data,"<MAGNITUDES>","</MAGNITUDES>");
        std::string avgs = ExtractXML(data,"<AVERAGES>","</AVERAGES>");
        double mag_count = 0;
        double avg_count = 0;
        if (mags.empty()) return 0;
        double avg_of_magnitudes = GetAverageInList(mags,mag_count);
        double avg_of_projects   = GetAverageInList(avgs,avg_count);
        if (!bIgnoreBeacons) out_beacon_count = GetCountOf("beacon");
		double out_project_count = GetCountOf("project");
        out_participant_count = mag_count;
        out_average = avg_of_magnitudes;
        if (avg_of_magnitudes < 000010)  return -1;
        if (avg_of_magnitudes > 170000)  return -2;
        if (avg_of_projects   < 050000)  return -3;
		// Note bIgnoreBeacons is passed in when the chain is syncing from 0 (this is because the lists of beacons and projects are not full at that point)
        if (!fTestNet && !bIgnoreBeacons && (mag_count < out_beacon_count*.90 || mag_count > out_beacon_count*1.10)) return -4;
		if (fDebug10) LogPrintf(" CountOfProjInBlock %f vs WhitelistedCount %f Height %f \n",(double)avg_count,(double)out_project_count,(double)nHeight);
		if (!fTestNet && !bIgnoreBeacons && nHeight > 972000 && (avg_count < out_project_count*.50)) return -5;
        return avg_of_magnitudes + avg_of_projects;
    }
    catch (std::exception &e)
    {
                LogPrintf("Error in GetSuperblockAvgMag.");
                return 0;
    }
    catch(...)
    {
                LogPrintf("Error in GetSuperblockAvgMag.");
                return 0;
    }

}



bool TallyMagnitudesInSuperblock()
{
    try
    {
        std::string superblock = ReadCache("superblock","magnitudes").value;
        if (superblock.empty()) return false;
        std::vector<std::string> vSuperblock = split(superblock.c_str(),";");
        double TotalNetworkMagnitude = 0;
        double TotalNetworkEntries = 0;
        if (mvDPORCopy.size() > 0 && vSuperblock.size() > 1)    mvDPORCopy.clear();

        for (unsigned int i = 0; i < vSuperblock.size(); i++)
        {
            // For each CPID in the contract
            if (vSuperblock[i].length() > 1)
            {
                    std::string cpid = ExtractValue(vSuperblock[i],",",0);
                    double magnitude = RoundFromString(ExtractValue(vSuperblock[i],",",1),0);
                    if (cpid.length() > 10)
                    {
                        StructCPID stCPID = GetInitializedStructCPID2(cpid,mvDPORCopy);
                        stCPID.TotalMagnitude = magnitude;
                        stCPID.Magnitude = magnitude;
                        stCPID.cpid = cpid;
                        mvDPORCopy[cpid]=stCPID;
                        StructCPID stMagg = GetInitializedStructCPID2(cpid,mvMagnitudesCopy);
                        stMagg.cpid = cpid;
                        stMagg.Magnitude = stCPID.Magnitude;
                        stMagg.PaymentMagnitude = LederstrumpfMagnitude2(magnitude,GetAdjustedTime());
                        //Adjust total owed - in case they are a newbie:
                        if (true)
                        {
                            double total_owed = 0;
                            stMagg.owed = GetOutstandingAmountOwed(stMagg,cpid,(double)GetAdjustedTime(),total_owed,stCPID.Magnitude);
                            stMagg.totalowed = total_owed;
                        }

                        mvMagnitudesCopy[cpid] = stMagg;
                        TotalNetworkMagnitude += stMagg.Magnitude;
                        TotalNetworkEntries++;

                    }
            }
    }

    if (fDebug3) LogPrintf(".TMIS41.");
    double NetworkAvgMagnitude = TotalNetworkMagnitude / (TotalNetworkEntries+.01);
    // Store the Total Network Magnitude:
    StructCPID network = GetInitializedStructCPID2("NETWORK",mvNetworkCopy);
    network.projectname="NETWORK";
    network.NetworkMagnitude = TotalNetworkMagnitude;
    network.NetworkAvgMagnitude = NetworkAvgMagnitude;
    if (fDebug)
            LogPrintf("TallyMagnitudesInSuperblock: Extracted %.0f magnitude entries from cached superblock %s", TotalNetworkEntries, ReadCache("superblock","block_number").value);

    double TotalProjects = 0;
    double TotalRAC = 0;
    double AVGRac = 0;
    // Load boinc project averages from neural network
        std::string projects = ReadCache("superblock","averages").value;
    if (projects.empty()) return false;
    std::vector<std::string> vProjects = split(projects.c_str(),";");
    if (vProjects.size() > 0)
    {
        double totalRAC = 0;
        WHITELISTED_PROJECTS = 0;
        for (unsigned int i = 0; i < vProjects.size(); i++)
        {
            // For each Project in the contract
            if (vProjects[i].length() > 1)
            {
                    std::string project = ExtractValue(vProjects[i],",",0);
                    double avg = RoundFromString(ExtractValue("0" + vProjects[i],",",1),0);
                    if (project.length() > 1)
                    {
                        StructCPID stProject = GetInitializedStructCPID2(project,mvNetworkCopy);
                        stProject.projectname = project;
                        stProject.AverageRAC = avg;
                        //As of 7-16-2015, start pulling in Total RAC
                        totalRAC = 0;
                        totalRAC = RoundFromString("0" + ExtractValue(vProjects[i],",",2),0);
                        stProject.rac = totalRAC;
                        mvNetworkCopy[project]=stProject;
                        TotalProjects++;
                        WHITELISTED_PROJECTS++;
                        TotalRAC += avg;
                    }
                }
        }
    }
    AVGRac = TotalRAC/(TotalProjects+.01);
    network.AverageRAC = AVGRac;
    network.rac = TotalRAC;
    network.NetworkProjects = TotalProjects;
    mvNetworkCopy["NETWORK"] = network;
    if (fDebug3) LogPrintf(".TMS43.");
    return true;
    }
    catch (const std::exception &e)
    {
                LogPrintf("Error in TallySuperblock.");
                return false;
    }
}

std::string AddContract(std::string sType, std::string sName, std::string sContract)
{
            std::string sPass = (sType=="project" || sType=="projectmapping" || sType=="smart_contract") ? GetArgument("masterprojectkey", msMasterMessagePrivateKey) : msMasterMessagePrivateKey;
            std::string result = AddMessage(true,sType,sName,sContract,sPass,AmountFromValue(1),.00001,"");
            return result;
}


bool CPIDAcidTest2(std::string bpk, std::string externalcpid)
{
    uint256 hashRand = GetRandHash();
    std::string email = GetArgument("email", "NA");
    boost::to_lower(email);
    std::string cpidv2 = ComputeCPIDv2(email, bpk, hashRand);
    std::string cpidv1 = cpidv2.substr(0,32);
    return (externalcpid==cpidv1);
}

bool AdvertiseBeacon(std::string &sOutPrivKey, std::string &sOutPubKey, std::string &sError, std::string &sMessage)
{
     LOCK(cs_main);
     {
            GetNextProject(false);
            if (!IsResearcher(GlobalCPUMiningCPID.cpid))
            {
                sError = "INVESTORS_CANNOT_SEND_BEACONS";
                return false;
            }

            //If beacon is already in the chain, exit early
            std::string sBeaconPublicKey = GetBeaconPublicKey(GlobalCPUMiningCPID.cpid,true);
            if (!sBeaconPublicKey.empty())
            {
                // Ensure they can re-send the beacon if > 5 months old : GetBeaconPublicKey returns an empty string when > 5 months: OK.
                // Note that we allow the client to re-advertise the beacon in 5 months, so that they have a seamless and uninterrupted keypair in use (prevents a hacker from hijacking a keypair that is in use)
                sError = "ALREADY_IN_CHAIN";
                return false;
            }

            // Prevent users from advertising multiple times in succession by setting a limit of one advertisement per 5 blocks.
            // Realistically 1 should be enough however just to be sure we deny advertisements for 5 blocks.
            static int nLastBeaconAdvertised = 0;
            if ((nBestHeight - nLastBeaconAdvertised) < 5)
            {
                sError = _("A beacon was advertised less then 5 blocks ago. Please wait a full 5 blocks for your beacon to enter the chain.");
                return false;
            }
            uint256 hashRand = GetRandHash();
            std::string email = GetArgument("email", "NA");
            boost::to_lower(email);
            GlobalCPUMiningCPID.email=email;
            GlobalCPUMiningCPID.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, hashRand);

            bool IsCPIDValid2 = CPID_IsCPIDValid(GlobalCPUMiningCPID.cpid,GlobalCPUMiningCPID.cpidv2, hashRand);
            if (!IsCPIDValid2)
            {
                sError="Invalid CPID";
                return false;
            }

            double nBalance = GetTotalBalance();
            if (nBalance < 1.01)
            {
                sError = "Balance too low to send beacon, 1.01 GRC minimum balance required.";
                return false;
            }

            GenerateBeaconKeys(GlobalCPUMiningCPID.cpid, sOutPubKey, sOutPrivKey);
            if (sOutPrivKey.empty() || sOutPubKey.empty())
            {
                sError = "Keypair is empty.";
                return false;
            }

            GlobalCPUMiningCPID.lastblockhash = GlobalCPUMiningCPID.cpidhash;
            std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID,pindexBest->nVersion);
            std::string GRCAddress = DefaultWalletAddress();
            // Public Signing Key is stored in Beacon
            std::string contract = GlobalCPUMiningCPID.cpidv2 + ";" + hashRand.GetHex() + ";" + GRCAddress + ";" + sOutPubKey;
            LogPrintf("\n Creating beacon for cpid %s, %s",GlobalCPUMiningCPID.cpid, contract);
            std::string sBase = EncodeBase64(contract);
            std::string sAction = "add";
            std::string sType = "beacon";
            std::string sName = GlobalCPUMiningCPID.cpid;
            try
            {

                // Backup config with old keys like a normal backup
                if(!BackupConfigFile(GetBackupFilename("gridcoinresearch.conf")))
                {
                    sError = "Failed to backup old configuration file. Beacon not sent.";
                    return false;
                }

                // Backup config with new keys with beacon suffix
                StoreBeaconKeys(GlobalCPUMiningCPID.cpid, sOutPubKey, sOutPrivKey);
                if(!BackupConfigFile(GetBackupFilename("gridcoinresearch.conf", "beacon")))
                {
                    sError = "Failed to back up configuration file. Beacon not sent, please manually roll back to previous configuration.";
                    return false;
                }

                // Send the beacon transaction
                sMessage = AddContract(sType,sName,sBase);
                // This prevents repeated beacons
                nLastBeaconAdvertised = nBestHeight;
                // Activate Beacon Keys in memory. This process is not automatic and has caused users who have a new keys while old ones exist in memory to perform a restart of wallet.
                ActivateBeaconKeys(GlobalCPUMiningCPID.cpid, sOutPubKey, sOutPrivKey);

                return true;
            }
            catch(Object& objError)
            {
                sError = "Error: Unable to send beacon::"+json_spirit::write_string(json_spirit::Value(objError),true);
                return false;
            }
            catch (std::exception &e)
            {
                sError = "Error: Unable to send beacon;:"+std::string(e.what());
                return false;
            }
     }
}


std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5, std::string arg6)
{
     Array params;
     params.push_back(method);
     params.push_back(arg1);
     params.push_back(arg2);
     params.push_back(arg3);
     params.push_back(arg4);
     params.push_back(arg5);
     params.push_back(arg6);

     LogPrintf("Executing method %s\n",method);
     Value vResult;
     try
     {
        vResult = execute(params,false);
     }
     catch (std::exception& e)
     {
         LogPrintf("Std exception %s \n",method);

         std::string caught = e.what();
         return "Exception " + caught;

     }
     catch (...)
     {
            LogPrintf("Generic exception (Please try unlocking the wallet) %s \n",method);
            return "Generic Exception (Please try unlocking the wallet).";
     }
     std::string sResult = "";
     sResult = write_string(vResult, false) + "\n";
     LogPrintf("Response %s",sResult);
     return sResult;
}

std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5)
{
     Array params;
     params.push_back(method);
     params.push_back(arg1);
     params.push_back(arg2);
     params.push_back(arg3);
     params.push_back(arg4);
     params.push_back(arg5);

     LogPrintf("Executing method %s\n",method);
     Value vResult;
     try
     {
        vResult = execute(params,false);
     }
     catch (std::exception& e)
     {
         LogPrintf("Std exception %s \n",method);

         std::string caught = e.what();
         return "Exception " + caught;

     }
     catch (...)
     {
            LogPrintf("Generic exception (Please try unlocking the wallet) %s \n",method);
            return "Generic Exception (Please try unlocking the wallet).";
     }
     std::string sResult = "";
     sResult = write_string(vResult, false) + "\n";
     LogPrintf("Response %s",sResult);
     return sResult;
}


std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2)
{
     Array params;
     params.push_back(method);
     params.push_back(arg1);
     params.push_back(arg2);
     LogPrintf("Executing method %s\n",method);
     Value vResult;
     try
     {
        vResult = execute(params,false);
     }
     catch (std::exception& e)
     {
         LogPrintf("Std exception %s \n",method);

         std::string caught = e.what();
         return "Exception " + caught;

     }
     catch (...)
     {
            LogPrintf("Generic exception (Please try unlocking the wallet). %s \n",method);
            return "Generic Exception (Please try unlocking the wallet).";
     }
     std::string sResult = "";
     sResult = write_string(vResult, false) + "\n";
     LogPrintf("Response %s",sResult);
     return sResult;
}

int64_t AmountFromDouble(double dAmount)
{
    if (dAmount <= 0.0 || dAmount > MAX_MONEY)        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64_t nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))         throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

// Rpc

Value backupprivatekeys(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "backupprivatekeys\n"
                "\n"
                "Backup wallet private keys to file\n"
                "Wallet must be fully unlocked!\n");

    string sErrors;
    string sTarget;
    Object res;

    bool bBackupPrivateKeys = BackupPrivateKeys(*pwalletMain, sTarget, sErrors);

    if (!bBackupPrivateKeys)
        res.push_back(Pair("error", sErrors));

    else
        res.push_back(Pair("location", sTarget));

    res.push_back(Pair("result", bBackupPrivateKeys));

    return res;
}

Value burn2(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4)
        throw runtime_error(
                "burn2 <burnaddress> <burnamount> <burnkey> <burndetail>\n"
                "\n"
                "<burnaddress>    Address where the coins will be burned\n"
                "<burnamount>     Amount of coins to be burned\n"
                "<burnaddress>    Burn key to be used\n"
                "<burndetails>    Details of the burn\n");

    Object res;
    std::string sAddress = params[0].get_str();
    double dAmount  = Round(params[1].get_real(), 6);
    std::string sKey     = params[2].get_str();
    std::string sDetail  = params[3].get_str();
    CBitcoinAddress address(sAddress);
    bool isValid = address.IsValid();

    if (!isValid)
    {
        res.push_back(Pair("Error","Invalid GRC Burn Address."));
        return res;
    }

    if (dAmount == 0 || dAmount < 0)
    {
        res.push_back(Pair("Error","Burn amount must be > 0."));
        return res;
    }

    if (sKey.empty() || sDetail.empty())
    {
        res.push_back(Pair("Error","Burn Key and Burn Detail must be populated."));
        return res;
    }

    std::string sContract = "<KEY>" + sKey + "</KEY><DETAIL>" + sDetail + "</DETAIL>";

    std::string sResult = BurnCoinsWithNewContract(true,"burn",sKey,sContract,AmountFromValue(1),dAmount,"",sAddress);

    res.push_back(Pair("Burn_Response",sResult));

    return res;
}

Value encrypt(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "encrypt <walletpassphrase>\n"
                "\n"
                "<walletpassphrase>    The password of your encrypted wallet\n");

    Object res;
    //Encrypt a phrase
    std::string sParam = params[0].get_str();
    std::string encrypted = AdvancedCryptWithHWID(sParam);
    res.push_back(Pair("Passphrase",encrypted));
    res.push_back(Pair("[Specify in config file] autounlock=",encrypted));
    return res;
}

Value newburnaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "newburnaddress [burntemplate]\n"
                "\n"
                "[burntemplate]    Allow a vanity burn address\n");

    Object res;

    //3-12-2016 - R Halford - Allow the user to make vanity GRC Burn Addresses that have no corresponding private key
    std::string sBurnTemplate = "GRCBurnAddressGRCBurnAddressGRCBurnAddress";
    if (params.size() > 0)
    {
        sBurnTemplate = params[0].get_str();
    }
    // Address must start with the correct base58 network flag and address type for GRC
    std::string sPrefix = (fTestNet) ? "mp" : "Rx";
    std::string t34 = sPrefix + sBurnTemplate + "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    t34 = t34.substr(0,34); // Template must be 34 characters
    std::vector<unsigned char> vchDecoded34;
    DecodeBase58(t34, vchDecoded34);
    //Now we have the 34 digit address decoded from base58 to binary
    std::string sDecoded34(vchDecoded34.begin(), vchDecoded34.end());
    //Now we have a binary string - Chop off all but last 4 bytes (save space for the checksum)
    std::string sDecoded30 = sDecoded34.substr(0,sDecoded34.length()-4);
    //Convert to Hex first
    vector<unsigned char> vchDecoded30(sDecoded30.begin(), sDecoded30.end());
    std::string sDecodedHex = ConvertBinToHex(sDecoded30);
    // Get sha256 Checksum of DecodedHex
    uint256 hash = Hash(vchDecoded30.begin(), vchDecoded30.end());
    // The BTC address spec calls for double SHA256 hashing
    uint256 DoubleHash = Hash(hash.begin(),hash.end());
    std::string sSha256 = DoubleHash.GetHex();
    // Only use the first 8 hex bytes to retrieve the checksum
    sSha256  = sSha256.substr(0,8);
    // Combine the Hex Address prefix and the Sha256 Checksum to form the Hex version of the address (Note: There is no private key)
    std::string combined = sDecodedHex + sSha256;
    std::string sBinary = ConvertHexToBin(combined);
    vector<unsigned char> v(sBinary.begin(), sBinary.end());
    //Make the new address so that it passes base 58 Checks
    std::string encoded1 = EncodeBase58(v);
    res.push_back(Pair("CombinedHex",combined));
    std::string encoded2 = EncodeBase58Check(vchDecoded30);
    if (encoded2.length() != 34)
    {
        res.push_back(Pair("Burn Address Creation failed","NOTE: the input phrase must not include zeroes, or nonbase58 characters."));
        return res;
    }
    // Give the user the new vanity burn address
    res.push_back(Pair("Burn Address",encoded2));
    return res;
}

Value rain(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
                "rain Address<COL>Amount<ROW>...\n"
                "\n");

    Object res;

    CWalletTx wtx;
    wtx.mapValue["comment"] = "Rain";
    set<CBitcoinAddress> setAddress;
    vector<pair<CScript, int64_t> > vecSend;
    std::string sRecipients = params[0].get_str();
    std::string sRainCommand = ExtractXML(sRecipients,"<RAIN>","</RAIN>");
    std::string sRainMessage = MakeSafeMessage(ExtractXML(sRecipients,"<RAINMESSAGE>","</RAINMESSAGE>"));
    std::string sRain = "<NARR>Project Rain: " + sRainMessage + "</NARR>";
    if (!sRainCommand.empty()) sRecipients = sRainCommand;
    wtx.hashBoinc = sRain;
    int64_t totalAmount = 0;
    double dTotalToSend = 0;
    std::vector<std::string> vRecipients = split(sRecipients.c_str(),"<ROW>");
    printf("Creating Rain transaction with %f recipients. ",(double)vRecipients.size());
    for (unsigned int i = 0; i < vRecipients.size(); i++)
    {
        std::string sRow = vRecipients[i];
        std::vector<std::string> vReward = split(sRow.c_str(),"<COL>");
        if (vReward.size() > 1)
        {
            std::string sAddress = vReward[0];
            std::string sAmount = vReward[1];
            if (sAddress.length() > 10 && sAmount.length() > 0)
            {
                double dAmount = RoundFromString(sAmount,4);
                if (dAmount > 0)
                {
                    CBitcoinAddress address(sAddress);
                    if (!address.IsValid())
                        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+sAddress);
                    if (setAddress.count(address))
                        throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+sAddress);
                    setAddress.insert(address);
                    dTotalToSend += dAmount;
                    int64_t nAmount = AmountFromDouble(dAmount);
                    CScript scriptPubKey;
                    scriptPubKey.SetDestination(address.Get());
                    totalAmount += nAmount;
                    vecSend.push_back(make_pair(scriptPubKey, nAmount));
                }
            }
        }
    }

    EnsureWalletIsUnlocked();
    // Check funds
    double dBalance = GetTotalBalance();
    if (dTotalToSend > dBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");
    // Send
    CReserveKey keyChange(pwalletMain);
    int64_t nFeeRequired = 0;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);
    printf("Transaction Created.");
    if (!fCreated)
    {
        if (totalAmount + nFeeRequired > pwalletMain->GetBalance())
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
    }
    printf("Committing.");
    // Rain the recipients
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
    {
        printf("Commit failed.");
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");
    }
    std::string sNarr = "Rain successful:  Sent " + wtx.GetHash().GetHex() + ".";
    printf("Success %s",sNarr.c_str());
    res.push_back(Pair("Response", sNarr));

    return res;
}

Value unspentreport(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "unspentreport\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    Array aUnspentReport = GetJsonUnspentReport();

    return aUnspentReport;
}

Array LifetimeReport(std::string cpid)
{
       Array results;
       Object c;
       std::string Narr = ToString(GetAdjustedTime());
       c.push_back(Pair("Lifetime Payments Report",Narr));
       results.push_back(c);
       Object entry;
       CBlockIndex* pindex = pindexGenesisBlock;
       while (pindex->nHeight < pindexBest->nHeight)
       {
            pindex = pindex->pnext;
            if (pindex==NULL || !pindex->IsInMainChain()) continue;
            if (pindex == pindexBest) break;
            if (pindex->GetCPID() == cpid && (pindex->nResearchSubsidy > 0))
            {
                entry.push_back(Pair(RoundToString((double)pindex->nHeight,0), RoundToString(pindex->nResearchSubsidy,2)));
            }

       }
       //8-14-2015
       StructCPID stCPID = GetInitializedStructCPID2(cpid,mvResearchAge);
       entry.push_back(Pair("Average Magnitude",stCPID.ResearchAverageMagnitude));
       results.push_back(entry);
       return results;

}


Array SuperblockReport(std::string cpid)
{
    Array results;
    Object c;
    std::string Narr = ToString(GetAdjustedTime());
    c.push_back(Pair("SuperBlock Report (14 days)",Narr));
    if (!cpid.empty())      c.push_back(Pair("CPID",cpid));

    results.push_back(c);

    int nMaxDepth = nBestHeight;
    int nLookback = BLOCKS_PER_DAY * 14;
    int nMinDepth = (nMaxDepth - nLookback) - ( (nMaxDepth-nLookback) % BLOCK_GRANULARITY);
    //int iRow = 0;
    CBlockIndex* pblockindex = pindexBest;
    while (pblockindex->nHeight > nMaxDepth)
    {
        if (!pblockindex || !pblockindex->pprev || pblockindex == pindexGenesisBlock) return results;
        pblockindex = pblockindex->pprev;
    }

    while (pblockindex->nHeight > nMinDepth)
    {
        if (!pblockindex || !pblockindex->pprev) return results;
        pblockindex = pblockindex->pprev;
        if (pblockindex == pindexGenesisBlock) return results;
        if (!pblockindex->IsInMainChain()) continue;
        if (IsSuperBlock(pblockindex))
        {
            MiningCPID bb = GetBoincBlockByIndex(pblockindex);
            if (bb.superblock.length() > 20)
            {
                double out_beacon_count = 0;
                double out_participant_count = 0;
                double out_avg = 0;
                // Binary Support 12-20-2015
                std::string superblock = UnpackBinarySuperblock(bb.superblock);
                GetSuperblockAvgMag(superblock,out_beacon_count,out_participant_count,out_avg,true,pblockindex->nHeight);

                Object c;
                c.push_back(Pair("Block #" + ToString(pblockindex->nHeight),pblockindex->GetBlockHash().GetHex()));
                c.push_back(Pair("Date",TimestampToHRDate(pblockindex->nTime)));
                c.push_back(Pair("Average Mag",out_avg));
                c.push_back(Pair("Wallet Version",bb.clientversion));
                double mag = GetSuperblockMagnitudeByCPID(superblock, cpid);
                if (!cpid.empty())
                {
                    c.push_back(Pair("Magnitude",mag));
                }


                results.push_back(c);
            }
        }
    }

    return results;
}

Array MagnitudeReport(std::string cpid)
{
           Array results;
           Object c;
           std::string Narr = ToString(GetAdjustedTime());
           c.push_back(Pair("RSA Report",Narr));
           results.push_back(c);
           double total_owed = 0;
           double magnitude_unit = GRCMagnitudeUnit(GetAdjustedTime());
           if (!pindexBest) return results;

           try
           {
                   if (mvMagnitudes.size() < 1)
                   {
                           if (fDebug3) LogPrintf("no results");
                           return results;
                   }
                   for(map<string,StructCPID>::iterator ii=mvMagnitudes.begin(); ii!=mvMagnitudes.end(); ++ii)
                   {
                        // For each CPID on the network, report:
                        StructCPID structMag = mvMagnitudes[(*ii).first];
                        if (structMag.initialized && !structMag.cpid.empty())
                        {
                                if (cpid.empty() || (Contains(structMag.cpid,cpid)))
                                {
                                            Object entry;
                                            if (IsResearchAgeEnabled(pindexBest->nHeight))
                                            {

                                                StructCPID stCPID = GetLifetimeCPID(structMag.cpid,"MagnitudeReport");
                                                double days = (GetAdjustedTime() - stCPID.LowLockTime) / 86400.0;
                                                entry.push_back(Pair("CPID",structMag.cpid));
                                                StructCPID UH = GetInitializedStructCPID2(cpid,mvMagnitudes);
                                                entry.push_back(Pair("Earliest Payment Time",TimestampToHRDate(stCPID.LowLockTime)));
                                                entry.push_back(Pair("Magnitude (Last Superblock)", structMag.Magnitude));
                                                entry.push_back(Pair("Research Payments (14 days)",structMag.payments));
                                                entry.push_back(Pair("Daily Paid",structMag.payments/14));
                                                // Research Age - Calculate Expected 14 Day Owed, and Daily Owed:
                                                double dExpected14 = magnitude_unit * structMag.Magnitude * 14;
                                                entry.push_back(Pair("Expected Earnings (14 days)", dExpected14));
                                                entry.push_back(Pair("Expected Earnings (Daily)", dExpected14/14));

                                                // Fulfillment %
                                                double fulfilled = ((structMag.payments/14) / ((dExpected14/14)+.01)) * 100;
                                                entry.push_back(Pair("Fulfillment %", fulfilled));

                                                entry.push_back(Pair("CPID Lifetime Interest Paid", stCPID.InterestSubsidy));
                                                entry.push_back(Pair("CPID Lifetime Research Paid", stCPID.ResearchSubsidy));
                                                entry.push_back(Pair("CPID Lifetime Avg Magnitude", stCPID.ResearchAverageMagnitude));

                                                entry.push_back(Pair("CPID Lifetime Payments Per Day", stCPID.ResearchSubsidy/(days+.01)));
                                                entry.push_back(Pair("Last Blockhash Paid", stCPID.BlockHash));
                                                entry.push_back(Pair("Last Block Paid",stCPID.LastBlock));
                                                entry.push_back(Pair("Tx Count",(int)stCPID.Accuracy));

                                                results.push_back(entry);
                                            }
                                            else
                                            {
                                                entry.push_back(Pair("CPID",structMag.cpid));
                                                entry.push_back(Pair("Last Block Paid",structMag.LastBlock));
                                                entry.push_back(Pair("DPOR Magnitude",  structMag.Magnitude));
                                                entry.push_back(Pair("Payment Magnitude",structMag.PaymentMagnitude));
                                                entry.push_back(Pair("Payment Timespan (Days)",structMag.PaymentTimespan));
                                                entry.push_back(Pair("Total Earned (14 days)",structMag.totalowed));
                                                entry.push_back(Pair("DPOR Payments (14 days)",structMag.payments));
                                                double outstanding = Round(structMag.totalowed - structMag.payments,2);
                                                total_owed += outstanding;
                                                entry.push_back(Pair("Outstanding Owed (14 days)",outstanding));
                                                entry.push_back(Pair("InterestPayments (14 days)",structMag.interestPayments));
                                                entry.push_back(Pair("Last Payment Time",TimestampToHRDate(structMag.LastPaymentTime)));
                                                entry.push_back(Pair("Owed",structMag.owed));
                                                entry.push_back(Pair("Daily Paid",structMag.payments/14));
                                                entry.push_back(Pair("Daily Owed",structMag.totalowed/14));
                                                results.push_back(entry);
                                            }
                                }
                        }

                    }

                    if (fDebug3) LogPrintf("MR8");

                    Object entry2;
                    entry2.push_back(Pair("Magnitude Unit (GRC payment per Magnitude per day)", magnitude_unit));
                    if (!IsResearchAgeEnabled(pindexBest->nHeight) && cpid.empty()) entry2.push_back(Pair("Grand Total Outstanding Owed",total_owed));
                    results.push_back(entry2);

                    int nMaxDepth = (nBestHeight-CONSENSUS_LOOKBACK) - ( (nBestHeight-CONSENSUS_LOOKBACK) % BLOCK_GRANULARITY);
                    int nLookback = BLOCKS_PER_DAY*14; //Daily block count * Lookback in days = 14 days
                    int nMinDepth = (nMaxDepth - nLookback) - ( (nMaxDepth-nLookback) % BLOCK_GRANULARITY);
                    if (cpid.empty())
                    {
                        Object entry3;
                        entry3.push_back(Pair("Start Block",nMinDepth));
                        entry3.push_back(Pair("End Block",nMaxDepth));
                        results.push_back(entry3);

                    }
                    if (fDebug3) LogPrintf("*MR5*");

                    return results;
            }
            catch(...)
            {
                LogPrintf("\nError in Magnitude Report \n ");
                return results;
            }

}

std::string TimestampToHRDate(double dtm)
{
    if (dtm == 0) return "1-1-1970 00:00:00";
    if (dtm > 9888888888) return "1-1-2199 00:00:00";
    std::string sDt = DateTimeStrFormat("%m-%d-%Y %H:%M:%S",dtm);
    return sDt;
}

double GetMagnitudeByCpidFromLastSuperblock(std::string sCPID)
{
        StructCPID structMag = mvMagnitudes[sCPID];
        if (structMag.initialized && IsResearcher(structMag.cpid))
        {
            return structMag.Magnitude;
        }
        return 0;
}

bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature)
{
    std::string sBeaconPublicKey = GetBeaconPublicKey(sCPID, false);
    std::string sConcatMessage = sCPID + sBlockHash;
    bool bValid = CheckMessageSignature("R","cpid", sConcatMessage, sSignature, sBeaconPublicKey);
    if(!bValid)
        LogPrintf("VerifyCPIDSignature: invalid signature sSignature=%s, cached key=%s\n"
        ,sSignature, sBeaconPublicKey);

    return bValid;
}

bool SignBlockWithCPID(const std::string& sCPID, const std::string& sBlockHash, std::string& sSignature, std::string& sError, bool bAdvertising)
{
    // Check if there is a beacon for this user
    // If not then return false as GetStoresBeaconPrivateKey grabs from the config
    if (!HasActiveBeacon(sCPID) && !bAdvertising)
    {
        sError = "No active beacon";
        return false;
    }
    // Returns the Signature of the CPID+BlockHash message.
    std::string sPrivateKey = GetStoredBeaconPrivateKey(sCPID);
    std::string sMessage = sCPID + sBlockHash;
    sSignature = SignMessage(sMessage,sPrivateKey);
    // If we failed to sign then return false
    if (sSignature == "Unable to sign message, check private key.")
    {
        sError = sSignature;
        sSignature = "";
        return false;
    }

    return true;
}

std::string GetPollContractByTitle(std::string objecttype, std::string title)
{
    for(const auto& item : ReadCacheSection(objecttype))
    {
        const std::string& contract = item.second.value;
        const std::string& PollTitle = ExtractXML(contract,"<TITLE>","</TITLE>");
        if(boost::iequals(PollTitle, title))
            return contract;
    }

    return std::string();
}

bool PollExists(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    return contract.length() > 10 ? true : false;
}

bool PollExpired(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    double expiration = RoundFromString(ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>"),0);
    return (expiration < (double)GetAdjustedTime()) ? true : false;
}


bool PollCreatedAfterSecurityUpgrade(std::string pollname)
{
	// If the expiration is after July 1 2017, use the new security features.
	std::string contract = GetPollContractByTitle("poll",pollname);
	double expiration = RoundFromString(ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>"),0);
	return (expiration > 1498867200) ? true : false;
}


double PollDuration(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    double days = RoundFromString(ExtractXML(contract,"<DAYS>","</DAYS>"),0);
    return days;
}

double DoubleFromAmount(int64_t amount)
{
    return (double)amount / (double)COIN;
}


double GetMoneySupplyFactor()
{

        StructCPID structcpid = mvNetwork["NETWORK"];
        double TotalCPIDS = mvMagnitudes.size();
        double AvgMagnitude = structcpid.NetworkAvgMagnitude;
        double TotalNetworkMagnitude = TotalCPIDS*AvgMagnitude;
        if (TotalNetworkMagnitude < 100) TotalNetworkMagnitude=100;
        double MoneySupply = DoubleFromAmount(pindexBest->nMoneySupply);
        double Factor = (MoneySupply/TotalNetworkMagnitude+.01);
        return Factor;

}

double PollCalculateShares(std::string contract, double sharetype, double MoneySupplyFactor, unsigned int VoteAnswerCount)
{
    std::string address = ExtractXML(contract,"<GRCADDRESS>","</GRCADDRESS>");
    std::string cpid = ExtractXML(contract,"<CPID>","</CPID>");
   	double magnitude = ReturnVerifiedVotingMagnitude(contract,PollCreatedAfterSecurityUpgrade(contract));
	double balance = ReturnVerifiedVotingBalance(contract,PollCreatedAfterSecurityUpgrade(contract));
	if (VoteAnswerCount < 1) VoteAnswerCount=1;
    if (sharetype==1) return magnitude/VoteAnswerCount;
    if (sharetype==2) return balance/VoteAnswerCount;

    if (sharetype==3)
    {
        // https://github.com/gridcoin/Gridcoin-Research/issues/87#issuecomment-253999878
        // Researchers weight is Total Money Supply / 5.67 * Magnitude
        double UserWeightedMagnitude = (MoneySupplyFactor/5.67) * magnitude;
        return (UserWeightedMagnitude+balance) / VoteAnswerCount;
    }
    if (sharetype==4)
    {
        if (magnitude > 0) return 1;
        return 0;
    }
    if (sharetype==5)
    {
        if (address.length() > 5) return 1;
        return 0;
    }
    return 0;
}


double VotesCount(std::string pollname, std::string answer, double sharetype, double& out_participants)
{
    double total_shares = 0;
    out_participants = 0;

    double MoneySupplyFactor = GetMoneySupplyFactor();

    for(const auto& item : ReadCacheSection("vote"))
    {
        const std::string& contract = item.second.value;
        const std::string& Title = ExtractXML(contract,"<TITLE>","</TITLE>");
        const std::string& VoterAnswer = ExtractXML(contract,"<ANSWER>","</ANSWER>");
        const std::vector<std::string>& vVoterAnswers = split(VoterAnswer.c_str(),";");
        for (const std::string& voterAnswers : vVoterAnswers)
        {
            if (boost::iequals(pollname, Title) && boost::iequals(answer, voterAnswers))
            {
                double shares = PollCalculateShares(contract, sharetype, MoneySupplyFactor, vVoterAnswers.size());
                total_shares += shares;
                out_participants += 1.0 / vVoterAnswers.size();
            }
        }
    }

    return total_shares;
}



std::string GetPollXMLElementByPollTitle(std::string pollname, std::string XMLElement1, std::string XMLElement2)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    std::string sElement = ExtractXML(contract,XMLElement1,XMLElement2);
    return sElement;
}


bool PollAcceptableAnswer(std::string pollname, std::string answer)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    std::string answers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
    std::vector<std::string> vAnswers = split(answers.c_str(),";");

    //Allow multiple choice voting:
    std::vector<std::string> vUserAnswers = split(answer.c_str(),";");
    for (unsigned int x = 0; x < vUserAnswers.size(); x++)
    {
        bool bFoundAnswer = false;
        for (unsigned int i = 0; i < vAnswers.size(); i++)
        {
                boost::to_lower(vAnswers[i]); //Contains Poll acceptable answers
                std::string sUserAnswer = vUserAnswers[x];
                boost::to_lower(sUserAnswer);
                if (sUserAnswer == vAnswers[i])
                {
                        bFoundAnswer=true;
                        break;
                }
        }
        if (!bFoundAnswer) return false;
    }
    return true;
}

std::string PollAnswers(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    std::string answers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
    return answers;

}


std::string GetShareType(double dShareType)
{
    if (dShareType == 1) return "Magnitude";
    if (dShareType == 2) return "Balance";
    if (dShareType == 3) return "Magnitude+Balance";
    if (dShareType == 4) return "CPID Count";
    if (dShareType == 5) return "Participants";
    return "?";
}





std::string GetProvableVotingWeightXML()
{
    std::string sXML = "<PROVABLEMAGNITUDE>";
    //Retrieve the historical magnitude
    if (IsResearcher(msPrimaryCPID))
    {
        StructCPID st1 = GetLifetimeCPID(msPrimaryCPID,"ProvableMagnitude()");
        CBlockIndex* pHistorical = GetHistoricalMagnitude(msPrimaryCPID);
        if (pHistorical->nHeight > 1 && pHistorical->nMagnitude > 0)
        {
            std::string sBlockhash = pHistorical->GetBlockHash().GetHex();
            std::string sError;
            std::string sSignature;
            bool bResult = SignBlockWithCPID(msPrimaryCPID, pHistorical->GetBlockHash().GetHex(), sSignature, sError);
            // Just because below comment it'll keep in line with that
            if (!bResult)
                sSignature = sError;
            // Find the Magnitude from the last staked block, within the last 6 months, and ensure researcher has a valid current beacon (if the beacon is expired, the signature contain an error message)
            sXML += "<CPID>" + msPrimaryCPID + "</CPID><INNERMAGNITUDE>"
                    + RoundToString(pHistorical->nMagnitude,2) + "</INNERMAGNITUDE>" +
                    "<HEIGHT>" + ToString(pHistorical->nHeight)
                    + "</HEIGHT><BLOCKHASH>" + sBlockhash + "</BLOCKHASH><SIGNATURE>" + sSignature + "</SIGNATURE>";
        }
    }
    sXML += "</PROVABLEMAGNITUDE>";

    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    std::string sRow = "";
    double dTotal = 0;
    double dBloatThreshhold = 100;
    double dCurrentItemCount = 0;
    double dItemBloatThreshhold = 50;
    // Iterate unspent coins from transactions owned by me that total over 100GRC (this prevents XML bloat)
    sXML += "<PROVABLEBALANCE>";
    for (auto const& out : vecOutputs)
    {
        int64_t nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        Object entry;
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
        {
            if (CoinToDouble(nValue) > dBloatThreshhold)
            {
                std::string sScriptPubKey1 = HexStr(pk.begin(), pk.end());
                std::string strAddress=CBitcoinAddress(address).ToString();
                CKeyID keyID;
                const CBitcoinAddress& bcAddress = CBitcoinAddress(address);
                if (bcAddress.GetKeyID(keyID))
                {
                    bool IsCompressed;
                    CKey vchSecret;
                    if (pwalletMain->GetKey(keyID, vchSecret))
                    {
                        // Here we use the secret key to sign the coins, then we abandon the key.
                        CSecret csKey = vchSecret.GetSecret(IsCompressed);
                        CKey keyInner;
                        keyInner.SetSecret(csKey,IsCompressed);
                        std::string private_key = CBitcoinSecret(csKey,IsCompressed).ToString();
                        std::string public_key = HexStr(keyInner.GetPubKey().Raw());
                        std::vector<unsigned char> vchSig;
                        keyInner.Sign(out.tx->GetHash(), vchSig);
                        // Sign the coins we own
                        std::string sSig(vchSig.begin(), vchSig.end());
                        // Increment the total balance weight voting ability
                        dTotal += CoinToDouble(nValue);
                        sRow = "<ROW><TXID>" + out.tx->GetHash().GetHex() + "</TXID>" +
                                "<AMOUNT>" + RoundToString(CoinToDouble(nValue),2) + "</AMOUNT>" +
                                "<POS>" + RoundToString((double)out.i,0) + "</POS>" +
                                "<PUBKEY>" + public_key + "</PUBKEY>" +
                                "<SCRIPTPUBKEY>" + sScriptPubKey1  + "</SCRIPTPUBKEY>" +
                                "<SIG>" + EncodeBase64(sSig) + "</SIG>" +
                                "<MESSAGE></MESSAGE></ROW>";
                        sXML += sRow;
                        dCurrentItemCount++;
                        if (dCurrentItemCount >= dItemBloatThreshhold)
                            break;
                    }
                }
            }
        }
    }


    sXML += "<TOTALVOTEDBALANCE>" + RoundToString(dTotal,2) + "</TOTALVOTEDBALANCE>";
    sXML += "</PROVABLEBALANCE>";
    return sXML;

}


double ReturnVerifiedVotingBalance(std::string sXML, bool bCreatedAfterSecurityUpgrade)
{
	std::string sPayload = ExtractXML(sXML,"<PROVABLEBALANCE>","</PROVABLEBALANCE>");
	double dTotalVotedBalance = RoundFromString(ExtractXML(sPayload,"<TOTALVOTEDBALANCE>","</TOTALVOTEDBALANCE>"),2);
	double dLegacyBalance = RoundFromString(ExtractXML(sXML,"<BALANCE>","</BALANCE>"),0);

	if (fDebug10) LogPrintf(" \n Total Voted Balance %f, Legacy Balance %f \n",(float)dTotalVotedBalance,(float)dLegacyBalance);

	if (!bCreatedAfterSecurityUpgrade) return dLegacyBalance;

	double dCounted = 0;
	std::vector<std::string> vXML= split(sPayload.c_str(),"<ROW>");
	for (unsigned int x = 0; x < vXML.size(); x++)
	{
		// Prove the contents of the XML as a 3rd party
		CTransaction tx2;
		uint256 hashBlock = 0;
		uint256 uTXID(ExtractXML(vXML[x],"<TXID>","</TXID>"));
		std::string sAmt = ExtractXML(vXML[x],"<AMOUNT>","</AMOUNT>");
		std::string sPos = ExtractXML(vXML[x],"<POS>","</POS>");
		std::string sXmlSig = ExtractXML(vXML[x],"<SIG>","</SIG>");
		std::string sXmlMsg = ExtractXML(vXML[x],"<MESSAGE>","</MESSAGE>");
		std::string sScriptPubKeyXml = ExtractXML(vXML[x],"<SCRIPTPUBKEY>","</SCRIPTPUBKEY>");
		int32_t iPos = RoundFromString(sPos,0);
		std::string sPubKey = ExtractXML(vXML[x],"<PUBKEY>","</PUBKEY>");
		if (!sPubKey.empty() && !sAmt.empty() && !sPos.empty() && uTXID > 0)
		{
			if (GetTransaction(uTXID, tx2, hashBlock))
			{
				if (iPos >= 0 && iPos < (int32_t) tx2.vout.size())
				{
					    int64_t nValue2 = tx2.vout[iPos].nValue;
					    const CScript& pk2 = tx2.vout[iPos].scriptPubKey;
					    CTxDestination address2;
						std::string sVotedPubKey = HexStr(pk2.begin(), pk2.end());
						std::string sVotedGRCAddress = CBitcoinAddress(address2).ToString();
						std::string sCoinOwnerAddress = PubKeyToAddress(pk2);
						double dAmount = CoinToDouble(nValue2);
						if (ExtractDestination(tx2.vout[iPos].scriptPubKey, address2))
						{
							if (sScriptPubKeyXml == sVotedPubKey && RoundToString(dAmount,2) == sAmt)
							{
								Object entry;
      					   		entry.push_back(Pair("Audited Amount",ValueFromAmount(nValue2)));
 						    	std::string sDecXmlSig = DecodeBase64(sXmlSig);
							    CKey keyVerify;
							    if (keyVerify.SetPubKey(ParseHex(sPubKey)))
								{
									  	std::vector<unsigned char> vchMsg1 = vector<unsigned char>(sXmlMsg.begin(), sXmlMsg.end());
										std::vector<unsigned char> vchSig1 = vector<unsigned char>(sDecXmlSig.begin(), sDecXmlSig.end());
										bool bValid = keyVerify.Verify(uTXID,vchSig1);
										// Unspent Balance is proven to be owned by the voters public key, count the vote
										if (bValid) dCounted += dAmount;
								}

							}
						}
					}
				}
			}
		}

		return dCounted;
}

double ReturnVerifiedVotingMagnitude(std::string sXML, bool bCreatedAfterSecurityUpgrade)
{
	double dLegacyMagnitude  = RoundFromString(ExtractXML(sXML,"<MAGNITUDE>","</MAGNITUDE>"),2);
	if (!bCreatedAfterSecurityUpgrade) return dLegacyMagnitude;

	std::string sMagXML = ExtractXML(sXML,"<PROVABLEMAGNITUDE>","</PROVABLEMAGNITUDE>");
	std::string sMagnitude = ExtractXML(sMagXML,"<INNERMAGNITUDE>","</INNERMAGNITUDE>");
	std::string sXmlSigned = ExtractXML(sMagXML,"<SIGNATURE>","</SIGNATURE>");
	std::string sXmlBlockHash = ExtractXML(sMagXML,"<BLOCKHASH>","</BLOCKHASH>");
	std::string sXmlCPID = ExtractXML(sMagXML,"<CPID>","</CPID>");
	if (!sXmlBlockHash.empty() && !sMagnitude.empty() && !sXmlSigned.empty())
	{
		CBlockIndex* pblockindexMagnitude = mapBlockIndex[uint256(sXmlBlockHash)];
		if (pblockindexMagnitude)
		{
				bool fResult = VerifyCPIDSignature(sXmlCPID, sXmlBlockHash, sXmlSigned);
				bool fAudited = (RoundFromString(RoundToString(pblockindexMagnitude->nMagnitude,2),0)==RoundFromString(sMagnitude,0) && fResult);
				if (fAudited) return (double)pblockindexMagnitude->nMagnitude;
		}
	}
	return 0;
}




Array GetJsonUnspentReport()
{
    // The purpose of this report is to list the details of unspent coins in the wallet, create a signed XML payload and then audit those coins as a third party
    // Written on 5-28-2017 - R HALFORD
    // We can use this as the basis for proving the total coin balance, and the current researcher magnitude in the voting system.
    Array results;

    //Retrieve the historical magnitude
    if (IsResearcher(msPrimaryCPID))
    {
        StructCPID st1 = GetLifetimeCPID(msPrimaryCPID,"GetUnspentReport()");
        CBlockIndex* pHistorical = GetHistoricalMagnitude(msPrimaryCPID);
        Object entry1;
        entry1.push_back(Pair("Researcher Magnitude",pHistorical->nMagnitude));
        results.push_back(entry1);

        // Create the XML Magnitude Payload
        if (pHistorical->nHeight > 1 && pHistorical->nMagnitude > 0)
        {
            std::string sBlockhash = pHistorical->GetBlockHash().GetHex();
            std::string sError;
            std::string sSignature;
            bool bResult = SignBlockWithCPID(msPrimaryCPID, pHistorical->GetBlockHash().GetHex(), sSignature, sError);
            // Just because below comment it'll keep in line with that
            if (!bResult)
                sSignature = sError;

            // Find the Magnitude from the last staked block, within the last 6 months, and ensure researcher has a valid current beacon (if the beacon is expired, the signature contain an error message)

            std::string sMagXML = "<CPID>" + msPrimaryCPID + "</CPID><INNERMAGNITUDE>" + RoundToString(pHistorical->nMagnitude,2) + "</INNERMAGNITUDE>" +
                    "<HEIGHT>" + ToString(pHistorical->nHeight) + "</HEIGHT><BLOCKHASH>" + sBlockhash + "</BLOCKHASH><SIGNATURE>" + sSignature + "</SIGNATURE>";
            std::string sMagnitude = ExtractXML(sMagXML,"<INNERMAGNITUDE>","</INNERMAGNITUDE>");
            std::string sXmlSigned = ExtractXML(sMagXML,"<SIGNATURE>","</SIGNATURE>");
            std::string sXmlBlockHash = ExtractXML(sMagXML,"<BLOCKHASH>","</BLOCKHASH>");
            std::string sXmlCPID = ExtractXML(sMagXML,"<CPID>","</CPID>");
            Object entry;
            entry.push_back(Pair("CPID Signature", sSignature));
            entry.push_back(Pair("Historical Magnitude Block #", pHistorical->nHeight));
            entry.push_back(Pair("Historical Blockhash", sBlockhash));
            // Prove the magnitude from a 3rd party standpoint:
            if (!sXmlBlockHash.empty() && !sMagnitude.empty() && !sXmlSigned.empty())
            {
                CBlockIndex* pblockindexMagnitude = mapBlockIndex[uint256(sXmlBlockHash)];
                if (pblockindexMagnitude)
                {
                    bool fResult = VerifyCPIDSignature(sXmlCPID, sXmlBlockHash, sXmlSigned);
                    entry.push_back(Pair("Historical Magnitude",pblockindexMagnitude->nMagnitude));
                    entry.push_back(Pair("Signature Valid",fResult));
                    bool fAudited = (RoundFromString(RoundToString(pblockindexMagnitude->nMagnitude,2),0)==RoundFromString(sMagnitude,0) && fResult);
                    entry.push_back(Pair("Magnitude Audited",fAudited));
                    results.push_back(entry);
                }
            }


        }


    }

    // Now we move on to proving the coins we own are ours

    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
	std::string sXML = "";
	std::string sRow = "";
	double dTotal = 0;
	double dBloatThreshhold = 100;
	double dCurrentItemCount = 0;
	double dItemBloatThreshhold = 50;
	// Iterate unspent coins from transactions owned by me that total over 100GRC (this prevents XML bloat)
    for (auto const& out : vecOutputs)
    {
        int64_t nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        Object entry;
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
        {
            if (CoinToDouble(nValue) > dBloatThreshhold)
            {
                entry.push_back(Pair("TXID", out.tx->GetHash().GetHex()));
                entry.push_back(Pair("Address", CBitcoinAddress(address).ToString()));
                std::string sScriptPubKey1 = HexStr(pk.begin(), pk.end());
                entry.push_back(Pair("Amount",ValueFromAmount(nValue)));
                std::string strAddress=CBitcoinAddress(address).ToString();
                CKeyID keyID;
                const CBitcoinAddress& bcAddress = CBitcoinAddress(address);
                if (bcAddress.GetKeyID(keyID))
                {
                    bool IsCompressed;
                    CKey vchSecret;
                    if (pwalletMain->GetKey(keyID, vchSecret))
                    {
                        // Here we use the secret key to sign the coins, then we abandon the key.
                        CSecret csKey = vchSecret.GetSecret(IsCompressed);
                        CKey keyInner;
                        keyInner.SetSecret(csKey,IsCompressed);
                        std::string private_key = CBitcoinSecret(csKey,IsCompressed).ToString();
                        std::string public_key = HexStr(keyInner.GetPubKey().Raw());
                        std::vector<unsigned char> vchSig;
                        keyInner.Sign(out.tx->GetHash(), vchSig);
                        // Sign the coins we own
                        std::string sSig = std::string(vchSig.begin(), vchSig.end());
                        // Increment the total balance weight voting ability
                        dTotal += CoinToDouble(nValue);
                        sRow = "<ROW><TXID>" + out.tx->GetHash().GetHex() + "</TXID>" +
                                "<AMOUNT>" + RoundToString(CoinToDouble(nValue),2) + "</AMOUNT>" +
                                "<POS>" + RoundToString((double)out.i,0) + "</POS>" +
                                "<PUBKEY>" + public_key + "</PUBKEY>" +
                                "<SCRIPTPUBKEY>" + sScriptPubKey1  + "</SCRIPTPUBKEY>" +
                                "<SIG>" + EncodeBase64(sSig) + "</SIG>" +
                                "<MESSAGE></MESSAGE></ROW>";
                        sXML += sRow;
                        dCurrentItemCount++;
                        if (dCurrentItemCount >= dItemBloatThreshhold)
                            break;
                    }

                }
                results.push_back(entry);
            }
        }
    }

	// Now we will need to go back through the XML and Audit the claimed vote weight balance as a 3rd party

	double dCounted = 0;

	std::vector<std::string> vXML= split(sXML.c_str(),"<ROW>");
	for (unsigned int x = 0; x < vXML.size(); x++)
	{
		// Prove the contents of the XML as a 3rd party
		CTransaction tx2;
		uint256 hashBlock = 0;
		uint256 uTXID(ExtractXML(vXML[x],"<TXID>","</TXID>"));
		std::string sAmt = ExtractXML(vXML[x],"<AMOUNT>","</AMOUNT>");
		std::string sPos = ExtractXML(vXML[x],"<POS>","</POS>");
		std::string sXmlSig = ExtractXML(vXML[x],"<SIG>","</SIG>");
		std::string sXmlMsg = ExtractXML(vXML[x],"<MESSAGE>","</MESSAGE>");
		std::string sScriptPubKeyXml = ExtractXML(vXML[x],"<SCRIPTPUBKEY>","</SCRIPTPUBKEY>");

		int32_t iPos = RoundFromString(sPos,0);
		std::string sPubKey = ExtractXML(vXML[x],"<PUBKEY>","</PUBKEY>");

		if (!sPubKey.empty() && !sAmt.empty() && !sPos.empty() && uTXID > 0)
		{

			if (GetTransaction(uTXID, tx2, hashBlock))
			{
				if (iPos >= 0 && iPos < (int32_t) tx2.vout.size())
				{
					    int64_t nValue2 = tx2.vout[iPos].nValue;
					    const CScript& pk2 = tx2.vout[iPos].scriptPubKey;
					    CTxDestination address2;
						std::string sVotedPubKey = HexStr(pk2.begin(), pk2.end());
						std::string sVotedGRCAddress = CBitcoinAddress(address2).ToString();
						std::string sCoinOwnerAddress = PubKeyToAddress(pk2);
						double dAmount = CoinToDouble(nValue2);
						if (ExtractDestination(tx2.vout[iPos].scriptPubKey, address2))
						{
							if (sScriptPubKeyXml == sVotedPubKey && RoundToString(dAmount,2) == sAmt)
							{
								Object entry;
      					   		entry.push_back(Pair("Audited Amount",ValueFromAmount(nValue2)));
 						    	std::string sDecXmlSig = DecodeBase64(sXmlSig);
							    CKey keyVerify;
							    if (keyVerify.SetPubKey(ParseHex(sPubKey)))
								{
									  	std::vector<unsigned char> vchMsg1 = vector<unsigned char>(sXmlMsg.begin(), sXmlMsg.end());
										std::vector<unsigned char> vchSig1 = vector<unsigned char>(sDecXmlSig.begin(), sDecXmlSig.end());
										bool bValid = keyVerify.Verify(uTXID,vchSig1);
										// Unspent Balance is proven to be owned by the voters public key, count the vote
										if (bValid) dCounted += dAmount;
										entry.push_back(Pair("Verified",bValid));
								}

	  							results.push_back(entry);
							}
					}
				}
			}
		}
	}

	Object entry;
	// Note that the voter needs to have the wallet at least unlocked for staking in order for the coins to be signed, otherwise the coins-owned portion of the vote balance will be 0.
	// In simpler terms: The wallet must be unlocked to cast a provable vote.

	entry.push_back(Pair("Total Voting Balance Weight", dTotal));
    entry.push_back(Pair("Grand Verified Amount",dCounted));

	std::string sBalCheck2 = GetProvableVotingWeightXML();
	double dVerifiedBalance = ReturnVerifiedVotingBalance(sBalCheck2,true);
	double dVerifiedMag = ReturnVerifiedVotingMagnitude(sBalCheck2, true);
	entry.push_back(Pair("Balance check",dVerifiedBalance));
	entry.push_back(Pair("Mag check",dVerifiedMag));
	results.push_back(entry);

    return results;
}





Array GetJsonVoteDetailsReport(std::string pollname)
{
    double total_shares = 0;
    double participants = 0;
    double MoneySupplyFactor = GetMoneySupplyFactor();

    Array results;
    Object entry;
    entry.push_back(Pair("Votes","Votes Report " + pollname));
    entry.push_back(Pair("MoneySupplyFactor",RoundToString(MoneySupplyFactor,2)));

    // Add header
    entry.push_back(Pair("GRCAddress,CPID,Question,Answer,ShareType,URL", "Shares"));

    boost::to_lower(pollname);
    for(const auto& item : ReadCacheSection("vote"))
    {
        const std::string& contract = item.second.value;
        const std::string& Title = ExtractXML(contract,"<TITLE>","</TITLE>");
        if(boost::iequals(pollname, Title))
        {
            const std::string& OriginalContract = GetPollContractByTitle("poll",Title);
            const std::string& Question = ExtractXML(OriginalContract,"<QUESTION>","</QUESTION>");
            const std::string& GRCAddress = ExtractXML(contract,"<GRCADDRESS>","</GRCADDRESS>");
            const std::string& CPID = ExtractXML(contract,"<CPID>","</CPID>");

            double dShareType = RoundFromString(GetPollXMLElementByPollTitle(Title,"<SHARETYPE>","</SHARETYPE>"),0);
            std::string sShareType= GetShareType(dShareType);
            std::string sURL = ExtractXML(contract,"<URL>","</URL>");

            std::string Balance = ExtractXML(contract,"<BALANCE>","</BALANCE>");

            const std::string& VoterAnswer = boost::to_lower_copy(ExtractXML(contract,"<ANSWER>","</ANSWER>"));
            const std::vector<std::string>& vVoterAnswers = split(VoterAnswer.c_str(),";");
            for (const auto& answer : vVoterAnswers)
            {
                double shares = PollCalculateShares(contract, dShareType, MoneySupplyFactor, vVoterAnswers.size());
                total_shares += shares;
                participants += 1.0 / vVoterAnswers.size();
                const std::string& voter = GRCAddress + "," + CPID + "," + Question + "," + answer + "," + sShareType + "," + sURL;
                entry.push_back(Pair(voter,RoundToString(shares,0)));
            }
        }
    }

    entry.push_back(Pair("Total Participants",RoundToString(participants,2)));
    results.push_back(entry);
    return results;
}


Array GetJSONPollsReport(bool bDetail, std::string QueryByTitle, std::string& out_export, bool IncludeExpired)
{
    //Title,ExpirationDate, Question, Answers, ShareType(1=Magnitude,2=Balance,3=Both)
    Array results;
    Object entry;
    entry.push_back(Pair("Polls","Polls Report " + QueryByTitle));
    std::string rows;
    std::string row;
    double iPollNumber = 0;
    double total_participants = 0;
    double total_shares = 0;
    boost::to_lower(QueryByTitle);
    std::string sExport;
    std::string sExportRow;
    out_export.clear();

    for(const auto& item : ReadCacheSection("poll"))
    {
        const std::string& title = boost::to_lower_copy(item.first);
        const std::string& contract = item.second.value;
        std::string Expiration = ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>");
        std::string Question = ExtractXML(contract,"<QUESTION>","</QUESTION>");
        std::string Answers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
        std::string ShareType = ExtractXML(contract,"<SHARETYPE>","</SHARETYPE>");
        std::string sURL = ExtractXML(contract,"<URL>","</URL>");
        if (!PollExpired(title) || IncludeExpired)
        {
            if (QueryByTitle.empty() || QueryByTitle == title)
            {

                if( (title.length()>128) &&
                    (Expiration.length()>64) &&
                    (Question.length()>4096) &&
                    (Answers.length()>8192) &&
                    (ShareType.length()>64) &&
                    (sURL.length()>256)  )
                    continue;

                const std::vector<std::string>& vAnswers = split(Answers.c_str(),";");

                std::string::size_type longestanswer = 0;
                for (const std::string& answer : vAnswers)
                    longestanswer = std::max( longestanswer, answer.length() );

                if( longestanswer>128 )
                    continue;

                iPollNumber++;
                total_participants = 0;
                total_shares=0;
                std::string BestAnswer;
                double highest_share = 0;
                std::string ExpirationDate = TimestampToHRDate(RoundFromString(Expiration,0));
                std::string sShareType = GetShareType(RoundFromString(ShareType,0));
                std::string TitleNarr = "Poll #" + RoundToString((double)iPollNumber,0)
                                        + " (" + ExpirationDate + " ) - " + sShareType;

                entry.push_back(Pair(TitleNarr,title));
                sExportRow = "<POLL><URL>" + sURL + "</URL><TITLE>" + title + "</TITLE><EXPIRATION>" + ExpirationDate + "</EXPIRATION><SHARETYPE>" + sShareType + "</SHARETYPE><QUESTION>" + Question + "</QUESTION><ANSWERS>"+Answers+"</ANSWERS>";

                if (bDetail)
                {
                    entry.push_back(Pair("Question",Question));
                    sExportRow += "<ARRAYANSWERS>";
                    size_t i = 0;
                    for (const std::string& answer : vAnswers)
                    {
                        double participants=0;
                        double dShares = VotesCount(title, answer, RoundFromString(ShareType,0),participants);
                        if (dShares > highest_share)
                        {
                            highest_share = dShares;
                            BestAnswer = answer;
                        }

                        entry.push_back(Pair("#" + ToString(++i) + " [" + RoundToString(participants,3) + "]. " + answer,dShares));
                        total_participants += participants;
                        total_shares += dShares;
                        sExportRow += "<RESERVED></RESERVED><ANSWERNAME>" + answer + "</ANSWERNAME><PARTICIPANTS>" + RoundToString(participants,0) + "</PARTICIPANTS><SHARES>" + RoundToString(dShares,0) + "</SHARES>";
                    }
                    sExportRow += "</ARRAYANSWERS>";

                    //Totals:
                    entry.push_back(Pair("Participants",total_participants));
                    entry.push_back(Pair("Total Shares",total_shares));
                    if (total_participants < 3) BestAnswer = "";

                    entry.push_back(Pair("Best Answer",BestAnswer));
                    sExportRow += "<TOTALPARTICIPANTS>" + RoundToString(total_participants,0)
                                  + "</TOTALPARTICIPANTS><TOTALSHARES>" + RoundToString(total_shares,0)
                                  + "</TOTALSHARES><BESTANSWER>" + BestAnswer + "</BESTANSWER>";

                }
                sExportRow += "</POLL>";
                sExport += sExportRow;
            }
        }
    }

    results.push_back(entry);
    out_export = sExport;
    return results;
}





Array GetUpgradedBeaconReport()
{
    Array results;
    Object entry;
    entry.push_back(Pair("Report","Upgraded Beacon Report 1.0"));
    std::string rows = "";
    std::string row = "";
    int iBeaconCount = 0;
    int iUpgradedBeaconCount = 0;
    for(const auto& item : ReadCacheSection("beacon"))
    {
        const AppCacheEntry& entry = item.second;
        std::string contract = DecodeBase64(entry.value);
        std::string cpidv2 = ExtractValue(contract,";",0);
        std::string grcaddress = ExtractValue(contract,";",2);
        std::string sPublicKey = ExtractValue(contract,";",3);
        if (!sPublicKey.empty()) iUpgradedBeaconCount++;
        iBeaconCount++;
    }

    entry.push_back(Pair("Total Beacons", iBeaconCount));
    entry.push_back(Pair("Upgraded Beacon Count", iUpgradedBeaconCount));
    double dPct = ((double)iUpgradedBeaconCount / ((double)iBeaconCount) + .01);
    entry.push_back(Pair("Pct Of Upgraded Beacons",RoundToString(dPct*100,3)));
    results.push_back(entry);
    return results;
}





Array GetJSONBeaconReport()
{
    Array results;
    Object entry;
    entry.push_back(Pair("CPID","GRCAddress"));
    std::string row;
    for(const auto& item : ReadCacheSection("beacon"))
    {
        const std::string& key = item.first;
        const AppCacheEntry& cache = item.second;
        row = key + "<COL>" + cache.value;
        std::string contract = DecodeBase64(cache.value);
        std::string grcaddress = ExtractValue(contract,";",2);
        entry.push_back(Pair(key, grcaddress));
    }

    results.push_back(entry);
    return results;
}


double GetTotalNeuralNetworkHashVotes()
{
    double total = 0;
    std::string neural_hash = "";
    for(map<std::string,double>::iterator ii=mvNeuralNetworkHash.begin(); ii!=mvNeuralNetworkHash.end(); ++ii)
    {
                double popularity = mvNeuralNetworkHash[(*ii).first];
                neural_hash = (*ii).first;
                // d41d8 is the hash of an empty magnitude contract - don't count it
                if (neural_hash != "d41d8cd98f00b204e9800998ecf8427e" && neural_hash != "TOTAL_VOTES" && popularity >= .01)
                {
                    total += popularity;
                }

    }
    return total;
}


double GetTotalCurrentNeuralNetworkHashVotes()
{
    double total = 0;
    std::string neural_hash = "";
    for(map<std::string,double>::iterator ii=mvCurrentNeuralNetworkHash.begin(); ii!=mvCurrentNeuralNetworkHash.end(); ++ii)
    {
                double popularity = mvCurrentNeuralNetworkHash[(*ii).first];
                neural_hash = (*ii).first;
                // d41d8 is the hash of an empty magnitude contract - don't count it
                if (neural_hash != "d41d8cd98f00b204e9800998ecf8427e" && neural_hash != "TOTAL_VOTES" && popularity >= .01)
                {
                    total += popularity;
                }

    }
    return total;
}


Array GetJSONNeuralNetworkReport()
{
      Array results;
      //Returns a report of the networks neural hashes in order of popularity
      std::string neural_hash = "";
      std::string report = "Neural_hash, Popularity\n";
      std::string row = "";
      double pct = 0;
      Object entry;
      entry.push_back(Pair("Neural Hash","Popularity,Percent %"));
      double votes = GetTotalNeuralNetworkHashVotes();

      for(map<std::string,double>::iterator ii=mvNeuralNetworkHash.begin(); ii!=mvNeuralNetworkHash.end(); ++ii)
      {
                double popularity = mvNeuralNetworkHash[(*ii).first];
                neural_hash = (*ii).first;

                //If the hash != empty_hash: >= .01
                if (neural_hash != "d41d8cd98f00b204e9800998ecf8427e" && neural_hash != "TOTAL_VOTES" && popularity > 0)
                {
                    row = neural_hash + "," + RoundToString(popularity,0);
                    report += row + "\n";
                    pct = (((double)popularity)/(votes+.01))*100;
                    entry.push_back(Pair(neural_hash,RoundToString(popularity,0) + "; " + RoundToString(pct,2) + "%"));
                }
      }
      // If we have a pending superblock, append it to the report:
      std::string SuperblockHeight = ReadCache("neuralsecurity","pending").value;
      if (!SuperblockHeight.empty() && SuperblockHeight != "0")
      {
          entry.push_back(Pair("Pending",SuperblockHeight));
      }
      int64_t superblock_age = GetAdjustedTime() - ReadCache("superblock", "magnitudes").timestamp;

      entry.push_back(Pair("Superblock Age",superblock_age));
      if (superblock_age > GetSuperblockAgeSpacing(nBestHeight))
      {
          int iRoot = 30;
          int iModifier = (nBestHeight % iRoot);
          int iQuorumModifier = (nBestHeight % 10);
          int iLastNeuralSync = nBestHeight - iModifier;
          int iNextNeuralSync = iLastNeuralSync + iRoot;
          int iLastQuorum = nBestHeight - iQuorumModifier;
          int iNextQuorum = iLastQuorum + 10;
          entry.push_back(Pair("Last Sync", iLastNeuralSync));
          entry.push_back(Pair("Next Sync", iNextNeuralSync));
          entry.push_back(Pair("Next Quorum", iNextQuorum));
      }
      results.push_back(entry);
      return results;
}


Array GetJSONCurrentNeuralNetworkReport()
{
      Array results;
      //Returns a report of the networks neural hashes in order of popularity
      std::string neural_hash = "";
      std::string report = "Neural_hash, Popularity\n";
      std::string row = "";
      double pct = 0;
      Object entry;
      entry.push_back(Pair("Neural Hash","Popularity,Percent %"));
      double votes = GetTotalCurrentNeuralNetworkHashVotes();

      for(map<std::string,double>::iterator ii=mvCurrentNeuralNetworkHash.begin(); ii!=mvCurrentNeuralNetworkHash.end(); ++ii)
      {
                double popularity = mvCurrentNeuralNetworkHash[(*ii).first];
                neural_hash = (*ii).first;

                //If the hash != empty_hash: >= .01
                if (neural_hash != "d41d8cd98f00b204e9800998ecf8427e" && neural_hash != "TOTAL_VOTES" && popularity > 0)
                {
                    row = neural_hash + "," + RoundToString(popularity,0);
                    report += row + "\n";
                    pct = (((double)popularity)/(votes+.01))*100;
                    entry.push_back(Pair(neural_hash,RoundToString(popularity,0) + "; " + RoundToString(pct,2) + "%"));
                }
      }
      // If we have a pending superblock, append it to the report:
      std::string SuperblockHeight = ReadCache("neuralsecurity","pending").value;
      if (!SuperblockHeight.empty() && SuperblockHeight != "0")
      {
          entry.push_back(Pair("Pending",SuperblockHeight));
      }
      int64_t superblock_age = GetAdjustedTime() - ReadCache("superblock", "magnitudes").timestamp;

      entry.push_back(Pair("Superblock Age",superblock_age));
      if (superblock_age > GetSuperblockAgeSpacing(nBestHeight))
      {
          int iRoot = 30;
          int iModifier = (nBestHeight % iRoot);
          int iQuorumModifier = (nBestHeight % 10);
          int iLastNeuralSync = nBestHeight - iModifier;
          int iNextNeuralSync = iLastNeuralSync + iRoot;
          int iLastQuorum = nBestHeight - iQuorumModifier;
          int iNextQuorum = iLastQuorum + 10;
          entry.push_back(Pair("Last Sync", iLastNeuralSync));
          entry.push_back(Pair("Next Sync", iNextNeuralSync));
          entry.push_back(Pair("Next Quorum", iNextQuorum));
      }
      results.push_back(entry);
      return results;
}


Array GetJSONVersionReport()
{
      Array results;
      //Returns a report of the GRC Version staking blocks over the last 100 blocks
      std::string neural_ver = "";
      std::string report = "Version, Popularity\n";
      std::string row = "";
      double pct = 0;
      Object entry;
      entry.push_back(Pair("Version","Popularity,Percent %"));

      double votes = 0;
      for(auto it : mvNeuralVersion)
          votes += it.second;

      for(map<std::string,double>::iterator ii=mvNeuralVersion.begin(); ii!=mvNeuralVersion.end(); ++ii)
      {
                double popularity = mvNeuralVersion[(*ii).first];
                neural_ver = (*ii).first;
                //If the hash != empty_hash:
                if (popularity > 0)
                {
                    row = neural_ver + "," + RoundToString(popularity,0);
                    report += row + "\n";
                    pct = popularity/(votes+.01)*100;
                    entry.push_back(Pair(neural_ver,RoundToString(popularity,0) + "; " + RoundToString(pct,2) + "%"));
                }
      }
      results.push_back(entry);
      return results;

}

std::string GetBurnAddress()
{
    return fTestNet ? "mk1e432zWKH1MW57ragKywuXaWAtHy1AHZ" : "S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB";
}

std::string BurnCoinsWithNewContract(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue,
                     int64_t MinimumBalance, double dFees, std::string strPublicKey, std::string sBurnAddress)
{
    CBitcoinAddress address(sBurnAddress);
    if (!address.IsValid())       throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");
    std::string sMasterKey = (sType=="project" || sType=="projectmapping" || sType=="smart_contract") ? GetArgument("masterprojectkey", msMasterMessagePrivateKey) : msMasterMessagePrivateKey;

    int64_t nAmount = AmountFromValue(dFees);
    // Wallet comments
    CWalletTx wtx;
    if (pwalletMain->IsLocked())  throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    std::string sMessageType      = "<MT>" + sType  + "</MT>";  //Project or Smart Contract
    std::string sMessageKey       = "<MK>" + sPrimaryKey   + "</MK>";
    std::string sMessageValue     = "<MV>" + sValue + "</MV>";
    std::string sMessagePublicKey = "<MPK>"+ strPublicKey + "</MPK>";
    std::string sMessageAction    = bAdd ? "<MA>A</MA>" : "<MA>D</MA>"; //Add or Delete
    //Sign Message
    std::string sSig = SignMessage(sType+sPrimaryKey+sValue,sMasterKey);
    std::string sMessageSignature = "<MS>" + sSig + "</MS>";
    wtx.hashBoinc = sMessageType+sMessageKey+sMessageValue+sMessageAction+sMessagePublicKey+sMessageSignature;
    string strError = pwalletMain->SendMoneyToDestinationWithMinimumBalance(address.Get(), nAmount, MinimumBalance, wtx);
    if (!strError.empty())        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    return wtx.GetHash().GetHex().c_str();
}



std::string SendReward(std::string sAddress, int64_t nAmount)
{
    CBitcoinAddress address(sAddress);
    if (!address.IsValid()) return "Invalid Gridcoin address";
    // Wallet comments
    CWalletTx wtx;
    if (pwalletMain->IsLocked()) return "Error: Please enter the wallet passphrase with walletpassphrase first.";
    std::string sMessageType      = "<MT>REWARD</MT>";
    std::string sMessageValue     = "<MV>" + sAddress + "</MV>";
    wtx.hashBoinc = sMessageType + sMessageValue;
    string strError = pwalletMain->SendMoneyToDestinationWithMinimumBalance(address.Get(), nAmount, 1, wtx);
    if (!strError.empty()) return strError;
    return wtx.GetHash().GetHex().c_str();
}



std::string AddMessage(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue,
                    std::string sMasterKey, int64_t MinimumBalance, double dFees, std::string strPublicKey)
{
    std::string sAddress = GetBurnAddress();
    CBitcoinAddress address(sAddress);
    if (!address.IsValid())       throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");
    int64_t nAmount = AmountFromValue(dFees);
    // Wallet comments
    CWalletTx wtx;
    if (pwalletMain->IsLocked())  throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    std::string sMessageType      = "<MT>" + sType  + "</MT>";  //Project or Smart Contract
    std::string sMessageKey       = "<MK>" + sPrimaryKey   + "</MK>";
    std::string sMessageValue     = "<MV>" + sValue + "</MV>";
    std::string sMessagePublicKey = "<MPK>"+ strPublicKey + "</MPK>";
    std::string sMessageAction    = bAdd ? "<MA>A</MA>" : "<MA>D</MA>"; //Add or Delete
    //Sign Message
    std::string sSig = SignMessage(sType+sPrimaryKey+sValue,sMasterKey);
    std::string sMessageSignature = "<MS>" + sSig + "</MS>";
    wtx.hashBoinc = sMessageType+sMessageKey+sMessageValue+sMessageAction+sMessagePublicKey+sMessageSignature;
    string strError = pwalletMain->SendMoneyToDestinationWithMinimumBalance(address.Get(), nAmount, MinimumBalance, wtx);
    if (!strError.empty())        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    return wtx.GetHash().GetHex().c_str();
}

std::string SuccessFail(bool f)
{
    return f ? "SUCCESS" : "FAIL";
}

std::string YesNo(bool f)
{
    return f ? "Yes" : "No";
}

Value listitem(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 1 && params.size() != 2 && params.size() != 3 && params.size() != 4))
        throw runtime_error(
        "list <string::itemname>\n"
        "Returns details of a given item by name.\n"
        "list help\n"
        "Displays help on various available list commands.\n");

    std::string sitem = params[0].get_str();

    std::string args = "";
    if (params.size()==2)
    {
        args=params[1].get_str();
    }


    Array results;
    Object e2;
    e2.push_back(Pair("Command",sitem));
    results.push_back(e2);
    if (sitem == "networktime")
    {
            Object entry;
            entry.push_back(Pair("Network Time",GetAdjustedTime()));
            results.push_back(entry);
    }
    else if (sitem == "rsaweight")
    {
        double out_magnitude = 0;
        double out_owed = 0;
        int64_t RSAWEIGHT = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
        out_magnitude = GetUntrustedMagnitude(GlobalCPUMiningCPID.cpid,out_owed);


        Object entry;
        entry.push_back(Pair("RSA Weight",RSAWEIGHT));
        entry.push_back(Pair("Magnitude",out_magnitude));
        entry.push_back(Pair("RSA Owed",out_owed));
        results.push_back(entry);

	}
    else if (sitem == "explainmagnitude")
    {
        Object entry;

        bool bForce = false;

        if (params.size() == 2)
        {
            std::string sOptional = params[1].get_str();

            boost::to_lower(sOptional);

            if (sOptional == "true")
                bForce = true;
        }

        if (bForce)
        {
            if (msNeuralResponse.length() < 25)
            {
                entry.push_back(Pair("Neural Response", "Empty; Requesting a response.."));
                entry.push_back(Pair("WARNING", "Only force once and try again without force if response is not received. Doing too many force attempts gets a temporary ban from neural node responses"));

                msNeuralResponse = "";

                AsyncNeuralRequest("explainmag", GlobalCPUMiningCPID.cpid, 10);
            }
        }

        if (msNeuralResponse.length() > 25)
        {
            entry.push_back(Pair("Neural Response", "true"));

            std::vector<std::string> vMag = split(msNeuralResponse.c_str(),"<ROW>");

            for (unsigned int i = 0; i < vMag.size(); i++)
                entry.push_back(Pair(RoundToString(i+1,0),vMag[i].c_str()));
        }

        else
            entry.push_back(Pair("Neural Response", "false; Try again at a later time"));

        results.push_back(entry);
    }
    else if (sitem == "superblocks")
    {
        std::string cpid = "";
        if (params.size() == 2)
        {
            cpid = params[1].get_str();
        }

        results = SuperblockReport(cpid);
        return results;
    }
    else if (sitem == "magnitude")
    {
        std::string cpid = "";
        if (params.size() == 2)
        {
            cpid = params[1].get_str();
        }

        if (params.size() > 2)
        {
            Object entry;
            entry.push_back(Pair("Error","You must either specify 'list magnitude' to see the entire report, or 'list magnitude CPID' to see a specific CPID."));
            results.push_back(entry);
            return results;
        }
        results = MagnitudeReport(cpid);
        if (results.size() > 1000)
        {
            results.clear();
            Object entry;
            entry.push_back(Pair("Error","Magnitude report too large; try specifying the cpid : list magnitude cpid."));
            results.push_back(entry);
            return results;
        }
        return results;
    }
    else if (sitem == "lifetime")
    {
        std::string cpid = msPrimaryCPID;
        if (params.size() == 2)
        {
            cpid = params[1].get_str();
        }

        results = LifetimeReport(cpid);
        return results;
    }
    else if (sitem == "currenttime")
    {

        Object entry;
        entry.push_back(Pair("Unix",GetAdjustedTime()));
        entry.push_back(Pair("UTC",TimestampToHRDate(GetAdjustedTime())));
        results.push_back(entry);

    }
	else if (sitem == "seefile")
	{
		// This is a unit test to prove viability of transmitting a file from node to node
		std::string sFile = "C:\\test.txt";
        std::vector<unsigned char> v = readFileToVector(sFile);
		Object entry;
	    entry.push_back(Pair("byte1",v[1]));
        entry.push_back(Pair("bytes",(double)v.size()));
        for (unsigned int i = 0; i < v.size(); i++)
		{
			entry.push_back(Pair("bytes",v[i]));
		}
		std::string sManifest = FileManifest();
		entry.push_back(Pair("manifest",sManifest));
		results.push_back(entry);
	}
    else if (sitem == "mymagnitude")
    {
            results = MagnitudeReport(msPrimaryCPID);
            return results;
    }
    else if (sitem == "memorypool")
    {
        Object entry;
        entry.push_back(Pair("Excluded Tx",msMiningErrorsExcluded));
        entry.push_back(Pair("Included Tx",msMiningErrorsIncluded));
        results.push_back(entry);
    }
    else if (sitem == "rsa")
    {
            if (msPrimaryCPID=="") msPrimaryCPID="INVESTOR";
            results = MagnitudeReport(msPrimaryCPID);
            return results;
    }
    else if (sitem == "projects")
    {
        for (const auto& item : ReadCacheSection("project"))
        {
            Object entry;

            std::string sProjectName = ToOfficialName(item.first);
            if (sProjectName.empty())
                continue;

            std::string sProjectURL = item.second.value;
            sProjectURL.erase(std::remove(sProjectURL.begin(), sProjectURL.end(), '@'), sProjectURL.end());

            // If contains an additional stats URL for project stats; remove it for the user to goto the correct website.
            if (sProjectURL.find("stats/") != string::npos)
            {
                std::size_t tFound = sProjectURL.find("stats/");
                sProjectURL.erase(tFound, sProjectURL.length());
            }

            entry.push_back(Pair("Project", sProjectName));
            entry.push_back(Pair("URL", sProjectURL));
            results.push_back(entry);
        }
    }
    else if (sitem == "network")
    {
        for(map<string,StructCPID>::iterator ii=mvNetwork.begin(); ii!=mvNetwork.end(); ++ii)
        {

            StructCPID stNet = mvNetwork[(*ii).first];

            if (stNet.initialized)
            {
                Object entry;
                entry.push_back(Pair("Project",stNet.projectname));
                entry.push_back(Pair("Avg RAC",stNet.AverageRAC));
                if (stNet.projectname=="NETWORK")
                {
                        entry.push_back(Pair("Network Total Magnitude",stNet.NetworkMagnitude));
                        entry.push_back(Pair("Network Average Magnitude",stNet.NetworkAvgMagnitude));
                        double MaximumEmission = BLOCKS_PER_DAY*GetMaximumBoincSubsidy(GetAdjustedTime());
                        entry.push_back(Pair("Network Avg Daily Payments", stNet.payments/14));
                        entry.push_back(Pair("Network Max Daily Payments",MaximumEmission));
                        entry.push_back(Pair("Network Interest Paid (14 days)", stNet.InterestSubsidy));
                        entry.push_back(Pair("Network Avg Daily Interest", stNet.InterestSubsidy/14));
                        double MoneySupply = DoubleFromAmount(pindexBest->nMoneySupply);
                        entry.push_back(Pair("Total Money Supply", MoneySupply));
                        double iPct = ( (stNet.InterestSubsidy/14) * 365 / (MoneySupply+.01));
                        entry.push_back(Pair("Network Interest %", iPct));
                        double magnitude_unit = GRCMagnitudeUnit(GetAdjustedTime());
                        entry.push_back(Pair("Magnitude Unit (GRC payment per Magnitude per day)", magnitude_unit));
                }
                results.push_back(entry);

            }
        }
        return results;
    }
    else if (sitem=="validcpids")
    {
        //Dump vectors:
        if (mvCPIDs.size() < 1)
        {
            HarvestCPIDs(false);
        }
        for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii)
        {

            StructCPID structcpid = mvCPIDs[(*ii).first];

            if (structcpid.initialized)
            {

                if (structcpid.cpid == GlobalCPUMiningCPID.cpid || !IsResearcher(structcpid.cpid))
                {
                    if (structcpid.verifiedteam=="gridcoin")
                    {
                        Object entry;
                        entry.push_back(Pair("Project",structcpid.projectname));
                        entry.push_back(Pair("CPID",structcpid.cpid));
                        entry.push_back(Pair("CPIDhash",structcpid.cpidhash));
                        entry.push_back(Pair("Email",structcpid.emailhash));
                        entry.push_back(Pair("UTC",structcpid.utc));
                        entry.push_back(Pair("RAC",structcpid.rac));
                        entry.push_back(Pair("Team",structcpid.team));
                        entry.push_back(Pair("RecTime",structcpid.rectime));
                        entry.push_back(Pair("Age",structcpid.age));
                        entry.push_back(Pair("Is my CPID Valid?",structcpid.Iscpidvalid));
                        entry.push_back(Pair("CPID Link",structcpid.link));
                        entry.push_back(Pair("Errors",structcpid.errors));
                        results.push_back(entry);
                    }
                }

            }
        }


    }
    else if (sitem=="cpids")
    {
        //Dump vectors:

        if (mvCPIDs.size() < 1)
        {
            HarvestCPIDs(false);
        }
        LogPrintf ("generating cpid report %s",sitem);

        for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii)
        {

            StructCPID structcpid = mvCPIDs[(*ii).first];

            if (structcpid.initialized)
            {

                if ((GlobalCPUMiningCPID.cpid.length() > 3 &&
                    structcpid.cpid == GlobalCPUMiningCPID.cpid)
                    || !IsResearcher(structcpid.cpid) || !IsResearcher(GlobalCPUMiningCPID.cpid))
                {
                    Object entry;
                    entry.push_back(Pair("Project",structcpid.projectname));
                    entry.push_back(Pair("CPID",structcpid.cpid));
                    entry.push_back(Pair("RAC",structcpid.rac));
                    entry.push_back(Pair("Team",structcpid.team));
                    entry.push_back(Pair("CPID Link",structcpid.link));
                    entry.push_back(Pair("Debug Info",structcpid.errors));
                    entry.push_back(Pair("Project Settings Valid for Gridcoin",structcpid.Iscpidvalid));
                    results.push_back(entry);
                }

            }
        }

    }
    else if (sitem == "help")
    {
        Object entry;
        entry.push_back(Pair("list cpids", "Displays information on cpids and the projects they are associated with"));
        entry.push_back(Pair("list currenttime", "Displays current unix time as well as UTC time and date"));
        entry.push_back(Pair("list explainmagnitude <true>", "Displays information about your magnitude from NN; Optional true to force response"));
        entry.push_back(Pair("list lifetime", "Displays information on the life time of your cpid"));
        entry.push_back(Pair("list magnitude <cpid>", "Displays information on magnitude. cpid is optional."));
        entry.push_back(Pair("list memorypool", "Displays information currently on Txs in memory pool"));
        entry.push_back(Pair("list network", "Displays detailed information on the network"));
        entry.push_back(Pair("list projects", "Displays information on whitelisted projects on the network"));
        entry.push_back(Pair("list rsa", "Displays information on your RSA/CPID history"));
        entry.push_back(Pair("list rsaweight", "Displays information on RSA Weight"));
        entry.push_back(Pair("list staking", "Displays information on your staking"));
        entry.push_back(Pair("list superblocks", "Displays information on superblocks over last 14 days. cpid optional"));
        entry.push_back(Pair("list validcpids", "Displays information on your valid cpid"));
        results.push_back(entry);
    }
    else
    {
        throw runtime_error("Item invalid.");
    }
    return results;

}



// ppcoin: get information of sync-checkpoint
Value getcheckpoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getcheckpoint\n"
                "Show info of synchronized checkpoint.\n");

    Object result;
    const CBlockIndex* pindexCheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
    if(pindexCheckpoint != NULL)
    {
        result.push_back(Pair("synccheckpoint", pindexCheckpoint->GetBlockHash().ToString().c_str()));
        result.push_back(Pair("height", pindexCheckpoint->nHeight));
        result.push_back(Pair("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str()));
    }

    return result;
}

//Brod
Value rpc_reorganize(const Array& params, bool fHelp)
{
    Object results;
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "reorganize <hash>\n"
            "Roll back the block chain to specified block hash.\n"
            "The block hash must already be present in block index");

    uint256 NewHash;
    NewHash.SetHex(params[0].get_str());

    bool fResult = ForceReorganizeToHash(NewHash);
    results.push_back(Pair("RollbackChain",fResult));
    return results;
}

// Brod
static bool compare_second(const pair<std::string, long>  &p1, const pair<std::string, long> &p2)
{
    return p1.second > p2.second;
}

Value rpc_getblockstats(const json_spirit::Array& params, bool fHelp)
{
    if(fHelp || params.size() < 1 || params.size() > 3 )
        throw runtime_error(
            "getblockstats mode [startheight [endheight]]\n"
            "Show stats on what wallets and cpids staked recent blocks.\n");
    long mode= params[0].get_int();
    (void)mode; //TODO
    long lowheight= 0;
    long highheight= INT_MAX;
    long maxblocks= 14000;
    if (mode==0)
    {
        if(params.size()>=2)
        {
            lowheight= params[1].get_int();
            maxblocks= INT_MAX;
        }
        if(params.size()>=3)
            highheight= params[2].get_int();
    }
    else if(mode==1)
    {
        /* count highheight */
        maxblocks= 30000;
        if(params.size()>=2)
            maxblocks= params[1].get_int();
        if(params.size()>=3)
            highheight= params[2].get_int();
    }
    else throw runtime_error("getblockstats: Invalid mode specified");
    CBlockIndex* cur;
    Object result1;
    {
        LOCK(cs_main);
        cur= pindexBest;
    }
    int64_t blockcount = 0;
    int64_t transactioncount = 0;
    std::map<int,long> c_blockversion;
    std::map<std::string,long> c_version;
    std::map<std::string,long> c_cpid;
    std::map<std::string,long> c_org;
    int64_t researchcount = 0;
    double researchtotal = 0;
    double interesttotal = 0;
    int64_t minttotal = 0;
    //int64_t stakeinputtotal = 0;
    int64_t poscount = 0;
    int64_t emptyblockscount = 0;
    int64_t l_first = INT_MAX;
    int64_t l_last = 0;
    unsigned int l_first_time = 0;
    unsigned int l_last_time = 0;
    unsigned size_min_blk=INT_MAX;
    unsigned size_max_blk=0;
    uint64_t size_sum_blk=0;
    double diff_sum = 0;
    double diff_max=0;
    double diff_min=INT_MAX;
    int64_t super_count = 0;
    for( ; (cur
            &&( cur->nHeight>=lowheight )
            &&( blockcount<maxblocks )
        );
        cur= cur->pprev
        )
    {
        if(cur->nHeight>highheight)
            continue;
        if(l_first>cur->nHeight)
        {
            l_first=cur->nHeight;
            l_first_time=cur->nTime;
        }
        if(l_last<cur->nHeight)
        {
            l_last=cur->nHeight;
            l_last_time=cur->nTime;
        }
        blockcount++;
        CBlock block;
        if(!block.ReadFromDisk(cur->nFile,cur->nBlockPos,true))
            throw runtime_error("failed to read block");
        assert(block.vtx.size() > 0);
        unsigned txcountinblock = 0;
        if(block.vtx.size()>=2)
        {
            txcountinblock+=block.vtx.size()-2;
            if(block.vtx[1].IsCoinStake())
            {
                poscount++;
                //stakeinputtotal+=block.vtx[1].vin[0].nValue;
                double diff = GetDifficulty(cur);
                diff_sum += diff;
                diff_max=std::max(diff_max,diff);
                diff_min=std::min(diff_min,diff);
            }
            else
                txcountinblock+=1;
        }
        transactioncount+=txcountinblock;
        emptyblockscount+=(txcountinblock==0);
        c_blockversion[block.nVersion]++;
        MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc, block.nVersion);
        c_cpid[bb.cpid]++;
        c_org[bb.Organization]++;
        c_version[bb.clientversion]++;
        researchtotal+=bb.ResearchSubsidy;
        interesttotal+=bb.InterestSubsidy;
        researchcount+=(bb.ResearchSubsidy>0.001);
        minttotal+=cur->nMint;
        unsigned sizeblock = block.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);
        size_min_blk=std::min(size_min_blk,sizeblock);
        size_max_blk=std::max(size_max_blk,sizeblock);
        size_sum_blk+=sizeblock;
        super_count += (bb.superblock.length()>20);
    }

    {
        Object result;
        result.push_back(Pair("blocks", blockcount));
        result.push_back(Pair("first_height", l_first));
        result.push_back(Pair("last_height", l_last));
        result.push_back(Pair("first_time", TimestampToHRDate(l_first_time)));
        result.push_back(Pair("last_time", TimestampToHRDate(l_last_time)));
        result.push_back(Pair("time_span_hour", ((double)l_last_time-(double)l_first_time)/(double)3600));
        result.push_back(Pair("min_blocksizek", size_min_blk/(double)1024));
        result.push_back(Pair("max_blocksizek", size_max_blk/(double)1024));
        result.push_back(Pair("min_posdiff", diff_min));
        result.push_back(Pair("max_posdiff", diff_max));
        result1.push_back(Pair("general", result));
    }
    {
        Object result;
        result.push_back(Pair("block", blockcount));
        result.push_back(Pair("empty_block", emptyblockscount));
        result.push_back(Pair("transaction", transactioncount));
        result.push_back(Pair("proof_of_stake", poscount));
        result.push_back(Pair("boincreward", researchcount));
        result.push_back(Pair("super", super_count));
        result1.push_back(Pair("counts", result));
    }
    {
        Object result;
        result.push_back(Pair("block", blockcount));
        result.push_back(Pair("research", researchtotal));
        result.push_back(Pair("interest", interesttotal));
        result.push_back(Pair("mint", minttotal/(double)COIN));
        //result.push_back(Pair("stake_input", stakeinputtotal/(double)COIN));
        result.push_back(Pair("blocksizek", size_sum_blk/(double)1024));
        result.push_back(Pair("posdiff", diff_sum));
        result1.push_back(Pair("totals", result));
    }
    {
        Object result;
        result.push_back(Pair("research", researchtotal/(double)researchcount));
        result.push_back(Pair("interest", interesttotal/(double)blockcount));
        result.push_back(Pair("mint", (minttotal/(double)blockcount)/(double)COIN));
        //result.push_back(Pair("stake_input", (stakeinputtotal/(double)poscount)/(double)COIN));
        result.push_back(Pair("spacing_sec", ((double)l_last_time-(double)l_first_time)/(double)blockcount));
        result.push_back(Pair("block_per_day", ((double)blockcount*86400.0)/((double)l_last_time-(double)l_first_time)));
        result.push_back(Pair("transaction", transactioncount/(double)(blockcount-emptyblockscount)));
        result.push_back(Pair("blocksizek", size_sum_blk/(double)blockcount/(double)1024));
        result.push_back(Pair("posdiff", diff_sum/(double)poscount));
        result.push_back(Pair("super_spacing_hrs", (((double)l_last_time-(double)l_first_time)/(double)super_count)/3600.0));
        result1.push_back(Pair("averages", result));
    }
    {
        Object result;
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_version.begin(), c_version.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        for (auto const& item : list)
        {
            result.push_back(Pair(item.first, item.second/(double)blockcount));
        }
        result1.push_back(Pair("versions", result));
    }
    {
        Object result;
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_cpid.begin(), c_cpid.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        int limit=64;
        for (auto const& item : list)
        {
            if(!(limit--)) break;
            result.push_back(Pair(item.first, item.second/(double)blockcount));
        }
        result1.push_back(Pair("cpids", result));
    }
    {
        Object result;
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_org.begin(), c_org.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        int limit=64;
        for (auto const& item : list)
        {
            if(!(limit--)) break;
            result.push_back(Pair(item.first, item.second/(double)blockcount));
        }
        result1.push_back(Pair("orgs", result));
    }
    return result1;
}
