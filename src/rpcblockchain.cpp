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
                "\n"
                "<index> Block number\n"
                "\n"
                "Returns all information about the block at <index>\n");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range\n");

    LOCK(cs_main);

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
                "\n"
                "Returns the hash of the best block in the longest block chain\n");

    LOCK(cs_main);

    return hashBestChain.GetHex();
}

Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getblockcount\n"
                "\n"
                "Returns the number of blocks in the longest block chain\n");

    LOCK(cs_main);

    return nBestHeight;
}

Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getdifficulty\n"
                "\n"
                "Returns the difficulty as a multiple of the minimum difficulty\n");

    LOCK(cs_main);

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
                "\n"
                "<amount> is a real and is rounded to the nearest 0.01\n"
                "\n"
                "Sets the txfee for transactions\n");

    LOCK(cs_main);

    nTransactionFee = AmountFromValue(params[0]);
    nTransactionFee = (nTransactionFee / CENT) * CENT;  // round to cent

    return true;
}

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getrawmempool\n"
                "\n"
                "Returns all transaction ids in memory pool\n");


    vector<uint256> vtxid;

    {
        LOCK(mempool.cs);

        mempool.queryHashes(vtxid);
    }

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
                "\n"
                "<index> Block number for requested hash\n"
                "\n"
                "Returns hash of block in best-block-chain at <index>\n");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");
    if (fDebug10)
        LogPrintf("Getblockhash %d", nHeight);

    LOCK(cs_main);

    CBlockIndex* RPCpblockindex = RPCBlockFinder.FindByHeight(nHeight);

    return RPCpblockindex->phashBlock->GetHex();
}

Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getblock <hash> [bool:txinfo]\n"
                "\n"
                "[bool:txinfo] optional to print more detailed tx info\n"
                "\n"
                "Returns details of a block with given block-hash\n");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    LOCK(cs_main);

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

Value getblockbynumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getblockbynumber <number> [bool:txinfo]\n"
                "\n"
                "[bool:txinfo] optional to print more detailed tx info\n"
                "\n"
                "Returns details of a block with given block-number\n");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range");

    LOCK(cs_main);

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
    params.push_back(arg1);
    params.push_back(RoundFromString(arg2, 0));
    params.push_back(arg3);
    params.push_back(arg4);
    params.push_back(RoundFromString(arg5, 0));
    params.push_back(arg6);

    LogPrintf("Executing method %s\n",method);
    Value vResult;
    try
    {
        if (method == "addpoll")
            vResult = addpoll(params, false);
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
    params.push_back(arg1);

    if (method == "vote")
        params.push_back(arg2);
    LogPrintf("Executing method %s\n",method);
    Value vResult;
    try
    {
        if (method == "vote")
            vResult = vote(params,false);

        if (method == "rain")
            vResult = rain(params,false);
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
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "backupprivatekeys\n"
                "\n"
                "Backup wallet private keys to file (Wallet must be fully unlocked!)\n");

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
                "<burnaddress> -> Address where the coins will be burned\n"
                "<burnamount>  -> Amount of coins to be burned\n"
                "<burnaddress> -> Burn key to be used\n"
                "<burndetails> -> Details of the burn\n"
                "\n"
                "Burn coins on the network\n");

    Object res;
    std::string sAddress = params[0].get_str();
    double dAmount  = Round(params[1].get_real(), 6);
    std::string sKey     = params[2].get_str();
    std::string sDetail  = params[3].get_str();
    CBitcoinAddress address(sAddress);
    bool isValid = address.IsValid();

    if (!isValid)
    {
        res.push_back(Pair("Error", "Invalid GRC Burn Address"));

        return res;
    }

    if (dAmount == 0 || dAmount < 0)
    {
        res.push_back(Pair("Error", "Burn amount must be > 0"));

        return res;
    }

    if (sKey.empty() || sDetail.empty())
    {
        res.push_back(Pair("Error", "Burn Key and Burn Detail must be populated"));

        return res;
    }

    std::string sContract = "<KEY>" + sKey + "</KEY><DETAIL>" + sDetail + "</DETAIL>";

    std::string sResult = BurnCoinsWithNewContract(true, "burn", sKey, sContract, AmountFromValue(1), dAmount, "", sAddress);

    res.push_back(Pair("Burn_Response", sResult));

    return res;
}

Value encrypt(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "encrypt <walletpassphrase>\n"
                "\n"
                "<walletpassphrase> -> The password of your encrypted wallet\n"
                "\n"
                "Encrypts a walletpassphrase\n");

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
                "[burntemplate] -> Allow a vanity burn address\n"
                "\n"
                "Creates a new burn address\n");

    Object res;

    //3-12-2016 - R Halford - Allow the user to make vanity GRC Burn Addresses that have no corresponding private key
    std::string sBurnTemplate = "GRCBurnAddressGRCBurnAddressGRCBurnAddress";

    if (params.size() > 0)
        sBurnTemplate = params[0].get_str();

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
    std::string encoded2 = EncodeBase58Check(vchDecoded30);

    res.push_back(Pair("CombinedHex",combined));

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
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "rain [Array]\n"
                "\n"
                "[Array] -> Address<COL>Amount<ROW>...\n"
                "\n"
                "rains coins on the network\n");

    Object res;

    CWalletTx wtx;
    wtx.mapValue["comment"] = "Rain";
    set<CBitcoinAddress> setAddress;
    vector<pair<CScript, int64_t> > vecSend;
    std::string sRecipients = params[0].get_str();
    std::string sRainCommand = ExtractXML(sRecipients,"<RAIN>","</RAIN>");
    std::string sRainMessage = MakeSafeMessage(ExtractXML(sRecipients,"<RAINMESSAGE>","</RAINMESSAGE>"));
    std::string sRain = "<NARR>Project Rain: " + sRainMessage + "</NARR>";

    if (!sRainCommand.empty())
        sRecipients = sRainCommand;

    wtx.hashBoinc = sRain;
    int64_t totalAmount = 0;
    double dTotalToSend = 0;
    std::vector<std::string> vRecipients = split(sRecipients.c_str(),"<ROW>");
    LogPrintf("Creating Rain transaction with %" PRId64 " recipients. ", vRecipients.size());

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
    LogPrintf("Transaction Created.");

    if (!fCreated)
    {
        if (totalAmount + nFeeRequired > pwalletMain->GetBalance())
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
    }

    LogPrintf("Committing.");
    // Rain the recipients
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
    {
        LogPrintf("Commit failed.");

        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");
    }
    std::string sNarr = "Rain successful:  Sent " + wtx.GetHash().GetHex() + ".";
    LogPrintf("Success %s",sNarr.c_str());

    res.push_back(Pair("Response", sNarr));

    return res;
}

Value unspentreport(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "unspentreport\n"
                "\n"
                "Displays unspentreport\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    Array aUnspentReport = GetJsonUnspentReport();

    return aUnspentReport;
}

