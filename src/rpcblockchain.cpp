// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "init.h" // for pwalletMain
#include "block.h"
#include "checkpoints.h"
#include "txdb.h"
#include "beacon.h"
#include "neuralnet/contract/contract.h"
#include "neuralnet/contract/message.h"
#include "neuralnet/project.h"
#include "neuralnet/quorum.h"
#include "neuralnet/researcher.h"
#include "neuralnet/tally.h"
#include "backup.h"
#include "appcache.h"
#include "contract/cpid.h"
#include "contract/rain.h"
#include "util.h"

#include <univalue.h>

extern CCriticalSection cs_ConvergedScraperStatsCache;
extern ConvergedScraperStats ConvergedScraperStatsCache;

using namespace std;

extern std::string YesNo(bool bin);
bool AskForOutstandingBlocks(uint256 hashStart);
bool ForceReorganizeToHash(uint256 NewHash);
extern UniValue MagnitudeReport(const NN::Cpid cpid);
extern std::string ExtractValue(std::string data, std::string delimiter, int pos);
extern UniValue SuperblockReport(int lookback = 14, bool displaycontract = false, std::string cpid = "");
bool LoadAdminMessages(bool bFullTableScan);
extern bool AdvertiseBeacon(std::string &sOutPrivKey, std::string &sOutPubKey, std::string &sError, std::string &sMessage);
extern bool ScraperSynchronizeDPOR();
std::string ExplainMagnitude(std::string sCPID);

extern UniValue GetJSONVersionReport(const int64_t lookback, const bool full_version);
extern UniValue GetJSONBeaconReport();

bool GetEarliestStakeTime(std::string grcaddress, std::string cpid);
double GetTotalBalance();
double CoinToDouble(double surrogate);
extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);

BlockFinder RPCBlockFinder;

namespace {
UniValue ClaimToJson(const NN::Claim& claim, const CBlockIndex* const pindex)
{
    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (int)claim.m_version);
    json.pushKV("mining_id", claim.m_mining_id.ToString());
    json.pushKV("client_version", claim.m_client_version);
    json.pushKV("organization", claim.m_organization);

    json.pushKV("block_subsidy", ValueFromAmount(claim.m_block_subsidy));

    json.pushKV("research_subsidy", ValueFromAmount(claim.m_research_subsidy));

    // Version 11 blocks remove magnitude and magnitude unit from claims:
    if (pindex->nVersion >= 11) {
        json.pushKV("magnitude", pindex->nMagnitude);
        json.pushKV("magnitude_unit", NN::Tally::GetMagnitudeUnit(pindex));
    } else {
        json.pushKV("magnitude", claim.m_magnitude);
        json.pushKV("magnitude_unit", claim.m_magnitude_unit);
    }

    json.pushKV("signature", EncodeBase64(claim.m_signature.data(), claim.m_signature.size()));

    json.pushKV("quorum_hash", claim.m_quorum_hash.ToString());
    json.pushKV("quorum_address", claim.m_quorum_address);

    return json;
}

UniValue SuperblockToJson(const NN::Superblock& superblock)
{
    UniValue magnitudes(UniValue::VOBJ);

    for (const auto& cpid_iter : superblock.m_cpids) {
        magnitudes.pushKV(cpid_iter.Cpid().ToString(), cpid_iter.Magnitude().Floating());
    }

    UniValue projects(UniValue::VOBJ);

    for (const auto& project_pair : superblock.m_projects) {
        UniValue project(UniValue::VOBJ);

        project.pushKV("average_rac", project_pair.second.m_average_rac);
        project.pushKV("rac", project_pair.second.m_rac);
        project.pushKV("total_credit", project_pair.second.m_total_credit);

        projects.pushKV(project_pair.first, project);
    }

    UniValue json(UniValue::VOBJ);

    json.pushKV("version", (int)superblock.m_version);
    json.pushKV("magnitudes", std::move(magnitudes));
    json.pushKV("projects", std::move(projects));

    return json;
}
} // anonymous namespace

UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail)
{
    UniValue result(UniValue::VOBJ);

    result.pushKV("hash", block.GetHash().GetHex());

    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);

    result.pushKV("confirmations", txGen.GetDepthInMainChain());
    result.pushKV("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION));
    result.pushKV("height", blockindex->nHeight);
    result.pushKV("version", block.nVersion);
    result.pushKV("merkleroot", block.hashMerkleRoot.GetHex());
    result.pushKV("mint", ValueFromAmount(blockindex->nMint));
    result.pushKV("MoneySupply", blockindex->nMoneySupply);
    result.pushKV("time", block.GetBlockTime());
    result.pushKV("nonce", (uint64_t)block.nNonce);
    result.pushKV("bits", strprintf("%08x", block.nBits));
    result.pushKV("difficulty", GetDifficulty(blockindex));
    result.pushKV("blocktrust", leftTrim(blockindex->GetBlockTrust().GetHex(), '0'));
    result.pushKV("chaintrust", leftTrim(blockindex->nChainTrust.GetHex(), '0'));

    if (blockindex->pprev)
        result.pushKV("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    if (blockindex->pnext)
        result.pushKV("nextblockhash", blockindex->pnext->GetBlockHash().GetHex());

    const NN::Claim& claim = block.GetClaim();

    std::string PoRNarr = "";
    if (blockindex->IsProofOfStake() && claim.HasResearchReward()) {
        PoRNarr = "proof-of-research";
    }

    result.pushKV("flags", strprintf("%s%s",
        blockindex->IsProofOfStake() ? "proof-of-stake" : "proof-of-work",
        blockindex->GeneratedStakeModifier()? " stake-modifier": "") + " " + PoRNarr);

    result.pushKV("proofhash", blockindex->hashProof.GetHex());
    result.pushKV("entropybit", (int)blockindex->GetStakeEntropyBit());
    result.pushKV("modifier", strprintf("%016" PRIx64, blockindex->nStakeModifier));
    result.pushKV("modifierchecksum", strprintf("%08x", blockindex->nStakeModifierChecksum));

    UniValue txinfo(UniValue::VARR);

    for (auto const& tx : block.vtx)
    {
        if (fPrintTransactionDetail)
        {
            UniValue entry(UniValue::VOBJ);

            entry.pushKV("txid", tx.GetHash().GetHex());
            TxToJSON(tx, uint256(), entry);

            txinfo.push_back(entry);
        }
        else
            txinfo.push_back(tx.GetHash().GetHex());
    }

    result.pushKV("tx", txinfo);

    if (block.IsProofOfStake())
        result.pushKV("signature", HexStr(block.vchBlockSig.begin(), block.vchBlockSig.end()));

    result.pushKV("claim", ClaimToJson(block.GetClaim(), blockindex));

    if (LogInstance().WillLogCategory(BCLog::LogFlags::NET)) result.pushKV("BoincHash",block.vtx[0].hashBoinc);

    if (fPrintTransactionDetail && blockindex->nIsSuperBlock == 1) {
        result.pushKV("superblock", SuperblockToJson(block.GetSuperblock()));
    }

    result.pushKV("fees_collected", ValueFromAmount(GetFeesCollected(block)));
    result.pushKV("IsSuperBlock", (int)blockindex->nIsSuperBlock);
    result.pushKV("IsContract", (int)blockindex->nIsContract);

    return result;
}


UniValue showblock(const UniValue& params, bool fHelp)
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

UniValue getbestblockhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getbestblockhash\n"
                "\n"
                "Returns the hash of the best block in the longest block chain\n");

    LOCK(cs_main);

    return hashBestChain.GetHex();
}

