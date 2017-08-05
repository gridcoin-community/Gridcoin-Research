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

#include <boost/filesystem.hpp>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <fstream>

using namespace json_spirit;
using namespace std;
extern std::string YesNo(bool bin);
bool BackupConfigFile(const string& strDest);
std::string getHardDriveSerial();
int64_t GetRSAWeightByCPIDWithRA(std::string cpid);
extern double DoubleFromAmount(int64_t amount);
std::string PubKeyToAddress(const CScript& scriptPubKey);
CBlockIndex* GetHistoricalMagnitude(std::string cpid);
std::string UnpackBinarySuperblock(std::string sBlock);
std::string PackBinarySuperblock(std::string sBlock);
extern std::string GetProvableVotingWeightXML();
extern double ReturnVerifiedVotingBalance(std::string sXML, bool bCreatedAfterSecurityUpgrade);
extern double ReturnVerifiedVotingMagnitude(std::string sXML, bool bCreatedAfterSecurityUpgrade);
extern void GetBeaconElements(std::string sBeacon,std::string& out_cpid, std::string& out_address, std::string& out_publickey);
bool AskForOutstandingBlocks(uint256 hashStart);
bool CleanChain();
extern std::string SendReward(std::string sAddress, int64_t nAmount);
extern double GetMagnitudeByCpidFromLastSuperblock(std::string sCPID);
std::string GetBeaconPublicKey(const std::string& cpid, bool bAdvertising);
extern std::string SuccessFail(bool f);
extern Array GetUpgradedBeaconReport();
extern Array MagnitudeReport(std::string cpid);
std::string ConvertBinToHex(std::string a);
std::string ConvertHexToBin(std::string a);
extern std::vector<unsigned char> readFileToVector(std::string filename);
bool TallyResearchAverages(bool Forcefully);
int RestartClient();
extern std::string SignBlockWithCPID(std::string sCPID, std::string sBlockHash);
std::string BurnCoinsWithNewContract(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue, int64_t MinimumBalance, double dFees, std::string strPublicKey, std::string sBurnAddress);
extern std::string GetBurnAddress();
bool NeuralNodeParticipates();
bool StrLessThanReferenceHash(std::string rh);
extern std::string AddMessage(bool bAdd, std::string sType, std::string sKey, std::string sValue, std::string sSig, int64_t MinimumBalance, double dFees, std::string sPublicKey);
extern std::string ExtractValue(std::string data, std::string delimiter, int pos);
extern Array SuperblockReport(std::string cpid);
bool IsSuperBlock(CBlockIndex* pIndex);
MiningCPID GetBoincBlockByIndex(CBlockIndex* pblockindex);
extern double GetSuperblockMagnitudeByCPID(std::string data, std::string cpid);
extern int64_t BeaconTimeStamp(std::string cpid, bool bZeroOutAfterPOR);
extern bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature);
bool NeedASuperblock();
bool VerifySuperblock(std::string superblock, int nHeight);
double ExtractMagnitudeFromExplainMagnitude();
std::string GetQuorumHash(const std::string& data);
double GetOutstandingAmountOwed(StructCPID &mag, std::string cpid, int64_t locktime, double& total_owed, double block_magnitude);
bool ComputeNeuralNetworkSupermajorityHashes();
bool UpdateNeuralNetworkQuorumData();
extern Array LifetimeReport(std::string cpid);
Array StakingReport();
extern std::string AddContract(std::string sType, std::string sName, std::string sContract);
StructCPID GetLifetimeCPID(const std::string& cpid, const std::string& sFrom);
void WriteCache(std::string section, std::string key, std::string value, int64_t locktime);
bool HasActiveBeacon(const std::string& cpid);
int64_t GetEarliestWalletTransaction();
extern bool CheckMessageSignature(std::string sAction,std::string messagetype, std::string sMsg, std::string sSig, std::string opt_pubkey);
bool LoadAdminMessages(bool bFullTableScan,std::string& out_errors);
int64_t GetMaximumBoincSubsidy(int64_t nTime);
double GRCMagnitudeUnit(int64_t locktime);
std::string ExtractXML(std::string XMLdata, std::string key, std::string key_end);
std::string ExtractHTML(std::string HTMLdata, std::string tagstartprefix,  std::string tagstart_suffix, std::string tag_end);
std::string NeuralRequest(std::string MyNeuralRequest);
extern bool AdvertiseBeacon(bool bFromService, std::string &sOutPrivKey, std::string &sOutPubKey, std::string &sError, std::string &sMessage);

double Round(double d, int place);
bool UnusualActivityReport();
double GetCountOf(std::string datatype);
extern double GetSuperblockAvgMag(std::string data,double& out_beacon_count,double& out_participant_count,double& out_average, bool bIgnoreBeacons,int nHeight);
extern bool CPIDAcidTest2(std::string bpk, std::string externalcpid);

bool AsyncNeuralRequest(std::string command_name,std::string cpid,int NodeLimit);
bool FullSyncWithDPORNodes();
bool LoadSuperblock(std::string data, int64_t nTime, double height);

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
extern std::string GetListOf(std::string datatype);
extern std::string GetListOfWithConsensus(std::string datatype);

void qtSyncWithDPORNodes(std::string data);
std::string qtGetNeuralHash(std::string data);
std::string qtGetNeuralContract(std::string data);

extern bool TallyMagnitudesInSuperblock();
double GetTotalBalance();

std::string strReplace(std::string& str, const std::string& oldStr, const std::string& newStr);
std::string ReadCache(std::string section, std::string key);
MiningCPID GetNextProject(bool bForce);
std::string SerializeBoincBlock(MiningCPID mcpid);
extern std::string TimestampToHRDate(double dtm);

std::string qtGRCCodeExecutionSubsystem(std::string sCommand);
std::string LegacyDefaultBoincHashArgs();
std::string GetHttpPage(std::string url);
double CoinToDouble(double surrogate);
int64_t GetRSAWeightByCPID(std::string cpid);
double GetUntrustedMagnitude(std::string cpid, double& out_owed);
extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);
extern enum Checkpoints::CPMode CheckpointsMode;
int RebootClient();
int ReindexWallet();
extern Array MagnitudeReportCSV(bool detail);
std::string getfilecontents(std::string filename);
int CreateRestorePoint();
int DownloadBlocks();
double cdbl(std::string s, int place);
std::vector<std::string> split(std::string s, std::string delim);
double LederstrumpfMagnitude2(double mag,int64_t locktime);
bool IsCPIDValidv2(MiningCPID& mc, int height);
std::string RetrieveMd5(std::string s1);

std::string getfilecontents(std::string filename);
MiningCPID DeserializeBoincBlock(std::string block);

extern double GetNetworkAvgByProject(std::string projectname);
void HarvestCPIDs(bool cleardata);
std::string GetHttpPage(std::string cpid, bool usedns, bool clearcache);
void ExecuteCode();
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
   boost::filesystem::path dir_path = GetDataDir() / "nn2";
   boost::filesystem::directory_iterator it(dir_path), eod;
   std::string sMyManifest = "";
   BOOST_FOREACH(boost::filesystem::path const &p, std::make_pair(it, eod))   
   { 
      if(boost::filesystem::is_regular_file(p))
      {
        sMyManifest += p.string();
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






double GetPoWMHashPS()
{
    if (pindexBest->nHeight >= LAST_POW_BLOCK)
        return 0;

    int nPoWInterval = 72;
    int64_t nTargetSpacingWorkMin = 30, nTargetSpacingWork = 30;

    CBlockIndex* pindex = pindexGenesisBlock;
    CBlockIndex* pindexPrevWork = pindexGenesisBlock;

    while (pindex)
    {
        if (pindex->IsProofOfWork())
        {
            int64_t nActualSpacingWork = pindex->GetBlockTime() - pindexPrevWork->GetBlockTime();
            nTargetSpacingWork = ((nPoWInterval - 1) * nTargetSpacingWork + nActualSpacingWork + nActualSpacingWork) / (nPoWInterval + 1);
            nTargetSpacingWork = max(nTargetSpacingWork, nTargetSpacingWorkMin);
            pindexPrevWork = pindex;
        }

        pindex = pindex->pnext;
    }

    return GetDifficulty() * 4294.967296 / nTargetSpacingWork;
}

double GetPoSKernelPS()
{
    int nPoSInterval = 72;
    double dStakeKernelsTriedAvg = 0;
    int nStakesHandled = 0, nStakesTime = 0;

    CBlockIndex* pindex = pindexBest;;
    CBlockIndex* pindexPrevStake = NULL;

    while (pindex && nStakesHandled < nPoSInterval)
    {
        if (pindex->IsProofOfStake())
        {
            dStakeKernelsTriedAvg += GetDifficulty(pindex) * 4294967296.0;
            nStakesTime += pindexPrevStake ? (pindexPrevStake->nTime - pindex->nTime) : 0;
            pindexPrevStake = pindex;
            nStakesHandled++;
        }

        pindex = pindex->pprev;
    }

    double result = 0;

    if (nStakesTime)
        result = dStakeKernelsTriedAvg / nStakesTime;

    if (IsProtocolV2(nBestHeight))
        result *= STAKE_TIMESTAMP_MASK + 1;

    return result/100;
}
Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);
    result.push_back(Pair("confirmations", (int)txGen.GetDepthInMainChain()));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    double mint = CoinToDouble(blockindex->nMint);
    result.push_back(Pair("mint", mint));
    result.push_back(Pair("time", (int64_t)block.GetBlockTime()));
    result.push_back(Pair("nonce", (uint64_t)block.nNonce));
    result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("blocktrust", leftTrim(blockindex->GetBlockTrust().GetHex(), '0')));
    result.push_back(Pair("chaintrust", leftTrim(blockindex->nChainTrust.GetHex(), '0')));
    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    if (blockindex->pnext)
        result.push_back(Pair("nextblockhash", blockindex->pnext->GetBlockHash().GetHex()));
    MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc);
    uint256 blockhash = block.GetPoWHash();
    std::string sblockhash = blockhash.GetHex();
    bool IsPoR = false;
    IsPoR = (bb.Magnitude > 0 && bb.cpid != "INVESTOR" && blockindex->IsProofOfStake());
    std::string PoRNarr = "";
    if (IsPoR) PoRNarr = "proof-of-research";
    result.push_back(Pair("flags", 
        strprintf("%s%s", blockindex->IsProofOfStake()? "proof-of-stake" : "proof-of-work", blockindex->GeneratedStakeModifier()? " stake-modifier": "") + " " + PoRNarr        )       );
    result.push_back(Pair("proofhash", blockindex->hashProof.GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016" PRIx64, blockindex->nStakeModifier)));
    result.push_back(Pair("modifierchecksum", strprintf("%08x", blockindex->nStakeModifierChecksum)));
    Array txinfo;
    BOOST_FOREACH (const CTransaction& tx, block.vtx)
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
    obj.push_back(Pair("search-interval",      (int)nLastCoinStakeSearchInterval));
    return obj;
}


Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1 || AmountFromValue(params[0]) < MIN_TX_FEE)
        throw runtime_error(
            "settxfee <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.01");

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
    BOOST_FOREACH(const uint256& hash, vtxid)
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
    if (fDebug10)   printf("Getblockhash %f",(double)nHeight);
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



    
std::string BackupGridcoinWallet()
{
    printf("Starting Wallet Backup\r\n");
    std::string filename = "grc_" + DateTimeStrFormat("%m-%d-%Y",  GetAdjustedTime()) + ".dat";
    std::string filename_backup = "backup.dat";
    // std::string standard_filename = "wallet_" + DateTimeStrFormat("%m-%d-%Y",  GetAdjustedTime()) + ".dat";
    // std::string sConfig_FileName = "gridcoinresearch_" + DateTimeStrFormat("%m-%d-%Y",  GetAdjustedTime()) + ".conf";
    std::string standard_filename = GetBackupFilename("wallet.dat");
    std::string sConfig_FileName = GetBackupFilename("gridcoinresearch.conf");
    std::string source_filename   = "wallet.dat";
    boost::filesystem::path path = GetDataDir() / "walletbackups" / filename;
    boost::filesystem::path target_path_standard = GetDataDir() / "walletbackups" / standard_filename;
    boost::filesystem::path sTargetPathConfig = GetDataDir() / "walletbackups" / sConfig_FileName;
    boost::filesystem::path source_path_standard = GetDataDir() / source_filename;
    boost::filesystem::path dest_path_std = GetDataDir() / "walletbackups" / filename_backup;
    boost::filesystem::create_directories(path.parent_path());
    std::string errors = "";
    //Copy the standard wallet first:  (1-30-2015)
    BackupWallet(*pwalletMain, target_path_standard.string().c_str());
    BackupConfigFile(sTargetPathConfig.string().c_str());

    //Per Forum, do not dump the keys and abort:
    if (true)
    {
            printf("User does not want private keys backed up. Exiting.");
            return "";
    }
                    
    //Dump all private keys into the Level 2 backup
    ofstream myBackup;
    myBackup.open (path.string().c_str());
    string strAccount;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, string)& item, pwalletMain->mapAddressBook)
    {
         const CBitcoinAddress& address = item.first;
         //const std::string& strName = item.second;
         bool fMine = IsMine(*pwalletMain, address.Get());
         if (fMine) 
         {
            std::string strAddress=CBitcoinAddress(address).ToString();

            CKeyID keyID;
            if (!address.GetKeyID(keyID))   
            {
                errors = errors + "During wallet backup, Address does not refer to a key"+ "\r\n";
            }
            else
            {
                 bool IsCompressed;
                 CKey vchSecret;
                 if (!pwalletMain->GetKey(keyID, vchSecret))
                 {
                    errors = errors + "During Wallet Backup, Private key for address is not known\r\n";
                 }
                 else
                 {
                    CSecret secret = vchSecret.GetSecret(IsCompressed);
                    std::string private_key = CBitcoinSecret(secret,IsCompressed).ToString();
                    //Append to file
                    std::string strAddr = CBitcoinAddress(keyID).ToString();
                    std::string record = private_key + "<|>" + strAddr + "<KEY>";
                    myBackup << record;
                }
            }

         }
    }

    std::string reserve_keys = pwalletMain->GetAllGridcoinKeys();
    myBackup << reserve_keys;
    myBackup.close();
    fileopen_and_copy(path.string().c_str(),dest_path_std.string().c_str());
    return errors;

}





std::string RestoreGridcoinBackupWallet()
{
    //AdvancedBackup-AdvancedSalvage
    
    boost::filesystem::path path = GetDataDir() / "walletbackups" / "backup.dat";
    std::string errors = "";
    std::string sWallet = getfilecontents(path.string().c_str());
    if (sWallet == "-1") return "Unable to open backup file.";
        
    string strSecret = "from file";
    string strLabel = "Restored";

    std::vector<std::string> vWallet = split(sWallet.c_str(),"<KEY>");
    if (vWallet.size() > 1)
    {
        for (unsigned int i = 0; i < vWallet.size(); i++)
        {
            std::string sKey = vWallet[i];
            if (sKey.length() > 2)
            {
                    printf("Restoring private key %s",sKey.substr(0,5).c_str());
                    //Key is delimited by <|>
                    std::vector<std::string> vKey = split(sKey.c_str(),"<|>");
                    if (vKey.size() > 1)
                    {
                            std::string sSecret = vKey[0];
                            std::string sPublic = vKey[1];

                            bool IsCompressed;
                            CBitcoinSecret vchSecret;
                            bool fGood = vchSecret.SetString(sSecret);
                            if (!fGood)
                            {
                                errors = errors + "Invalid private key : " + sSecret + "\r\n";
                            }
                            else
                            {
                                 CKey key;
                                 CSecret secret = vchSecret.GetSecret(IsCompressed);
                                 key.SetSecret(secret,IsCompressed);
                                 //                              key = vchSecret.GetKey();
                                 CPubKey pubkey = key.GetPubKey();
                                
                                 CKeyID vchAddress = pubkey.GetID();
                                 {
                                     LOCK2(cs_main, pwalletMain->cs_wallet);
                                     //                            if (!pwalletMain->AddKey(key)) {            fGood = false;
         
                                     if (!pwalletMain->AddKey(key)) 
                                     {
                                         errors = errors + "Error adding key to wallet: " + sKey + "\r\n";
                                     }

                                     if (i==0)
                                     {
                                        pwalletMain->SetDefaultKey(pubkey);
                                        pwalletMain->SetAddressBookName(vchAddress, strLabel);
                                     }
                                     pwalletMain->MarkDirty();
                            
      
                                 }
                            }
                    }
            }

        }

    }


    //Rescan
    {
           LOCK2(cs_main, pwalletMain->cs_wallet);
            if (true) {
                pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
                pwalletMain->ReacceptWalletTransactions();
            }
     
    }

    printf("Rebuilding wallet, results: %s",errors.c_str());
    return errors;

}