Value advertisebeacon(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "advertisebeacon\n"
                "\n"
                "Advertise a beacon (Requires wallet to be fully unlocked)\n");

    Object res;

    std::string sOutPubKey = "";
    std::string sOutPrivKey = "";
    std::string sError = "";
    std::string sMessage = "";
    bool fResult = AdvertiseBeacon(sOutPrivKey,sOutPubKey,sError,sMessage);

    res.push_back(Pair("Result",SuccessFail(fResult)));
    res.push_back(Pair("CPID",GlobalCPUMiningCPID.cpid.c_str()));
    res.push_back(Pair("Message",sMessage.c_str()));

    if (!sError.empty())
        res.push_back(Pair("Errors",sError));

    if (!fResult)
        res.push_back(Pair("FAILURE","Note: if your wallet is locked this command will fail; to solve that unlock the wallet: 'walletpassphrase <yourpassword> <240>'."));

    else
    {
        res.push_back(Pair("Public Key",sOutPubKey.c_str()));
        res.push_back(Pair("Warning!","Your public and private research keys have been stored in gridcoinresearch.conf.  Do not lose your private key (It is non-recoverable).  It is recommended that you back up your gridcoinresearch.conf file on a regular basis."));
    }

    return res;
}

Value beaconreport(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "beaconreport\n"
                "\n"
                "Displays list of valid beacons in the network\n");

    LOCK(cs_main);

    Array res = GetJSONBeaconReport();

    return res;
}

Value beaconstatus(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "beaconstatus [cpid]\n"
                "\n"
                "[cpid] -> Optional parameter of cpid\n"
                "\n"
                "Displays status of your beacon or specified beacon on the network\n");

    Object res;

    // Search for beacon, and report on beacon status.

    std::string sCPID = msPrimaryCPID;

    if (params.size() > 0)
        sCPID = params[0].get_str();

    LOCK(cs_main);

    std::string sPubKey =  GetBeaconPublicKey(sCPID, false);
    std::string sPrivKey = GetStoredBeaconPrivateKey(sCPID);
    int64_t iBeaconTimestamp = BeaconTimeStamp(sCPID, false);
    std::string timestamp = TimestampToHRDate(iBeaconTimestamp);
    bool hasBeacon = HasActiveBeacon(sCPID);

    res.push_back(Pair("CPID", sCPID));
    res.push_back(Pair("Beacon Exists",YesNo(hasBeacon)));
    res.push_back(Pair("Beacon Timestamp",timestamp.c_str()));
    res.push_back(Pair("Public Key", sPubKey.c_str()));
    res.push_back(Pair("Private Key", sPrivKey.c_str()));

    std::string sErr = "";

    if (sPubKey.empty())
        sErr += "Public Key Missing. ";
    if (sPrivKey.empty())
        sErr += "Private Key Missing. ";

    // Verify the users Local Public Key matches the Beacon Public Key
    std::string sLocalPubKey = GetStoredBeaconPublicKey(sCPID);

    res.push_back(Pair("Local Configuration Public Key", sLocalPubKey.c_str()));

    if (sLocalPubKey.empty())
        sErr += "Local configuration file Public Key missing. ";

    if (sLocalPubKey != sPubKey && !sPubKey.empty())
        sErr += "Local configuration public key does not match beacon public key.  This can happen if you copied the wrong public key into your configuration file.  Please request that your beacon is deleted, or look into walletbackups for the correct keypair. ";

    // Prior superblock Magnitude
    double dMagnitude = GetMagnitudeByCpidFromLastSuperblock(sCPID);

    res.push_back(Pair("Magnitude (As of last superblock)", dMagnitude));

    if (dMagnitude==0)
        res.push_back(Pair("Warning","Your magnitude is 0 as of the last superblock: this may keep you from staking POR blocks."));

    // Staking Test 10-15-2016 - Simulate signing an actual block to verify this CPID keypair will work.
    uint256 hashBlock = GetRandHash();

    if (!sPubKey.empty())
    {
        bool bResult;
        std::string sSignature;
        std::string sError;

        bResult = SignBlockWithCPID(sCPID, hashBlock.GetHex(), sSignature, sError);

        if (!bResult)
        {
            sErr += "Failed to sign block with cpid ";
            sErr += sError;
            sErr += ";";
        }

        bool fResult = VerifyCPIDSignature(sCPID, hashBlock.GetHex(), sSignature);

        res.push_back(Pair("Block Signing Test Results", fResult));

        if (!fResult)
            sErr += "Failed to sign POR block.  This can happen if your keypair is invalid.  Check walletbackups for the correct keypair, or request that your beacon is deleted. ";
    }

    if (!sErr.empty())
    {
        res.push_back(Pair("Errors", sErr));
        res.push_back(Pair("Help", "Note: If your beacon is missing its public key, or is not in the chain, you may try: execute advertisebeacon."));
        res.push_back(Pair("Configuration Status","FAIL"));
    }

    else
        res.push_back(Pair("Configuration Status", "SUCCESSFUL"));

    return res;
}

Value cpids(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "cpids\n"
                "\n"
                "Displays information on your cpids\n");

    Object res;

    //Dump vectors:

    LOCK(cs_main);

    if (mvCPIDs.size() < 1)
        HarvestCPIDs(false);

    LogPrintf("generating cpid report\n");

    for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii)
    {

        StructCPID structcpid = mvCPIDs[(*ii).first];

        if (structcpid.initialized)
        {
            if ((GlobalCPUMiningCPID.cpid.length() > 3 &&
                 structcpid.cpid == GlobalCPUMiningCPID.cpid)
                || !IsResearcher(structcpid.cpid) || !IsResearcher(GlobalCPUMiningCPID.cpid))
            {
                res.push_back(Pair("Project",structcpid.projectname));
                res.push_back(Pair("CPID",structcpid.cpid));
                res.push_back(Pair("RAC",structcpid.rac));
                res.push_back(Pair("Team",structcpid.team));
                res.push_back(Pair("CPID Link",structcpid.link));
                res.push_back(Pair("Debug Info",structcpid.errors));
                res.push_back(Pair("Project Settings Valid for Gridcoin",structcpid.Iscpidvalid));

            }
        }
    }

    return res;
}

Value currentneuralhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currentneuralhash\n"
                "\n"
                "Displays information for the current popular neural hash in network\n");

    Object res;

    double popularity = 0;

    LOCK(cs_main);

    std::string consensus_hash = GetCurrentNeuralNetworkSupermajorityHash(popularity);

    res.push_back(Pair("Popular",consensus_hash));

    return res;
}

Value currentneuralreport(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currentneuralreport\n"
                "\n"
                "Displays information for the current neural hashes in network\n");

    LOCK(cs_main);

    Array res = GetJSONCurrentNeuralNetworkReport();

    return res;
}