UniValue getblockcount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getblockcount\n"
                "\n"
                "Returns the number of blocks in the longest block chain\n");

    LOCK(cs_main);

    return nBestHeight;
}

UniValue getdifficulty(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getdifficulty\n"
                "\n"
                "Returns the difficulty as a multiple of the minimum difficulty\n");

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("current", GetDifficulty(GetLastBlockIndex(pindexBest, true)));
    obj.pushKV("target", GetBlockDifficulty(GetNextTargetRequired(pindexBest)));

    return obj;
}

UniValue settxfee(const UniValue& params, bool fHelp)
{
    LOCK(cs_main);

    CTransaction txDummy;

    // Min Fee
    int64_t nMinFee = txDummy.GetBaseFee(GMF_SEND);

    if (fHelp || params.size() < 1 || params.size() > 1 || AmountFromValue(params[0]) < nMinFee)
        throw runtime_error(
                "settxfee <amount>\n"
                "\n"
                "<amount> is a real and is rounded to the nearest 0.00000001\n"
                "\n"
                "Sets the txfee for transactions\n");

    nTransactionFee = AmountFromValue(params[0]);

    return true;
}

UniValue getrawmempool(const UniValue& params, bool fHelp)
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

    UniValue a(UniValue::VARR);
    for (auto const& hash : vtxid)
        a.push_back(hash.ToString());

    return a;
}

UniValue getblockhash(const UniValue& params, bool fHelp)
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

UniValue getblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getblock <hash> [bool:txinfo]\n"
                "\n"
                "[bool:txinfo] optional to print more detailed tx info\n"
                "\n"
                "Returns details of a block with given block-hash\n");

    std::string strHash = params[0].get_str();
    uint256 hash = uint256S(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    LOCK(cs_main);

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

UniValue getblockbynumber(const UniValue& params, bool fHelp)
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

bool AdvertiseBeacon(std::string &sOutPrivKey, std::string &sOutPubKey, std::string &sError, std::string &sMessage)
{
    sOutPrivKey = "BUG! deprecated field used";
    LOCK2(cs_main, pwalletMain->cs_wallet);
    {
        const std::string primary_cpid = NN::GetPrimaryCpid();

        if (!IsResearcher(primary_cpid))
        {
            sError = "INVESTORS_CANNOT_SEND_BEACONS";
            return false;
        }

        //If beacon is already in the chain, exit early
        if (!GetBeaconPublicKey(primary_cpid, true).empty())
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
        double nBalance = GetTotalBalance();
        if (nBalance < 1.01)
        {
            sError = "Balance too low to send beacon, 1.01 GRC minimum balance required.";
            return false;
        }

        CKey keyBeacon;
        if(!GenerateBeaconKeys(primary_cpid, keyBeacon))
        {
            sError = "GEN_KEY_FAIL";
            return false;
        }

        // Convert the new pubkey into legacy hex format
        sOutPubKey = HexStr(keyBeacon.GetPubKey().Raw());

        std::string value = "UNUSED;" + hashRand.GetHex() + ";" + DefaultWalletAddress() + ";" + sOutPubKey;

        LogPrintf("Creating beacon for cpid %s, %s", primary_cpid, value);

        NN::Contract contract(
            NN::ContractType::BEACON,
            NN::ContractAction::ADD,
            primary_cpid,
            EncodeBase64(std::move(value)));

        try
        {
            // Backup config with old keys like a normal backup
            // not needed, but extra backup does not hurt.
            // Also back up the wallet for extra safety measure.

            LOCK(pwalletMain->cs_wallet);

            if(!BackupConfigFile(GetBackupFilename("gridcoinresearch.conf"))
                    || !BackupWallet(*pwalletMain, GetBackupFilename("wallet.dat")))
            {
                sError = "Failed to backup old configuration file and wallet. Beacon not sent.";
                return false;
            }

            // Send the beacon transaction
            sMessage = SendPublicContract(std::move(contract));

            // This prevents repeated beacons
            nLastBeaconAdvertised = nBestHeight;

            // Clear "unable to send beacon" warning message (if any):
            msMiningErrors6.clear();

            return true;
        }
        catch(UniValue& objError)
        {
            sError = "Error: Unable to send beacon::"+objError.write();
            return false;
        }
        catch (std::exception &e)
        {
            sError = "Error: Unable to send beacon;:"+std::string(e.what());
            return false;
        }
    }
}

// Rpc

UniValue backupprivatekeys(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "backupprivatekeys\n"
                "\n"
                "Backup wallet private keys to file (Wallet must be fully unlocked!)\n");

    string sErrors;
    string sTarget;
    UniValue res(UniValue::VOBJ);

    bool bBackupPrivateKeys = BackupPrivateKeys(*pwalletMain, sTarget, sErrors);

    if (!bBackupPrivateKeys)
        res.pushKV("error", sErrors);

    else
        res.pushKV("location", sTarget);

    res.pushKV("result", bBackupPrivateKeys);

    return res;
}

UniValue rain(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "rain [UniValue](UniValue::VARR)\n"
                "\n"
                "[UniValue] -> Address<COL>Amount<ROW>...(UniValue::VARR)\n"
                "\n"
                "rains coins on the network\n");

    UniValue res(UniValue::VOBJ);
    std::string sNarr = executeRain(params[0].get_str());
    res.pushKV("Response", sNarr);
    return res;
}