void WriteCPIDToRPC(std::string email, std::string bpk, uint256 block, Array &results)
{
    std::string output = "";
    output = ComputeCPIDv2(email,bpk,block);
    Object entry;
    entry.push_back(Pair("Long CPID for " + email + " " + block.GetHex(),output));
    output = RetrieveMd5(bpk + email);
    std::string shortcpid = RetrieveMd5(bpk + email);
    entry.push_back(Pair("std_md5",output));
    //Stress test
    std::string me = ComputeCPIDv2(email,bpk,block);
    entry.push_back(Pair("LongCPID2",me));
    bool result;
    result =  CPID_IsCPIDValid(shortcpid, me,block);
    entry.push_back(Pair("Stress Test 1",result));
    result =  CPID_IsCPIDValid(shortcpid, me,block+1);
    entry.push_back(Pair("Stress Test 2",result));
    results.push_back(entry);
    shortcpid = RetrieveMd5(bpk + "0" + email);
    result = CPID_IsCPIDValid(shortcpid,me,block);
    entry.push_back(Pair("Stress Test 3 (missing bpk)",result));
    results.push_back(entry);
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
        std::vector<std::string> vSuperblock = split(superblock.c_str(),";");
        if (vSuperblock.size() < 2) return 0;
        double rows_above_zero = 0;
        double rows_with_zero = 0;
        double total_mag = 0;
        for (unsigned int i = 0; i < vSuperblock.size(); i++)
        {
            // For each CPID in the contract
            if (vSuperblock[i].length() > 1)
            {
                    std::string cpid = ExtractValue("0"+vSuperblock[i],",",0);
                    double magnitude = cdbl(ExtractValue("0"+vSuperblock[i],",",1),0);
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
        }
        out_count = rows_above_zero + rows_with_zero;
        double avg = total_mag/(rows_above_zero+.01);
        return avg;
    }
    catch(...)
    {
        printf("Error in GetAvgInList");
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
            printf(".");
            if (vSuperblock[i].length() > 1)
            {
                std::string sTempCPID = ExtractValue(vSuperblock[i],",",0);
                double magnitude = cdbl(ExtractValue("0"+vSuperblock[i],",",1),0);
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
	   if (fDebug10) printf(" GSPC:CountOfProjInBlock %f vs WhitelistedCount %f  \r\n",(double)out_project_count,(double)out_whitelist_count);
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
		if (fDebug10) printf(" CountOfProjInBlock %f vs WhitelistedCount %f Height %f \r\n",(double)avg_count,(double)out_project_count,(double)nHeight);
		if (!fTestNet && !bIgnoreBeacons && nHeight > 972000 && (avg_count < out_project_count*.50)) return -5;
        return avg_of_magnitudes + avg_of_projects;
    }
    catch (std::exception &e) 
    {
                printf("Error in GetSuperblockAvgMag.");
                return 0;
    }
    catch(...)
    {
                printf("Error in GetSuperblockAvgMag.");
                return 0;
    }
     
}



bool TallyMagnitudesInSuperblock()
{
    try
    {
        std::string superblock = ReadCache("superblock","magnitudes");
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
                    double magnitude = cdbl(ExtractValue(vSuperblock[i],",",1),0);
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

    if (fDebug3) printf(".TMIS41.");
    double NetworkAvgMagnitude = TotalNetworkMagnitude / (TotalNetworkEntries+.01);
    // Store the Total Network Magnitude:
    StructCPID network = GetInitializedStructCPID2("NETWORK",mvNetworkCopy);
    network.projectname="NETWORK";
    network.NetworkMagnitude = TotalNetworkMagnitude;
    network.NetworkAvgMagnitude = NetworkAvgMagnitude;
    
    double TotalProjects = 0;
    double TotalRAC = 0;
    double AVGRac = 0;
    // Load boinc project averages from neural network
    std::string projects = ReadCache("superblock","averages");
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
                    double avg = cdbl(ExtractValue("0" + vProjects[i],",",1),0);
                    if (project.length() > 1)
                    {
                        StructCPID stProject = GetInitializedStructCPID2(project,mvNetworkCopy);
                        stProject.projectname = project;
                        stProject.AverageRAC = avg;
                        //As of 7-16-2015, start pulling in Total RAC
                        totalRAC = 0;
                        totalRAC = cdbl("0" + ExtractValue(vProjects[i],",",2),0);
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
    //8-16-2015 Store the quotes
    std::string q = ReadCache("superblock","quotes");
    if (fDebug3) printf("q %s",q.c_str());
    std::vector<std::string> vQ = split(q.c_str(),";");
    if (vQ.size() > 0)
    {
        for (unsigned int i = 0; i < vQ.size(); i++)
        {
            // For each quote in the contract
            if (vQ[i].length() > 1)
            {
                    std::string symbol = ExtractValue(vQ[i],",",0);
                    double price = cdbl(ExtractValue("0" + vQ[i],",",1),0);
                    
                    WriteCache("quotes",symbol,RoundToString(price,2),GetAdjustedTime());
                    if (fDebug3) printf("symbol %s price %f ",symbol.c_str(),price);
            }
        }
    }

    mvNetworkCopy["NETWORK"] = network;
    if (fDebug3) printf(".TMS43.");
    return true;
    }
    catch (std::exception &e) 
    {
                printf("Error in TallySuperblock.");
                return false;
    }
    catch(...)
    {
                printf("Error in TallySuperblock");
                return false;
    }

}

double GetCountOf(std::string datatype)
{
    std::string data = GetListOf(datatype);
    std::vector<std::string> vScratchPad = split(data.c_str(),"<ROW>");
    return vScratchPad.size()+1;
}

std::string GetListOf(std::string datatype)
{
            std::string rows = "";
            std::string row = "";
            for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
            {
                std::string key_name  = (*ii).first;
                if (key_name.length() > datatype.length())
                {
                    if (key_name.substr(0,datatype.length())==datatype)
                    {
                                std::string key_value = mvApplicationCache[(*ii).first];
                                std::string subkey = key_name.substr(datatype.length()+1,key_name.length()-datatype.length()-1);
                                row = subkey + "<COL>" + key_value;
                                if (Contains(row,"INVESTOR") && datatype=="beacon") row = "";
                                if (row != "")
                                {
                                    rows += row + "<ROW>";
                                }
                    }
               
                }
           }
           return rows;
}



std::string GetListOfWithConsensus(std::string datatype)
{
       std::string rows = "";
       std::string row = "";
	   int64_t iEndTime= (GetAdjustedTime()-CONSENSUS_LOOKBACK) - ( (GetAdjustedTime()-CONSENSUS_LOOKBACK) % BLOCK_GRANULARITY);
       int64_t nLookback = 30 * 6 * 86400; 
       int64_t iStartTime = (iEndTime - nLookback) - ( (iEndTime - nLookback) % BLOCK_GRANULARITY);
       printf(" getlistofwithconsensus startime %f , endtime %f, lookback %f \r\n ",(double)iStartTime,(double)iEndTime, (double)nLookback);
	   for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
       {
             std::string key_name  = (*ii).first;
             if (key_name.length() > datatype.length())
             {
                 if (key_name.substr(0,datatype.length())==datatype)
                 {
 			           int64_t iBeaconTimestamp = mvApplicationCacheTimestamp[(*ii).first];
				       if (iBeaconTimestamp > iStartTime && iBeaconTimestamp < iEndTime)
					   {		
							std::string key_value = mvApplicationCache[(*ii).first];
							std::string subkey = key_name.substr(datatype.length()+1,key_name.length()-datatype.length()-1);
							row = subkey + "<COL>" + key_value;
							if (Contains(row,"INVESTOR") && datatype=="beacon") row = "";
							if (row != "")
							{
								rows += row + "<ROW>";
							}
						}
                  }
             }
       }
       return rows;
}

int64_t BeaconTimeStamp(std::string cpid, bool bZeroOutAfterPOR)
{
            std::string sBeacon = mvApplicationCache["beacon;" + cpid];
            int64_t iLocktime = mvApplicationCacheTimestamp["beacon;" + cpid];
            int64_t iRSAWeight = GetRSAWeightByCPIDWithRA(cpid);
            if (fDebug10) printf("\r\n Beacon %s, Weight %f, Locktime %f \r\n",sBeacon.c_str(),(double)iRSAWeight,(double)iLocktime);
            if (bZeroOutAfterPOR && iRSAWeight==0) iLocktime = 0;
            return iLocktime;

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
        

bool AdvertiseBeacon(bool bFromService, std::string &sOutPrivKey, std::string &sOutPubKey, std::string &sError, std::string &sMessage)
{	
     LOCK(cs_main);
     {
            GetNextProject(false);
            if (GlobalCPUMiningCPID.cpid=="INVESTOR")
            {
                sError = "INVESTORS_CANNOT_SEND_BEACONS";
                return bFromService ? true : false;
            }

            //If beacon is already in the chain, exit early
            std::string sBeaconPublicKey = GetBeaconPublicKey(GlobalCPUMiningCPID.cpid,bFromService);
            if (!sBeaconPublicKey.empty()) 
            {
                // Ensure they can re-send the beacon if > 5 months old : GetBeaconPublicKey returns an empty string when > 5 months: OK.
                // Note that we allow the client to re-advertise the beacon in 5 months, so that they have a seamless and uninterrupted keypair in use (prevents a hacker from hijacking a keypair that is in use)		
                sError = "ALREADY_IN_CHAIN";
                return bFromService ? true : false;
            }
            
            uint256 hashRand = GetRandHash();
            std::string email = GetArgument("email", "NA");
            boost::to_lower(email);
            GlobalCPUMiningCPID.email=email;
            GlobalCPUMiningCPID.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, hashRand);

            bool IsCPIDValid2 = CPID_IsCPIDValid(GlobalCPUMiningCPID.cpid,GlobalCPUMiningCPID.cpidv2, hashRand);
            if (!IsCPIDValid2) return "Invalid CPID";

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
            std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID);
            std::string GRCAddress = DefaultWalletAddress();
            // Public Signing Key is stored in Beacon
            std::string contract = GlobalCPUMiningCPID.cpidv2 + ";" + hashRand.GetHex() + ";" + GRCAddress + ";" + sOutPubKey;
            printf("\r\n Creating beacon for cpid %s, %s",GlobalCPUMiningCPID.cpid.c_str(),contract.c_str());
            std::string sBase = EncodeBase64(contract);
            std::string sAction = "add";
            std::string sType = "beacon";
            std::string sName = GlobalCPUMiningCPID.cpid;
            try
            {
                // Store the key 
                sMessage = AddContract(sType,sName,sBase);
                // Backup config with old keys like a normal backup
                std::string sBeaconBackupOldConfigFilename = GetBackupFilename("gridcoinresearch.conf");
                boost::filesystem::path sBeaconBackupOldConfigTarget = GetDataDir() / "walletbackups" / sBeaconBackupOldConfigFilename;
                BackupConfigFile(sBeaconBackupOldConfigTarget.string().c_str());
                StoreBeaconKeys(GlobalCPUMiningCPID.cpid, sOutPubKey, sOutPrivKey);
                // Backup config with new keys with beacon suffix
                std::string sBeaconBackupNewConfigFilename = GetBackupFilename("gridcoinresearch.conf", "beacon");
                boost::filesystem::path sBeaconBackupNewConfigTarget = GetDataDir() / "walletbackups" / sBeaconBackupNewConfigFilename;
                BackupConfigFile(sBeaconBackupNewConfigTarget.string().c_str());
                // Activate Beacon Keys in memory. This process is not automatic and has caused users who have a new keys while old ones exist in memory to perform a restart of wallet.
                ActivateBeaconKeys(GlobalCPUMiningCPID.cpid, sOutPubKey, sOutPrivKey);
                return true;
            }
            catch(Object& objError)
            {
                sError = "Error: Unable to send beacon::Wallet Locked::Please enter the wallet passphrase with walletpassphrase first.";
                return false;
            }
            catch (std::exception &e) 
            {
                sError = "Error: Unable to send beacon::Wallet Locked::Please enter the wallet passphrase with walletpassphrase first.";
                return false;
            }
            catch(...)
            {
                sError = "Error: Unable to send beacon::Wallet Locked::Please enter the wallet passphrase with walletpassphrase first.";
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

     printf("Executing method %s\r\n",method.c_str());
     Value vResult;
     try
     {
        vResult = execute(params,false);
     }
     catch (std::exception& e)
     {
         printf("Std exception %s \r\n",method.c_str());
         
         std::string caught = e.what();
         return "Exception " + caught;

     } 
     catch (...) 
     {
            printf("Generic exception (Please try unlocking the wallet) %s \r\n",method.c_str());
            return "Generic Exception (Please try unlocking the wallet).";
     }
     std::string sResult = "";
     sResult = write_string(vResult, false) + "\n";
     printf("Response %s",sResult.c_str());
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

     printf("Executing method %s\r\n",method.c_str());
     Value vResult;
     try
     {
        vResult = execute(params,false);
     }
     catch (std::exception& e)
     {
         printf("Std exception %s \r\n",method.c_str());
         
         std::string caught = e.what();
         return "Exception " + caught;

     } 
     catch (...) 
     {
            printf("Generic exception (Please try unlocking the wallet) %s \r\n",method.c_str());
            return "Generic Exception (Please try unlocking the wallet).";
     }
     std::string sResult = "";
     sResult = write_string(vResult, false) + "\n";
     printf("Response %s",sResult.c_str());
     return sResult;
}


std::string ExecuteRPCCommand(std::string method, std::string arg1, std::string arg2)
{
     Array params;
     params.push_back(method);
     params.push_back(arg1);
     params.push_back(arg2);
     printf("Executing method %s\r\n",method.c_str());
     Value vResult;
     try
     {
        vResult = execute(params,false);
     }
     catch (std::exception& e)
     {
         printf("Std exception %s \r\n",method.c_str());
         
         std::string caught = e.what();
         return "Exception " + caught;

     } 
     catch (...) 
     {
            printf("Generic exception (Please try unlocking the wallet). %s \r\n",method.c_str());
            return "Generic Exception (Please try unlocking the wallet).";
     }
     std::string sResult = "";
     sResult = write_string(vResult, false) + "\n";
     printf("Response %s",sResult.c_str());
     return sResult;
}

int64_t AmountFromDouble(double dAmount)
{
    if (dAmount <= 0.0 || dAmount > MAX_MONEY)        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64_t nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))         throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}


std::string GetDomainForSymbol(std::string sSymbol)
{
            std::string RegRest   = ReadCache("daorest",sSymbol);
            return RegRest;
}


Value dao(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 1 && params.size() != 2  && params.size() != 3 && params.size() != 4 && params.size() != 5 && params.size() != 6 && params.size() != 7))
        throw runtime_error(
        "dao <string::itemname> <string::parameter> \r\n"
        "Executes a DAO based command by name.");
    // Add DAO features - 1-30-2016
    std::string sItem = params[0].get_str();

    if (sItem=="") throw runtime_error("Item invalid.");

    Array results;
    Object oOut;
    oOut.push_back(Pair("Command",sItem));
    results.push_back(oOut);
    Object entry;
        
    if (sItem == "metric")
    {
        if (params.size() < 3)
        {
            entry.push_back(Pair("Error","You must specify the ticker and metric_name.  Example: dao metric GRCQ nav."));
            results.push_back(entry);
        }
        else
        {
            // Verify the Ticker exists:  1-27-2016
            std::string sTicker = params[1].get_str();
            boost::to_upper(sTicker);
            std::string sMetric = params[2].get_str();
            boost::to_upper(sMetric);
            std::string sGRCAddress = DefaultWalletAddress();
            // Query DAO
            //std::string OrgPubKey = ReadCache("daopubkey",org);
            //std::string RegSymbol = ReadCache("daosymbol",org);
            std::string RegName   = ReadCache("daoorgname",sTicker);
            std::string RegRest   = ReadCache("daorest",sTicker);
            if (RegName.empty())
            {
                    entry.push_back(Pair("Error","DAO does not exist."));
                    results.push_back(entry);
                    return results;
            }
            entry.push_back(Pair("DAO ID",sTicker));
            entry.push_back(Pair("Metric",sMetric));
            entry.push_back(Pair("Org Name",RegName));
            entry.push_back(Pair("REST Access",RegRest));
            std::string sDomain = GetDomainForSymbol(sTicker);
            // Domain Example = [http://]RESTsubdomain.RESTdomain.DNSdomain/RESTWebServiceDirectory/RESTPage.[protocol]?[QuerystringDirective]=[Key]&[QueryDirectiveII]=[Key]
            std::string sURL = sDomain + "?address=" + sGRCAddress + "&metric=" + sMetric + "&id=" + sGRCAddress;
            std::string sResults = GetHttpPage(sURL);
            std::string sOutput = ExtractXML(sResults,"<metric>","</metric>");
            entry.push_back(Pair(sMetric.c_str(),sOutput));
            results.push_back(entry);
        }
    }
    else if (sItem == "link")
    {
        if (params.size() < 4)
        {
            entry.push_back(Pair("Error","You must specify the ticker, username and password.  Example: dao link myusername mypassword."));
            results.push_back(entry);
        }
        else
        {
            // Verify the Ticker exists:  1-27-2016
            std::string sTicker = params[1].get_str();
            std::string sUser   = params[2].get_str();
            std::string sPass   = params[3].get_str();
            boost::to_upper(sTicker);
            entry.push_back(Pair("DAO ID",sTicker));
            entry.push_back(Pair("User",sUser));
            std::string sEncodedPassword = EncodeBase64(sPass);
            std::string sGRCAddress = DefaultWalletAddress();
            std::string sDomain = GetDomainForSymbol(sTicker);
            std::string RegName   = ReadCache("daoorgname",sTicker);
            std::string RegRest   = ReadCache("daorest",sTicker);
            if (RegName.empty())
            {
                    entry.push_back(Pair("Error","DAO does not exist."));
                    results.push_back(entry);
                    return results;
            }
            std::string sURL = sDomain + "?metric=link&address=" + sGRCAddress + "&username=" + sUser + "&pass=" + sEncodedPassword;
            std::string sResults = GetHttpPage(sURL);
            std::string sOutput = ExtractXML(sResults,"<response>","</response>");
            entry.push_back(Pair("Result",sOutput));
            results.push_back(entry);
        }
    }
    else if (sItem == "adddao")
    {
        //execute adddao org_name dao_currency_symbol org_receive_grc_address RESTfulURL

        if (params.size() != 5)
        {
            entry.push_back(Pair("Error","You must specify the Organization_Name, DAO_CURRENCY_SYMBOL, ORG_RECEIVING_ADDRESS and RESTfulURL.  For example: execute adddao PetsRUs PETS xKBQaegxasEyBVxmsTMg3HnasNg77CtjLo http://petsrus.com/rest/restquery.php"));

            results.push_back(entry);
        }
        else
        {
                std::string org    = params[1].get_str();
                std::string symbol = params[2].get_str();
                std::string grc    = params[3].get_str();
                std::string rest   = params[4].get_str();
                boost::to_upper(org);
                boost::to_upper(symbol);

                CBitcoinAddress address(grc);
                bool isValid = address.IsValid();
                std::string               err = "";
                if (org.empty())          err = "Org must be specified.";
                if (symbol.length() != 3 && symbol.length() != 4) err = "Symbol must be 3-4 characters.";
                if (!isValid)             err = "You must specify a valid GRC receiving address.";
                std::string OrgPubKey    = ReadCache("daopubkey",org);
                std::string CachedSymbol = ReadCache("daosymbol",org);
                std::string CachedName   = ReadCache("daoname",CachedSymbol);
                if (!CachedSymbol.empty())                         err = "DAO Symbol already exists.  Please choose a different symbol.";
                if (!OrgPubKey.empty() || !CachedName.empty())     err = "DAO already exists.  Please choose a different Org Name.";
                if (rest.empty())        err = "You must specify the RESTful URL.";
                if (!err.empty())
                {
                    entry.push_back(Pair("Error",err));
                    results.push_back(entry);
                }
                else
                {
                    //Generate the key pair for the org
                    CKey key;
                    key.MakeNewKey(false);
                    CPrivKey vchPrivKey = key.GetPrivKey();
                    std::string PrivateKey =  HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end());
                    std::string PubKey = HexStr(key.GetPubKey().Raw());
                    std::string sAction = "add";
                    std::string sType = "dao";
                    std::string sPass = PrivateKey;
                    std::string sName = symbol;
                    std::string contract = "<NAME>" + org + "</NAME><SYMBOL>" + symbol + "</SYMBOL><ADDRESS>" + grc + "</ADDRESS><PUBKEY>" + PubKey + "</PUBKEY><REST>" + rest + "</REST>";
                    std::string result = AddMessage(true,sType,sName,contract,sPass,AmountFromValue(100),1,PubKey);
                    entry.push_back(Pair("DAO Name",org));
                    entry.push_back(Pair("SYMBOL",symbol));
                    entry.push_back(Pair("Rest URL",rest));
                    entry.push_back(Pair("Default Receive Address",grc));
                    std::string sKeyNarr = "dao" + symbol + "=" + PrivateKey;
                    entry.push_back(Pair("PrivateKey",sKeyNarr));
                    entry.push_back(Pair("Warning!","Do not lose your private key.  It is non-recoverable.  You may add it to your config file as noted above OR specify it manually via RPC commands."));
                    results.push_back(entry);
    
                }
            }
    }


    else
    {
            entry.push_back(Pair("Command " + sItem + " not found.",-1));
            results.push_back(entry);
    }
    return results;    

}