Value explainmagnitude(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "explainmagnitude [bool:force]\n"
                "\n"
                "[force] -> Optional: Force a response (excessive requests can result in temporary ban from neural responses)\n"
                "\n"
                "Displays information for the current neural hashes in network\n");

    Object res;

    bool bForce = false;

    if (params.size() > 0)
        bForce = params[0].get_bool();

    LOCK(cs_main);

    if (bForce)
    {
        if (msNeuralResponse.length() < 25)
        {
            res.push_back(Pair("Neural Response", "Empty; Requesting a response.."));
            res.push_back(Pair("WARNING", "Only force once and try again without force if response is not received. Doing too many force attempts gets a temporary ban from neural node responses"));

            msNeuralResponse = "";

            AsyncNeuralRequest("explainmag", GlobalCPUMiningCPID.cpid, 10);
        }
    }

    if (msNeuralResponse.length() > 25)
    {
        res.push_back(Pair("Neural Response", "true"));

        std::vector<std::string> vMag = split(msNeuralResponse.c_str(),"<ROW>");

        for (unsigned int i = 0; i < vMag.size(); i++)
            res.push_back(Pair(RoundToString(i+1,0),vMag[i].c_str()));
    }

    else
        res.push_back(Pair("Neural Response", "false; Try again at a later time"));

    return res;
}

Value lifetime(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "lifttime\n"
                "\n"
                "Displays information for the lifetime of your cpid in the network\n");

    Array results;
    Object c;
    Object res;

    std::string cpid = msPrimaryCPID;
    std::string Narr = ToString(GetAdjustedTime());

    c.push_back(Pair("Lifetime Payments Report", Narr));
    results.push_back(c);

    LOCK(cs_main);

    CBlockIndex* pindex = pindexGenesisBlock;

    while (pindex->nHeight < pindexBest->nHeight)
    {
        pindex = pindex->pnext;

        if (pindex==NULL || !pindex->IsInMainChain())
            continue;

        if (pindex == pindexBest)
            break;

        if (pindex->GetCPID() == cpid && (pindex->nResearchSubsidy > 0))
            res.push_back(Pair(ToString(pindex->nHeight), RoundToString(pindex->nResearchSubsidy, 2)));
    }
    //8-14-2015
    StructCPID stCPID = GetInitializedStructCPID2(cpid, mvResearchAge);

    res.push_back(Pair("Average Magnitude", stCPID.ResearchAverageMagnitude));
    results.push_back(res);

    return results;
}

Value magnitude(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "magnitude <cpid>\n"
                "\n"
                "<cpid> -> Required to be used on mainnet\n"
                "\n"
                "Displays information for the magnitude of all cpids or specified in the network\n");

    Array results;

    std::string cpid = "";

    if (params.size() > 1)
        cpid = params[0].get_str();

    {
        LOCK(cs_main);

        results = MagnitudeReport(cpid);
    }

    if (results.size() > 1000)
    {
        results.clear();

        Object entry;

        entry.push_back(Pair("Error","Magnitude report too large; try specifying the cpid : magnitude <cpid>."));

        results.push_back(entry);
    }

    return results;
}

Value mymagnitude(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "mymagnitude\n"
                "\n"
                "Displays information for your magnitude in the network\n");

    Array results;

    if (msPrimaryCPID.empty())
    {
        Object res;

        res.push_back(Pair("Error", "Your CPID appears to be empty"));

        results.push_back(res);
    }

    else
    {
        LOCK(cs_main);

        results = MagnitudeReport(msPrimaryCPID);
    }
    return results;
}

#ifdef WIN32
Value myneuralhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "myneuralhash\n"
                "\n"
                "Displays information about your neural networks client current hash\n");

    Object res;

    std::string myNeuralHash = NN::GetNeuralHash();

    res.push_back(Pair("My Neural Hash", myNeuralHash.c_str()));

    return res;
}

Value neuralhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "neuralhash\n"
                "\n"
                "Displays information about the popular\n");

    Object res;

    double popularity = 0;
    std::string consensus_hash = GetNeuralNetworkSupermajorityHash(popularity);

    res.push_back(Pair("Popular", consensus_hash));

    return res;
}
#endif

Value neuralreport(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "neuralreport\n"
                "\n"
                "Displays neural report for the network\n");

    LOCK(cs_main);

    Array res = GetJSONNeuralNetworkReport();

    return res;
}

Value proveownership(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "proveownership\n"
                "\n"
                "Prove ownership of your CPID\n");

    Object res;

    LOCK(cs_main);

    HarvestCPIDs(true);
    GetNextProject(true);

    std::string email = GetArgument("email", "NA");
    boost::to_lower(email);
    std::string sLongCPID = ComputeCPIDv2(email, GlobalCPUMiningCPID.boincruntimepublickey,1);
    std::string sShortCPID = RetrieveMd5(GlobalCPUMiningCPID.boincruntimepublickey + email);
    std::string sEmailMD5 = RetrieveMd5(email);
    std::string sBPKMD5 = RetrieveMd5(GlobalCPUMiningCPID.boincruntimepublickey);

    res.push_back(Pair("Boinc E-Mail", email));
    res.push_back(Pair("Boinc Public Key", GlobalCPUMiningCPID.boincruntimepublickey));
    res.push_back(Pair("CPID", GlobalCPUMiningCPID.cpid));
    res.push_back(Pair("Computed Email Hash", sEmailMD5));
    res.push_back(Pair("Computed BPK", sBPKMD5));
    res.push_back(Pair("Computed CPID", sLongCPID));
    res.push_back(Pair("Computed Short CPID", sShortCPID));

    bool fResult = CPID_IsCPIDValid(sShortCPID, sLongCPID, 1);

    if (GlobalCPUMiningCPID.boincruntimepublickey.empty())
    {
        fResult = false;

        res.push_back(Pair("Error", "Boinc Public Key empty.  Try mounting your boinc project first, and ensure the gridcoin datadir setting is set if boinc is not in the default location."));
    }

    res.push_back(Pair("CPID Valid", fResult));

    return res;
}

Value resetcpids(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "resetcpids\n"
                "\n"
                "Reloads cpids\n");

    Object res;

    LOCK(cs_main);

    ReadConfigFile(mapArgs, mapMultiArgs);
    HarvestCPIDs(true);
    GetNextProject(true);
    res.push_back(Pair("Reset", 1));

    return res;
}

Value rsa(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "rsa\n"
                "\n"
                "Displays RSA report for your CPID\n");

    Array res;

    if (msPrimaryCPID.empty() || msPrimaryCPID == "INVESTOR")
        throw runtime_error(
                "PrimaryCPID is empty or INVESTOR; No RSA available for this condition\n");

    LOCK(cs_main);

    res = MagnitudeReport(msPrimaryCPID);

    return res;
}

Value rsaweight(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "rsaweight\n"
                "\n"
                "Display Rsaweight for your CPID\n");

    Object res;

    double out_magnitude = 0;
    double out_owed = 0;

    LOCK(cs_main);

    int64_t RSAWEIGHT = GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
    out_magnitude = GetUntrustedMagnitude(GlobalCPUMiningCPID.cpid, out_owed);

    res.push_back(Pair("RSA Weight", RSAWEIGHT));
    res.push_back(Pair("Magnitude", out_magnitude));
    res.push_back(Pair("RSA Owed", out_owed));

    return res;
}

Value staketime(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "staketime\n"
                "\n"
                "Display information about staking time\n");

    Object res;

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::string cpid = GlobalCPUMiningCPID.cpid;
    std::string GRCAddress = DefaultWalletAddress();
    GetEarliestStakeTime(GRCAddress, cpid);

    res.push_back(Pair("GRCTime", ReadCache("global", "nGRCTime").timestamp));
    res.push_back(Pair("CPIDTime", ReadCache("global", "nCPIDTime").timestamp));

    return res;
}