UniValue rainbymagnitude(const UniValue& params, bool fHelp)
    {
    if (fHelp || (params.size() < 2 || params.size() > 3))
        throw runtime_error(
                "rainbymagnitude <whitelisted project> <amount> [message]\n"
                "\n"
                "<whitelisted project> --> Required: If a project is specified, rain will be limited to that project. Use * for network-wide.\n"
                "<amount> --> Required: Specify amount of coints in double to be rained\n"
                "[message] -> Optional: Provide a message rained to all rainees\n"
                "\n"
                "rain coins by magnitude on network");

    UniValue res(UniValue::VOBJ);

    std::string sProject = params[0].get_str();

    if (fDebug) LogPrintf("rainbymagnitude: sProject = %s", sProject.c_str());

    double dAmount = params[1].get_real();

    if (dAmount <= 0)
        throw runtime_error("Amount must be greater then 0");

    std::string sMessage = "";

    if (params.size() > 2)
        sMessage = params[2].get_str();

    // Make sure statistics are up to date. This will do nothing if a convergence has already been cached and is clean.
    bool bStatsAvail = ScraperSynchronizeDPOR();

    if (!bStatsAvail)
    {
        throw JSONRPCError(RPC_MISC_ERROR, "Wallet has not formed a convergence from statistics information.");
    }

    ScraperStats mScraperConvergedStats;
    {
        LOCK(cs_ConvergedScraperStatsCache);

        // Make a local copy of the cached stats in the convergence and release the lock.
        mScraperConvergedStats = ConvergedScraperStatsCache.mScraperConvergedStats;
    }

    if (fDebug) LogPrintf("rainbymagnitude: mScraperConvergedStats size = %u", mScraperConvergedStats.size());

    double dTotalAmount = 0;
    int64_t nTotalAmount = 0;

    double dTotalMagnitude = 0;

    statsobjecttype rainbymagmode = (sProject == "*" ? statsobjecttype::byCPID : statsobjecttype::byCPIDbyProject);

    //------- CPID ----------------CPID address -- Mag
    std::map<NN::Cpid, std::pair<CBitcoinAddress, double>> mCPIDRain;

    for (const auto& entry : mScraperConvergedStats)
    {
        // Only consider entries along the specfied dimension
        if (entry.first.objecttype == rainbymagmode)
        {
            NN::Cpid CPIDKey;

            if (rainbymagmode == statsobjecttype::byCPIDbyProject)
            {
                std::vector<std::string> vObjectStatsKey = split(entry.first.objectID, ",");

                // Only process elements that match the specified project if in project level rain.
                if (vObjectStatsKey[0] != sProject) continue;

                CPIDKey = NN::Cpid::Parse(vObjectStatsKey[1]);
            }
            else
            {
                CPIDKey = NN::Cpid::Parse(entry.first.objectID);
            }

            double dCPIDMag = std::round(entry.second.statsvalue.dMag);

            // Zero mag CPIDs do not get paid.
            if (!dCPIDMag) continue;

            // Find beacon grc address
            std::string scacheContract = ReadCache(Section::BEACON, CPIDKey.ToString()).value;

            // Should never occur but we know seg faults can occur in some cases
            if (scacheContract.empty()) continue;

            std::string sContract = DecodeBase64(scacheContract);
            std::string sGRCAddress = ExtractValue(sContract, ";", 2);

            if (fDebug) LogPrintf("INFO: rainbymagnitude: sGRCaddress = %s.", sGRCAddress);

            CBitcoinAddress address(sGRCAddress);

            if (!address.IsValid())
            {
                LogPrintf("ERROR: rainbymagnitude: Invalid Gridcoin address: %s.", sGRCAddress);
                continue;
            }

            mCPIDRain[CPIDKey] = std::make_pair(address, dCPIDMag);

            // Increment the accumulated mag. This will be equal to the total mag of the valid CPIDs entered
            // into the RAIN map, and will be used to normalize the payments.
            dTotalMagnitude += dCPIDMag;

            if (fDebug) LogPrintf("rainmagnitude: CPID = %s, address = %s, dCPIDMag = %f",
                                  CPIDKey.ToString(), sGRCAddress, dCPIDMag);
        }
    }

    if (mCPIDRain.empty() || !dTotalMagnitude)
    {
        throw JSONRPCError(RPC_MISC_ERROR, "No CPIDs to pay and/or total CPID magnitude is zero. This could be caused by an incorrect project specified.");
    }

    std::vector<std::pair<CScript, int64_t> > vecSend;

    // Setup the payment vector now that the CPID entries and mags have been validated and the total mag is computed.
    for(const auto& iter : mCPIDRain)
    {
        double dCPIDMag = iter.second.second;

            double dPayout = (dCPIDMag / dTotalMagnitude) * dAmount;

            dTotalAmount += dPayout;

            CScript scriptPubKey;
            scriptPubKey.SetDestination(iter.second.first.Get());

            int64_t nAmount = roundint64(dPayout * COIN);
            nTotalAmount += nAmount;

            vecSend.push_back(std::make_pair(scriptPubKey, nAmount));

            if (fDebug) LogPrintf("rainmagnitude: address = %s, amount = %f", iter.second.first.ToString(), CoinToDouble(nAmount));
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CWalletTx wtx;
    wtx.mapValue["comment"] = "Rain By Magnitude";
    wtx.hashBoinc = "<NARR>Rain By Magnitude: " + MakeSafeMessage(sMessage) + "</NARR>";

    EnsureWalletIsUnlocked();
    // Check funds
    double dBalance = GetTotalBalance();

    if (dTotalAmount > dBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");
    // Send
    CReserveKey keyChange(pwalletMain);

    int64_t nFeeRequired = 0;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);

    LogPrintf("Rain By Magnitude Transaction Created.");

    if (!fCreated)
    {
        if (nTotalAmount + nFeeRequired > pwalletMain->GetBalance())
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
    }

    LogPrintf("Committing.");
    // Rain the recipients
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
    {
        LogPrintf("Rain By Magnitude Commit failed.");

        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");
}
    res.pushKV("Rain By Magnitude",  "Sent");
    res.pushKV("TXID", wtx.GetHash().GetHex());
    res.pushKV("Rain Amount Sent", dTotalAmount);
    res.pushKV("TX Fee", ValueFromAmount(nFeeRequired));
    res.pushKV("# of Recipients", (uint64_t)vecSend.size());

    if (!sMessage.empty())
        res.pushKV("Message", sMessage);

    return res;
}

UniValue advertisebeacon(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "advertisebeacon\n"
                "\n"
                "Advertise a beacon (Requires wallet to be fully unlocked)\n");

    EnsureWalletIsUnlocked();

    UniValue res(UniValue::VOBJ);

    /* Try to copy key from config. The call is no-op if already imported or
     * nothing to import. This saves a migrating users from copy-pasting
     * the key string to importprivkey command.
     */
    const std::string primary_cpid = NN::GetPrimaryCpid();
    bool importResult= ImportBeaconKeysFromConfig(primary_cpid, pwalletMain);
    res.pushKV("ConfigKeyImported", importResult);

    std::string sOutPubKey = "";
    std::string sOutPrivKey = "";
    std::string sError = "";
    std::string sMessage = "";
    bool fResult = AdvertiseBeacon(sOutPrivKey,sOutPubKey,sError,sMessage);

    res.pushKV("Result", fResult ? "SUCCESS" : "FAIL");
    res.pushKV("CPID",primary_cpid.c_str());
    res.pushKV("Message",sMessage.c_str());

    if (!sError.empty())
        res.pushKV("Errors",sError);

    if (!fResult)
    {
        res.pushKV("FAILURE","Note: if your wallet is locked this command will fail; to solve that unlock the wallet: 'walletpassphrase <yourpassword> <240>'.");
    }
    else
    {
        res.pushKV("Public Key",sOutPubKey.c_str());
    }

    return res;
}