Value option(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 1 && params.size() != 2  && params.size() != 3 && params.size() != 4 && params.size() != 5 && params.size() != 6 && params.size() != 7))
        throw runtime_error(
        "option <string::itemname> <string::parameter> \r\n"
        "Executes an option based command by name.");

    std::string sItem = params[0].get_str();
    if (sItem=="") throw runtime_error("Item invalid.");
    Array results;
    Object oOut;
    oOut.push_back(Pair("Command",sItem));
    results.push_back(oOut);
    Object entry;
    return results;    
}



Value execute(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 1 && params.size() != 2  && params.size() != 3 && params.size() != 4 && params.size() != 5 && params.size() != 6 && params.size() != 7))
        throw runtime_error(
        "execute <string::itemname> <string::parameter> \r\n"
        "Executes an arbitrary command by name.");

    std::string sItem = params[0].get_str();

    if (sItem=="") throw runtime_error("Item invalid.");

    Array results;
    Object oOut;
    oOut.push_back(Pair("Command",sItem));
    results.push_back(oOut);
    Object entry;
        
    if (sItem == "restorepoint")
    {
            int r=-1;
            #if defined(WIN32) && defined(QT_GUI)
            //We must stop the node before we can do this
            r = CreateRestorePoint();
            //RestartGridcoin();
            #endif 
            entry.push_back(Pair("Restore Point",r));
            results.push_back(entry);
    }
    else if (sItem == "reboot")
    {
            int r=1;
            #if defined(WIN32) && defined(QT_GUI)
            RebootClient();
            #endif 
            entry.push_back(Pair("RebootClient",r));
            results.push_back(entry);
    
    }
    else if (sItem == "restartclient")
    {
            printf("Restarting Gridcoin...");
            int iResult = 0;
            #if defined(WIN32) && defined(QT_GUI)
                iResult = RestartClient();
            #endif
            entry.push_back(Pair("Restarting",(double)iResult));
            results.push_back(entry);
    }
    else if (sItem == "cleanchain")
    {
        bool fResult = CleanChain();
        entry.push_back(Pair("CleanChain",fResult));
        results.push_back(entry);
    }
    else if (sItem == "sendblock")
    {
            if (params.size() != 2)
            {
                entry.push_back(Pair("Error","You must specify the block hash to send."));
                results.push_back(entry);
            } 
            std::string sHash = params[1].get_str();
            uint256 hash = uint256(sHash);
            bool fResult = AskForOutstandingBlocks(hash);
            entry.push_back(Pair("Requesting",hash.ToString()));
            entry.push_back(Pair("Result",fResult));
            results.push_back(entry);
    }
    else if (sItem=="burn")
    {
        if (params.size() < 5)
        {
            entry.push_back(Pair("Error","You must specify Burn Address, Amount, Burn_Key and Burn_Message.  Example: execute burn GRCADDRESS 10 burn_key burn_detail..."));
            results.push_back(entry);
        }
        else
        {
                std::string sAddress = params[1].get_str();
                std::string sAmount  = params[2].get_str();
                std::string sKey     = params[3].get_str();
                std::string sDetail  = params[4].get_str();
                CBitcoinAddress address(sAddress);
                bool isValid = address.IsValid();
                if (!isValid) 
                {
                    entry.push_back(Pair("Error","Invalid GRC Burn Address."));
                    results.push_back(entry);
                    return results;
                }

                double dAmount = cdbl(sAmount,6);

                if (dAmount == 0 || dAmount < 0)
                {
                    entry.push_back(Pair("Error","Burn amount must be > 0."));
                    results.push_back(entry);
                    return results;
                }

                if (sKey.empty() || sDetail.empty())
                {
                    entry.push_back(Pair("Error","Burn Key and Burn Detail must be populated."));
                    results.push_back(entry);
                    return results;
                }
                std::string sContract = "<KEY>" + sKey + "</KEY><DETAIL>" + sDetail + "</DETAIL>";
                std::string sResult = BurnCoinsWithNewContract(true,"burn",sKey,sContract,AmountFromValue(1),dAmount,"",sAddress);

                //std::string BurnCoinsWithNewContract(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue,                    int64_t MinimumBalance, double dFees, std::string strPublicKey, std::string sBurnAddress)


                entry.push_back(Pair("Burn_Response",sResult));
                results.push_back(entry);
        }
    }
    else if (sItem == "newburnaddress")
    {
        //3-12-2016 - R Halford - Allow the user to make vanity GRC Burn Addresses that have no corresponding private key
        std::string sBurnTemplate = "GRCBurnAddressGRCBurnAddressGRCBurnAddress";
        if (params.size() > 1)
        {
            sBurnTemplate = params[1].get_str();
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
        entry.push_back(Pair("CombinedHex",combined));
        std::string encoded2 = EncodeBase58Check(vchDecoded30);
        if (encoded2.length() != 34)
        {
            entry.push_back(Pair("Burn Address Creation failed","NOTE: the input phrase must not include zeroes, or nonbase58 characters."));
            results.push_back(entry);
            return results;
        }
        // Give the user the new vanity burn address
        entry.push_back(Pair("Burn Address",encoded2));
        results.push_back(entry);
    }
    else if (sItem=="proveownership")
    {
        mvCPIDCache.clear();
        HarvestCPIDs(true);
        GetNextProject(true);
        std::string email = GetArgument("email", "NA");
        boost::to_lower(email);
        entry.push_back(Pair("Boinc E-Mail",email));
        entry.push_back(Pair("Boinc Public Key",GlobalCPUMiningCPID.boincruntimepublickey));
        entry.push_back(Pair("CPID",GlobalCPUMiningCPID.cpid));
        std::string sLongCPID = ComputeCPIDv2(email,GlobalCPUMiningCPID.boincruntimepublickey,1);
        std::string sShortCPID = RetrieveMd5(GlobalCPUMiningCPID.boincruntimepublickey + email);
        std::string sEmailMD5 = RetrieveMd5(email);
        std::string sBPKMD5 = RetrieveMd5(GlobalCPUMiningCPID.boincruntimepublickey);
        entry.push_back(Pair("Computed Email Hash",sEmailMD5));
        entry.push_back(Pair("Computed BPK",sBPKMD5));
        
        entry.push_back(Pair("Computed CPID",sLongCPID));
        entry.push_back(Pair("Computed Short CPID", sShortCPID));
        bool fResult = CPID_IsCPIDValid(sShortCPID,sLongCPID,1);
        if (GlobalCPUMiningCPID.boincruntimepublickey.empty())
        {
            fResult=false;
            entry.push_back(Pair("Error","Boinc Public Key empty.  Try mounting your boinc project first, and ensure the gridcoin datadir setting is set if boinc is not in the default location."));
        }
        entry.push_back(Pair("CPID Valid",fResult));
        results.push_back(entry);
    }
    else if (sItem=="beaconstatus")
    {
        // Search for beacon, and report on beacon status.
            
        std::string sCPID = msPrimaryCPID;
        if (params.size()==2)
        {
            sCPID = params[1].get_str();
        }
        
        entry.push_back(Pair("CPID", sCPID));
        std::string sPubKey =  GetBeaconPublicKey(sCPID, false);
        std::string sPrivKey = GetStoredBeaconPrivateKey(sCPID);
        int64_t iBeaconTimestamp = BeaconTimeStamp(sCPID, false);
        std::string timestamp = TimestampToHRDate(iBeaconTimestamp);
    
        bool hasBeacon = HasActiveBeacon(sCPID);
        entry.push_back(Pair("Beacon Exists",YesNo(hasBeacon)));
        entry.push_back(Pair("Beacon Timestamp",timestamp.c_str()));

        entry.push_back(Pair("Public Key", sPubKey.c_str()));
        entry.push_back(Pair("Private Key", sPrivKey.c_str()));
        
        std::string sErr = "";
        if (sPubKey.empty())
        {
            sErr += "Public Key Missing. ";
        }
        if (sPrivKey.empty())
        {
            sErr += "Private Key Missing. ";
        }
        // Verify the users Local Public Key matches the Beacon Public Key
        std::string sLocalPubKey = GetStoredBeaconPublicKey(sCPID);
        entry.push_back(Pair("Local Configuration Public Key", sLocalPubKey.c_str()));

        if (sLocalPubKey.empty())
        {
            sErr += "Local configuration file Public Key missing. ";
        }

        if (sLocalPubKey != sPubKey && !sPubKey.empty())
        {
            sErr += "Local configuration public key does not match beacon public key.  This can happen if you copied the wrong public key into your configuration file.  Please request that your beacon is deleted, or look into walletbackups for the correct keypair. ";
        }
        
        // Prior superblock Magnitude 
        double dMagnitude = GetMagnitudeByCpidFromLastSuperblock(sCPID);
        entry.push_back(Pair("Magnitude (As of last superblock)", dMagnitude));
        if (dMagnitude==0)
        {
            entry.push_back(Pair("Warning","Your magnitude is 0 as of the last superblock: this may keep you from staking POR blocks."));
        }

        // Staking Test 10-15-2016 - Simulate signing an actual block to verify this CPID keypair will work.
        uint256 hashBlock = GetRandHash();
        if (!sPubKey.empty())
        {
            std::string sSignature = SignBlockWithCPID(sCPID,hashBlock.GetHex());
            bool fResult = VerifyCPIDSignature(sCPID, hashBlock.GetHex(), sSignature);
            entry.push_back(Pair("Block Signing Test Results", fResult));
            if (!fResult)
            {
                sErr += "Failed to sign POR block.  This can happen if your keypair is invalid.  Check walletbackups for the correct keypair, or request that your beacon is deleted. ";
            }
        }

        if (!sErr.empty()) 
        {
            entry.push_back(Pair("Errors", sErr));
            entry.push_back(Pair("Help", "Note: If your beacon is missing its public key, or is not in the chain, you may try: execute advertisebeacon."));
            entry.push_back(Pair("Configuration Status","FAIL"));
        }
        else
        {
            entry.push_back(Pair("Configuration Status", "SUCCESSFUL"));
        }
        results.push_back(entry);

    }
    else if (sItem=="testnet0917")
    {
    
        WriteKey("testnet10","09172016");       
        std::string testnet = GetArgument("testnet10", "NA3");
        entry.push_back(Pair("testnetval4", testnet.c_str()));
        WriteKey("testnet11","0917");
        WriteKey("testnet11","0920162");
        testnet = GetArgument("testnet11", "NA4");
        entry.push_back(Pair("testnetval5", testnet.c_str()));
        results.push_back(entry);
    }
    else if (sItem=="rain")
    {
        if (params.size() < 2)
        {
            entry.push_back(Pair("Error","You must specify Rain Recipients in format: Address<COL>Amount<ROW>..."));
            results.push_back(entry);
        }
        else
        {
            CWalletTx wtx;
            wtx.mapValue["comment"] = "Rain";
            set<CBitcoinAddress> setAddress;
            vector<pair<CScript, int64_t> > vecSend;
            std::string sRecipients = params[1].get_str();
            std::string sRainCommand = ExtractXML(sRecipients,"<RAIN>","</RAIN>");
            std::string sRain = "<NARR>Project Rain: " + ExtractXML(sRecipients,"<RAINMESSAGE>","</RAINMESSAGE>") + "</NARR>";
            if (!sRainCommand.empty()) sRecipients = sRainCommand;
            //std::string sRainMessage = AdvancedCrypt(sRain);
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
                        double dAmount = cdbl(sAmount,4);
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
            entry.push_back(Pair("Response", sNarr));
            results.push_back(entry);
        }
    }
    else if (sItem == "neuralrequest")
    {

            std::string response = NeuralRequest("REQUEST");
            entry.push_back(Pair("Response", response));
            results.push_back(entry);

    }
    else if (sItem == "advertisebeacon")
    {
                std::string sOutPubKey = "";
                std::string sOutPrivKey = "";
                std::string sError = "";
                std::string sMessage = "";

                bool fResult = AdvertiseBeacon(false,sOutPrivKey,sOutPubKey,sError,sMessage);
                entry.push_back(Pair("Result",SuccessFail(fResult)));
                entry.push_back(Pair("CPID",GlobalCPUMiningCPID.cpid.c_str()));
                entry.push_back(Pair("Message",sMessage.c_str()));
                
                if (!sError.empty())        entry.push_back(Pair("Errors",sError));
            
                if (!fResult)
                {
                    entry.push_back(Pair("FAILURE","Note: if your wallet is locked this command will fail; to solve that unlock the wallet: 'walletpassphrase <yourpassword> <240>'."));
                }
                if (fResult)
                {
                    entry.push_back(Pair("Public Key",sOutPubKey.c_str()));
                    entry.push_back(Pair("Warning!","Your public and private research keys have been stored in gridcoinresearch.conf.  Do not lose your private key (It is non-recoverable).  It is recommended that you back up your gridcoinresearch.conf file on a regular basis."));
                }
                results.push_back(entry);
            
    }
    else if (sItem == "syncdpor2")
    {
        std::string sOut = "";
        double dBC = GetCountOf("beacon");
        bool bFull = dBC < 50 ? true : false;
        LoadAdminMessages(bFull,sOut);
        FullSyncWithDPORNodes();
        entry.push_back(Pair("Syncing",1));
        results.push_back(entry);
    }
    else if(sItem=="gatherneuralhashes")
    {
        GatherNeuralHashes();
        entry.push_back(Pair("Sent","."));
        results.push_back(entry);

    }
    else if (sItem == "beaconreport")
    {
            Array myBeaconJSONReport = GetJSONBeaconReport();
            results.push_back(myBeaconJSONReport);
    }
	else if (sItem == "unspentreport")
	{
		Array aUnspentReport = GetJsonUnspentReport();
		results.push_back(aUnspentReport);
	}
    else if (sItem == "upgradedbeaconreport")
    {
            Array aUpgBR = GetUpgradedBeaconReport();
            results.push_back(aUpgBR);
    }
    else if (sItem == "neuralreport")
    {
            Array myNeuralJSON = GetJSONNeuralNetworkReport();
            results.push_back(myNeuralJSON);
    }
    else if (sItem == "currentneuralreport")
    {
            Array myNeuralJSON = GetJSONCurrentNeuralNetworkReport();
            results.push_back(myNeuralJSON);
    }
    else if (sItem == "tallyneural")
    {
            ComputeNeuralNetworkSupermajorityHashes();
            UpdateNeuralNetworkQuorumData();
            entry.push_back(Pair("Ready","."));
            results.push_back(entry);
    }
    else if (sItem == "versionreport")
    {
            Array myNeuralJSON = GetJSONVersionReport();
            results.push_back(myNeuralJSON);
    }
    else if (sItem=="myneuralhash")
    {
        #if defined(WIN32) && defined(QT_GUI)
            std::string myNeuralHash = qtGetNeuralHash("");
            entry.push_back(Pair("My Neural Hash",myNeuralHash.c_str()));
            results.push_back(entry);
        #endif
    }
    else if (sItem == "superblockage")
    {
        int64_t superblock_age = GetAdjustedTime() - mvApplicationCacheTimestamp["superblock;magnitudes"];
        entry.push_back(Pair("Superblock Age",superblock_age));
        std::string timestamp = TimestampToHRDate(mvApplicationCacheTimestamp["superblock;magnitudes"]);
        entry.push_back(Pair("Superblock Timestamp",timestamp));
        entry.push_back(Pair("Superblock Block Number",mvApplicationCache["superblock;block_number"]));
        double height = cdbl(ReadCache("neuralsecurity","pending"),0);
        entry.push_back(Pair("Pending Superblock Height",height));
        results.push_back(entry);
    }
    else if (sItem == "unusual")
    {
        
            UnusualActivityReport();
            entry.push_back(Pair("UAR",1));
            results.push_back(entry);


    }
    else if (sItem == "neuralhash")
    {
            double popularity = 0;
            std::string consensus_hash = GetNeuralNetworkSupermajorityHash(popularity);
            entry.push_back(Pair("Popular",consensus_hash));
            results.push_back(entry);
    }
    else if (sItem == "currentneuralhash")
    {
            double popularity = 0;
            std::string consensus_hash = GetCurrentNeuralNetworkSupermajorityHash(popularity);
            entry.push_back(Pair("Popular",consensus_hash));
            results.push_back(entry);
    }
    else if (sItem=="updatequorumdata")
    {
            UpdateNeuralNetworkQuorumData();
            entry.push_back(Pair("Updated.",""));
            results.push_back(entry);
    }
    else if (sItem == "askforoutstandingblocks")
    {
        bool fResult = AskForOutstandingBlocks(uint256(0));
        entry.push_back(Pair("Sent.",fResult));
        results.push_back(entry);
    }
    else if (sItem == "joindao")
    {
        //execute joindao dao_symbol your_receive_grc_address your_email

        if (params.size() != 5)
        {
            entry.push_back(Pair("Error","You must specify the DAO_CURRENCY_SYMBOL, your nickname, your GRC_RECEIVING_ADDRESS, and Your_Email (do not use your boinc email; this is used for emergency communication from the DAO to clients).  For example: execute joindao CRG jondoe SKBQaegxasEyBVxmsTMg3HnasNg77CtjLo myemail@mail.com"));
            results.push_back(entry);
        }
        else
        {
                std::string symbol       = params[1].get_str();
                std::string nickname     = params[2].get_str();
                std::string grc          = params[3].get_str();
                std::string client_email = params[4].get_str();
                boost::to_upper(grc);
                boost::to_upper(symbol);
                boost::to_upper(client_email);

                CBitcoinAddress address(grc);
                bool isValid = address.IsValid();
                std::string               err = "";
                if (symbol.length() != 3 && symbol.length() != 4) err = "Symbol must be 3-4 characters.";
                if (nickname.empty())     err = "You must specify a nickname.";
                if (!isValid)             err = "You must specify a valid GRC receiving address.";
                std::string OrgPubKey    = ReadCache("daopubkey",symbol);
                std::string CachedSymbol = ReadCache("daosymbol",symbol);
                std::string CachedName   = ReadCache("daoname",CachedSymbol);
                if (CachedSymbol.empty())  err = "DAO does not exist.  Please choose a different symbol.";
                if (OrgPubKey.empty() || CachedName.empty())  err = "DAO does not exist or cannot be found.  Please choose a different DAO Symbol.";

                
                if (!err.empty())
                {
                    entry.push_back(Pair("Error",err));
                    results.push_back(entry);
                }
                else
                {
                    //Generate the key pair for the client
                    CKey key;
                    key.MakeNewKey(false);
                    CPrivKey vchPrivKey = key.GetPrivKey();
                    std::string PrivateKey =  HexStr<CPrivKey::iterator>(vchPrivKey.begin(), vchPrivKey.end());
                    std::string PubKey = HexStr(key.GetPubKey().Raw());
                    std::string sAction = "add";
                    std::string sType = "daoclient";
                    std::string sPass = PrivateKey;
                    std::string sName = nickname + "-" + symbol;
                    std::string contract = "<NAME>" + nickname + "</NAME><SYMBOL>" + symbol + "</SYMBOL><ADDRESS>" + grc + "</ADDRESS><PUBKEY>" + PubKey + "</PUBKEY><EMAIL>" + client_email + "</EMAIL>";

                    std::string result = AddMessage(true,sType,sName,contract,sPass,AmountFromValue(2500),.1,PubKey);
                    entry.push_back(Pair("Nickname",nickname));
                    entry.push_back(Pair("SYMBOL",symbol));
                    entry.push_back(Pair("Default Receive Address",grc));

                    std::string sKeyNarr = "daoclient" + nickname + "-" + symbol + "=" + PrivateKey;
                    entry.push_back(Pair("PrivateKey",sKeyNarr));
                    entry.push_back(Pair("Warning!","Do not lose your private key.  It is non-recoverable.  You may add it to your config file as noted above OR specify it manually via RPC commands."));
                    results.push_back(entry);
    
                }
            }
    }
    
    else if (sItem == "readconfig")
    {
        ReadConfigFile(mapArgs, mapMultiArgs);

    }
    else if (sItem == "readfeedvalue")
    {
        //execute readfeedvalue org_name feed_key
        if (params.size() != 3)
        {
            entry.push_back(Pair("Error","You must specify the org_name, and feed_key: Ex.: execute readfeedvalue COOLORG shares."));
            results.push_back(entry);
        }
        else
        {
                std::string orgname   = params[1].get_str();
                std::string feedkey   = params[2].get_str();
                std::string symbol    = ReadCache("daosymbol",orgname);
                boost::to_upper(orgname);
                boost::to_upper(feedkey);
                boost::to_upper(symbol);

                if (symbol.empty())
                {
                    entry.push_back(Pair("Error","DAO does not exist."));
                    results.push_back(entry);
                }
                else
                {
                    //Get the latest feed value (daofeed,symbol-feedkey)
                    std::string contract = ReadCache("daofeed",symbol+"-"+feedkey);
                    std::string OrgPubKey = ReadCache("daopubkey",orgname);
                    std::string feed_value = ExtractXML(contract,"<FEEDVALUE>","</FEEDVALUE>");
                    entry.push_back(Pair("Contract",contract));
                    entry.push_back(Pair(feedkey,feed_value));
                    results.push_back(entry);
                }
        }

    }
    else if (sItem == "daowithdrawalrequest")
    {
        //execute sendfeed org_name feed_key feed_value
        if (params.size() != 4)
        {
            entry.push_back(Pair("Error","You must specify your nickname, the dao_symbol and amount: Ex.: execute daowithdrawalrequest jondoe CLG 10000"));
            results.push_back(entry);
        }
        else
        {
                
                std::string nickname  = params[1].get_str();
                std::string symbol    = params[2].get_str();
                std::string sAmount   = params[3].get_str();
                boost::to_upper(symbol);
                std::string orgname   = ReadCache("daoname",symbol);
            
                if (orgname.empty())
                {
                    entry.push_back(Pair("Error","DAO does not exist."));
                    results.push_back(entry);
                }
                else
                {
                    ReadConfigFile(mapArgs, mapMultiArgs);
                                        
                    std::string privkey   = GetArgument("daoclient" + nickname+"-"+symbol, "");
                    std::string OrgPubKey = ReadCache("daoclientpubkey",nickname+"-"+symbol);
        
                    if (OrgPubKey.empty())
                    {
                        entry.push_back(Pair("Error","Public Key is missing. Org is corrupted or not yet synchronized."));
                        results.push_back(entry);
                    }
                    else
                    {
        
                        if (privkey.empty())
                        {
                            entry.push_back(Pair("Error","Private Key is missing.  To send a message to a dao, you must set the private key." 
                            + symbol + " key."));
                            results.push_back(entry);
                        }
                        else
                        {
                            std::string sAction = "add";
                            std::string sType = "daowithdrawalrequest";
                            std::string contract = "<NAME>" + nickname + "</NAME><AMOUNT>" + sAmount + "</AMOUNT>";
                            std::string result = AddMessage(true,sType,nickname+"-"+symbol,
                                contract,privkey,AmountFromValue(2500),.01,OrgPubKey);
                            entry.push_back(Pair("SYMBOL",symbol));
                            entry.push_back(Pair("PrivKeyPrefix",privkey.substr(0,10)));
                            entry.push_back(Pair("Amount",sAmount));
                            entry.push_back(Pair("Results",result));
                            results.push_back(entry);
                        }
                    }
                }
        }
    }
    
    else if (sItem == "sendfeed")
    {
        //execute sendfeed org_name feed_key feed_value
        if (params.size() != 4)
        {
            entry.push_back(Pair("Error","You must specify the orgname, feed_key and feed_value: Ex.: execute sendfeed COOLORG shares 10000"));
            results.push_back(entry);
        }
        else
        {
                std::string orgname   = params[1].get_str();
                std::string feedkey   = params[2].get_str();
                std::string feedvalue = params[3].get_str();
                boost::to_upper(orgname);
                std::string symbol    = ReadCache("daosymbol",orgname);
        
                boost::to_upper(feedkey);
                boost::to_upper(symbol);
                boost::to_upper(feedvalue);
            
                if (symbol.empty())
                {
                    entry.push_back(Pair("Error","DAO does not exist."));
                    results.push_back(entry);
                }
                else
                {
                    ReadConfigFile(mapArgs, mapMultiArgs);
                    std::string privkey   = GetArgument("dao" + symbol, "");
                    std::string OrgPubKey = ReadCache("daopubkey",orgname);
        
                    if (OrgPubKey.empty())
                    {
                        entry.push_back(Pair("Error","Public Key is missing. Org is corrupted or not yet synchronized."));
                        results.push_back(entry);
                    }
                    else
                    {
        
                        if (privkey.empty())
                        {
                            entry.push_back(Pair("Error","Private Key is missing.  To update a feed value, you must set the dao" 
                            + symbol + " key."));
                            results.push_back(entry);
                        }
                        else
                        {
                            std::string sAction = "add";
                            std::string sType = "daofeed";
                            std::string contract = "<FEEDKEY>" + feedkey + "</FEEDKEY><FEEDVALUE>" + feedvalue + "</FEEDVALUE>";
                            std::string result = AddMessage(true,sType,symbol+"-"+feedkey,
                                contract,privkey,AmountFromValue(2500),.01,OrgPubKey);
                            entry.push_back(Pair("SYMBOL",symbol));
                            entry.push_back(Pair("Feed Key Field",feedkey));
                            entry.push_back(Pair("PrivKeyPrefix",privkey.substr(0,10)));

                            entry.push_back(Pair("Feed Key Value",feedvalue));
                            entry.push_back(Pair("Results",result));
                            results.push_back(entry);
                        }
                    }
                }
        }
    }
    else if (sItem == "writedata")
    {
        if (params.size() != 3)
        {
            entry.push_back(Pair("Error","You must specify the Key and Value.  For example execute writedata dog_color black."));
            results.push_back(entry);
        }
        else
        {
                std::string sKey = params[1].get_str();
                std::string sValue = params[2].get_str();
                //CTxDB txdb("rw");
                CTxDB txdb;
                txdb.TxnBegin();
                std::string result = "Success.";
                if (!txdb.WriteGenericData(sKey,sValue)) result = "Unable to write.";
                if (!txdb.TxnCommit()) result = "Unable to Commit.";
                entry.push_back(Pair("Result",result));
                results.push_back(entry);
        }
            
    }
    else if (sItem == "readdata")
    {
        if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify the Key.  For example execute readdata dog_color."));
            results.push_back(entry);
        }
        else
        {
                std::string sKey = params[1].get_str();
                std::string sValue = "?";
                //CTxDB txdb("cr");
                CTxDB txdb;
                if (!txdb.ReadGenericData(sKey,sValue)) 
                {
                        entry.push_back(Pair("Error",sValue));
            
                        sValue = "Failed to read from disk.";
                }
                entry.push_back(Pair("Key",sKey));
                
                entry.push_back(Pair("Result",sValue));
                results.push_back(entry);
        }
            
    }
    else if (sItem == "refhash")
    {
        if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify the Reference Hash."));
            results.push_back(entry);
        }
        else
        {
                std::string rh = params[1].get_str();
                bool r1 = StrLessThanReferenceHash(rh);
                bool r2 = NeuralNodeParticipates();
                entry.push_back(Pair("<Ref Hash",r1));
                entry.push_back(Pair("WalletAddress<Ref Hash",r2));
                results.push_back(entry);

        }
        
    }
    else if (sItem == "vote")
    {
        if (params.size() != 3)
        {
            entry.push_back(Pair("Error","You must specify the Poll Title, and the Answer.  For example: execute vote gender male."));
            results.push_back(entry);
        }
        else
        {
                std::string Title = params[1].get_str();
                std::string Answer = params[2].get_str();
                if (Title=="" || Answer == "" ) 
                {
                            entry.push_back(Pair("Error","You must specify both the answer and the title."));
                            results.push_back(entry);
    
                }
                else
                {
                    //Verify the Existence of the poll, the acceptability of the answer, and the expiration of the poll: (EXIST, EXPIRED, ACCEPTABLE)
                    //If polltype == 1, use magnitude, if 2 use Balance, if 3 use hybrid:
                    //6-20-2015
                    double nBalance = GetTotalBalance();
                    uint256 hashRand = GetRandHash();
                    std::string email = GetArgument("email", "NA");
                    boost::to_lower(email);
                    GlobalCPUMiningCPID.email=email;
                    GlobalCPUMiningCPID.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, hashRand);
                    GlobalCPUMiningCPID.lastblockhash = GlobalCPUMiningCPID.cpidhash;

                    if (!PollExists(Title))
                    {
                            entry.push_back(Pair("Error","Poll does not exist."));
                            results.push_back(entry);
    
                    }
                    else
                    {
                        if (PollExpired(Title))
                        {
                            entry.push_back(Pair("Error","Sorry, Poll is already expired."));
                            results.push_back(entry);
                        }
                        else
                        {
                            if (!PollAcceptableAnswer(Title,Answer))
                            {
                                std::string acceptable_answers = PollAnswers(Title);
                                entry.push_back(Pair("Error","Sorry, Answer " + Answer + " is not one of the acceptable answers, allowable answers are: " + acceptable_answers + ".  If you are voting multiple choice, please use a semicolon delimited vote string such as : 'dog;cat'."));
                                results.push_back(entry);
                            }
                            else
                            {
                                std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID);
                                std::string GRCAddress = DefaultWalletAddress();
                                StructCPID structMag = GetInitializedStructCPID2(GlobalCPUMiningCPID.cpid,mvMagnitudes);
                                double dmag = structMag.Magnitude;
                                double poll_duration = PollDuration(Title)*86400;

                                // Prevent Double Voting
                                std::string cpid1 = GlobalCPUMiningCPID.cpid;
                                std::string GRCAddress1 = DefaultWalletAddress();
                                GetEarliestStakeTime(GRCAddress1,cpid1);
                                int64_t nGRCTime = mvApplicationCacheTimestamp["nGRCTime"];
                                int64_t nCPIDTime = mvApplicationCacheTimestamp["nCPIDTime"];
                                double cpid_age = GetAdjustedTime() - nCPIDTime;
                                double stake_age = GetAdjustedTime() - nGRCTime;

                                // Phase II - Prevent Double Voting
                                                                                                
                                StructCPID structGRC = GetInitializedStructCPID2(GRCAddress,mvMagnitudes);

                                
                                printf("CPIDAge %f,StakeAge %f,Poll Duration %f \r\n",cpid_age,stake_age,poll_duration);

                                double dShareType= cdbl(GetPollXMLElementByPollTitle(Title,"<SHARETYPE>","</SHARETYPE>"),0);
                            
                                // Share Type 1 == "Magnitude"
                                // Share Type 2 == "Balance"
                                // Share Type 3 == "Both"
                                if (cpid_age < poll_duration) dmag = 0;
                                if (stake_age < poll_duration) nBalance = 0;

                                if ((dShareType == 1) && cpid_age < poll_duration) 
                                {
                                    entry.push_back(Pair("Error","Sorry, When voting in a magnitude poll, your CPID must be older than the poll duration."));
                                    results.push_back(entry);
                                }
                                else if (dShareType == 2 && stake_age < poll_duration)
                                {
                                    entry.push_back(Pair("Error","Sorry, When voting in a Balance poll, your stake age must be older than the poll duration."));
                                    results.push_back(entry);
                                }
                                else if (dShareType == 3 && stake_age < poll_duration && cpid_age < poll_duration)
                                {
                                    entry.push_back(Pair("Error","Sorry, When voting in a Both Share Type poll, your stake age Or your CPID age must be older than the poll duration."));
                                    results.push_back(entry);
                                }
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
									entry.push_back(Pair("Success",narr + " " + "Your vote has been cast for topic " + Title + ": With an Answer of " + Answer + ": " + result.c_str()));
									results.push_back(entry);
                                }
                            }
                        }
                    }

               }
          }

    }
    else if (sItem == "addpoll")
    {
        if (params.size() != 7)
        {
            entry.push_back(Pair("Error","You must specify the Poll Title, Expiration In DAYS from Now, Question, Answers delimited by a semicolon, ShareType (1=Magnitude,2=Balance,3=Both,4=CPIDCount,5=ParticipantCount) and discussion URL (use TinyURL.com to make a small URL).  Please use underscores in place of spaces inside a sentence.  "));
            results.push_back(entry);
        }
        else
        {
                std::string Title = params[1].get_str();
                std::string Days = params[2].get_str();
                double days = cdbl(Days,0);
                std::string Question = params[3].get_str();
                std::string Answers = params[4].get_str();
                std::string ShareType = params[5].get_str();
                std::string sURL = params[6].get_str();
                double sharetype = cdbl(ShareType,0);
                if (Title=="" || Question == "" || Answers == "") 
                {
                        entry.push_back(Pair("Error","You must specify a Poll Title, Poll Question and Poll Answers."));
                        results.push_back(entry);
                }
                else
                {
                if (days < 7) 
                {
                        entry.push_back(Pair("Error","Minimum duration is 7 days; please specify a longer poll duration."));
                        results.push_back(entry);
                }
                else
                {
                    double nBalance = GetTotalBalance();
                
                    if (nBalance < 100000)
                    {
                        entry.push_back(Pair("Error","You must have a balance > 100,000 GRC to create a poll.  Please post the desired poll on cryptocointalk or PM RTM with the poll."));
                        results.push_back(entry);
                    }
                    else
                    {
                        if (days < 0 || days == 0)
                        {
                            entry.push_back(Pair("Error","You must specify a positive value for days for the expiration date."));
                            results.push_back(entry);
            
                        }
                        else
                        {
                            if (sharetype != 1 && sharetype != 2 && sharetype != 3 && sharetype !=4 && sharetype !=5)
                            {
                                entry.push_back(Pair("Error","You must specify a value of 1, 2, 3, 4 or 5 for the sharetype."));
                                results.push_back(entry);

                            }
                            else
                            {
                                std::string expiration = RoundToString(GetAdjustedTime() + (days*86400),0);
                                std::string contract = "<TITLE>" + Title + "</TITLE><DAYS>" + RoundToString(days,0) + "</DAYS><QUESTION>" + Question + "</QUESTION><ANSWERS>" + Answers + "</ANSWERS><SHARETYPE>" + RoundToString(sharetype,0) + "</SHARETYPE><URL>" + sURL + "</URL><EXPIRATION>" + expiration + "</EXPIRATION>";
                                std::string result = AddContract("poll",Title,contract);
                                entry.push_back(Pair("Success","Your poll has been added: " + result));
                                results.push_back(entry);
                            }

                       }
                  }
               }
            }
        }

    }
    else if (sItem == "votedetails")
    {
        if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify the Poll Title."));
            results.push_back(entry);
        }
        else
        {
            std::string Title1 = params[1].get_str();
                
            if (!PollExists(Title1))
            {
                entry.push_back(Pair("Error","Poll does not exist.  Please execute listpolls."));
                results.push_back(entry);

            }
            else
            {
                std::string Title = params[1].get_str();
                Array myVotes = GetJsonVoteDetailsReport(Title);
                results.push_back(myVotes);
            }
        }
    }
    else if (sItem == "listpolls")
    {
            std::string out1 = "";
            Array myPolls = GetJSONPollsReport(false,"",out1,false);
            results.push_back(myPolls);
    }
    else if (sItem == "listallpolls")
    {
            std::string out1 = "";
            Array myPolls = GetJSONPollsReport(false,"",out1,true);
            results.push_back(myPolls);
    }
    else if (sItem == "listallpolldetails")
    {
            std::string out1 = "";
            Array myPolls = GetJSONPollsReport(true,"",out1,true);
            results.push_back(myPolls);

    }
    else if (sItem=="listpolldetails")
    {
            std::string out1 = "";
            Array myPolls = GetJSONPollsReport(true,"",out1,false);
            results.push_back(myPolls);
    }
    else if (sItem=="listpollresults")
    {
        bool bIncExpired = false;
        if (params.size() == 3)
        {
            std::string include_expired = params[2].get_str();
            if (include_expired=="true") bIncExpired=true;
        }

        if (params.size() != 2 && params.size() != 3)
        {
            entry.push_back(Pair("Error","You must specify the Poll Title.  Example: listpollresults pollname.  You may also specify execute listpollresults pollname true if you want to see expired polls."));
            results.push_back(entry);
        }
        else
        {
            std::string Title1 = params[1].get_str();
                
            if (!PollExists(Title1))
            {
                entry.push_back(Pair("Error","Poll does not exist.  Please execute listpolls."));
                results.push_back(entry);

            }
            else
            {
                std::string Title = params[1].get_str();
                std::string out1 = "";
                Array myPolls = GetJSONPollsReport(true,Title,out1,bIncExpired);
                results.push_back(myPolls);
            }
        }
    }
    else if (sItem=="staketime")
    {
            std::string cpid = GlobalCPUMiningCPID.cpid;
            std::string GRCAddress = DefaultWalletAddress();
            GetEarliestStakeTime(GRCAddress,cpid);
            entry.push_back(Pair("GRCTime",mvApplicationCacheTimestamp["nGRCTime"]));
            entry.push_back(Pair("CPIDTime",mvApplicationCacheTimestamp["nCPIDTime"]));
            results.push_back(entry);
    }
    else if (sItem=="testnewcontract")
    {
        std::string contract = "";
        std::string myNeuralHash = "";
        #if defined(WIN32) && defined(QT_GUI)
            contract = qtGetNeuralContract("");
        #endif
        #if defined(WIN32) && defined(QT_GUI)
            myNeuralHash = qtGetNeuralHash("");
            entry.push_back(Pair("My Neural Hash",myNeuralHash.c_str()));
            results.push_back(entry);
        #endif
        LoadSuperblock(contract,GetAdjustedTime(),280000);
        entry.push_back(Pair("Contract Test",contract));
        // Convert to Binary
        std::string sBin = PackBinarySuperblock(contract);
        entry.push_back(Pair("Contract Length",(double)contract.length()));
        entry.push_back(Pair("Binary Length",(double)sBin.length()));
        //entry.push_back(Pair("Binary",sBin.c_str()));
        // Hash of current superblock
        std::string sUnpacked = UnpackBinarySuperblock(sBin);
        entry.push_back(Pair("Unpacked length",(double)sUnpacked.length()));

        entry.push_back(Pair("Unpacked",sUnpacked.c_str()));
        std::string neural_hash = GetQuorumHash(contract);
        std::string binary_neural_hash = GetQuorumHash(sUnpacked);

        entry.push_back(Pair("Local Core Quorum Hash",neural_hash));
        entry.push_back(Pair("Binary Local Core Quorum Hash",binary_neural_hash));

        entry.push_back(Pair("Neural Network Live Quorum Hash",myNeuralHash));
        results.push_back(entry);
    }
    else if (sItem == "forcequorum")
    {
            AsyncNeuralRequest("quorum","gridcoin",10);
            entry.push_back(Pair("Requested a quorum - waiting for resolution.",1));
            results.push_back(entry);
    }
    else if (sItem == "explainmagnitude2")
    {
        bool force = false;
        if (params.size() == 2)
        {
            std::string optional = params[1].get_str();
            boost::to_lower(optional);
            if (optional == "force") force = true;
        }

        if (force) msNeuralResponse = "";
        if (msNeuralResponse=="")
        {
            AsyncNeuralRequest("explainmag",GlobalCPUMiningCPID.cpid,10);
            entry.push_back(Pair("Requested Explain Magnitude For",GlobalCPUMiningCPID.cpid));
        }

        std::vector<std::string> vMag = split(msNeuralResponse.c_str(),"<ROW>");
        for (unsigned int i = 0; i < vMag.size(); i++)
        {
                entry.push_back(Pair(RoundToString(i+1,0),vMag[i].c_str()));
        }
        if (msNeuralResponse=="") 
        {
                    entry.push_back(Pair("Response","No response."));
        }

        results.push_back(entry);
    }
    else if (sItem == "tally")
    {
            bNetAveragesLoaded = false;
            nLastTallied = 0;
            TallyResearchAverages(true);
            entry.push_back(Pair("Tally Network Averages",1));
            results.push_back(entry);
    }
    else if (sItem == "peek")
    {
            std::vector<std::string> s = split(msPeek,"<CR>");
            
            for (int i = 0; i < ((int)(s.size()-1)); i++)
            {
                entry.push_back(Pair(s[i],i + 1));
            }
            results.push_back(entry);
    }
    else if (sItem == "encrypt")
    {
        //Encrypt a phrase
        if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must also specify a wallet passphrase."));
            results.push_back(entry);
        }
        else
        {
            std::string sParam = params[1].get_str();
            std::string encrypted = AdvancedCryptWithHWID(sParam);
            entry.push_back(Pair("Passphrase",encrypted));
            entry.push_back(Pair("[Specify in config file] autounlock=",encrypted));
            results.push_back(entry);
        }
    
    }
    else if (sItem == "testboinckey")
    {
        std::string sType="project";

        std::string strMasterPrivateKey = (sType=="project" || sType=="projectmapping") ? GetArgument("masterprojectkey", msMasterMessagePrivateKey) : msMasterMessagePrivateKey;
        std::string sig = SignMessage("hello",strMasterPrivateKey);
        entry.push_back(Pair("hello",sig));
        //Forged Key
        std::string forgedKey = "3082011302010104202b1c9faef66d42218eefb7c66fb6e49292972c8992b4100bb48835d325ec2d34a081a53081a2020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a144034200040cb218ee7495c2ba5d6ba069f97810e85dae8b446c0e4a1c6dec7fe610ab0fa4378bda5320f00e7a08a8e428489f41ad79d0428a091aa548aca18adbbbe64d41";
        std::string sig2 = SignMessage("hello",forgedKey);
        bool r10 = CheckMessageSignature("R",sType,"hello",sig2,"");
        entry.push_back(Pair("FK10",r10));
        std::string sig3 = SignMessage("hi",strMasterPrivateKey);
        bool r11 = CheckMessageSignature("R","project","hi",sig3,"");
        entry.push_back(Pair("FK11",r11));
        bool r12 = CheckMessageSignature("R","general","1",sig3,"");
        entry.push_back(Pair("FK12",r12));
        results.push_back(entry);
    
    }
    else if (sItem == "genboinckey")
    {
        //Gridcoin - R Halford - Generate Boinc Mining Key - 2-6-2015
        GetNextProject(false);
        std::string email = GetArgument("email", "NA");
        boost::to_lower(email);
        GlobalCPUMiningCPID.email=email;
        GlobalCPUMiningCPID.cpidv2 = ComputeCPIDv2(GlobalCPUMiningCPID.email, GlobalCPUMiningCPID.boincruntimepublickey, 0);
        //Store the BPK in the aesskein, and the cpid in version
        GlobalCPUMiningCPID.aesskein = email; //Good
        GlobalCPUMiningCPID.lastblockhash = GlobalCPUMiningCPID.cpidhash;

        std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID);
        if (fDebug3) printf("GenBoincKey: Utilizing email %s with %s for  %s \r\n",GlobalCPUMiningCPID.email.c_str(),GlobalCPUMiningCPID.boincruntimepublickey.c_str(),sParam.c_str());
        std::string sBase = EncodeBase64(sParam);
        entry.push_back(Pair("[Specify in config file without quotes] boinckey=",sBase));
        results.push_back(entry);
    }
    else if (sItem == "encryptphrase")
    {
            if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify a phrase"));
            results.push_back(entry);
        }
        else
        {
            std::string sParam1 = params[1].get_str();
            entry.push_back(Pair("Param1",sParam1));
            std::string test = AdvancedCrypt(sParam1);
            entry.push_back(Pair("EncPhrase",test));
            results.push_back(entry);
    
        }
    
    }

    else if (sItem == "decryptphrase")
    {
            if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify a phrase"));
            results.push_back(entry);
        }
        else
        {
            std::string sParam1 = params[1].get_str();
            entry.push_back(Pair("Param1",sParam1));
            std::string test = AdvancedDecrypt(sParam1);
            entry.push_back(Pair("DecPhrase",test));
            results.push_back(entry);
    
        }
    
    }
    else if (sItem == "dportally")
    {
        TallyMagnitudesInSuperblock();
        entry.push_back(Pair("Done","Done"));
        results.push_back(entry);
    
    }
    else if (sItem == "addkey")
    {
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

        if (params.size() != 5)
        {
            entry.push_back(Pair("Error","You must specify an action, message type, message name and value."));
            entry.push_back(Pair("Example","execute addkey add_or_delete keytype projectname value."));
            results.push_back(entry);
        }
        else
        {
            //execute addkey add project grid20
            std::string sAction = params[1].get_str();
            entry.push_back(Pair("Action",sAction));
            bool bAdd = (sAction=="add") ? true : false;
            std::string sType = params[2].get_str();
            entry.push_back(Pair("Type",sType));
            std::string sPass = "";
            sPass = (sType=="project" || sType=="projectmapping") ? GetArgument("masterprojectkey", msMasterMessagePrivateKey) : msMasterMessagePrivateKey;
            if (sType=="beacon" && sAction=="delete")  sPass = GetArgument("masterprojectkey","");
            entry.push_back(Pair("Passphrase",sPass));
            std::string sName = params[3].get_str();
            entry.push_back(Pair("Name",sName));
            std::string sValue = params[4].get_str();
            entry.push_back(Pair("Value",sValue));
            std::string result = AddMessage(bAdd,sType,sName,sValue,sPass,AmountFromValue(5),.1,"");
            entry.push_back(Pair("Results",result));
            results.push_back(entry);
        }
    }
    else if (sItem == "memorizekeys")
    {
        std::string sOut = "";
        LoadAdminMessages(true,sOut);
        entry.push_back(Pair("Results",sOut));
        results.push_back(entry);
        
    }
    else if (sItem == "superblockaverage")
    {
        std::string superblock = ReadCache("superblock","all");
        double out_beacon_count = 0;
        double out_participant_count = 0;
        double out_avg = 0;
        double avg = GetSuperblockAvgMag(superblock,out_beacon_count,out_participant_count,out_avg,false,nBestHeight);
        entry.push_back(Pair("avg",avg));
        entry.push_back(Pair("beacon_count",out_beacon_count));
        entry.push_back(Pair("beacon_participant_count",out_participant_count));
        entry.push_back(Pair("average_magnitude",out_avg));
        entry.push_back(Pair("superblock_valid",VerifySuperblock(superblock,pindexBest->nHeight)));
        int64_t superblock_age = GetAdjustedTime() - mvApplicationCacheTimestamp["superblock;magnitudes"];
        entry.push_back(Pair("Superblock Age",superblock_age));
        bool bDireNeed = NeedASuperblock();
        entry.push_back(Pair("Dire Need of Superblock",bDireNeed));
        results.push_back(entry);
    }
    else if (sItem == "currentcontractaverage")
    {
        std::string contract = "";
        #if defined(WIN32) && defined(QT_GUI)
                    contract = qtGetNeuralContract("");
        #endif
        entry.push_back(Pair("Contract",contract));
        double out_beacon_count = 0;
        double out_participant_count = 0;
        double out_avg = 0;
        double avg = GetSuperblockAvgMag(contract,out_beacon_count,out_participant_count,out_avg,false,nBestHeight);
        bool bValid = VerifySuperblock(contract,pindexBest->nHeight);
        entry.push_back(Pair("avg",avg));
        entry.push_back(Pair("beacon_count",out_beacon_count));
        entry.push_back(Pair("avg_mag",out_avg));
        entry.push_back(Pair("beacon_participant_count",out_participant_count));
        entry.push_back(Pair("superblock_valid",bValid));
        //Show current contract neural hash
        std::string sNeuralHash = "";

        #if defined(WIN32) && defined(QT_GUI)
            sNeuralHash = qtGetNeuralHash("");
            entry.push_back(Pair(".NET Neural Hash",sNeuralHash.c_str()));
        #endif
    
        entry.push_back(Pair("Length",(double)contract.length()));
        std::string neural_hash = GetQuorumHash(contract);
        entry.push_back(Pair("Wallet Neural Hash",neural_hash));
        
        results.push_back(entry);
        
    }
    else if (sItem == "getlistof")
    {
        if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify a keytype."));
            results.push_back(entry);
        }
        else
        {
            std::string sType = params[1].get_str();
            entry.push_back(Pair("Key Type",sType));
            std::string data = GetListOf(sType);
            entry.push_back(Pair("Data",data));
            results.push_back(entry);
        }


    }
    else if (sItem == "listdata")
    {
        if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify a keytype (IE execute dumpkeys project)"));
            results.push_back(entry);
        }
        else
        {
            std::string sType = params[1].get_str();
            entry.push_back(Pair("Key Type",sType));
            for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
            {
                std::string key_name  = (*ii).first;
                if (key_name.length() > sType.length())
                {
                    if (key_name.substr(0,sType.length())==sType)
                    {
                                std::string key_value = mvApplicationCache[(*ii).first];
                                entry.push_back(Pair(key_name,key_value));
                    }
               
                }
           }
           results.push_back(entry);
        }

    }
    else if (sItem == "genorgkey")
    {
        if (params.size() != 3)
        {
            entry.push_back(Pair("Error","You must specify a passphrase and organization name"));
            results.push_back(entry);
        }
        else
        {
            std::string sParam1 = params[1].get_str();
            entry.push_back(Pair("Passphrase",sParam1));
            std::string sParam2 = params[2].get_str();
            entry.push_back(Pair("OrgName",sParam2));

            std::string sboinchashargs = LegacyDefaultBoincHashArgs();
            if (sParam1 != sboinchashargs)
            {
                entry.push_back(Pair("Error","Admin must be logged in"));
            }
            else
            {
                std::string modulus = sboinchashargs.substr(0,12);
                std::string key = sParam2 + "," + AdvancedCryptWithSalt(modulus,sParam2);
                entry.push_back(Pair("OrgKey",key));
            }
            results.push_back(entry);
    
        }
    
    }
    else if (sItem == "chainrsa")
    {
        if (params.size() != 2)
        {
            entry.push_back(Pair("Error","You must specify a cpid"));
            results.push_back(entry);
        }
        else
        {
            std::string sParam1 = params[1].get_str();
            entry.push_back(Pair("CPID",sParam1));
            
        }


    }
    else if (sItem == "testorgkey")
    {
        if (params.size() != 3)
        {
            entry.push_back(Pair("Error","You must specify an org and public key"));
            results.push_back(entry);
        }
        else
        {
            std::string sParam1 = params[1].get_str();
            entry.push_back(Pair("Org",sParam1));
            std::string sParam2 = params[2].get_str();
            entry.push_back(Pair("Key",sParam2));
            std::string key = sParam1 + "," + AdvancedDecryptWithSalt(sParam2,sParam1);
            entry.push_back(Pair("PubKey",key));
            results.push_back(entry);
    
        }
    
    }

    else if (sItem == "testcpidv2")
    {
        if (params.size() != 3)
        {
            entry.push_back(Pair("Error","You must specify both parameters boincruntimepublickey and boinccpid"));
            results.push_back(entry);
        }
        else
        {
            std::string sParam1 = params[1].get_str();
            std::string sParam2 = params[2].get_str();
            entry.push_back(Pair("Param1",sParam1));
            entry.push_back(Pair("Param2",sParam2));
            //12-25-2014 Test CPIDv2
            std::string newcpid = ComputeCPIDv2(sParam2,sParam1,0);
            std::string shortcpid = RetrieveMd5(sParam1+sParam2);
            entry.push_back(Pair("CPID1",shortcpid));
            bool isvalid = CPID_IsCPIDValid(shortcpid, newcpid,0);
            entry.push_back(Pair("CPIDv2 is valid",isvalid));
            isvalid = CPID_IsCPIDValid(shortcpid, newcpid,10);
            entry.push_back(Pair("CPIDv2 is valid on bad block",isvalid));
            std::string me = ComputeCPIDv2(sParam2,sParam1,0);
            entry.push_back(Pair("CPIDv2 on block0",me));
            me = ComputeCPIDv2(sParam2,sParam1,10);
            entry.push_back(Pair("CPIDv2 on block10",me));
            results.push_back(entry);
        }
    

    }
    else if (sItem == "DISABLE_WINDOWS_ERROR_REPORTING")
    {
        std::string result = "FAIL";
        #if defined(WIN32) && defined(QT_GUI)
            qtGRCCodeExecutionSubsystem("DISABLE_WINDOWS_ERROR_REPORTING");
        #endif
        Object entry;
        entry.push_back(Pair("DISABLE_WINDOWS_ERROR_REPORTING",result));
        results.push_back(entry);
    }

    else if (sItem == "testcpid")
    {
        std::string bpk = "29dbf4a4f2e2baaff5f5e89e2df98bc8";
        std::string email = "ebola@gridcoin.us";
        uint256 block("0x000005a247b397eadfefa58e872bc967c2614797bdc8d4d0e6b09fea5c191599");
        std::string hi = "";
        //1
        WriteCPIDToRPC(email,bpk,block,results);
        //2
        email = "ebol349324923849023908429084892098023423432423423424332a@gridcoin.us";
        WriteCPIDToRPC(email,bpk,block,results);
        email="test";
        WriteCPIDToRPC(email,bpk,block,results);
        //Empty
        email="";
        WriteCPIDToRPC(email,bpk,block,results);
        //Wrong Block
        email="ebola@gridcoin.us";
        WriteCPIDToRPC(email,bpk,0,results);
        WriteCPIDToRPC(email,bpk,1,results);
        WriteCPIDToRPC(email,bpk,123499934534,results);
        //Stolen CPID either missing bpk or unknown email
        WriteCPIDToRPC(email,"0",0,results);

    }
    else if (sItem == "reindex")
    {
            int r=-1;
            #if defined(WIN32) && defined(QT_GUI)
            ReindexWallet();
            r = CreateRestorePoint();
            #endif 
            entry.push_back(Pair("Reindex Chain",r));
            results.push_back(entry);
    }
    else if (sItem == "downloadblocks")
    {
            int r=-1;
            #if defined(WIN32) && defined(QT_GUI)
                r = DownloadBlocks();
            #endif 
            entry.push_back(Pair("Download Blocks",r));
            results.push_back(entry);
    }
    else if (sItem == "executecode")
    {
            printf("Executing .net code\r\n");
            #if defined(WIN32) && defined(QT_GUI)
        
            ExecuteCode();
            #endif

    }
    else if (sItem == "volatilecode")
    {
        bExecuteCode = true;
        printf("Executing volatile code \r\n");
    }
    else if (sItem == "getnextproject")
    {
            GetNextProject(true);
            entry.push_back(Pair("GetNext",1));
            results.push_back(entry);
    }
    else if (sItem == "resetcpids")
    {
            //Reload the config file
            ReadConfigFile(mapArgs, mapMultiArgs);
            mvCPIDCache.clear();
            HarvestCPIDs(true);
            GetNextProject(true);
            entry.push_back(Pair("Reset",1));
            results.push_back(entry);
    }
    else if (sItem == "backupwallet")
    {
            std::string result = BackupGridcoinWallet();
            entry.push_back(Pair("Backup Wallet Result", result));
            results.push_back(entry);
    }
    else if (sItem == "restorewallet")
    {
            std::string result = RestoreGridcoinBackupWallet();
            entry.push_back(Pair("Restore Wallet Result", result));
            results.push_back(entry);
    }
    else if (sItem == "resendwallettx")
    {
            ResendWalletTransactions(true);
            entry.push_back(Pair("Resending unsent wallet transactions...",1));
            results.push_back(entry);
    } 
    else if (sItem == "encrypt_deprecated")
    {
            std::string s1 = "1234";
            std::string s1dec = AdvancedCrypt(s1);
            std::string s1out = AdvancedDecrypt(s1dec);
            entry.push_back(Pair("Execute Encrypt result1",s1));
            entry.push_back(Pair("Execute Encrypt result2",s1dec));
            entry.push_back(Pair("Execute Encrypt result3",s1out));
            results.push_back(entry);
    }
    else if (sItem == "debug")
    {
        if (params.size() == 2 && (params[1].get_str() == "true" || params[1].get_str() == "false"))
        {
            fDebug = (params[1].get_str() == "true") ? true : false;
            entry.push_back(Pair("Debug", fDebug ? "Entering debug mode." : "Exiting debug mode."));
        }
        else
            entry.push_back(Pair("Error","You must specify true or false as an option."));
        results.push_back(entry);
    }
    else if (sItem == "debugnet")
    {
        if (params.size() == 2 && (params[1].get_str() == "true" || params[1].get_str() == "false"))
        {
            fDebugNet = (params[1].get_str() == "true") ? true : false;
            entry.push_back(Pair("DebugNet", fDebugNet ? "Entering debug mode." : "Exiting debug mode."));
        }
        else
            entry.push_back(Pair("Error","You must specify true or false as an option."));
        results.push_back(entry);
    }
    else if (sItem == "debug2")
    {
        if (params.size() == 2 && (params[1].get_str() == "true" || params[1].get_str() == "false"))
        {
            fDebug2 = (params[1].get_str() == "true") ? true : false;
            entry.push_back(Pair("Debug2", fDebug2 ? "Entering debug mode." : "Exiting debug mode."));
        }
        else
            entry.push_back(Pair("Error","You must specify true or false as an option."));
        results.push_back(entry);
    }
    else if (sItem == "debug3")
    {
        if (params.size() == 2 && (params[1].get_str() == "true" || params[1].get_str() == "false"))
        {
            fDebug3 = (params[1].get_str() == "true") ? true : false;
            entry.push_back(Pair("Debug3", fDebug3 ? "Entering debug mode." : "Exiting debug mode."));
        }
        else
            entry.push_back(Pair("Error","You must specify true or false as an option."));
        results.push_back(entry);
    }
    else if (sItem == "debug4")
    {
        if (params.size() == 2 && (params[1].get_str() == "true" || params[1].get_str() == "false"))
        {
            fDebug4 = (params[1].get_str() == "true") ? true : false;
            entry.push_back(Pair("Debug4", fDebug4 ? "Entering debug mode." : "Exiting debug mode."));
        }
        else
            entry.push_back(Pair("Error","You must specify true or false as an option."));
        results.push_back(entry);
    }
    else if (sItem == "debug5")
    {
        if (params.size() == 2 && (params[1].get_str() == "true" || params[1].get_str() == "false"))
        {
            fDebug5 = (params[1].get_str() == "true") ? true : false;
            entry.push_back(Pair("Debug5", fDebug5 ? "Entering debug mode." : "Exiting debug mode."));
        }
        else
            entry.push_back(Pair("Error","You must specify true or false as an option."));
        results.push_back(entry);
    }
    else if (sItem == "debug10")
    {
        if (params.size() == 2 && (params[1].get_str() == "true" || params[1].get_str() == "false"))
        {
            fDebug10 = (params[1].get_str() == "true") ? true : false;
            entry.push_back(Pair("Debug10", fDebug10 ? "Entering debug mode." : "Exiting debug mode."));
        }
        else
            entry.push_back(Pair("Error","You must specify true or false as an option."));
        results.push_back(entry);
    }
    else
    {
            entry.push_back(Pair("Command " + sItem + " not found.",-1));
            results.push_back(entry);
    }
    return results;
        
}