Value superblockage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "superblockage\n"
                "\n"
                "Display information regarding superblock age\n");

    Object res;

    int64_t superblock_time = ReadCache("superblock", "magnitudes").timestamp;
    int64_t superblock_age = GetAdjustedTime() - superblock_time;

    res.push_back(Pair("Superblock Age", superblock_age));
    res.push_back(Pair("Superblock Timestamp", TimestampToHRDate(superblock_time)));
    res.push_back(Pair("Superblock Block Number", ReadCache("superblock", "block_number").value));
    res.push_back(Pair("Pending Superblock Height", ReadCache("neuralsecurity", "pending").value));

    return res;
}

Value superblocks(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "superblocks [cpid]\n"
                "\n"
                "[cpid] -> Optional: Shows magnitude for a cpid for recent superblocks\n"
                "\n"
                "Display data on recent superblocks\n");

    Array res;

    std::string cpid = "";

    if (params.size() > 0)
        cpid = params[0].get_str();

    LOCK(cs_main);

    res = SuperblockReport(cpid);

    return res;
}

Value syncdpor2(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "syncdpor2\n"
                "\n"
                "Synchronize with the neural network\n");

    Object res;

    std::string sOut = "";

    LOCK(cs_main);

    bool bFull = GetCountOf("beacon") < 50 ? true : false;

    LoadAdminMessages(bFull, sOut);
    FullSyncWithDPORNodes();

    res.push_back(Pair("Syncing", 1));

    return res;
}

Value upgradedbeaconreport(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "upgradedbeaconreport\n"
                "\n"
                "Display upgraded beacon report of the network\n");

    LOCK(cs_main);

    Array aUpgBR = GetUpgradedBeaconReport();

    return aUpgBR;
}

Value validcpids(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "validcpids\n"
                "\n"
                "Displays information about valid CPIDs collected from BOINC\n");

    Array res;

    LOCK(cs_main);

    //Dump vectors:
    if (mvCPIDs.size() < 1)
        HarvestCPIDs(false);

    for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii)
    {
        StructCPID structcpid = mvCPIDs[(*ii).first];

        if (structcpid.initialized)
        {
            if (structcpid.cpid == GlobalCPUMiningCPID.cpid || !IsResearcher(structcpid.cpid))
            {
                if (structcpid.verifiedteam == "gridcoin")
                {
                    Object entry;

                    entry.push_back(Pair("Project", structcpid.projectname));
                    entry.push_back(Pair("CPID", structcpid.cpid));
                    entry.push_back(Pair("CPIDhash", structcpid.cpidhash));
                    entry.push_back(Pair("Email", structcpid.emailhash));
                    entry.push_back(Pair("UTC", structcpid.utc));
                    entry.push_back(Pair("RAC", structcpid.rac));
                    entry.push_back(Pair("Team", structcpid.team));
                    entry.push_back(Pair("RecTime", structcpid.rectime));
                    entry.push_back(Pair("Age", structcpid.age));
                    entry.push_back(Pair("Is my CPID Valid?", structcpid.Iscpidvalid));
                    entry.push_back(Pair("CPID Link", structcpid.link));
                    entry.push_back(Pair("Errors", structcpid.errors));

                    res.push_back(entry);
                }
            }
        }
    }

    return res;
}

Value addkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
                "addkey <action> <keytype> <keyname> <keyvalue>\n"
                "\n"
                "<action> ---> Specify add or delete of key\n"
                "<keytype> --> Specify keytype ex: project\n"
                "<keyname> --> Specify keyname ex: milky\n"
                "<keyvalue> -> Specify keyvalue ex: 1\n"
                "\n"
                "Add a key to the network\n");

    Object res;

    //To whitelist a project:
    //execute addkey add project projectname 1
    //To blacklist a project:
    //execute addkey delete project projectname 1
    //Key examples:

    //execute addkey add project milky 1
    //execute addkey delete project milky 1
    //execute addkey add project milky 2
    //execute addkey add price grc .0000046
    //execute addkey add price grc .0000045
    //execute addkey delete price grc .0000045
    //GRC will only memorize the *last* value it finds for a key in the highest block
    //execute memorizekeys
    //execute listdata project
    //execute listdata price
    //execute addkey add project grid20

    std::string sAction = params[0].get_str();
    bool bAdd = (sAction == "add") ? true : false;
    std::string sType = params[1].get_str();
    std::string sPass = "";
    std::string sName = params[2].get_str();
    std::string sValue = params[3].get_str();

    sPass = (sType == "project" || sType == "projectmapping" || (sType == "beacon" && sAction == "delete")) ? GetArgument("masterprojectkey", msMasterMessagePrivateKey) : msMasterMessagePrivateKey;

    res.push_back(Pair("Action", sAction));
    res.push_back(Pair("Type", sType));
    res.push_back(Pair("Passphrase", sPass));
    res.push_back(Pair("Name", sName));
    res.push_back(Pair("Value", sValue));

    std::string result = AddMessage(bAdd, sType, sName, sValue, sPass, AmountFromValue(5), .1, "");

    res.push_back(Pair("Results", result));

    return res;
}

#ifdef WIN32
Value currentcontractaverage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currentcontractaverage\n"
                "\n"
                "Displays information on your current contract average with regards to superblock contract\n");

    Object res;

    std::string contract = NN::GetNeuralContract();
    double out_beacon_count = 0;
    double out_participant_count = 0;
    double out_avg = 0;
    double avg = GetSuperblockAvgMag(contract, out_beacon_count, out_participant_count, out_avg, false, nBestHeight);
    bool bValid = VerifySuperblock(contract, pindexBest);
    //Show current contract neural hash
    std::string sNeuralHash = NN::GetNeuralHash();
    std::string neural_hash = GetQuorumHash(contract);

    res.push_back(Pair("Contract", contract));
    res.push_back(Pair("avg", avg));
    res.push_back(Pair("beacon_count", out_beacon_count));
    res.push_back(Pair("avg_mag", out_avg));
    res.push_back(Pair("beacon_participant_count", out_participant_count));
    res.push_back(Pair("superblock_valid", bValid));
    res.push_back(Pair(".NET Neural Hash", sNeuralHash.c_str()));
    res.push_back(Pair("Length", (int)contract.length()));
    res.push_back(Pair("Wallet Neural Hash", neural_hash));

    return res;
}
#endif

Value debug(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable debug mode on the fly\n");

    Object res;

    fDebug = params[0].get_bool();

    res.push_back(Pair("Debug", fDebug ? "Entering debug mode." : "Exiting debug mode."));

    return res;
}

Value debug10(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug10 <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "Enable or disable debug mode on the fly\n");

    Object res;

    fDebug10 = params[0].get_bool();

    res.push_back(Pair("Debug10", fDebug10 ? "Entering debug mode." : "Exiting debug mode."));

    return res;
}

Value debug2(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug2 <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable debug mode on the fly\n");

    Object res;

    fDebug2 = params[0].get_bool();

    res.push_back(Pair("Debug2", fDebug2 ? "Entering debug mode." : "Exiting debug mode."));

    return res;
}