UniValue beaconreport(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "beaconreport\n"
                "\n"
                "Displays list of valid beacons in the network\n");

    LOCK(cs_main);

    UniValue res = GetJSONBeaconReport();

    return res;
}

UniValue beaconstatus(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "beaconstatus [cpid]\n"
                "\n"
                "[cpid] -> Optional parameter of cpid\n"
                "\n"
                "Displays status of your beacon or specified beacon on the network\n");

    UniValue res(UniValue::VOBJ);

    // Search for beacon, and report on beacon status.

    std::string sCPID;

    if (params.size() > 0)
        sCPID = params[0].get_str();

    // If sCPID is supplied, uses that. If not, then sCPID is filled in from GetPrimaryCPID.
    BeaconStatus beacon_status = GetBeaconStatus(sCPID);

    res.pushKV("CPID", sCPID);
    res.pushKV("Beacon Exists", YesNo(beacon_status.hasBeacon));
    res.pushKV("Beacon Timestamp", beacon_status.timestamp.c_str());
    res.pushKV("Public Key", beacon_status.sPubKey.c_str());

    std::string sErr = "";

    if (beacon_status.sPubKey.empty())
        sErr += "Public Key Missing. ";

    // Prior superblock Magnitude
    res.pushKV("Magnitude (As of last superblock)", beacon_status.dPriorSBMagnitude);

    res.pushKV("Mine", beacon_status.is_mine);

    if (beacon_status.is_mine && beacon_status.dPriorSBMagnitude == 0)
        res.pushKV("Warning","Your magnitude is 0 as of the last superblock: this may keep you from staking POR blocks.");

    if (!beacon_status.sPubKey.empty() && beacon_status.is_mine)
    {
        LOCK(cs_main);

        EnsureWalletIsUnlocked();

        bool bResult;
        std::string sSignature;
        std::string sError;

        // Staking Test 10-15-2016 - Simulate signing an actual block to verify this CPID keypair will work.
        uint256 hashBlock = GetRandHash();

        bResult = SignBlockWithCPID(sCPID, hashBlock.GetHex(), sSignature, sError);

        if (!bResult)
        {
            sErr += "Failed to sign block with cpid: ";
            sErr += sError;
            sErr += "; ";
        }

        bool fResult = VerifyCPIDSignature(sCPID, hashBlock.GetHex(), sSignature);

        res.pushKV("Block Signing Test Results", fResult);

        if (!fResult)
            sErr += "Failed to sign POR block.  This can happen if your keypair is invalid.  Check walletbackups for the correct keypair, or request that your beacon is deleted. ";
    }

    if (!sErr.empty())
    {
        res.pushKV("Errors", sErr);
        res.pushKV("Help", "Note: If your beacon is missing its public key, or is not in the chain, you may try: advertisebeacon.");
        res.pushKV("Configuration Status","FAIL");
    }

    else
        res.pushKV("Configuration Status", "SUCCESSFUL");

    return res;
}

UniValue explainmagnitude(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "explainmagnitude\n"
                "\n"
                "Itemize your CPID magnitudes by project.\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    const std::string primary_cpid = NN::GetPrimaryCpid();
    std::string sNeuralResponse = ExplainMagnitude(primary_cpid);

    if (sNeuralResponse.length() < 25)
    {
        res.pushKV("Neural Response", "false; Try again at a later time");

    }
    else
    {
        res.pushKV("Neural Response", "true (from THIS node)");

        std::vector<std::string> vMag = split(sNeuralResponse.c_str(),"<ROW>");

        for (unsigned int i = 0; i < vMag.size(); i++)
            res.pushKV(RoundToString(i+1,0),vMag[i].c_str());
    }

    return res;
}

UniValue lifetime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "lifetime [cpid]\n"
                "\n"
                "Displays research rewards for the lifetime of a CPID.\n");

    const NN::MiningId mining_id = params.size() > 0
        ? NN::MiningId::Parse(params[0].get_str())
        : NN::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    const NN::CpidOption cpid = mining_id.TryCpid();

    if (!cpid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
    }

    UniValue results(UniValue::VOBJ);

    LOCK(cs_main);

    for (const CBlockIndex* pindex = pindexGenesisBlock;
        pindex;
        pindex = pindex->pnext)
    {
        if (pindex->nResearchSubsidy > 0 && pindex->GetMiningId() == *cpid) {
            results.pushKV(
                std::to_string(pindex->nHeight),
                ValueFromAmount(pindex->nResearchSubsidy));
        }
    }

    return results;
}

UniValue magnitude(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "magnitude <cpid>\n"
                "\n"
                "<cpid> -> cpid to look up\n"
                "\n"
                "Displays information for the magnitude of all cpids or specified in the network\n");

    const NN::MiningId mining_id = params.size() > 0
        ? NN::MiningId::Parse(params[0].get_str())
        : NN::Researcher::Get()->Id();

    if (!mining_id.Valid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid CPID.");
    }

    if (const NN::CpidOption cpid = mining_id.TryCpid()) {
        LOCK(cs_main);

        return MagnitudeReport(*cpid);
    }

    throw JSONRPCError(RPC_INVALID_PARAMETER, "No data for investor.");
}

UniValue myneuralhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "myneuralhash\n"
                "\n"
                "Displays information about your neural networks client current hash\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    res.pushKV("My Neural Hash", NN::Quorum::CreateSuperblock().GetHash().ToString());

    return res;
}

UniValue neuralhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "neuralhash\n"
                "\n"
                "Displays information about the popular\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    res.pushKV("Popular", NN::Quorum::FindPopularHash(pindexBest).ToString());

    return res;
}

UniValue resetcpids(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "resetcpids\n"
                "\n"
                "Reloads cpids\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    ReadConfigFile(mapArgs, mapMultiArgs);
    NN::Researcher::Reload();

    res.pushKV("Reset", 1);

    return res;
}

