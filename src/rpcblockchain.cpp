// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include <fstream>
#include "cpid.h"

#include "kernel.h"

// R Halford - Removing Reference to OptionsModel

#include "init.h" // for pwalletMain
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()

using namespace json_spirit;
using namespace std;
extern std::string YesNo(bool bin);
void ReloadBlockChain1();
MiningCPID GetMiningCPID();
StructCPID GetStructCPID();
MiningCPID GetNextProject(bool bForce);
std::string GetBestBlockHash(std::string sCPID);

int64_t CPIDChronoStart(std::string cpid);
bool IsCPIDTimeValid(std::string cpid, int64_t locktime);

std::string TestHTTPProtocol(std::string sCPID);

std::string ComputeCPIDv2(std::string email, std::string bpk, uint256 blockhash);

uint256 GetBlockHash256(const CBlockIndex* pindex_hash);
std::string SerializeBoincBlock(MiningCPID mcpid);
extern std::string TimestampToHRDate(double dtm);

std::string qtGRCCodeExecutionSubsystem(std::string sCommand);

std::string LegacyDefaultBoincHashArgs();
double GetChainDailyAvgEarnedByCPID(std::string cpid, int64_t locktime, double& out_payments, double& out_daily_avg_payments);

bool ChainPaymentViolation(std::string cpid, int64_t locktime, double Proposed_Subsidy);

double CoinToDouble(double surrogate);

int64_t GetRSAWeightByCPID(std::string cpid);
	
double GetUntrustedMagnitude(std::string cpid, double& out_owed);

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);
extern enum Checkpoints::CPMode CheckpointsMode;
extern bool Resuscitate();
bool ProjectIsValid(std::string project);
int RebootClient();

extern double GetNetworkProjectCountWithRAC();
int ReindexWallet();
extern Array MagnitudeReportCSV(bool detail);
std::string getfilecontents(std::string filename);

std::string NewbieLevelToString(int newbie_level);

void stopWireFrameRenderer();
void startWireFrameRenderer();

int CreateRestorePoint();
int DownloadBlocks();
double GetBlockValueByHash(uint256 hash);
double cdbl(std::string s, int place);
void ScriptPubKeyToJSON(const CScript& scriptPubKey, Object& out);
bool GetBlockNew(uint256 blockhash, int& out_height, CBlock& blk, bool bForceDiskRead);
bool AESSkeinHash(unsigned int diffbytes, double rac, uint256 scrypthash, std::string& out_skein, std::string& out_aes512);
				  std::string aes_complex_hash(uint256 scrypt_hash);
std::vector<std::string> split(std::string s, std::string delim);
double LederstrumpfMagnitude2(double mag,int64_t locktime);


int TestAESHash(double rac, unsigned int diffbytes, uint256 scrypt_hash, std::string aeshash);
std::string TxToString(const CTransaction& tx, const uint256 hashBlock, int64_t& out_amount, int64_t& out_locktime, int64_t& out_projectid, 
	std::string& out_projectaddress, std::string& comments, std::string& out_grcaddress);
extern double GetPoBDifficulty();
bool IsCPIDValid_Retired(std::string cpid, std::string ENCboincpubkey);

bool IsCPIDValidv2(MiningCPID& mc, int height);

std::string RetrieveMd5(std::string s1);
std::string getfilecontents(std::string filename);

MiningCPID DeserializeBoincBlock(std::string block);
void StopGridcoin3();
std::string GridcoinHttpPost(std::string msg, std::string boincauth, std::string urlPage, bool bUseDNS);
std::string RacStringFromDiff(double RAC, unsigned int diffbytes);
void PobSleep(int milliseconds);
extern double GetNetworkAvgByProject(std::string projectname);
extern bool FindRAC(bool CheckingWork, std::string TargetCPID, std::string TargetProjectName, double pobdiff, bool bCreditNodeVerification, std::string& out_errors, int& out_position);
void HarvestCPIDs(bool cleardata);
bool TallyNetworkAverages(bool ColdBoot);
void RestartGridcoin10();

std::string GetHttpPage(std::string cpid);
std::string GetHttpPage(std::string cpid, bool usedns, bool clearcache);

bool GridDecrypt(const std::vector<unsigned char>& vchCiphertext,std::vector<unsigned char>& vchPlaintext);
bool GridEncrypt(std::vector<unsigned char> vchPlaintext, std::vector<unsigned char> &vchCiphertext);

uint256 GridcoinMultipleAlgoHash(std::string t1);
void ExecuteCode();


void CreditCheck(std::string cpid, bool clearcache);

double CalculatedMagnitude(int64_t locktime);




double GetPoBDifficulty()
{
	//ToDo:Retire
	return 0;
}



double GetNetworkProjectCountWithRAC()
{
	double count = 0;
	for(map<string,StructCPID>::iterator ibp=mvNetwork.begin(); ibp!=mvNetwork.end(); ++ibp) 
	{
		StructCPID NetworkProject = GetStructCPID();
		NetworkProject = mvNetwork[(*ibp).first];
		if (NetworkProject.initialized)
		{
			if (NetworkProject.AverageRAC > 100) count++;
		}
	}
	return count;
}