Value debug3(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug3 <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable debug mode on the fly\n");

    Object res;

    fDebug3 = params[0].get_bool();

    res.push_back(Pair("Debug3", fDebug3 ? "Entering debug mode." : "Exiting debug mode."));

    return res;
}

Value debug4(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug4 <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable debug mode on the fly\n");

    Object res;

    fDebug4 = params[0].get_bool();

    res.push_back(Pair("Debug4", fDebug4 ? "Entering debug mode." : "Exiting debug mode."));

    return res;
}
Value debugnet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debugnet <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable debug mode on the fly\n");

    Object res;

    fDebugNet = params[0].get_bool();

    res.push_back(Pair("DebugNet", fDebugNet ? "Entering debug mode." : "Exiting debug mode."));

    return res;
}

Value dportally(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "dporttally\n"
                "\n"
                "Request a tally of DPOR in superblock\n");

    Object res;

    LOCK(cs_main);

    TallyMagnitudesInSuperblock();

    res.push_back(Pair("Done", "Done"));

    return res;
}

Value forcequorom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "forcequorom\n"
                "\n"
                "Requests neural network for force a quorom among nodes\n");

    Object res;

    AsyncNeuralRequest("quorum", "gridcoin", 10);

    res.push_back(Pair("Requested a quorum - waiting for resolution.", 1));

    return res;
}

Value gatherneuralhashes(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "gatherneuralhashes\n"
                "\n"
                "Requests neural network to gather neural hashes\n");

    Object res;

    LOCK(cs_main);

    GatherNeuralHashes();

    res.push_back(Pair("Sent", "."));

    return res;
}

Value genboinckey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "genboinckey\n"
                "\n"
                "Generates a boinc key\n");

    Object res;

    //Gridcoin - R Halford - Generate Boinc Mining Key - 2-6-2015
    GetNextProject(false);
    std::string email = GetArgument("email", "NA");
    boost::to_lower(email);
    GlobalCPUMiningCPID.email = email;
    GlobalCPUMiningCPID.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, 0);
    //Store the BPK in the aesskein, and the cpid in version
    GlobalCPUMiningCPID.aesskein = email; //Good
    GlobalCPUMiningCPID.lastblockhash = GlobalCPUMiningCPID.cpidhash;

    //block version not needed for keys for now
    std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID,7);
    std::string sBase = EncodeBase64(sParam);

    if (fDebug3)
        LogPrintf("GenBoincKey: Utilizing email %s with %s for %s\r\n", GlobalCPUMiningCPID.email.c_str(), GlobalCPUMiningCPID.boincruntimepublickey.c_str(), sParam.c_str());

    res.push_back(Pair("[Specify in config file without quotes] boinckey=", sBase));

    return res;
}

Value getlistof(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getlistof <keytype>\n"
                "\n"
                "<keytype> -> key of requested data\n"
                "\n"
                "Displays data associated to a specified key type\n");

    Object res;

    std::string sType = params[0].get_str();

    res.push_back(Pair("Key Type", sType));

    LOCK(cs_main);

    res.push_back(Pair("Data", GetListOf(sType)));

    return res;
}

Value getnextproject(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getnextproject\n"
                "\n"
                "Requests wallet to get next project\n");

    Object res;

    LOCK(cs_main);

    GetNextProject(true);

    res.push_back(Pair("GetNext", 1));

    return res;
}

Value listdata(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "listdata <keytype>\n"
                "\n"
                "<keytype> -> key in cache\n"
                "\n"
                "Displays data associated to a key stored in cache\n");

    Object res;

    std::string sType = params[0].get_str();

    res.push_back(Pair("Key Type", sType));

    LOCK(cs_main);

    for(const auto& item : ReadCacheSection(sType))
        res.push_back(Pair(item.first, item.second.value));

    return res;
}

Value memorizekeys(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "memorizekeys\n"
                "\n"
                "Runs a full table scan of Load Admin Messages\n");

    Object res;

    std::string sOut;

    LOCK(cs_main);

    LoadAdminMessages(true, sOut);

    res.push_back(Pair("Results", sOut));

    return res;
}

Value network(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "network\n"
                "\n"
                "Display information about the network health\n");

    Array res;

    LOCK(cs_main);

    for(map<string,StructCPID>::iterator ii=mvNetwork.begin(); ii!=mvNetwork.end(); ++ii)
    {
        StructCPID stNet = mvNetwork[(*ii).first];

        if (stNet.initialized)
        {
            Object results;

            results.push_back(Pair("Project", stNet.projectname));
            results.push_back(Pair("Avg RAC", stNet.AverageRAC));

            if (stNet.projectname == "NETWORK")
            {
                double MaximumEmission = BLOCKS_PER_DAY*GetMaximumBoincSubsidy(GetAdjustedTime());
                double MoneySupply = DoubleFromAmount(pindexBest->nMoneySupply);
                double iPct = ( (stNet.InterestSubsidy/14) * 365 / (MoneySupply+.01));
                double magnitude_unit = GRCMagnitudeUnit(GetAdjustedTime());

                results.push_back(Pair("Network Total Magnitude", stNet.NetworkMagnitude));
                results.push_back(Pair("Network Average Magnitude", stNet.NetworkAvgMagnitude));
                results.push_back(Pair("Network Avg Daily Payments", stNet.payments/14));
                results.push_back(Pair("Network Max Daily Payments", MaximumEmission));
                results.push_back(Pair("Network Interest Paid (14 days)", stNet.InterestSubsidy));
                results.push_back(Pair("Network Avg Daily Interest", stNet.InterestSubsidy/14));
                results.push_back(Pair("Total Money Supply", MoneySupply));
                results.push_back(Pair("Network Interest %", iPct));
                results.push_back(Pair("Magnitude Unit (GRC payment per Magnitude per day)", magnitude_unit));
            }

            res.push_back(results);
        }
    }

    return res;
}

Value neuralrequest(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "neuralrequest\n"
                "\n"
                "Sends a request to neural network and displays response\n");

    Object res;

    std::string response = NeuralRequest("REQUEST");

    res.push_back(Pair("Response", response));

    return res;
}

Value projects(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "projects\n"
                "\n"
                "Displays information on projects in the network\n");

    Array res;

    LOCK(cs_main);

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

        res.push_back(entry);
    }

    return res;
}

Value readconfig(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "readconfig\n"
                "\n"
                "Re-reads config file; Does not overwrite pre-existing loaded values\n");

    Object res;

    LOCK(cs_main);

    ReadConfigFile(mapArgs, mapMultiArgs);

    res.push_back(Pair("readconfig", 1));

    return res;
}

Value readdata(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "readdata <key>\n"
                "\n"
                "<key> -> generic key\n"
                "\n"
                "Reads generic data from disk from a specified key\n");

    Object res;

    std::string sKey = params[0].get_str();
    std::string sValue = "?";

    //CTxDB txdb("cr");
    CTxDB txdb;

    if (!txdb.ReadGenericData(sKey, sValue))
    {
        res.push_back(Pair("Error", sValue));

        sValue = "Failed to read from disk.";
    }

    res.push_back(Pair("Key", sKey));
    res.push_back(Pair("Result", sValue));

    return res;
}