UniValue staketime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "staketime\n"
                "\n"
                "Display information about staking time\n");

    UniValue res(UniValue::VOBJ);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const std::string cpid = NN::GetPrimaryCpid();
    const std::string GRCAddress = DefaultWalletAddress();
    GetEarliestStakeTime(GRCAddress, cpid);

    res.pushKV("GRCTime", ReadCache(Section::GLOBAL, "nGRCTime").timestamp);
    res.pushKV("CPIDTime", ReadCache(Section::GLOBAL, "nCPIDTime").timestamp);

    return res;
}

UniValue superblockage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "superblockage\n"
                "\n"
                "Display information regarding superblock age\n");

    UniValue res(UniValue::VOBJ);

    const NN::SuperblockPtr superblock = NN::Quorum::CurrentSuperblock();

    res.pushKV("Superblock Age", superblock.Age());
    res.pushKV("Superblock Timestamp", TimestampToHRDate(superblock.m_timestamp));
    res.pushKV("Superblock Block Number", superblock.m_height);
    res.pushKV("Pending Superblock Height", NN::Quorum::PendingSuperblock().m_height);

    return res;
}

UniValue superblocks(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
                "superblocks [lookback [displaycontract [cpid]]]\n"
                "\n"
                "[lookback] -> Optional: # of SB's to show (default 14)\n"
                "[displaycontract] -> Optional true/false: display SB contract (default false)\n"
                "[cpid] -> Optional: Shows magnitude for a cpid for recent superblocks\n"
                "\n"
                "Display data on recent superblocks\n");

    UniValue res(UniValue::VARR);

    int lookback = 14;
    bool displaycontract = false;
    std::string cpid = "";

    if (params.size() > 0)
    {
        lookback = params[0].get_int();
        if (fDebug) LogPrintf("INFO: superblocks: lookback %i", lookback);
    }

    if (params.size() > 1)
    {
        displaycontract = params[1].get_bool();
        if (fDebug) LogPrintf("INFO: superblocks: display contract %i", displaycontract);
    }

    if (params.size() > 2)
    {
        cpid = params[2].get_str();
        if (fDebug) LogPrintf("INFO: superblocks: CPID %s", cpid);
    }

    LOCK(cs_main);

    res = SuperblockReport(lookback, displaycontract, cpid);

    return res;
}

//!
//! \brief Send a transaction that contains an administrative contract.
//!
//! Before invoking this command, import the master key used to sign and verify
//! transactions that contain administrative contracts. The label is optional:
//!
//!     importprivkey <private_key_hex> master
//!
//! Send some coins to the master key address if necessary:
//!
//!     sendtoaddress <address> <amount>
//!
//! To whitelist a project:
//!
//!     addkey add project projectname url
//!
//! To de-whitelist a project:
//!
//!     addkey delete project projectname 1
//!
//! Key examples:
//!
//!     addkey add project milkyway@home http://milkyway.cs.rpi.edu/milkyway/@
//!     addkey delete project milkyway@home 1
//!
//! GRC will only memorize the *last* value it finds for a key in the highest
//! block.
//!
UniValue addkey(const UniValue& params, bool fHelp)
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

    if (pwalletMain->IsLocked()) {
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    }

    // TODO: remove this after Elizabeth mandatory block. We don't need to sign
    // version 2 contracts (the signature is discarded after the threshold):
    CKey key = pwalletMain->MasterPrivateKey();

    if (!key.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Missing or invalid master key.");
    }

    if (key.GetPubKey() != CWallet::MasterPublicKey()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Master private key mismatch.");
    }

    NN::ContractAction action = NN::ContractAction::UNKNOWN;

    if (params[0].get_str() == "add") {
        action = NN::ContractAction::ADD;
    } else if (params[0].get_str() == "delete") {
        action = NN::ContractAction::REMOVE;
    }

    NN::Contract contract(
        NN::Contract::Type::Parse(params[1].get_str()),
        action,
        params[2].get_str(),   // key
        params[3].get_str());  // value

    if (contract.m_type == NN::ContractType::UNKNOWN) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown contract type.");
    }

    if (contract.m_action == NN::ContractAction::UNKNOWN) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Action must be 'add' or 'delete'.");
    }

    if (!contract.RequiresMasterKey()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Not an admin contract type.");
    }

    // TODO: remove this after the v11 mandatory block. We don't need to sign
    // version 2 contracts (the signature is discarded after the threshold):
    if (!IsV11Enabled(nBestHeight + 1)) {
        contract.m_version = 1;

        if (!contract.Sign(key)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Failed to sign.");
        }

        if (!contract.VerifySignature()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Failed to verify signature.");
        }
    }

    std::pair<CWalletTx, std::string> result = SendContract(contract);
    std::string error = result.second;

    if (!error.empty()) {
        throw JSONRPCError(RPC_WALLET_ERROR, std::move(error));
    }

    UniValue res(UniValue::VOBJ);

    res.pushKV("Action", contract.m_action.ToString());
    res.pushKV("Type", contract.m_type.ToString());
    res.pushKV("Passphrase", contract.m_public_key.ToString());
    res.pushKV("Name", contract.m_key);
    res.pushKV("Value", contract.m_value);
    res.pushKV("Results", result.first.GetHash().GetHex().c_str());

    return res;
}

UniValue currentcontractaverage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currentcontractaverage\n"
                "\n"
                "Displays information on your current contract average with regards to superblock contract\n");

    UniValue res(UniValue::VOBJ);

    const NN::Superblock superblock = NN::Quorum::CreateSuperblock();

    res.pushKV("Contract", SuperblockToJson(superblock));
    res.pushKV("beacon_count", (uint64_t)superblock.m_cpids.TotalCount());
    res.pushKV("avg_mag", superblock.m_cpids.AverageMagnitude());
    res.pushKV("beacon_participant_count", (uint64_t)superblock.m_cpids.size());
    res.pushKV("superblock_valid", superblock.WellFormed());
    res.pushKV(".NET Neural Hash", superblock.GetHash().ToString());
    res.pushKV("Length", (uint64_t)GetSerializeSize(superblock, SER_NETWORK, PROTOCOL_VERSION));
    res.pushKV("Wallet Neural Hash", superblock.GetHash().ToString());

    return res;
}

UniValue debug(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable debug mode on the fly\n");

    UniValue res(UniValue::VOBJ);

    fDebug = params[0].get_bool();

    res.pushKV("Debug", fDebug ? "Entering debug mode." : "Exiting debug mode.");

    return res;
}

UniValue debug10(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug10 <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "Enable or disable debug mode on the fly\n");

    UniValue res(UniValue::VOBJ);

    fDebug10 = params[0].get_bool();

    res.pushKV("Debug10", fDebug10 ? "Entering debug mode." : "Exiting debug mode.");

    return res;
}