Array LifetimeReport(std::string cpid)
{
       Array results;
       Object c;
       std::string Narr = RoundToString(GetAdjustedTime(),0);
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
      std::string Narr = RoundToString(GetAdjustedTime(),0);
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
                                        double avg_mag = GetSuperblockAvgMag(superblock,out_beacon_count,out_participant_count,out_avg,true,pblockindex->nHeight);
                                        if (avg_mag > 10)
                                        {
                                                Object c;
                                                c.push_back(Pair("Block #" + RoundToString(pblockindex->nHeight,0),pblockindex->GetBlockHash().GetHex()));
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
                    
                        }
      return results;

}

Array MagnitudeReport(std::string cpid)
{
           Array results;
           Object c;
           std::string Narr = RoundToString(GetAdjustedTime(),0);
           c.push_back(Pair("RSA Report",Narr));
           results.push_back(c);
           double total_owed = 0;
           double magnitude_unit = GRCMagnitudeUnit(GetAdjustedTime());
           msRSAOverview = "";
           if (!pindexBest) return results;
            
           try
           {
                   if (mvMagnitudes.size() < 1)
                   {
                           if (fDebug3) printf("no results");
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
                                                if (cpid==msPrimaryCPID && !msPrimaryCPID.empty() && msPrimaryCPID != "INVESTOR")
                                                {
                                                    msRSAOverview = "Exp PPD: " + RoundToString(dExpected14/14,0) 
                                                        + ", Act PPD: " + RoundToString(structMag.payments/14,0) 
                                                        + ", Fulf %: " + RoundToString(fulfilled,2) 
                                                        + ", GRCMagUnit: " + RoundToString(magnitude_unit,4);
                                                }
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
                    
                    if (fDebug3) printf("MR8");

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
                    if (fDebug3) printf("*MR5*");
                                    
                    return results;
            }
            catch(...)
            {
                printf("\r\nError in Magnitude Report \r\n ");
                return results;
            }

}



void CSVToFile(std::string filename, std::string data)
{
    boost::filesystem::path path = GetDataDir() / "reports" / filename;
    boost::filesystem::create_directories(path.parent_path());
    ofstream myCSV;
    myCSV.open (path.string().c_str());
    myCSV << data;
    myCSV.close();
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
        if (structMag.initialized && structMag.cpid.length() > 2 && structMag.cpid != "INVESTOR") 
        { 
            return structMag.Magnitude;
        }
        return 0;
}

bool HasActiveBeacon(const std::string& cpid)
{
    return GetBeaconPublicKey(cpid, false).empty() == false;
}

std::string RetrieveBeaconValueWithMaxAge(const std::string& cpid, int64_t iMaxSeconds)
{
    const std::string key = "beacon;" + cpid;
    const std::string& value = mvApplicationCache[key];

    // Compare the age of the beacon to the age of the current block. If we have
    // no current block we assume that the beacon is valid.
    int64_t iAge = pindexBest != NULL
          ? pindexBest->nTime - mvApplicationCacheTimestamp[key]
          : 0;

    return (iAge > iMaxSeconds)
          ? ""
          : value;
}

std::string GetBeaconPublicKey(const std::string& cpid, bool bAdvertisingBeacon)
{
   //3-26-2017 - Ensure beacon public key is within 6 months of network age (If advertising, let it be returned as missing after 5 months, to ensure the public key is renewed seamlessly).
   int iMonths = bAdvertisingBeacon ? 5 : 6;
   int64_t iMaxSeconds = 60 * 24 * 30 * iMonths * 60;
   std::string sBeacon = RetrieveBeaconValueWithMaxAge(cpid, iMaxSeconds);
   if (sBeacon.empty()) return "";
   // Beacon data structure: CPID,hashRand,Address,beacon public key: base64 encoded
   std::string sContract = DecodeBase64(sBeacon);
   std::vector<std::string> vContract = split(sContract.c_str(),";");
   if (vContract.size() < 4) return "";
   std::string sBeaconPublicKey = vContract[3];
   return sBeaconPublicKey;
}



void GetBeaconElements(std::string sBeacon,std::string& out_cpid, std::string& out_address, std::string& out_publickey)
{
   if (sBeacon.empty()) return;
   std::string sContract = DecodeBase64(sBeacon);
   std::vector<std::string> vContract = split(sContract.c_str(),";");
   if (vContract.size() < 4) return;
   out_cpid = vContract[0];
   out_address = vContract[2];
   out_publickey = vContract[3];
}




std::string GetBeaconPublicKeyFromContract(std::string sEncContract)
{
   if (sEncContract.empty()) return "";
   // Beacon data structure: CPID,hashRand,Address,beacon public key: base64 encoded
   std::string sContract = DecodeBase64(sEncContract);
   std::vector<std::string> vContract = split(sContract.c_str(),";");
   if (vContract.size() < 4) return "";
   std::string sBeaconPublicKey = vContract[3];
   return sBeaconPublicKey;
}

bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature)
{
    std::string sBeaconPublicKey = GetBeaconPublicKey(sCPID, false);
    std::string sConcatMessage = sCPID + sBlockHash;
    bool bValid = CheckMessageSignature("R","cpid", sConcatMessage, sSignature, sBeaconPublicKey);
    return bValid;
}