Value seefile(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "seefile\n"
                "\n"
                "Unit test for sending a file from node to node\n");

    Object res;

    // This is a unit test to prove viability of transmitting a file from node to node
    std::string sFile = "C:\\test.txt";
    std::vector<unsigned char> v = readFileToVector(sFile);

    res.push_back(Pair("byte1", v[1]));
    res.push_back(Pair("bytes", (int)v.size()));

    for (unsigned int i = 0; i < v.size(); i++)
        res.push_back(Pair("bytes", v[i]));

    std::string sManifest = FileManifest();

    res.push_back(Pair("manifest", sManifest));

    return res;
}

#ifdef WIN32
Value refhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "refhash <walletaddress>\n"
                "\n"
                "<walletaddress> -> GRC address to test against\n"
                "\n"
                "Tests to see if a GRC Address is a participant in neural network along with default wallet address\n");

    Object res;

    std::string rh = params[0].get_str();
    bool r1 = StrLessThanReferenceHash(rh);
    bool r2 = IsNeuralNodeParticipant(DefaultWalletAddress(), GetAdjustedTime());

    res.push_back(Pair("<Ref Hash", r1));

    res.push_back(Pair("WalletAddress<Ref Hash", r2));

    return res;
}
#endif

Value sendblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "sendblock <blockhash>\n"
                "\n"
                "<blockhash> Blockhash of block to send to network\n"
                "\n"
                "Sends a block to the network\n");

    Object res;

    std::string sHash = params[0].get_str();
    uint256 hash = uint256(sHash);
    bool fResult = AskForOutstandingBlocks(hash);

    res.push_back(Pair("Requesting", hash.ToString()));
    res.push_back(Pair("Result", fResult));

    return res;
}

Value sendrawcontract(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "sendrawcontract <contract>\n"
                "\n"
                "<contract> -> custom contract\n"
                "\n"
                "Send a raw contract in a transaction on the network\n");

    Object res;

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    std::string sAddress = GetBurnAddress();

    CBitcoinAddress address(sAddress);

    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gridcoin address");

    std::string sContract = params[0].get_str();
    int64_t nAmount = CENT;
    // Wallet comments
    CWalletTx wtx;
    wtx.hashBoinc = sContract;

    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, wtx, false);

    if (!strError.empty())
        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    res.push_back(Pair("Contract", sContract));
    res.push_back(Pair("Recipient", sAddress));
    res.push_back(Pair("TrxID", wtx.GetHash().GetHex()));

    return res;
}

Value superblockaverage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "superblockaverage\n"
                "\n"
                "Displays average information for current superblock\n");

    Object res;

    LOCK(cs_main);

    std::string superblock = ReadCache("superblock", "all").value;
    double out_beacon_count = 0;
    double out_participant_count = 0;
    double out_avg = 0;
    double avg = GetSuperblockAvgMag(superblock, out_beacon_count, out_participant_count, out_avg, false, nBestHeight);
    int64_t superblock_age = GetAdjustedTime() - ReadCache("superblock", "magnitudes").timestamp;
    bool bDireNeed = NeedASuperblock();

    res.push_back(Pair("avg", avg));
    res.push_back(Pair("beacon_count", out_beacon_count));
    res.push_back(Pair("beacon_participant_count", out_participant_count));
    res.push_back(Pair("average_magnitude", out_avg));
    res.push_back(Pair("superblock_valid", VerifySuperblock(superblock, pindexBest)));
    res.push_back(Pair("Superblock Age", superblock_age));
    res.push_back(Pair("Dire Need of Superblock", bDireNeed));

    return res;
}

Value tally(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "tally\n"
                "\n"
                "Requests a tally of research averages\n");

    Object res;

    LOCK(cs_main);

    bNetAveragesLoaded_retired = false;
    CBlockIndex* tallyIndex = FindTallyTrigger(pindexBest);
    TallyResearchAverages_v9(tallyIndex);

    res.push_back(Pair("Tally Network Averages", 1));

    return res;
}

Value tallyneural(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "tallyneural\n"
                "\n"
                "Requests a tally of neural network\n");

    Object res;

    LOCK(cs_main);

    ComputeNeuralNetworkSupermajorityHashes();
    UpdateNeuralNetworkQuorumData();

    res.push_back(Pair("Ready", "."));

    return res;
}

#ifdef WIN32
Value testnewcontract(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "testnewcontract\n"
                "\n"
                "Tests current local neural contract\n");

    Object res;

    std::string contract = NN::GetNeuralContract();
    std::string myNeuralHash = NN::GetNeuralHash();
    // Convert to Binary
    std::string sBin = PackBinarySuperblock(contract);
    // Hash of current superblock
    std::string sUnpacked = UnpackBinarySuperblock(sBin);
    std::string neural_hash = GetQuorumHash(contract);
    std::string binary_neural_hash = GetQuorumHash(sUnpacked);

    res.push_back(Pair("My Neural Hash", myNeuralHash));
    res.push_back(Pair("Contract Test", contract));
    res.push_back(Pair("Contract Length", (int)contract.length()));
    res.push_back(Pair("Binary Length", (int)sBin.length()));
    res.push_back(Pair("Unpacked length", (int)sUnpacked.length()));
    res.push_back(Pair("Unpacked", sUnpacked));
    res.push_back(Pair("Local Core Quorum Hash", neural_hash));
    res.push_back(Pair("Binary Local Core Quorum Hash", binary_neural_hash));
    res.push_back(Pair("Neural Network Live Quorum Hash", myNeuralHash));

    return res;
}
#endif

Value unusual(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "unusual\n"
                "\n"
                "Displays report on unusual network activity in debug.log\n");

    Object res;

    LOCK(cs_main);

    UnusualActivityReport();

    res.push_back(Pair("UAR", 1));

    return res;
}

Value updatequoromdata(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "updatequoromdata\n"
                "\n"
                "Requests update of neural network quorom data\n");

    Object res;

    UpdateNeuralNetworkQuorumData();

    res.push_back(Pair("Updated.", ""));

    return res;
}

Value versionreport(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "versionreport\n"
                "\n"
                "Displays a report on various versions recently stake on the network\n");

    LOCK(cs_main);

    Array myNeuralJSON = GetJSONVersionReport();

    return myNeuralJSON;
}

Value writedata(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "writedata <key> <value>\n"
                "\n"
                "<key> ---> Key where value will be written\n"
                "<value> -> Value to be written to specified key\n"
                "\n"
                "Writes a value to specified key\n");

    Object res;

    std::string sKey = params[0].get_str();
    std::string sValue = params[1].get_str();
    //CTxDB txdb("rw");
    CTxDB txdb;
    txdb.TxnBegin();
    std::string result = "Success.";

    if (!txdb.WriteGenericData(sKey, sValue))
        result = "Unable to write.";

    if (!txdb.TxnCommit())
        result = "Unable to Commit.";

    res.push_back(Pair("Result", result));

    return res;
}

// Network RPC commands