UniValue debug2(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "debug2 <bool>\n"
                "\n"
                "<bool> -> Specify true or false\n"
                "\n"
                "Enable or disable debug mode on the fly\n");

    UniValue res(UniValue::VOBJ);

    fDebug2 = params[0].get_bool();

    res.pushKV("Debug2", fDebug2 ? "Entering debug mode." : "Exiting debug mode.");

    return res;
}

UniValue getlistof(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getlistof <keytype>\n"
                "\n"
                "<keytype> -> key of requested data\n"
                "\n"
                "Displays data associated to a specified key type\n");

    UniValue res(UniValue::VOBJ);

    std::string sType = params[0].get_str();

    res.pushKV("Key Type", sType);

    LOCK(cs_main);

    UniValue entries(UniValue::VOBJ);
    for(const auto& entry : ReadSortedCacheSection(StringToSection(sType)))
    {
        const auto& key = entry.first;
        const auto& value = entry.second;

        UniValue obj(UniValue::VOBJ);
        obj.pushKV("value", value.value);
        obj.pushKV("timestamp", value.timestamp);
        entries.pushKV(key, obj);
    }

    res.pushKV("entries", entries);
    return res;
}

UniValue listdata(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "listdata <keytype>\n"
                "\n"
                "<keytype> -> key in cache\n"
                "\n"
                "Displays data associated to a key stored in cache\n");

    UniValue res(UniValue::VOBJ);

    std::string sType = params[0].get_str();

    res.pushKV("Key Type", sType);

    LOCK(cs_main);

    Section section = StringToSection(sType);
    for(const auto& item : ReadCacheSection(section))
        res.pushKV(item.first, item.second.value);

    return res;
}

UniValue listprojects(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "listprojects\n"
                "\n"
                "Displays information about whitelisted projects.\n");

    UniValue res(UniValue::VOBJ);

    for (const auto& project : NN::GetWhitelist().Snapshot().Sorted()) {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("display_name", project.DisplayName());
        entry.pushKV("url", project.m_url);
        entry.pushKV("base_url", project.BaseUrl());
        entry.pushKV("display_url", project.DisplayUrl());
        entry.pushKV("stats_url", project.StatsUrl());
        entry.pushKV("time", DateTimeStrFormat(project.m_timestamp));

        res.pushKV(project.m_name, entry);
    }

    return res;
}

UniValue memorizekeys(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "memorizekeys\n"
                "\n"
                "Runs a full table scan of Load Admin Messages\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    LoadAdminMessages(true);

    res.pushKV("Results", "done");

    return res;
}

UniValue network(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "network\n"
                "\n"
                "Display information about the network health\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    const int64_t now = pindexBest->nTime;
    const int64_t two_weeks_ago = now - (60 * 60 * 24 * 14);
    const double money_supply = pindexBest->nMoneySupply;
    const NN::SuperblockPtr superblock = NN::Quorum::CurrentSuperblock();

    int64_t two_week_block_subsidy = 0;
    int64_t two_week_research_subsidy = 0;

    for (const CBlockIndex* pindex = pindexBest;
        pindex && pindex->nTime > two_weeks_ago;
        pindex = pindex->pprev)
    {
        two_week_block_subsidy += pindex->nInterestSubsidy;
        two_week_research_subsidy += pindex->nResearchSubsidy;
    }

    res.pushKV("total_magnitude", (int)superblock->m_cpids.TotalMagnitude());
    res.pushKV("average_magnitude", superblock->m_cpids.AverageMagnitude());
    res.pushKV("magnitude_unit", NN::Tally::GetMagnitudeUnit(pindexBest));
    res.pushKV("research_paid_two_weeks", ValueFromAmount(two_week_research_subsidy));
    res.pushKV("research_paid_daily_average", ValueFromAmount(two_week_research_subsidy / 14));
    res.pushKV("research_paid_daily_limit", ValueFromAmount(NN::Tally::MaxEmission(now)));
    res.pushKV("stake_paid_two_weeks", ValueFromAmount(two_week_block_subsidy));
    res.pushKV("stake_paid_daily_average", ValueFromAmount(two_week_block_subsidy / 14));
    res.pushKV("total_money_supply", ValueFromAmount(money_supply));
    res.pushKV("network_interest_percent", money_supply > 0
        ? (two_week_block_subsidy / 14) * 365 / money_supply
        : 0);

    return res;
}

UniValue parselegacysb(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
                "parselegacysb\n"
                "\n"
                "Convert a legacy superblock contract to JSON.\n");

    UniValue json(UniValue::VOBJ);

    NN::Superblock superblock = NN::Superblock::UnpackLegacy(params[0].get_str());

    json.pushKV("contract", SuperblockToJson(superblock));
    json.pushKV("legacy_hash", superblock.GetHash().ToString());

    return json;
}

UniValue projects(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "projects\n"
                "\n"
                "Displays information on projects in the network as well as researcher data if available\n");

    UniValue res(UniValue::VARR);
    NN::ResearcherPtr researcher = NN::Researcher::Get();

    for (const auto& item : NN::GetWhitelist().Snapshot().Sorted())
    {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("Project", item.DisplayName());
        entry.pushKV("URL", item.DisplayUrl());

        if (const NN::ProjectOption project = researcher->Project(item.m_name)) {
            UniValue researcher(UniValue::VOBJ);

            researcher.pushKV("CPID", project->m_cpid.ToString());
            researcher.pushKV("Team", project->m_team);
            researcher.pushKV("Valid for Research", project->Eligible());

            if (!project->Eligible()) {
                researcher.pushKV("Errors", project->ErrorMessage());
            }

            entry.pushKV("Researcher", researcher);
        }

        res.push_back(entry);
    }

    return res;
}

UniValue readconfig(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "readconfig\n"
                "\n"
                "Re-reads config file; Does not overwrite pre-existing loaded values\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    ReadConfigFile(mapArgs, mapMultiArgs);

    res.pushKV("readconfig", 1);

    return res;
}

UniValue readdata(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "readdata <key>\n"
                "\n"
                "<key> -> generic key\n"
                "\n"
                "Reads generic data from disk from a specified key\n");

    UniValue res(UniValue::VOBJ);

    std::string sKey = params[0].get_str();
    std::string sValue = "?";

    //CTxDB txdb("cr");
    CTxDB txdb;

    if (!txdb.ReadGenericData(sKey, sValue))
    {
        res.pushKV("Error", sValue);

        sValue = "Failed to read from disk.";
    }

    res.pushKV("Key", sKey);
    res.pushKV("Result", sValue);

    return res;
}