double GetNetworkAvgByProject(std::string projectname)
{
	try 
	{
		if (mvNetwork.size() < 1)
		{
			return 0;
		}
	
		StructCPID structcpid = GetStructCPID();
		structcpid = mvNetwork[projectname];
		if (!structcpid.initialized) return 0;
		double networkavgrac = structcpid.AverageRAC;
		return networkavgrac;
	}
	catch (std::exception& e)
	{
			printf("Error retrieving Network Avg\r\n");
			return 0;
	}


}






bool FindRAC(bool CheckingWork, std::string TargetCPID, std::string TargetProjectName, double pobdiff, bool bCreditNodeVerification,
	std::string& out_errors, int& out_position)
{

	try 
	{
		
					//Gridcoin; Find CPID+Project+RAC in chain
					int nMaxDepth = nBestHeight-1;

					if (nMaxDepth < 3) nMaxDepth=3;

					double pobdifficulty;
					if (bCreditNodeVerification)
					{
							pobdifficulty=14;
					}
					else
					{
							pobdifficulty = pobdiff;
					}

	

					if (pobdifficulty < .002) pobdifficulty=.002;
					int nLookback = 576*pobdifficulty; //Daily block count * Lookback in days
					int nMinDepth = nMaxDepth - nLookback;
					if (nMinDepth < 2) nMinDepth = 2;
					out_position = 0;

	
					////////////////////////////
					if (CheckingWork) nMinDepth=nMinDepth+10;
					if (nMinDepth > nBestHeight) nMinDepth=nBestHeight-1;

					////////////////////////////
					if (nMinDepth > nMaxDepth) 
					{
						nMinDepth = nMaxDepth-1;
					}
	
					if (nMaxDepth < 5 || nMinDepth < 5) return false;
	
	
					//Check the cache first:
					StructCPIDCache cache;
					std::string sKey = TargetCPID + ":" + TargetProjectName;
					cache = mvCPIDCache[sKey]; 
					double cachedblocknumber = 0;
					if (cache.initialized)
					{
						cachedblocknumber=cache.blocknumber;
					}
					if (cachedblocknumber > 0 && cachedblocknumber >= nMinDepth && cachedblocknumber <= nMaxDepth && cache.cpidproject==sKey) 
					{
	
						out_position = cache.blocknumber;
							if (CheckingWork) printf("Project %s  found at position %i   PoBLevel %f    Start depth %i     end depth %i   \r\n",
							TargetProjectName.c_str(),out_position,pobdifficulty,nMaxDepth,nMinDepth);

						return true;
					}
	
					CBlock block;
					out_errors = "";

					for (int ii = nMaxDepth; ii > nMinDepth; ii--)
					{
     					CBlockIndex* pblockindex = FindBlockByHeight(ii);
						int out_height = 0;
						bool result1 = GetBlockNew(pblockindex->GetBlockHash(), out_height, block, false);
						
						if (result1)
						{
		
							
				    			MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc);

				
								if (bb.cpid==TargetCPID && bb.projectname==TargetProjectName && block.nVersion==3)
								{
									out_position = ii;
									//Cache this:
									cache = mvCPIDCache[sKey]; 
									if (!cache.initialized)
										{
											cache.initialized = true;
											mvCPIDCache.insert(map<string,StructCPIDCache>::value_type(sKey, cache));
										}
									cache.cpid = TargetCPID;
									cache.cpidproject = sKey;
									cache.blocknumber = ii;
									if (CheckingWork) printf("Project %s  found at position %i   PoBLevel %f    Start depth %i     end depth %i   \r\n",TargetProjectName.c_str(),ii,pobdifficulty,nMaxDepth,nMinDepth);

    								mvCPIDCache[sKey]=cache;
									return true;
								}
						}

					}

					printf("Start depth %i end depth %i",nMaxDepth,nMinDepth);
					out_errors = out_errors + "Start depth " + RoundToString(nMaxDepth,0) + "; ";
					out_errors = out_errors + "Not found; ";
					return false;
		}
	catch (std::exception& e)
	{
		return false;
	}
	
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

	//Prevent Exploding Diff - Fix the root cause of the issue asap
	if (false)
	{
		if (dDiff > 1000      && dDiff < 10000)        dDiff = (dDiff/100)      + 1000;
		if (dDiff >= 10000    && dDiff < 100000)       dDiff = (dDiff/1000)     + 2000;
		if (dDiff >= 100000   && dDiff < 1000000)      dDiff = (dDiff/10000)    + 3000;
		if (dDiff >= 1000000  && dDiff < 100000000)    dDiff = (dDiff/100000)   + 4000;
		if (dDiff >= 10000000 && dDiff < 100000000000) dDiff = (dDiff/10000000) + 5000;
		if (dDiff >= 100000000000) dDiff = (dDiff/10000000000000) + 7000;
		if (dDiff > 10000000) dDiff = 10000000;
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
		strprintf("%s%s", blockindex->IsProofOfStake()? "proof-of-stake" : "proof-of-work", blockindex->GeneratedStakeModifier()? " stake-modifier": "") + " " + PoRNarr		)		);
    result.push_back(Pair("proofhash", blockindex->hashProof.GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016"PRIx64, blockindex->nStakeModifier)));
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
	result.push_back(Pair("ProjectName", bb.projectname));
	result.push_back(Pair("RAC", bb.rac));
	result.push_back(Pair("NetworkRAC", bb.NetworkRAC));
	result.push_back(Pair("Magnitude", bb.Magnitude));
	if (fDebug2) result.push_back(Pair("BoincHash",block.vtx[0].hashBoinc));

	result.push_back(Pair("RSAWeight",bb.RSAWeight));
	result.push_back(Pair("LastPaymentTime",TimestampToHRDate(bb.LastPaymentTime)));
	result.push_back(Pair("ResearchSubsidy",bb.ResearchSubsidy));
	double interest = mint-bb.ResearchSubsidy;
	result.push_back(Pair("Interest",bb.InterestSubsidy));

	double blockdiff = GetBlockDifficulty(block.nBits);
	result.push_back(Pair("GRCAddress",bb.GRCAddress));
	result.push_back(Pair("Organization",bb.Organization));
	result.push_back(Pair("OrganizationKeyPrefix",bb.OrganizationKey));
	result.push_back(Pair("ClientVersion",bb.clientversion));	
	if (blockindex->nHeight < 70000) 
	{
			bool IsCpidValid = IsCPIDValid_Retired(bb.cpid, bb.enccpid);
			result.push_back(Pair("CPIDValid",IsCpidValid));
	}
	else
	{
		if (bb.cpidv2.length() > 10) 	result.push_back(Pair("CPIDv2",bb.cpidv2.substr(0,32)));
		bool IsCPIDValid2 = IsCPIDValidv2(bb,blockindex->nHeight);
		result.push_back(Pair("CPIDValid",IsCPIDValid2));
		if (fDebug3 && !IsCPIDValid2)
		{
			//std::string cpidv2debug = AES512(bb.enccpid);
			//result.push_back(Pair("CPIDValidV2debug",cpidv2debug));
		}
	}
	if (false)
	{
		uint256 pbh = 0;
		if (blockindex->pprev) pbh = blockindex->pprev->GetBlockHash();
		std::string me = ComputeCPIDv2(GlobalCPUMiningCPID.email,GlobalCPUMiningCPID.boincruntimepublickey,pbh);
		result.push_back(Pair("TestCPID",me));
	}
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
    CBlockIndex* pblockindex = RPCFindBlockByHeight(nHeight);

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
	if (fDebug)	printf("Getblockhash %f",(double)nHeight);
	CBlockIndex* RPCpblockindex = RPCFindBlockByHeight(nHeight);
	if (fDebug)	printf("@r");
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



bool xCreateNewConfigFile(std::string boinc_email)
{
	std::string filename = "gridcoinresearch.conf";
	boost::filesystem::path path = GetDataDir() / filename;
	ofstream myConfig;
	myConfig.open (path.string().c_str());
	std::string row = "cpumining=true\r\n";
	myConfig << row;
	row = "email=" + boinc_email + "\r\n";
	myConfig << row;
	myConfig.close();
	return true;
}

	
std::string BackupGridcoinWallet()
{

	printf("Staring Wallet Backup\r\n");
	std::string filename = "grc_" + DateTimeStrFormat("%m-%d-%Y",  GetAdjustedTime()) + ".dat";
	std::string filename_backup = "backup.dat";
	std::string standard_filename = "wallet_" + DateTimeStrFormat("%m-%d-%Y",  GetAdjustedTime()) + ".dat";
	std::string source_filename   = "wallet.dat";
	boost::filesystem::path path = GetDataDir() / "walletbackups" / filename;
	boost::filesystem::path target_path_standard = GetDataDir() / "walletbackups" / standard_filename;
	boost::filesystem::path source_path_standard = GetDataDir() / source_filename;
	boost::filesystem::path dest_path_std = GetDataDir() / "walletbackups" / filename_backup;
    boost::filesystem::create_directories(path.parent_path());
	std::string errors = "";
	//Copy the standard wallet first:  (1-30-2015)
	BackupWallet(*pwalletMain, target_path_standard.string().c_str());

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
		 const std::string& strName = item.second;
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
								 //								 key = vchSecret.GetKey();
								 CPubKey pubkey = key.GetPubKey();
								
								 CKeyID vchAddress = pubkey.GetID();
								 {
									 LOCK2(cs_main, pwalletMain->cs_wallet);
									 //							   if (!pwalletMain->AddKey(key)) {            fGood = false;
         
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


uint256 Skein(std::string sInput)
{
	uint256 uiSkein = 0;
	uiSkein = GridcoinMultipleAlgoHash(sInput);
	return uiSkein;
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


Value execute(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 1 && params.size() != 2  && params.size() != 3))
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
	else if (sItem == "encrypt")
	{
		//11-5-2014 Gridcoin - R Halford - Encrypt a phrase
		if (params.size() != 2)
		{
			entry.push_back(Pair("Error","You must also specify a wallet passphrase."));
			results.push_back(entry);
		
		}
		else
		{
			std::string sParam = params[1].get_str();
			std::string encrypted = AdvancedCrypt(sParam);
			entry.push_back(Pair("Passphrase",encrypted));
			entry.push_back(Pair("[Specify in config file] autounlock=",encrypted));

			results.push_back(entry);
		}
	
	}
	else if (sItem == "genboinckey")
	{
		//Gridcoin - R Halford - Generate Boinc Mining Key
		GetNextProject(false);
		std::string sParam = SerializeBoincBlock(GlobalCPUMiningCPID);
		if (fDebug) printf("Utilizing %s",sParam.c_str());
		std::string sBase = EncodeBase64(sParam);
		entry.push_back(Pair("[Specify in config file without quotes] boinckey=",sBase));
		results.push_back(entry);
	}
	else if (sItem == "testcpid3")
	{
		if (params.size() != 2)
		{
			entry.push_back(Pair("Error","You must specify an argument"));
			results.push_back(entry);
		}
		else
		{
			std::string sParam1 = params[1].get_str();
			entry.push_back(Pair("Param1",sParam1));
			std::string test = AdvancedDecrypt(sParam1);
			entry.push_back(Pair("Out",test));
		}
	
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
	else if (sItem == "testpoolhash")
	{

		std::string result = GetBestBlockHash(GlobalCPUMiningCPID.cpidv2);
		entry.push_back(Pair("PoolHash",result));
		results.push_back(entry);
		
	}
	else if (sItem=="testpoolhash2")
	{
		std::string result = TestHTTPProtocol(GlobalCPUMiningCPID.cpidv2);
		entry.push_back(Pair("PoolHash",result));
		results.push_back(entry);
		
	}
	else if (sItem =="nonce")
	{
	
	}
	else if (sItem == "testnonce")
	{
	
	
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

			//12-28-2014
			std::string sboinchashargs = LegacyDefaultBoincHashArgs();
			if (sParam1 != sboinchashargs)
			{
				entry.push_back(Pair("Error","Admin must be logged in"));
			}
			else
			{
				//ClientVersionNew()
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
			//1-17-2015
			double Payments = 0;
			double AvgDailyPayments = 0;
			double DailyOwed = 0;
			DailyOwed = GetChainDailyAvgEarnedByCPID(sParam1,GetAdjustedTime(),Payments,AvgDailyPayments);
			bool ChainPaymentApproved = ChainPaymentViolation(sParam1,GetAdjustedTime(),5);
			entry.push_back(Pair("DailyOwed",DailyOwed));
			entry.push_back(Pair("AvgPayments",AvgDailyPayments));
			entry.push_back(Pair("Payments",Payments));
			entry.push_back(Pair("Chain Payment Approved 5",ChainPaymentApproved));
			ChainPaymentApproved = ChainPaymentViolation(sParam1,GetAdjustedTime(),100);
			entry.push_back(Pair("Chain Payment Approved 100",ChainPaymentApproved));
			ChainPaymentApproved = ChainPaymentViolation(sParam1,GetAdjustedTime(),400);
			entry.push_back(Pair("Chain Payment Approved 400",ChainPaymentApproved));
			ChainPaymentApproved = ChainPaymentViolation(sParam1,GetAdjustedTime(),800);
			entry.push_back(Pair("Chain Payment Approved 800",ChainPaymentApproved));
			results.push_back(entry);
	
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
		email="ha";
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

	else if (sItem == "restartnetlayer")
	{
		    bool response=true;
		    RestartGridcoin10();
		 	entry.push_back(Pair("Restartnetlayer Result",response));
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
	else if (sItem == "startwireframe")
	{
			#if defined(WIN32) && defined(QT_GUI)
			
			startWireFrameRenderer();
			#endif

	}
	else if (sItem == "endwireframe")
	{
			#if defined(WIN32) && defined(QT_GUI)
		
			stopWireFrameRenderer();
			#endif
	}
	else if (sItem == "tally")
	{
			TallyNetworkAverages(true);
			entry.push_back(Pair("Tally Network Averages",1));
			results.push_back(entry);
	}
	else if (sItem == "testhash")
	{
		    uint256 testhash = 0;
			testhash = Skein("test1234");
			entry.push_back(Pair("GMAH",testhash.GetHex()));
    		results.push_back(entry);
	}
	else if (sItem == "fork")
	{
		
 	      ReloadBlockChain1();
		  //	CTxDB txdb("r");
          //  txdb.LoadBlockIndex();
         // PrintBlockTree();
    	entry.push_back(Pair("Fork",1));
    	results.push_back(entry);
	  
	}
	else if (sItem == "resetcpids")
	{
			mvCPIDCache.clear();
    	    HarvestCPIDs(true);
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
	else if (sItem == "restartnetlayer")
	{
				entry.push_back(Pair("Restarting Net Layer",1));
				RestartGridcoin10();
			    LoadBlockIndex(true);
	}
	else if (sItem == "postcpid")
	{
			std::string result = GetHttpPage("859038ff4a9",true,true);
			entry.push_back(Pair("POST Result",result));
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
	else if (sItem == "restartnet_old")
	{
			printf("Restarting gridcoin's network layer;");
			RestartGridcoin10();
			entry.push_back(Pair("Execute","Restarted Gridcoins network layer."));
	   		results.push_back(entry);
	}
	else if (sItem == "findrac")
	{
			int position = 0;
			std::string out_errors = "";
	    	std::string TargetCPID = "123";
			std::string TargetProjectName="Docking";
			bool result = FindRAC(false,TargetCPID, TargetProjectName, 1, false,out_errors, position);
			entry.push_back(Pair("TargetCPID",TargetCPID));
			entry.push_back(Pair("Errors",out_errors));
		   	results.push_back(entry);
	}
	else
	{
			entry.push_back(Pair("Command " + sItem + " not found.",-1));
			results.push_back(entry);
	}
	return results;    
		
}



Array MagnitudeReport(bool bMine)
{
	       Array results;
		   Object c;
		   std::string Narr = "Research Savings Account Report - Generated " + RoundToString(GetAdjustedTime(),0);
		   c.push_back(Pair("RSA Report",Narr));
		   results.push_back(c);
		   StructCPID globalmag = GetStructCPID();
		   globalmag = mvMagnitudes["global"];
		   double payment_timespan = 14; //(globalmag.HighLockTime-globalmag.LowLockTime)/86400;  //Lock time window in days
		   Object entry;
		   entry.push_back(Pair("Payment Window",payment_timespan));
		   results.push_back(entry);

		   for(map<string,StructCPID>::iterator ii=mvMagnitudes.begin(); ii!=mvMagnitudes.end(); ++ii) 
		   {
				// For each CPID on the network, report:
				StructCPID structMag = GetStructCPID();
				structMag = mvMagnitudes[(*ii).first];
				if (structMag.initialized && structMag.cpid.length() > 2) 
				{ 
						if (!bMine || (bMine && structMag.cpid == GlobalCPUMiningCPID.cpid))
						{
									Object entry;
									entry.push_back(Pair("CPID",structMag.cpid));
									entry.push_back(Pair("Magnitude",structMag.ConsensusMagnitude));
									entry.push_back(Pair("Payment Magnitude",structMag.PaymentMagnitude));
									entry.push_back(Pair("Payment Timespan (Days)",structMag.PaymentTimespan));
									entry.push_back(Pair("Magnitude Accuracy",structMag.Accuracy));
									entry.push_back(Pair("Long Term Owed (14 days)",structMag.totalowed));
									entry.push_back(Pair("Payments (14 days)",structMag.payments));
									entry.push_back(Pair("InterestPayments (14 days)",structMag.interestPayments));
									entry.push_back(Pair("Last Payment Time",TimestampToHRDate(structMag.LastPaymentTime)));
									entry.push_back(Pair("Total Owed",structMag.owed));
									entry.push_back(Pair("Next Expected Payment",structMag.owed/2));
									entry.push_back(Pair("Daily Paid",structMag.payments/14));
									entry.push_back(Pair("Daily Owed",structMag.totalowed/14));
									
									if (structMag.cpid==GlobalCPUMiningCPID.cpid)
									{
											int64_t cpid_chrono = CPIDChronoStart(GlobalCPUMiningCPID.cpid);
											entry.push_back(Pair("Registered Payment Time",TimestampToHRDate(cpid_chrono)));
									}

									results.push_back(entry);
						}
				}

			}
		
			return results;
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
	std::string sDt = DateTimeStrFormat("%m-%d-%Y %H:%M:%S",dtm);
	return sDt;
}



Array MagnitudeReportCSV(bool detail)
{
	       Array results;
		   Object c;
		   StructCPID globalmag = GetStructCPID();
		   globalmag = mvMagnitudes["global"];
		   double payment_timespan = 14; //(globalmag.HighLockTime-globalmag.LowLockTime)/86400;  //Lock time window in days
		   std::string Narr = "Research Savings Account Report - Generated " + RoundToString(GetAdjustedTime(),0) + " - Timespan: " + RoundToString(payment_timespan,0);
		   c.push_back(Pair("RSA Report",Narr));
		   results.push_back(c);
		   double totalpaid = 0;
		   double lto  = 0;
		   double rows = 0;
		   double outstanding = 0;
		   double totaloutstanding = 0;
		   std::string header = "CPID,Magnitude,PaymentMagnitude,Accuracy,LongTermOwed14day,LongTermOwedDaily,Payments,InterestPayments,LastPaymentTime,CurrentDailyOwed,NextExpectedPayment,AvgDailyPayments,Outstanding,PaymentTimespan,PaymentDate,ResearchPaymentAmount,InterestPaymentAmount\r\n";

		   std::string row = "";
		   for(map<string,StructCPID>::iterator ii=mvMagnitudes.begin(); ii!=mvMagnitudes.end(); ++ii) 
		   {
				// For each CPID on the network, report:
				StructCPID structMag = GetStructCPID();
				structMag = mvMagnitudes[(*ii).first];
				if (structMag.initialized && structMag.cpid.length() > 2) 
				{ 
					if (structMag.cpid != "INVESTOR")
					{
						outstanding = structMag.totalowed - structMag.payments;
						if (outstanding < 0) outstanding = 0;
  						row = structMag.cpid + "," + RoundToString(structMag.ConsensusMagnitude,2) + "," 
							+ RoundToString(structMag.PaymentMagnitude,0) + "," + RoundToString(structMag.Accuracy,0) + "," + RoundToString(structMag.totalowed,2) 
							+ "," + RoundToString(structMag.totalowed/14,2)
							+ "," + RoundToString(structMag.payments,2) + "," 
							+ RoundToString(structMag.interestPayments,2) + "," + TimestampToHRDate(structMag.LastPaymentTime) 
							+ "," + RoundToString(structMag.owed,2) 
							+ "," + RoundToString(structMag.owed/2,2)
							+ "," + RoundToString(structMag.payments/14,2) + "," + RoundToString(outstanding,2) + "," + RoundToString(structMag.PaymentTimespan,0) +  "\n";
						header += row;
						if (detail)
						{
							//Add payment detail - Halford - Christmas Eve 2014
	   						std::vector<std::string> vCPIDTimestamps = split(structMag.PaymentTimestamps.c_str(),",");
							std::vector<std::string> vCPIDPayments   = split(structMag.PaymentAmountsResearch.c_str(),",");
							std::vector<std::string> vCPIDInterestPayments = split(structMag.PaymentAmountsInterest.c_str(),",");
							for (unsigned int i = 0; i < vCPIDTimestamps.size(); i++)
							{
									double dTime = cdbl(vCPIDTimestamps[i],0);
									std::string sResearchAmount = vCPIDPayments[i];
									std::string sPaymentDate = DateTimeStrFormat("%m-%d-%Y %H:%M:%S", dTime);
									std::string sInterestAmount = vCPIDInterestPayments[i];
								
									if (dTime > 0)
									{
										row = " , , , , , , , , , , , , , ," + sPaymentDate + "," + sResearchAmount + "," + sInterestAmount + "\n";
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
		   std::string footer = RoundToString(rows,0) + ", , , ," + RoundToString(lto,2) + ", ," + RoundToString(totalpaid,2) + ", , , , ," + RoundToString(totaloutstanding,2) + "\n";
		   header += footer;
		   Object entry;
		   entry.push_back(Pair("CSV Complete","\\reports\\magnitude.csv"));
		   results.push_back(entry);
     	   CSVToFile("magnitude.csv",header);
		   return results;
}





std::string YesNo(bool bin)
{
	if (bin) return "Yes";
	return "No";
}

Value listitem(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 1  && params.size() != 2))
        throw runtime_error(
		"list <string::itemname>\n"
        "Returns details of a given item by name.");

    std::string sitem = params[0].get_str();
	
	std::string args = "";
	if (params.size()==2)
	{
		args=params[1].get_str();
	}
	
	if (sitem=="") throw runtime_error("Item invalid.");

    Array results;
	Object e2;
	e2.push_back(Pair("Command",sitem));
	results.push_back(e2);

	if (sitem=="creditcheck")
	{
			Object entry;
	
			CreditCheck(GlobalCPUMiningCPID.cpid,true);
			double boincmagnitude = CalculatedMagnitude( GetAdjustedTime());
			entry.push_back(Pair("Magnitude",boincmagnitude));
			results.push_back(entry);
	}
	if (sitem == "networktime")
	{
			Object entry;
			entry.push_back(Pair("Network Time",GetAdjustedTime()));
			results.push_back(entry);
	}
	if (sitem == "rsaweight")
	{
		double out_magnitude = 0;
		double out_owed = 0;
		int64_t RSAWEIGHT =	GetRSAWeightByCPID(GlobalCPUMiningCPID.cpid);
		out_magnitude = GetUntrustedMagnitude(GlobalCPUMiningCPID.cpid,out_owed);
		Object entry;
		entry.push_back(Pair("RSA Weight",RSAWEIGHT));
		entry.push_back(Pair("Remote Magnitude",out_magnitude));
		entry.push_back(Pair("RSA Owed",out_owed));
		results.push_back(entry);

	}
	if (sitem == "explainmagnitude")
	{

		double mytotalrac = 0;
		double nettotalrac  = 0;
		double projpct = 0;
		double mytotalpct = 0;
		double ParticipatingProjectCount = 0;
		double TotalMagnitude = 0;
		double Mag = 0;
		double NetworkProjectCountWithRAC = 0;

		Object entry;
		
		//Halford 9-28-2014: Note: mvCPIDs[ProjectName] contains StructCPID of Boinc Projects by Project Name
		//As of survey result held by RTM : https://cryptocointalk.com/topic/16082-gridcoin-research-magnitude-calculation-survey-final/
		//Switch to Magnitude Calculation Method 2 (Assess magnitude by All whitelisted projects):
		std::string narr = "";
		std::string narr_desc = "";
					
		for(map<string,StructCPID>::iterator ibp=mvBoincProjects.begin(); ibp!=mvBoincProjects.end(); ++ibp) 
		{
			StructCPID WhitelistedProject = GetStructCPID();
			WhitelistedProject = mvBoincProjects[(*ibp).first];
			if (WhitelistedProject.initialized)
			{
				double ProjectRAC = GetNetworkAvgByProject(WhitelistedProject.projectname);
				if (ProjectRAC > 100) NetworkProjectCountWithRAC++;
				StructCPID structcpid = GetStructCPID();
				structcpid = mvCPIDs[WhitelistedProject.projectname];
				bool projectvalid = ProjectIsValid(structcpid.projectname);
				bool including = false;
				narr = "";
				narr_desc = "";
				double UserVerifiedRAC = 0;
				if (structcpid.initialized) 
				{ 
					if (structcpid.projectname.length() > 2 && projectvalid)
					{
						including = (ProjectRAC > 0 && structcpid.Iscpidvalid && structcpid.verifiedrac > 100);
						UserVerifiedRAC = structcpid.verifiedrac;
						if (UserVerifiedRAC < 0) UserVerifiedRAC=0;
						narr_desc = "NetRac: " + RoundToString(ProjectRAC,0) + ", CPIDValid: " + YesNo(structcpid.Iscpidvalid) + ", VerifiedRAC: " +RoundToString(structcpid.verifiedrac,0);
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
				
				double project_magnitude = UserVerifiedRAC/(ProjectRAC+.01) * 100;
				
				
				if (including)
				{
						TotalMagnitude += project_magnitude;
					
						ParticipatingProjectCount++;
				
						//entry.push_back(Pair("Participating Project Count",ParticipatingProjectCount));
						entry.push_back(Pair("User " + structcpid.projectname + " Verified RAC",UserVerifiedRAC));
						entry.push_back(Pair(structcpid.projectname + " Network RAC",ProjectRAC));
						entry.push_back(Pair("Your Project Magnitude",project_magnitude));
				}
				else
				{
					//std::string NonParticipatingBlurb = "Network RAC: " + RoundToString(ProjectRAC,0);
					//entry.push_back(Pair("Non Participating Project " + WhitelistedProject.projectname,NonParticipatingBlurb));
				}

				Mag = ( (TotalMagnitude/WHITELISTED_PROJECTS) * NetworkProjectCountWithRAC);
				
		     }
		}
		

		entry.push_back(Pair("Grand-Total Verified RAC",mytotalrac));
		entry.push_back(Pair("Grand-Total Network RAC",nettotalrac));

		entry.push_back(Pair("Total Magnitude for All Projects",TotalMagnitude));
		entry.push_back(Pair("Grand-Total Whitelisted Projects",RoundToString(WHITELISTED_PROJECTS,0)));

		entry.push_back(Pair("Participating Project Count",ParticipatingProjectCount));
						
		entry.push_back(Pair("Grand-Total Count Of Network Projects With RAC",NetworkProjectCountWithRAC));
		Mag = (TotalMagnitude/WHITELISTED_PROJECTS) * NetworkProjectCountWithRAC;
			
		std::string babyNarr = RoundToString(TotalMagnitude,2) + "/" + RoundToString(WHITELISTED_PROJECTS,0) + "*" + RoundToString(NetworkProjectCountWithRAC,0) + "=";

		entry.push_back(Pair(babyNarr,Mag));
		results.push_back(entry);
		return results;

	}


	if (sitem == "magnitude")
	{
			results = MagnitudeReport(false);
			return results;

	}

	if (sitem == "debug3")
	{
		fDebug3 = true;
		Object entry;
		entry.push_back(Pair("Entering Debug Mode 3",1));
		results.push_back(entry);
	}

	if (sitem == "currenttime")
	{

		Object entry;
		entry.push_back(Pair("Unix",GetAdjustedTime()));
		entry.push_back(Pair("UTC",TimestampToHRDate(GetAdjustedTime())));
		results.push_back(entry);

	}

	if (sitem == "testcpidtime")
	{

		Object entry;
		entry.push_back(Pair("Net",GetAdjustedTime()));
		entry.push_back(Pair("UTC",TimestampToHRDate(GetAdjustedTime())));
		
		int64_t cpid1 = CPIDChronoStart("00565aba8b273e72f5292dd54c2e9d9c");
		int64_t cpid2 = CPIDChronoStart("02d04a476979ec5526c28c3ad0a7d988");
		int64_t cpid3 = CPIDChronoStart("9dbd2eb638bfda3dc573a8e5f1ce7a4a");
		int64_t cpid4 = CPIDChronoStart("d7f6b0c023e06d4797fc257aea29050b");
		int64_t cpid5 = CPIDChronoStart("f985012508217b233308079b42e2cc1b");
		entry.push_back(Pair("Cpid1",cpid1));
		//Test validity
		bool valid1 = IsCPIDTimeValid("00565aba8b273e72f5292dd54c2e9d9c", cpid1+10);
		bool valid2 = IsCPIDTimeValid("00565aba8b273e72f5292dd54c2e9d9c", cpid1-1000);
		bool valid3 = IsCPIDTimeValid("00565aba8b273e72f5292dd54c2e9d9c", cpid1+1000);
		bool valid4 = IsCPIDTimeValid("00565aba8b273e72f5292dd54c2e9d9c", cpid1-86400+1000);
		bool valid5 = IsCPIDTimeValid("00565aba8b273e72f5292dd54c2e9d9c", cpid1-86400-5000);
		bool valid6 = IsCPIDTimeValid("00565aba8b273e72f5292dd54c2e9d9c", cpid1-(86400*2)+100);
		entry.push_back(Pair("v1",valid1));
		entry.push_back(Pair("v2",valid2));
		entry.push_back(Pair("v3",valid3));
		entry.push_back(Pair("v4",valid4));
		entry.push_back(Pair("v5",valid5));
		entry.push_back(Pair("v6",valid6));
		entry.push_back(Pair("Cpid2",cpid2));
		entry.push_back(Pair("Cpid3",cpid3));
		entry.push_back(Pair("Cpid4",cpid4));
		entry.push_back(Pair("Cpid5",cpid5));
		entry.push_back(Pair("cpid1UTC",TimestampToHRDate(cpid1)));
		entry.push_back(Pair("cpid2UTC",TimestampToHRDate(cpid2)));
		entry.push_back(Pair("cpid3UTC",TimestampToHRDate(cpid3)));
		entry.push_back(Pair("cpid4UTC",TimestampToHRDate(cpid4)));
		entry.push_back(Pair("cpid5UTC",TimestampToHRDate(cpid5)));
		results.push_back(entry);
	}


	if (sitem == "magnitudecsv")
	{
		results = MagnitudeReportCSV(false);
		return results;
	}

	
	if (sitem=="detailmagnitudecsv")
	{
		results = MagnitudeReportCSV(true);
		return results;
	}


	if (sitem == "mymagnitude")
	{
			results = MagnitudeReport(true);
			return results;
	}

	if (sitem == "rsa")
	{
	    	results = MagnitudeReport(true);
			return results;
	}

	if (sitem == "projects") 
	{
		for(map<string,StructCPID>::iterator ii=mvBoincProjects.begin(); ii!=mvBoincProjects.end(); ++ii) 
		{

			StructCPID structcpid = GetStructCPID();
			structcpid = mvBoincProjects[(*ii).first];

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

	if (sitem == "leder")
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


	if (sitem == "network") 
	{
		for(map<string,StructCPID>::iterator ii=mvNetwork.begin(); ii!=mvNetwork.end(); ++ii) 
		{

			StructCPID structcpid = GetStructCPID();
			structcpid = mvNetwork[(*ii).first];

	        if (structcpid.initialized) 
			{ 
				Object entry;
				entry.push_back(Pair("Project",structcpid.projectname));
				entry.push_back(Pair("RAC",structcpid.rac));
				entry.push_back(Pair("Avg RAC",structcpid.AverageRAC));

				entry.push_back(Pair("Entries",structcpid.entries));

				
				if (structcpid.projectname=="NETWORK") 
				{
						entry.push_back(Pair("Network Projects",structcpid.NetworkProjects));

				}
				results.push_back(entry);

			}
		}
		return results;

	}

	if (sitem=="validcpids") 
	{
		//Dump vectors:
		if (mvCPIDs.size() < 1) 
		{
			HarvestCPIDs(false);
		}
		printf ("generating cpid report %s",sitem.c_str());

		for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii) 
		{

			StructCPID structcpid = GetStructCPID();
			structcpid = mvCPIDs[(*ii).first];

	        if (structcpid.initialized) 
			{ 
			
				if (structcpid.cpid == GlobalCPUMiningCPID.cpid || structcpid.cpid=="INVESTOR" || structcpid.cpid=="investor")
				{
					if (structcpid.verifiedrac > 100 && structcpid.verifiedteam=="gridcoin")
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
						entry.push_back(Pair("Verified UTC",structcpid.verifiedutc));
						entry.push_back(Pair("Verified RAC",structcpid.verifiedrac));
						entry.push_back(Pair("Verified Team",structcpid.verifiedteam));
						entry.push_back(Pair("Verified RecTime",structcpid.verifiedrectime));
						entry.push_back(Pair("Verified RAC Age",structcpid.verifiedage));
						entry.push_back(Pair("Is my CPID Valid?",structcpid.Iscpidvalid));
						entry.push_back(Pair("CPID Link",structcpid.link));
						entry.push_back(Pair("Errors",structcpid.errors));
						results.push_back(entry);
					}
				}

			}
		}


    }


	if (sitem=="cpids") 
	{
		//Dump vectors:
		
		if (mvCPIDs.size() < 1) 
		{
			HarvestCPIDs(false);
		}
		printf ("generating cpid report %s",sitem.c_str());

		for(map<string,StructCPID>::iterator ii=mvCPIDs.begin(); ii!=mvCPIDs.end(); ++ii) 
		{

			StructCPID structcpid = GetStructCPID();
			structcpid = mvCPIDs[(*ii).first];

	        if (structcpid.initialized) 
			{ 
			
				if ((GlobalCPUMiningCPID.cpid.length() > 3 && structcpid.cpid == GlobalCPUMiningCPID.cpid) || structcpid.cpid=="INVESTOR" || GlobalCPUMiningCPID.cpid=="INVESTOR" || GlobalCPUMiningCPID.cpid.length()==0)
				{
					Object entry;
	
					entry.push_back(Pair("Project",structcpid.projectname));
					entry.push_back(Pair("CPID",structcpid.cpid));
					//entry.push_back(Pair("CPIDhash",structcpid.cpidhash));
					entry.push_back(Pair("RAC",structcpid.rac));
					entry.push_back(Pair("Team",structcpid.team));
					//entry.push_back(Pair("RecTime",structcpid.rectime));
					//entry.push_back(Pair("Age",structcpid.age));
					entry.push_back(Pair("Verified RAC",structcpid.verifiedrac));
					entry.push_back(Pair("Verified Team",structcpid.verifiedteam));
					//entry.push_back(Pair("Verified RecTime",structcpid.verifiedrectime));
					entry.push_back(Pair("Is my CPID Valid?",structcpid.Iscpidvalid));
					entry.push_back(Pair("CPID Link",structcpid.link));
					entry.push_back(Pair("Errors",structcpid.errors));
					results.push_back(entry);
				}

			}
		}

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