Value addpoll(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
        throw runtime_error(
                "addpoll <title> <days> <question> <answers> <sharetype> <url>\n"
                "\n"
                "<title> -----> The title for poll with no spaces. Use _ in between words\n"
                "<days> ------> The number of days the poll will run\n"
                "<question> --> The question with no spaces. Use _ in between words\n"
                "<answers> ---> The answers available for voter to choose from. Use - in between words and ; to separate answers\n"
                "<sharetype> -> The share type of the poll; 1 = Magnitude 2 = Balance 3 = Magnitude + Balance 4 = CPID count 5 = Participant count\n"
                "<url> -------> The corresponding url for the poll\n"
                "\n"
                "Add a poll to the network; Requires 100K GRC balance\n");

    Object res;

    std::string Title = params[0].get_str();
    double days = Round(params[1].get_real(), 0);
    std::string Question = params[2].get_str();
    std::string Answers = params[3].get_str();
    double sharetype = Round(params[4].get_real(), 0);
    std::string sURL = params[5].get_str();

    if (Title.empty() || Question.empty() || Answers.empty() || sURL.empty())
    {
        res.push_back(Pair("Error", "Must specify a poll title, question, answers, and URL\n"));

        return res;
    }

    if (days < 7)
        res.push_back(Pair("Error", "Minimum duration is 7 days; please specify a longer poll duration."));

    else
    {
        double nBalance = GetTotalBalance();

        if (nBalance < 100000)
            res.push_back(Pair("Error", "You must have a balance > 100,000 GRC to create a poll.  Please post the desired poll on https://cryptocurrencytalk.com/forum/464-gridcoin-grc/ or https://github.com/Erkan-Yilmaz/Gridcoin-tasks/issues/45"));

        else
        {
            if (days < 0 || days == 0)
                res.push_back(Pair("Error", "You must specify a positive value for days for the expiration date."));

            else
            {
                if (sharetype != 1 && sharetype != 2 && sharetype != 3 && sharetype != 4 && sharetype != 5)
                    res.push_back(Pair("Error", "You must specify a value of 1, 2, 3, 4 or 5 for the sharetype."));

                else
                {
                    std::string expiration = RoundToString(GetAdjustedTime() + (days*86400), 0);
                    std::string contract = "<TITLE>" + Title + "</TITLE><DAYS>" + RoundToString(days, 0) + "</DAYS><QUESTION>" + Question + "</QUESTION><ANSWERS>" + Answers + "</ANSWERS><SHARETYPE>" + ToString(sharetype) + "</SHARETYPE><URL>" + sURL + "</URL><EXPIRATION>" + expiration + "</EXPIRATION>";
                    std::string result = AddContract("poll", Title,contract);

                    res.push_back(Pair("Success", "Your poll has been added: " + result));
                }
            }
        }
    }

    return res;
}

Value askforoutstandingblocks(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "askforoutstandingblocks\n"
                "\n"
                "Requests network for outstanding blocks\n");

    Object res;

    bool fResult = AskForOutstandingBlocks(uint256(0));

    res.push_back(Pair("Sent.", fResult));

    return res;
}

Value getblockchaininfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getblockchaininfo\n"
                "\n"
                "Displays data on current blockchain\n");

    LOCK(cs_main);

    Object res, diff;

    res.push_back(Pair("blocks",          nBestHeight));
    res.push_back(Pair("moneysupply",     ValueFromAmount(pindexBest->nMoneySupply)));
    diff.push_back(Pair("proof-of-work",  GetDifficulty()));
    diff.push_back(Pair("proof-of-stake", GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    res.push_back(Pair("difficulty",      diff));
    res.push_back(Pair("testnet",         fTestNet));
    res.push_back(Pair("errors",          GetWarnings("statusbar")));

    return res;
}


Value currenttime(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currenttime\n"
                "\n"
                "Displays UTC unix time as well as date and time in UTC\n");

    Object res;

    res.push_back(Pair("Unix", GetAdjustedTime()));
    res.push_back(Pair("UTC", TimestampToHRDate(GetAdjustedTime())));

    return res;
}

Value decryptphrase(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "decryptphrase <phrase>\n"
                "\n"
                "<phrase> -> Encrypted phrase to decrypt\n"
                "\n"
                "Decrypts an encrypted phrase\n");

    Object res;

    std::string sParam1 = params[0].get_str();
    std::string test = AdvancedDecrypt(sParam1);

    res.push_back(Pair("Phrase", sParam1));
    res.push_back(Pair("Decrypted Phrase", test));

    return res;
}

/*
#ifdef WIN32
Value downloadblocks(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "downloadblocks\n"
                "\n"
                "Download blocks from a snapshot\n");

    Object res;

    int r = Restarter::DownloadGridcoinBlocks();

    res.push_back(Pair("Download Blocks", r));

    return res;
}
#endif
*/

Value encryptphrase(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "encryptphrase <phrase>\n"
                "\n"
                "<phrase> Phase you wish to encrypt\n"
                "\n"
                "Encrypt a phrase\n");

    Object res;

    std::string sParam1 = params[0].get_str();
    std::string test = AdvancedCrypt(sParam1);

    res.push_back(Pair("Phrase", sParam1));
    res.push_back(Pair("Encrypted Phrase", test));

    return res;
}

Value listallpolls(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listallpolls\n"
                "\n"
                "Lists all polls\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(false, "", out1, true);

    return res;
}

Value listallpolldetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listallpolldetails\n"
                "\n"
                "Lists all polls with details\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(true, "", out1, true);

    return res;
}

Value listpolldetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listpolldetails\n"
                "\n"
                "Lists poll details\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(true, "", out1, false);

    return res;
}

Value listpollresults(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2 || params.size() < 1)
        throw runtime_error(
                "listpollresults <pollname> [bool:showexpired]\n"
                "\n"
                "<pollname> ----> name of the poll\n"
                "[showexpired] -> Optional; Default false\n"
                "\n"
                "Displays results for specified poll\n");

    LOCK(cs_main);

    Array res;
    bool bIncExpired = false;

    if (params.size() == 2)
        bIncExpired = params[1].get_bool();

    std::string Title1 = params[0].get_str();

    if (!PollExists(Title1))
    {
        Object result;

        result.push_back(Pair("Error", "Poll does not exist.  Please listpolls."));
        res.push_back(result);
    }
    else
    {
        std::string Title = params[0].get_str();
        std::string out1 = "";
        Array myPolls = GetJSONPollsReport(true, Title, out1, bIncExpired);
        res.push_back(myPolls);
    }

    return res;
}

Value listpolls(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listpolls\n"
                "\n"
                "Lists polls\n");

    LOCK(cs_main);

    std::string out1;
    Array res = GetJSONPollsReport(false, "", out1, false);

    return res;
}

Value memorypool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "memorypool\n"
                "\n"
                "Displays included and excluded memory pool txs\n");

    Object res;

    res.push_back(Pair("Excluded Tx", msMiningErrorsExcluded));
    res.push_back(Pair("Included Tx", msMiningErrorsIncluded));

    return res;
}

Value networktime(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "networktime\n"
                "\n"
                "Displays current network time\n");

    Object res;

    res.push_back(Pair("Network Time", GetAdjustedTime()));

    return res;
}