UniValue refhash(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "refhash <walletaddress>\n"
                "\n"
                "<walletaddress> -> GRC address to test against\n"
                "\n"
                "Tests to see if a GRC Address is a participant in neural network along with default wallet address\n");

    UniValue res(UniValue::VOBJ);

    bool r1 = NN::Quorum::Participating(params[0].get_str(), GetAdjustedTime());
    bool r2 = NN::Quorum::Participating(DefaultWalletAddress(), GetAdjustedTime());

    res.pushKV("<Ref Hash", r1);
    res.pushKV("WalletAddress<Ref Hash", r2);

    return res;
}

UniValue sendblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "sendblock <blockhash>\n"
                "\n"
                "<blockhash> Blockhash of block to send to network\n"
                "\n"
                "Sends a block to the network\n");

    UniValue res(UniValue::VOBJ);

    uint256 hash = uint256S(params[0].get_str());
    bool fResult = AskForOutstandingBlocks(hash);

    res.pushKV("Requesting", hash.ToString());
    res.pushKV("Result", fResult);

    return res;
}

UniValue sendrawcontract(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "sendrawcontract <contract>\n"
                "\n"
                "<contract> -> custom contract\n"
                "\n"
                "Send a raw contract in a transaction on the network\n");

    UniValue res(UniValue::VOBJ);

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    CBitcoinAddress address(NN::Contract::BurnAddress());

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

    res.pushKV("Contract", sContract);
    res.pushKV("Recipient", address.ToString());
    res.pushKV("TrxID", wtx.GetHash().GetHex());

    return res;
}

UniValue superblockaverage(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "superblockaverage\n"
                "\n"
                "Displays average information for current superblock\n");

    UniValue res(UniValue::VOBJ);

    LOCK(cs_main);

    const NN::SuperblockPtr superblock = NN::Quorum::CurrentSuperblock();

    res.pushKV("beacon_count", (uint64_t)superblock->m_cpids.TotalCount());
    res.pushKV("beacon_participant_count", (uint64_t)superblock->m_cpids.size());
    res.pushKV("average_magnitude", superblock->m_cpids.AverageMagnitude());
    res.pushKV("superblock_valid", superblock->WellFormed());
    res.pushKV("Superblock Age", superblock.Age());
    res.pushKV("Dire Need of Superblock", NN::Quorum::SuperblockNeeded());

    return res;
}

UniValue versionreport(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "versionreport <lookback:int> <full:bool>\n"
                "\n"
                "<lookback> --> Number of blocks to tally from the chain head "
                    "(default: " + std::to_string(BLOCKS_PER_DAY) + ").\n"
                "<full> ------> Classify by commit suffix (default: false).\n"
                "\n"
                "Display the software versions of nodes that recently staked.\n");

    const int64_t lookback = params.size() > 0
        ? std::max(params[0].get_int(), 1)
        : BLOCKS_PER_DAY;

    const bool full_version = params.size() > 1
        ? params[1].get_bool()
        : false;

    LOCK(cs_main);

    return GetJSONVersionReport(lookback, full_version);
}

UniValue writedata(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "writedata <key> <value>\n"
                "\n"
                "<key> ---> Key where value will be written\n"
                "<value> -> Value to be written to specified key\n"
                "\n"
                "Writes a value to specified key\n");

    UniValue res(UniValue::VOBJ);

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

    res.pushKV("Result", result);

    return res;
}

// Network RPC commands

UniValue askforoutstandingblocks(const UniValue& params, bool fHelp)
        {
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "askforoutstandingblocks\n"
                "\n"
                "Requests network for outstanding blocks\n");

    UniValue res(UniValue::VOBJ);

    bool fResult = AskForOutstandingBlocks(uint256());

    res.pushKV("Sent.", fResult);

    return res;
}

UniValue getblockchaininfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getblockchaininfo\n"
                "\n"
                "Displays data on current blockchain\n");

    LOCK(cs_main);

    UniValue res(UniValue::VOBJ), diff(UniValue::VOBJ);

    res.pushKV("blocks", nBestHeight);
    res.pushKV("moneysupply", ValueFromAmount(pindexBest->nMoneySupply));
    diff.pushKV("current", GetDifficulty(GetLastBlockIndex(pindexBest, true)));
    diff.pushKV("target", GetBlockDifficulty(GetNextTargetRequired(pindexBest)));
    res.pushKV("difficulty", diff);
    res.pushKV("testnet", fTestNet);
    res.pushKV("errors", GetWarnings("statusbar"));

    return res;
}


UniValue currenttime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "currenttime\n"
                "\n"
                "Displays UTC Unix time as well as date and time in UTC\n");

    UniValue res(UniValue::VOBJ);

    res.pushKV("Unix", GetAdjustedTime());
    res.pushKV("UTC", TimestampToHRDate(GetAdjustedTime()));

    return res;
}

UniValue memorypool(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "memorypool\n"
                "\n"
                "Displays included and excluded memory pool txs\n");

    UniValue res(UniValue::VOBJ);

    res.pushKV("Excluded Tx", msMiningErrorsExcluded);
    res.pushKV("Included Tx", msMiningErrorsIncluded);

    return res;
}

UniValue networktime(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "networktime\n"
                "\n"
                "Displays current network time\n");

    UniValue res(UniValue::VOBJ);

    res.pushKV("Network Time", GetAdjustedTime());

    return res;
}

UniValue execute(const UniValue& params, bool fHelp)
{
    throw JSONRPCError(RPC_DEPRECATED, "execute function has been deprecated; run the command as previously done so but without execute");
}