std::string SignBlockWithCPID(std::string sCPID, std::string sBlockHash)
{
    // Returns the Signature of the CPID+BlockHash message. 
    std::string sPrivateKey = GetStoredBeaconPrivateKey(sCPID);
    std::string sMessage = sCPID + sBlockHash;
    std::string sSignature = SignMessage(sMessage,sPrivateKey);
    return sSignature;
}

std::string GetPollContractByTitle(std::string objecttype, std::string title)
{
        for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
        {
                std::string key_name  = (*ii).first;
                if (key_name.length() > objecttype.length())
                {
                    if (key_name.substr(0,objecttype.length())==objecttype)
                    {
                                std::string contract = mvApplicationCache[(*ii).first];
                                std::string PollTitle = ExtractXML(contract,"<TITLE>","</TITLE>");
                                boost::to_lower(PollTitle);
                                boost::to_lower(title);
                                if (PollTitle==title)
                                {
                                    return contract;
                                }
                    }
                }
        }
        return "";
}

bool PollExists(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    return contract.length() > 10 ? true : false;
}

bool PollExpired(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    double expiration = cdbl(ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>"),0);
    return (expiration < (double)GetAdjustedTime()) ? true : false;
}


bool PollCreatedAfterSecurityUpgrade(std::string pollname)
{
	// If the expiration is after July 1 2017, use the new security features.
	std::string contract = GetPollContractByTitle("poll",pollname);
	double expiration = cdbl(ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>"),0);
	return (expiration > 1498867200) ? true : false;
}


double PollDuration(std::string pollname)
{
    std::string contract = GetPollContractByTitle("poll",pollname);
    double days = cdbl(ExtractXML(contract,"<DAYS>","</DAYS>"),0);
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
    std::string objecttype="vote";
    
    double MoneySupplyFactor = GetMoneySupplyFactor();

    for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
    {
                std::string key_name  = (*ii).first;
                if (key_name.length() > objecttype.length())
                {
                    if (key_name.substr(0,objecttype.length())==objecttype)
                    {
                                std::string contract = mvApplicationCache[(*ii).first];
                                std::string Title = ExtractXML(contract,"<TITLE>","</TITLE>");
                                std::string VoterAnswer = ExtractXML(contract,"<ANSWER>","</ANSWER>");
                                boost::to_lower(Title);
                                boost::to_lower(pollname);
                                boost::to_lower(VoterAnswer);
                                boost::to_lower(answer);
                                std::vector<std::string> vVoterAnswers = split(VoterAnswer.c_str(),";");
                                for (unsigned int x = 0; x < vVoterAnswers.size(); x++)
                                {
                                    if (pollname == Title && answer == vVoterAnswers[x])
                                    {
                                        double shares = PollCalculateShares(contract,sharetype,MoneySupplyFactor,vVoterAnswers.size());
                                        total_shares += shares;
                                        out_participants += (double)((double)1/(double)vVoterAnswers.size());
                                    }
                                }
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
	if (!msPrimaryCPID.empty() && msPrimaryCPID != "INVESTOR")
	{
		StructCPID st1 = GetLifetimeCPID(msPrimaryCPID,"ProvableMagnitude()");
		CBlockIndex* pHistorical = GetHistoricalMagnitude(msPrimaryCPID);
		if (pHistorical->nHeight > 1 && pHistorical->nMagnitude > 0)
		{
			std::string sBlockhash = pHistorical->GetBlockHash().GetHex();
			std::string sSignature = SignBlockWithCPID(msPrimaryCPID,pHistorical->GetBlockHash().GetHex());
			// Find the Magnitude from the last staked block, within the last 6 months, and ensure researcher has a valid current beacon (if the beacon is expired, the signature contain an error message)
			sXML += "<CPID>" + msPrimaryCPID + "</CPID><INNERMAGNITUDE>" 
				+ RoundToString(pHistorical->nMagnitude,2) + "</INNERMAGNITUDE>" + 
				"<HEIGHT>" + RoundToString(pHistorical->nHeight,0) 
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
    BOOST_FOREACH(const COutput& out, vecOutputs)
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
	double dTotalVotedBalance = cdbl(ExtractXML(sPayload,"<TOTALVOTEDBALANCE>","</TOTALVOTEDBALANCE>"),2);
	double dLegacyBalance = cdbl(ExtractXML(sXML,"<BALANCE>","</BALANCE>"),0);

	if (fDebug10) printf(" \r\n Total Voted Balance %f, Legacy Balance %f \r\n",(float)dTotalVotedBalance,(float)dLegacyBalance);

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
		int32_t iPos = cdbl(sPos,0);
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
	double dLegacyMagnitude  = cdbl(ExtractXML(sXML,"<MAGNITUDE>","</MAGNITUDE>"),2);
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
				bool fAudited = (cdbl(RoundToString(pblockindexMagnitude->nMagnitude,2),0)==cdbl(sMagnitude,0) && fResult);
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
	if (!msPrimaryCPID.empty() && msPrimaryCPID != "INVESTOR")
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
			std::string sSignature = SignBlockWithCPID(msPrimaryCPID,pHistorical->GetBlockHash().GetHex());
			// Find the Magnitude from the last staked block, within the last 6 months, and ensure researcher has a valid current beacon (if the beacon is expired, the signature contain an error message)

			std::string sMagXML = "<CPID>" + msPrimaryCPID + "</CPID><INNERMAGNITUDE>" + RoundToString(pHistorical->nMagnitude,2) + "</INNERMAGNITUDE>" + 
				"<HEIGHT>" + RoundToString(pHistorical->nHeight,0) + "</HEIGHT><BLOCKHASH>" + sBlockhash + "</BLOCKHASH><SIGNATURE>" + sSignature + "</SIGNATURE>";
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
						bool fAudited = (cdbl(RoundToString(pblockindexMagnitude->nMagnitude,2),0)==cdbl(sMagnitude,0) && fResult);
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
    BOOST_FOREACH(const COutput& out, vecOutputs)
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

		int32_t iPos = cdbl(sPos,0);
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

    std::string objecttype="vote";
    Array results;
    Object entry;
    entry.push_back(Pair("Votes","Votes Report " + pollname));
    entry.push_back(Pair("MoneySupplyFactor",RoundToString(MoneySupplyFactor,2)));

    std::string header = "GRCAddress,CPID,Question,Answer,ShareType,URL";

    entry.push_back(Pair(header,"Shares"));
                                    
    int iRow = 0;
    for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
    {
            std::string key_name  = (*ii).first;
            if (key_name.length() > objecttype.length())
            {
                    if (key_name.substr(0,objecttype.length())==objecttype)
                    {
                                std::string contract = mvApplicationCache[(*ii).first];
                                std::string Title = ExtractXML(contract,"<TITLE>","</TITLE>");

                                std::string OriginalContract = GetPollContractByTitle("poll",Title);
                                std::string Question = ExtractXML(OriginalContract,"<QUESTION>","</QUESTION>");

                                std::string VoterAnswer = ExtractXML(contract,"<ANSWER>","</ANSWER>");
                                std::string GRCAddress = ExtractXML(contract,"<GRCADDRESS>","</GRCADDRESS>");
                                std::string CPID = ExtractXML(contract,"<CPID>","</CPID>");
                                std::string Mag = ExtractXML(contract,"<MAGNITUDE>","</MAGNITUDE>");

                                double dShareType= cdbl(GetPollXMLElementByPollTitle(Title,"<SHARETYPE>","</SHARETYPE>"),0);
                                std::string sShareType= GetShareType(dShareType);
                                std::string sURL = ExtractXML(contract,"<URL>","</URL>");

                                std::string Balance = ExtractXML(contract,"<BALANCE>","</BALANCE>");
                                boost::to_lower(Title);
                                boost::to_lower(pollname);
                                boost::to_lower(VoterAnswer);
                            
                                if (pollname == Title)
                                {
                                    std::vector<std::string> vVoterAnswers = split(VoterAnswer.c_str(),";");
                                    for (unsigned int x = 0; x < vVoterAnswers.size(); x++)
                                    {
                                        double shares = PollCalculateShares(contract,dShareType,MoneySupplyFactor,vVoterAnswers.size());
                                        total_shares += shares;
                                        participants += (double)((double)1/(double)vVoterAnswers.size());
                                        iRow++;
                                        std::string voter = GRCAddress + "," + CPID + "," + Question + "," + vVoterAnswers[x] + "," + sShareType + "," + sURL;
                                        entry.push_back(Pair(voter,RoundToString(shares,0)));
                                    }
                                }
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
        std::string datatype="poll";
        std::string rows = "";
        std::string row = "";
        double iPollNumber = 0;
        double total_participants = 0;
        double total_shares = 0;
        boost::to_lower(QueryByTitle);
        std::string sExport = "";
        std::string sExportRow = "";
        out_export="";
        for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
        {
                std::string key_name  = (*ii).first;
                if (key_name.length() > datatype.length())
                {
                    if (key_name.substr(0,datatype.length())==datatype)
                    {
                                std::string contract = mvApplicationCache[(*ii).first];
                                std::string Title = key_name.substr(datatype.length()+1,key_name.length()-datatype.length()-1);
                                std::string Expiration = ExtractXML(contract,"<EXPIRATION>","</EXPIRATION>");
                                std::string Question = ExtractXML(contract,"<QUESTION>","</QUESTION>");
                                std::string Answers = ExtractXML(contract,"<ANSWERS>","</ANSWERS>");
                                std::string ShareType = ExtractXML(contract,"<SHARETYPE>","</SHARETYPE>");
                                std::string sURL = ExtractXML(contract,"<URL>","</URL>");
                                boost::to_lower(Title);
                                if (!PollExpired(Title) || IncludeExpired)
                                {
                                    if (QueryByTitle=="" || QueryByTitle == Title)
                                    {
                                        iPollNumber++;
                                        total_participants = 0;
                                        total_shares=0;
                                        std::string BestAnswer  = "";
                                        double highest_share = 0;
                                        std::string ExpirationDate = TimestampToHRDate(cdbl(Expiration,0));
                                        std::string sShareType = GetShareType(cdbl(ShareType,0));
                                        std::string TitleNarr = "Poll #" + RoundToString((double)iPollNumber,0) 
                                            + " (" + ExpirationDate + " ) - " + sShareType;
                                        
                                        entry.push_back(Pair(TitleNarr,Title));
                                        sExportRow = "<POLL><URL>" + sURL + "</URL><TITLE>" + Title + "</TITLE><EXPIRATION>" + ExpirationDate + "</EXPIRATION><SHARETYPE>" + sShareType + "</SHARETYPE><QUESTION>" + Question + "</QUESTION><ANSWERS>"+Answers+"</ANSWERS>";

                                        if (bDetail)
                                        {
                                    
                                            entry.push_back(Pair("Question",Question));
                                            std::vector<std::string> vAnswers = split(Answers.c_str(),";");
                                             sExportRow += "<ARRAYANSWERS>";
                                            for (unsigned int i = 0; i < vAnswers.size(); i++)
                                            {
                                                double participants=0;
                                                double dShares = VotesCount(Title,vAnswers[i],cdbl(ShareType,0),participants);
                                                if (dShares > highest_share) 
                                                {
                                                        highest_share = dShares;
                                                        BestAnswer = vAnswers[i];
                                                }

                                                entry.push_back(Pair("#" + RoundToString((double)i+1,0) + " [" + RoundToString(participants,3) + "]. " 
                                                    + vAnswers[i],dShares));
                                                total_participants += participants;
                                                total_shares += dShares;
                                                sExportRow += "<RESERVED></RESERVED><ANSWERNAME>" + vAnswers[i] + "</ANSWERNAME><PARTICIPANTS>" + RoundToString(participants,0) + "</PARTICIPANTS><SHARES>" + RoundToString(dShares,0) + "</SHARES>";


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
        std::string datatype="beacon";
        std::string rows = "";
        std::string row = "";
        int iBeaconCount = 0;
        int iUpgradedBeaconCount = 0;
        for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
        {
                std::string key_name  = (*ii).first;
                if (key_name.length() > datatype.length())
                {
                    if (key_name.substr(0,datatype.length())==datatype)
                    {
                                std::string key_value = mvApplicationCache[(*ii).first];
                                std::string subkey = key_name.substr(datatype.length()+1,key_name.length()-datatype.length()-1);
                                std::string contract = DecodeBase64(key_value);
                                std::string cpidv2 = ExtractValue(contract,";",0);
                                std::string grcaddress = ExtractValue(contract,";",2);
                                std::string sPublicKey = ExtractValue(contract,";",3);
                                if (!sPublicKey.empty()) iUpgradedBeaconCount++;
                                iBeaconCount++;
                    }
                }
      }
      entry.push_back(Pair("Total Beacons",(double)iBeaconCount));
      entry.push_back(Pair("Upgraded Beacon Count",(double)iUpgradedBeaconCount));
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
        std::string datatype="beacon";
        std::string row = "";
        for(map<string,string>::iterator ii=mvApplicationCache.begin(); ii!=mvApplicationCache.end(); ++ii) 
        {
                std::string key_name  = (*ii).first;
                if (key_name.length() > datatype.length())
                {
                    if (key_name.substr(0,datatype.length())==datatype)
                    {
                                std::string key_value = mvApplicationCache[(*ii).first];
                                std::string subkey = key_name.substr(datatype.length()+1,key_name.length()-datatype.length()-1);
                                row = subkey + "<COL>" + key_value;
                                //                              std::string contract = GlobalCPUMiningCPID.cpidv2 + ";" + hashRand.GetHex() + ";" + GRCAddress;
                                std::string contract = DecodeBase64(key_value);
                                std::string cpid = subkey;
                                std::string cpidv2 = ExtractValue(contract,";",0);
                                std::string grcaddress = ExtractValue(contract,";",2);
                                entry.push_back(Pair(cpid,grcaddress));
                    }
                }
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
      std::string report = "Neural_hash, Popularity\r\n";
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
                    report += row + "\r\n";
                    pct = (((double)popularity)/(votes+.01))*100;
                    entry.push_back(Pair(neural_hash,RoundToString(popularity,0) + "; " + RoundToString(pct,2) + "%"));
                }
      }
      // If we have a pending superblock, append it to the report:
      std::string SuperblockHeight = ReadCache("neuralsecurity","pending");
      if (!SuperblockHeight.empty() && SuperblockHeight != "0")
      {
          entry.push_back(Pair("Pending",SuperblockHeight));
      }
      int64_t superblock_age = GetAdjustedTime() - mvApplicationCacheTimestamp["superblock;magnitudes"];
     
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
          entry.push_back(Pair("Last Sync", (double)iLastNeuralSync));
          entry.push_back(Pair("Next Sync", (double)iNextNeuralSync));
          entry.push_back(Pair("Next Quorum", (double)iNextQuorum));
      }
      results.push_back(entry);
      return results;
}


Array GetJSONCurrentNeuralNetworkReport()
{
      Array results;
      //Returns a report of the networks neural hashes in order of popularity
      std::string neural_hash = "";
      std::string report = "Neural_hash, Popularity\r\n";
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
                    report += row + "\r\n";
                    pct = (((double)popularity)/(votes+.01))*100;
                    entry.push_back(Pair(neural_hash,RoundToString(popularity,0) + "; " + RoundToString(pct,2) + "%"));
                }
      }
      // If we have a pending superblock, append it to the report:
      std::string SuperblockHeight = ReadCache("neuralsecurity","pending");
      if (!SuperblockHeight.empty() && SuperblockHeight != "0")
      {
          entry.push_back(Pair("Pending",SuperblockHeight));
      }
      int64_t superblock_age = GetAdjustedTime() - mvApplicationCacheTimestamp["superblock;magnitudes"];
     
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
          entry.push_back(Pair("Last Sync", (double)iLastNeuralSync));
          entry.push_back(Pair("Next Sync", (double)iNextNeuralSync));
          entry.push_back(Pair("Next Quorum", (double)iNextQuorum));
      }
      results.push_back(entry);
      return results;
}


Array GetJSONVersionReport()
{
      Array results;
      //Returns a report of the GRC Version staking blocks over the last 100 blocks
      std::string neural_ver = "";
      std::string report = "Version, Popularity\r\n";
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
                    report += row + "\r\n";
                    pct = popularity/(votes+.01)*100;
                    entry.push_back(Pair(neural_ver,RoundToString(popularity,0) + "; " + RoundToString(pct,2) + "%"));
                }
      }
      results.push_back(entry);
      return results;

}



Array MagnitudeReportCSV(bool detail)
{
           Array results;
           Object c;
           StructCPID globalmag = mvMagnitudes["global"];
           double payment_timespan = 14; 
           std::string Narr = "Research Savings Account Report - Generated " + RoundToString(GetAdjustedTime(),0) + " - Timespan: " + RoundToString(payment_timespan,0);
           c.push_back(Pair("RSA Report",Narr));
           results.push_back(c);
           double totalpaid = 0;
           double lto  = 0;
           double rows = 0;
           double outstanding = 0;
           double totaloutstanding = 0;
           std::string header = "CPID,GRCAddress,Magnitude,PaymentMagnitude,Accuracy,LongTermOwed14day,LongTermOwedDaily,Payments,InterestPayments,LastPaymentTime,CurrentDailyOwed,NextExpectedPayment,AvgDailyPayments,Outstanding,PaymentTimespan";
           
           if (detail) header += ",PaymentDate,ResearchPaymentAmount,InterestPaymentAmount,Block#";
           header += "\r\n";

           std::string row = "";
           for(map<string,StructCPID>::iterator ii=mvMagnitudes.begin(); ii!=mvMagnitudes.end(); ++ii) 
           {
                // For each CPID on the network, report:
                StructCPID structMag = mvMagnitudes[(*ii).first];
                if (structMag.initialized && structMag.cpid.length() > 2) 
                { 
                    if (structMag.cpid != "INVESTOR")
                    {
                        outstanding = structMag.totalowed - structMag.payments;
                        
                        StructCPID stDPOR = mvDPOR[structMag.cpid];
                    
                        row = structMag.cpid + "," + structMag.GRCAddress + "," + RoundToString(structMag.Magnitude,2) + "," 
                            + RoundToString(structMag.PaymentMagnitude,0) + "," + RoundToString(structMag.Accuracy,0) + "," + RoundToString(structMag.totalowed,2) 
                            + "," + RoundToString(structMag.totalowed/14,2)
                            + "," + RoundToString(structMag.payments,2) + "," 
                            + RoundToString(structMag.interestPayments,2) + "," + TimestampToHRDate(structMag.LastPaymentTime) 
                            + "," + RoundToString(structMag.owed,2) 
                            + "," + RoundToString(structMag.owed/2,2)
                            + "," + RoundToString(structMag.payments/14,2) + "," + RoundToString(outstanding,2) + "," 
                            + RoundToString(structMag.PaymentTimespan,0) + "\n";
                        header += row;
                        if (detail)
                        {
                            //Add payment detail - Halford - Christmas Eve 2014
                            std::vector<std::string> vCPIDTimestamps = split(structMag.PaymentTimestamps.c_str(),",");
                            std::vector<std::string> vCPIDPayments   = split(structMag.PaymentAmountsResearch.c_str(),",");
                            std::vector<std::string> vCPIDInterestPayments = split(structMag.PaymentAmountsInterest.c_str(),",");
                            std::vector<std::string> vCPIDPaymentBlocks    = split(structMag.PaymentAmountsBlocks.c_str(),",");
                            
                            for (unsigned int i = 0; i < vCPIDTimestamps.size(); i++)
                            {
                                    double dTime = cdbl(vCPIDTimestamps[i],0);
                                    std::string sResearchAmount = vCPIDPayments[i];
                                    std::string sPaymentDate = DateTimeStrFormat("%m-%d-%Y %H:%M:%S", dTime);
                                    std::string sInterestAmount = vCPIDInterestPayments[i];
                                    std::string sPaymentBlock = vCPIDPaymentBlocks[i];
                                    //std::string sPaymentHash = vCPIDPaymentBlockHashes[i];
                                    if (dTime > 0)
                                    {
                                        row = " , , , , , , , , , , , , , , , , " + sPaymentDate + "," + sResearchAmount + "," + sInterestAmount + "," + sPaymentBlock + "\n";
                                        header += row;
                                    }
                            }
                        }
                        rows++;
                        totalpaid += structMag.payments;
                        lto += structMag.totalowed;
                        totaloutstanding += outstanding;
                    }

                }

           }
           int64_t timestamp = GetTime();
           std::string footer = RoundToString(rows,0) + ", , , , ," + RoundToString(lto,2) + ", ," + RoundToString(totalpaid,2) + ", , , , , ," + RoundToString(totaloutstanding,2) + "\n";
           header += footer;
           Object entry;
           entry.push_back(Pair("CSV Complete",strprintf("\\reports\\magnitude_%" PRId64 ".csv",timestamp)));
           results.push_back(entry);
           
           CSVToFile(strprintf("magnitude_%" PRId64 ".csv",timestamp), header);
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
        "Returns details of a given item by name.");

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
    else if (sitem == "betatest")
    {
        Object entry;
        // Test a sample CPID keypair
        entry.push_back(Pair("CPID",msPrimaryCPID));
        std::string sBeaconPublicKey = GetBeaconPublicKey(msPrimaryCPID, false);
        entry.push_back(Pair("Beacon Public Key",sBeaconPublicKey));
        std::string sSignature = SignBlockWithCPID(msPrimaryCPID,"1000");
        entry.push_back(Pair("Signature",sSignature));
        // Validate the signature
        bool fResult = VerifyCPIDSignature(msPrimaryCPID, "1000", sSignature);
        entry.push_back(Pair("Sig Valid",fResult));
        fResult = VerifyCPIDSignature(msPrimaryCPID, "1001", sSignature);
        entry.push_back(Pair("Wrong Block Sig Valid",fResult));
        fResult = VerifyCPIDSignature(msPrimaryCPID, "1000", "wrong_signature" + sSignature + "wrong_signature");
        entry.push_back(Pair("Right block, Wrong Signature Valid",fResult));
        // Missing Beacon, with wrong CPID
        std::string sCPID = "1234567890";
        sBeaconPublicKey = GetBeaconPublicKey(sCPID, false);
        sSignature = SignBlockWithCPID(sCPID,"1001");
        entry.push_back(Pair("Signature",sSignature));
        fResult = VerifyCPIDSignature(sCPID, "1001", sSignature);
        entry.push_back(Pair("Bad CPID with missing beacon",fResult));
        results.push_back(entry);
    }
    else if (sitem == "debugexplainmagnitude")
    {
        double dMag = ExtractMagnitudeFromExplainMagnitude();
        Object entry;
        entry.push_back(Pair("Mag",dMag));
        results.push_back(entry);
    }
    else if (sitem == "explainmagnitude")
    {

        if (msNeuralResponse.length() > 25)
        {
            Object entry;
            std::vector<std::string> vMag = split(msNeuralResponse.c_str(),"<ROW>");
            for (unsigned int i = 0; i < vMag.size(); i++)
            {
                entry.push_back(Pair(RoundToString(i+1,0),vMag[i].c_str()));
            }
            results.push_back(entry);
        }
        else
        {
    
        double mytotalrac = 0;
        double nettotalrac  = 0;
        double projpct = 0;
        double mytotalpct = 0;
        double ParticipatingProjectCount = 0;
        double TotalMagnitude = 0;
        double Mag = 0;
        Object entry;
        std::string narr = "";
        std::string narr_desc = "";
        double TotalProjectRAC = 0;
        double TotalUserVerifiedRAC = 0;

        for(map<string,StructCPID>::iterator ibp=mvBoincProjects.begin(); ibp!=mvBoincProjects.end(); ++ibp) 
        {
            StructCPID WhitelistedProject = mvBoincProjects[(*ibp).first];
            if (WhitelistedProject.initialized)
            {
                double ProjectRAC = GetNetworkTotalByProject(WhitelistedProject.projectname);
                StructCPID structcpid = mvCPIDs[WhitelistedProject.projectname];
                bool including = false;
                narr = "";
                narr_desc = "";
                double UserVerifiedRAC = 0;
                if (structcpid.initialized) 
                { 
                    if (structcpid.projectname.length() > 1)
                    {
                        including = (ProjectRAC > 0 && structcpid.Iscpidvalid && structcpid.rac > 1);
                        UserVerifiedRAC = structcpid.rac;
                        narr_desc = "NetRac: " + RoundToString(ProjectRAC,0) + ", CPIDValid: " 
                            + YesNo(structcpid.Iscpidvalid) + ", RAC: " +RoundToString(structcpid.rac,0);
                    }
                }
                narr = including ? ("Participating " + narr_desc) : ("Enumerating " + narr_desc);
                if (structcpid.projectname.length() > 1 && including)
                {
                        entry.push_back(Pair(narr + " Project",structcpid.projectname));
                }

                projpct = UserVerifiedRAC/(ProjectRAC+.01);
                nettotalrac += ProjectRAC;
                mytotalrac = mytotalrac + UserVerifiedRAC;
                mytotalpct = mytotalpct + projpct;
                
                double project_magnitude = 
                    ((UserVerifiedRAC / (ProjectRAC + 0.01)) / (WHITELISTED_PROJECTS + 0.01)) * NeuralNetworkMultiplier;

                if (including)
                {
                        TotalMagnitude += project_magnitude;
                        TotalUserVerifiedRAC += UserVerifiedRAC;
                        TotalProjectRAC += ProjectRAC;
                        ParticipatingProjectCount++;
                        
                        entry.push_back(Pair("User " + structcpid.projectname + " Verified RAC",UserVerifiedRAC));
                        entry.push_back(Pair(structcpid.projectname + " Network RAC",ProjectRAC));
                        entry.push_back(Pair("Your Project Magnitude",project_magnitude));
                }
                
             }
        }
        entry.push_back(Pair("Whitelisted Project Count",(double)WHITELISTED_PROJECTS));
        entry.push_back(Pair("Grand-Total Verified RAC",mytotalrac));
        entry.push_back(Pair("Grand-Total Network RAC",nettotalrac));
        entry.push_back(Pair("Total Magnitude for All Projects",TotalMagnitude));
        entry.push_back(Pair("Grand-Total Whitelisted Projects",RoundToString(WHITELISTED_PROJECTS,0)));
        entry.push_back(Pair("Participating Project Count",ParticipatingProjectCount));
        Mag = TotalMagnitude;
        std::string babyNarr = "(" + RoundToString(TotalUserVerifiedRAC,2) + "/" + RoundToString(TotalProjectRAC,2) + ")/" + RoundToString(WHITELISTED_PROJECTS,0) + "*" + RoundToString(NeuralNetworkMultiplier,0) + "=";
        entry.push_back(Pair(babyNarr,Mag));
        results.push_back(entry);
        return results;
        }

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
    else if (sitem == "staking")
    {
        results = StakingReport();
        return results;
    }
    else if (sitem == "currenttime")
    {

        Object entry;
        entry.push_back(Pair("Unix",GetAdjustedTime()));
        entry.push_back(Pair("UTC",TimestampToHRDate(GetAdjustedTime())));
        results.push_back(entry);

    }
    else if (sitem == "magnitudecsv")
    {
        results = MagnitudeReportCSV(false);
        return results;
    }
    else if (sitem=="detailmagnitudecsv")
    {
        results = MagnitudeReportCSV(true);
        return results;
    }
	else if (sitem == "seefile")
	{
		// This is a unit test to prove viability of transmitting a file from node to node
		std::string sFile = "C:\\test.txt";
        std::vector<unsigned char> v = readFileToVector(sFile);
		Object entry;
	    entry.push_back(Pair("byte1",v[1]));
        entry.push_back(Pair("bytes",(double)v.size()));
		for (int i = 0; i < v.size(); i++)
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
    else if (sitem == "harddriveserial")
    {
        Object entry;
        std::string response = getHardDriveSerial();
        entry.push_back(Pair("Serial",response));
        results.push_back(entry);
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
    else if (sitem=="newbieage")
    {
        double dBeaconDt = BeaconTimeStamp(args,false);
        Object entry;
        entry.push_back(Pair("Sent",dBeaconDt));
        dBeaconDt = BeaconTimeStamp(args,true);
        entry.push_back(Pair("Sent With ZeroOut Feature",dBeaconDt));
        results.push_back(entry);
        return results;
    }
    else if (sitem == "projects") 
    {
        for(map<string,StructCPID>::iterator ii=mvBoincProjects.begin(); ii!=mvBoincProjects.end(); ++ii) 
        {

            StructCPID structcpid = mvBoincProjects[(*ii).first];

            if (structcpid.initialized) 
            { 
                Object entry;
                entry.push_back(Pair("Project",structcpid.projectname));
                entry.push_back(Pair("URL",structcpid.link));
                results.push_back(entry);

            }
        }
        return results;
    }
    else if (sitem == "leder")
    {
        double subsidy = LederstrumpfMagnitude2(450, GetAdjustedTime());
        Object entry;
        entry.push_back(Pair("Mag Out For 450",subsidy));
        if (args.length() > 1)
        {
            double myrac=cdbl(args,0);
            subsidy = LederstrumpfMagnitude2(myrac, GetAdjustedTime());
            entry.push_back(Pair("Mag Out",subsidy));
        }
        results.push_back(entry);
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
            
                if (structcpid.cpid == GlobalCPUMiningCPID.cpid || structcpid.cpid=="INVESTOR" || structcpid.cpid=="investor")
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
        printf ("generating cpid report %s",sitem.c_str());

        for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii) 
        {

            StructCPID structcpid = mvCPIDs[(*ii).first];

            if (structcpid.initialized) 
            { 
            
                if ((GlobalCPUMiningCPID.cpid.length() > 3 && 
                    structcpid.cpid == GlobalCPUMiningCPID.cpid) 
                    || structcpid.cpid=="INVESTOR" || GlobalCPUMiningCPID.cpid=="INVESTOR" || GlobalCPUMiningCPID.cpid.length()==0)
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
    CBlockIndex* pindexCheckpoint;

    result.push_back(Pair("synccheckpoint", Checkpoints::hashSyncCheckpoint.ToString().c_str()));
    pindexCheckpoint = mapBlockIndex[Checkpoints::hashSyncCheckpoint];
    result.push_back(Pair("height", pindexCheckpoint->nHeight));
    result.push_back(Pair("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str()));

    // Check that the block satisfies synchronized checkpoint
    if (CheckpointsMode == Checkpoints::STRICT)
        result.push_back(Pair("policy", "strict"));

    if (CheckpointsMode == Checkpoints::ADVISORY)
        result.push_back(Pair("policy", "advisory"));

    if (CheckpointsMode == Checkpoints::PERMISSIVE)
        result.push_back(Pair("policy", "permissive"));

    if (mapArgs.count("-checkpointkey"))
        result.push_back(Pair("checkpointmaster", true));

    return result;
}

// Brod
static bool compare_second(const pair<std::string, long>  &p1, const pair<std::string, long> &p2)
{
    return p1.second > p2.second;
}
json_spirit::Value rpc_getblockstats(const json_spirit::Array& params, bool fHelp)
{
    if(fHelp || params.size() < 1 || params.size() > 3 )
        throw runtime_error(
            "getblockstats mode [startheight [endheight]]\n"
            "Show stats on what wallets and cpids staked recent blocks.\n");
    long mode= cdbl(params[0].get_str(),0);
    (void)mode; //TODO
    long lowheight= 0;
    long highheight= INT_MAX;
    if(params.size()>=2)
        lowheight= cdbl(params[1].get_str(),0);
    if(params.size()>=3)
        highheight= cdbl(params[2].get_str(),0);
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
    for( ; (cur
            &&( cur->nHeight>=lowheight )
            &&( lowheight>0 || blockcount<=14000 )
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
            }
            else
                txcountinblock+=1;
        }
        transactioncount+=txcountinblock;
        emptyblockscount+=(txcountinblock==0);
        c_blockversion[block.nVersion]++;
        MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc);
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
        result1.push_back(Pair("general", result));
    }
    {
        Object result;
        result.push_back(Pair("block", blockcount));
        result.push_back(Pair("empty_block", emptyblockscount));
        result.push_back(Pair("transaction", transactioncount));
        result.push_back(Pair("proof_of_stake", poscount));
        result.push_back(Pair("boincreward", researchcount));
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
        result1.push_back(Pair("averages", result));
    }
    {
        Object result;
        std::vector<PAIRTYPE(std::string, long)> list;
        std::copy(c_version.begin(), c_version.end(), back_inserter(list));
        std::sort(list.begin(),list.end(),compare_second);
        BOOST_FOREACH(const PAIRTYPE(std::string, long)& item, list)
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
        BOOST_FOREACH(const PAIRTYPE(std::string, long)& item, list)
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
        BOOST_FOREACH(const PAIRTYPE(std::string, long)& item, list)
        {
            if(!(limit--)) break;
            result.push_back(Pair(item.first, item.second/(double)blockcount));
        }
        result1.push_back(Pair("orgs", result));
    }
    return result1;
}