#ifdef WIN32
Value reindex(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "reindex\n"
                "\n"
                "Re-index the block chain\n");

    Object res;

    int r = Restarter::CreateGridcoinRestorePoint();
    Restarter::ReindexGridcoinWallet();
    res.push_back(Pair("Reindex Chain", r));

    return res;
}

Value restart(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "restart\n"
                "\n"
                "Restarts the wallet\n");

    Object res;

    LogPrintf("Restarting Gridcoin...");
    int iResult = Restarter::RestartGridcoin();
    res.push_back(Pair("RebootClient", iResult));

    return res;
}

Value restorepoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "restorepoint\n"
                "\n"
                "Create a restore point\n");

    Object res;

    int r= Restarter::CreateGridcoinRestorePoint();
    //We must stop the node before we can do this
    //RestartGridcoin();
    res.push_back(Pair("Restore Point", r));

    return res;
}
#endif

Value vote(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "vote <title> <answers>\n"
                "\n"
                "<title -> Title of poll being voted on\n"
                "<answers> -> Answers chosen for specified poll separated by ;\n"
                "\n"
                "Vote on a specific poll with specified answers\n");

    Object res;

    std::string Title = params[0].get_str();
    std::string Answer = params[1].get_str();

    if (Title.empty() || Answer.empty())
    {
        res.push_back(Pair("Error", "Must specify a poll title and answers\n"));

        return res;
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    //Verify the Existence of the poll, the acceptability of the answer, and the expiration of the poll: (EXIST, EXPIRED, ACCEPTABLE)
    //If polltype == 1, use magnitude, if 2 use Balance, if 3 use hybrid:
    //6-20-2015
    double nBalance = GetTotalBalance();
    uint256 hashRand = GetRandHash();
    std::string email = GetArgument("email", "NA");
    boost::to_lower(email);
    GlobalCPUMiningCPID.email = email;
    GlobalCPUMiningCPID.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, hashRand);
    GlobalCPUMiningCPID.lastblockhash = GlobalCPUMiningCPID.cpidhash;

    if (!PollExists(Title))
        res.push_back(Pair("Error", "Poll does not exist."));

    else
    {
        if (PollExpired(Title))
            res.push_back(Pair("Error", "Sorry, Poll is already expired."));

        else
        {
            if (!PollAcceptableAnswer(Title, Answer))
            {
                std::string acceptable_answers = PollAnswers(Title);
                res.push_back(Pair("Error", "Sorry, Answer " + Answer + " is not one of the acceptable answers, allowable answers are: " + acceptable_answers + ".  If you are voting multiple choice, please use a semicolon delimited vote string such as : 'dog;cat'."));
            }

            else
            {
                std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID, pindexBest->nVersion);
                std::string GRCAddress = DefaultWalletAddress();
                StructCPID structMag = GetInitializedStructCPID2(GlobalCPUMiningCPID.cpid, mvMagnitudes);
                double dmag = structMag.Magnitude;
                double poll_duration = PollDuration(Title) * 86400;

                // Prevent Double Voting
                std::string cpid1 = GlobalCPUMiningCPID.cpid;
                std::string GRCAddress1 = DefaultWalletAddress();
                GetEarliestStakeTime(GRCAddress1, cpid1);
                double cpid_age = GetAdjustedTime() - ReadCache("global", "nCPIDTime").timestamp;
                double stake_age = GetAdjustedTime() - ReadCache("global", "nGRCTime").timestamp;

                StructCPID structGRC = GetInitializedStructCPID2(GRCAddress, mvMagnitudes);

                LogPrintf("CPIDAge %f, StakeAge %f, Poll Duration %f \r\n", cpid_age, stake_age, poll_duration);

                double dShareType= RoundFromString(GetPollXMLElementByPollTitle(Title, "<SHARETYPE>", "</SHARETYPE>"), 0);

                // Share Type 1 == "Magnitude"
                // Share Type 2 == "Balance"
                // Share Type 3 == "Both"
                if (cpid_age < poll_duration) dmag = 0;

                if (stake_age < poll_duration) nBalance = 0;

                if ((dShareType == 1) && cpid_age < poll_duration)
                    res.push_back(Pair("Error", "Sorry, When voting in a magnitude poll, your CPID must be older than the poll duration."));

                else if (dShareType == 2 && stake_age < poll_duration)
                    res.push_back(Pair("Error", "Sorry, When voting in a Balance poll, your stake age must be older than the poll duration."));

                else if (dShareType == 3 && stake_age < poll_duration && cpid_age < poll_duration)
                    res.push_back(Pair("Error", "Sorry, When voting in a Both Share Type poll, your stake age Or your CPID age must be older than the poll duration."));

                else
                {
                    std::string voter = "<CPIDV2>"+GlobalCPUMiningCPID.cpidv2 + "</CPIDV2><CPID>"
                                        + GlobalCPUMiningCPID.cpid + "</CPID><GRCADDRESS>" + GRCAddress + "</GRCADDRESS><RND>"
                                        + hashRand.GetHex() + "</RND><BALANCE>" + RoundToString(nBalance,2)
                                        + "</BALANCE><MAGNITUDE>" + RoundToString(dmag,0) + "</MAGNITUDE>";
                    // Add the provable balance and the provable magnitude - this goes into effect July 1 2017
                    voter += GetProvableVotingWeightXML();
                    std::string pk = Title + ";" + GRCAddress + ";" + GlobalCPUMiningCPID.cpid;
                    std::string contract = "<TITLE>" + Title + "</TITLE><ANSWER>" + Answer + "</ANSWER>" + voter;
                    std::string result = AddContract("vote",pk,contract);
                    std::string narr = "Your CPID weight is " + RoundToString(dmag,0) + " and your Balance weight is " + RoundToString(nBalance,0) + ".";

                    res.push_back(Pair("Success", narr + " " + "Your vote has been cast for topic " + Title + ": With an Answer of " + Answer + ": " + result.c_str()));
                }
            }
        }
    }

    return res;
}

Value votedetails(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "votedetails <pollname>]n"
                "\n"
                "<pollname> Specified poll name\n"
                "\n"
                "Displays vote details of a specified poll\n");

    Array res;

    std::string Title = params[0].get_str();

    if (!PollExists(Title))
    {
        Object results;

        results.push_back(Pair("Error", "Poll does not exist.  Please listpolls."));
        res.push_back(results);
    }

    else
    {
        LOCK(cs_main);

        Array myVotes = GetJsonVoteDetailsReport(Title);

        res.push_back(myVotes);
    }

    return res;
}

Value execute(const Array& params, bool fHelp)
{
    throw JSONRPCError(RPC_DEPRECATED, "execute function has been deprecated; run the command as previously done so but without execute");
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
    throw JSONRPCError(RPC_DEPRECATED, "list is deprecated; Please run the command the same as previously without list");
}

// ppcoin: get information of sync-checkpoint
Value getcheckpoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getcheckpoint\n"
                "Show info of synchronized checkpoint.\n");

    Object result;

    LOCK(cs_main);

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
                "\n"
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