UniValue SuperblockReport(int lookback, bool displaycontract, std::string cpid)
{
    UniValue results(UniValue::VARR);

    int nMaxDepth = nBestHeight;
    int nLookback = BLOCKS_PER_DAY * lookback;
    int nMinDepth = (nMaxDepth - nLookback) - ( (nMaxDepth-nLookback) % BLOCK_GRANULARITY);
    //int iRow = 0;
    CBlockIndex* pblockindex = pindexBest;
    while (pblockindex->nHeight > nMaxDepth)
    {
        if (!pblockindex || !pblockindex->pprev || pblockindex == pindexGenesisBlock) return results;
        pblockindex = pblockindex->pprev;
    }

    const NN::CpidOption cpid_parsed = NN::MiningId::Parse(cpid).TryCpid();

    while (pblockindex->nHeight > nMinDepth)
    {
        if (!pblockindex || !pblockindex->pprev) return results;
        pblockindex = pblockindex->pprev;
        if (pblockindex == pindexGenesisBlock) return results;
        if (!pblockindex->IsInMainChain()) continue;
        if (IsSuperBlock(pblockindex))
        {
            const NN::ClaimOption claim = GetClaimByIndex(pblockindex);

            if (claim && claim->ContainsSuperblock())
            {
                const NN::Superblock& superblock = claim->m_superblock;

                UniValue c(UniValue::VOBJ);
                c.pushKV("height", ToString(pblockindex->nHeight));
                c.pushKV("block", pblockindex->GetBlockHash().GetHex());
                c.pushKV("date", TimestampToHRDate(pblockindex->nTime));
                c.pushKV("wallet_version", claim->m_client_version);

                c.pushKV("total_cpids", (int)superblock.m_cpids.TotalCount());
                c.pushKV("active_beacons", (int)superblock.m_cpids.size());
                c.pushKV("inactive_beacons", (int)superblock.m_cpids.Zeros());

                c.pushKV("total_magnitude", superblock.m_cpids.TotalMagnitude());
                c.pushKV("average_magnitude", superblock.m_cpids.AverageMagnitude());

                c.pushKV("total_projects", (int)superblock.m_projects.size());

                if (cpid_parsed)
                {
                    c.pushKV("Magnitude", superblock.m_cpids.MagnitudeOf(*cpid_parsed).Floating());
                }

                if (displaycontract)
                    c.pushKV("Contract Contents", SuperblockToJson(superblock));

                results.push_back(c);
            }
        }
    }

    return results;
}

UniValue MagnitudeReport(const NN::Cpid cpid)
{
    UniValue json(UniValue::VOBJ);

    const int64_t now = OutOfSyncByAge() ? pindexBest->nTime : GetAdjustedTime();
    const NN::ResearchAccount& account = NN::Tally::GetAccount(cpid);
    const NN::AccrualComputer calc = NN::Tally::GetComputer(cpid, now, pindexBest);

    json.pushKV("CPID", cpid.ToString());
    json.pushKV("Magnitude (Last Superblock)", NN::Quorum::GetMagnitude(cpid).Floating());
    json.pushKV("Current Magnitude Unit", calc->MagnitudeUnit());

    json.pushKV("First Payment Time", TimestampToHRDate(account.FirstRewardTime()));
    json.pushKV("First Block Paid", account.FirstRewardBlockHash().ToString());
    json.pushKV("First Height Paid", (int)account.FirstRewardHeight());

    json.pushKV("Last Payment Time", TimestampToHRDate(account.LastRewardTime()));
    json.pushKV("Last Block Paid", account.LastRewardBlockHash().ToString());
    json.pushKV("Last Height Paid", (int)account.LastRewardHeight());

    json.pushKV("Accrual Days", calc->AccrualDays());
    json.pushKV("Owed", ValueFromAmount(calc->Accrual()));

    if (fDebug) {
        json.pushKV("Owed (raw)", ValueFromAmount(calc->RawAccrual()));
        json.pushKV("Owed (last superblock)", ValueFromAmount(account.m_accrual));
    }

    json.pushKV("Expected Earnings (14 days)", ValueFromAmount(calc->ExpectedDaily() * 14));
    json.pushKV("Expected Earnings (Daily)", ValueFromAmount(calc->ExpectedDaily()));

    json.pushKV("Lifetime Research Paid", ValueFromAmount(account.m_total_research_subsidy));
    json.pushKV("Lifetime Magnitude Sum", (int)account.m_total_magnitude);
    json.pushKV("Lifetime Magnitude Average", account.AverageLifetimeMagnitude());
    json.pushKV("Lifetime Payments", (int)account.m_accuracy);

    json.pushKV("Lifetime Payments Per Day", ValueFromAmount(calc->PaymentPerDay()));
    json.pushKV("Lifetime Payments Per Day Limit", ValueFromAmount(calc->PaymentPerDayLimit()));

    return json;
}

UniValue GetJSONBeaconReport()
{
    UniValue results(UniValue::VARR);
    UniValue entry(UniValue::VOBJ);
    entry.pushKV("CPID","GRCAddress");
    std::string row;
    for(const auto& item : ReadSortedCacheSection(Section::BEACON))
    {
        const std::string& key = item.first;
        const AppCacheEntry& cache = item.second;
        row = key + "<COL>" + cache.value;
        std::string contract = DecodeBase64(cache.value);
        std::string grcaddress = ExtractValue(contract,";",2);
        entry.pushKV(key, grcaddress);
    }

    results.push_back(entry);
    return results;
}

UniValue GetJSONVersionReport(const int64_t lookback, const bool full_version)
{
    const int64_t min_height = std::max<int64_t>(nBestHeight - lookback, 0);

    std::map<std::string, uint64_t> version_tally;

    for (const CBlockIndex* pindex = pindexBest;
        pindex && pindex->nHeight > min_height;
        pindex = pindex->pprev)
    {
        CBlock block;
        block.ReadFromDisk(pindex);

        std::string version = block.PullClaim().m_client_version;

        if (version.empty()) {
            version = "unknown";
        } else if (!full_version) {
            // Ignore the source control version commit ID after the hyphen:
            const size_t separator_position = version.find('-');

            if (separator_position != std::string::npos) {
                version.erase(separator_position);
            }
        }

        ++version_tally[version];
    }

    UniValue json(UniValue::VARR);

    for (const auto& version_pair : version_tally) {
        UniValue entry(UniValue::VOBJ);

        entry.pushKV("version", version_pair.first);
        entry.pushKV("count", version_pair.second);
        entry.pushKV("percent", ((double)version_pair.second / lookback) * 100);

        json.push_back(entry);
    }

    return json;
}

std::string YesNo(bool f)
{
    return f ? "Yes" : "No";
}

UniValue listitem(const UniValue& params, bool fHelp)
{
    throw JSONRPCError(RPC_DEPRECATED, "list is deprecated; Please run the command the same as previously without list");
}

// ppcoin: get information of sync-checkpoint
UniValue getcheckpoint(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getcheckpoint\n"
                "Show info of synchronized checkpoint.\n");

    UniValue result(UniValue::VOBJ);

    LOCK(cs_main);

    const CBlockIndex* pindexCheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
    if(pindexCheckpoint != NULL)
    {
        result.pushKV("synccheckpoint", pindexCheckpoint->GetBlockHash().ToString().c_str());
        result.pushKV("height", pindexCheckpoint->nHeight);
        result.pushKV("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str());
    }

    return result;
}

//Brod
UniValue rpc_reorganize(const UniValue& params, bool fHelp)
{
    UniValue results(UniValue::VOBJ);
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "reorganize <hash>\n"
                "Roll back the block chain to specified block hash.\n"
                "The block hash must already be present in block index");

    uint256 NewHash;
    NewHash.SetHex(params[0].get_str());

    bool fResult = ForceReorganizeToHash(NewHash);
    results.pushKV("RollbackChain",fResult);
    return results;
}
