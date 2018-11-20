// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>

#include "base58.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "txdb.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "wallet.h"

using namespace std;
using namespace boost;
using namespace boost::assign;

extern std::vector<std::pair<std::string, std::string>> GetTxStakeBoincHashInfo(const CMerkleTx& mtx);
extern std::vector<std::pair<std::string, std::string>> GetTxNormalBoincHashInfo(const CMerkleTx& mtx);
std::string TimestampToHRDate(double dtm);
std::string GetPollXMLElementByPollTitle(std::string pollname, std::string XMLElement1, std::string XMLElement2);
std::string GetShareType(double dShareType);
bool PollCreatedAfterSecurityUpgrade(std::string pollname);
double DoubleFromAmount(int64_t amount);

std::vector<std::pair<std::string, std::string>> GetTxStakeBoincHashInfo(const CMerkleTx& mtx)
{
    assert(mtx.IsCoinStake() || mtx.IsCoinBase());
    std::vector<std::pair<std::string, std::string>> res;

    // Fetch BlockIndex for tx block
    CBlockIndex* pindex = NULL;
    CBlock block;
    {
        BlockMap::iterator mi = mapBlockIndex.find(mtx.hashBlock);
        if (mi == mapBlockIndex.end())
        {
            res.push_back(std::make_pair(_("ERROR"), _("Block not in index")));
            return res;
        }

        pindex = (*mi).second;

        if (!block.ReadFromDisk(pindex))
        {
            res.push_back(std::make_pair(_("ERROR"), _("Block read failed")));
            return res;
        }
    }

    //Deserialize
    MiningCPID bb = DeserializeBoincBlock(block.vtx[0].hashBoinc,block.nVersion);

    res.push_back(std::make_pair(_("Height"), ToString(pindex->nHeight)));
    res.push_back(std::make_pair(_("Block Version"), ToString(block.nVersion)));
    res.push_back(std::make_pair(_("Difficulty"), RoundToString(GetBlockDifficulty(block.nBits),8)));
    res.push_back(std::make_pair(_("CPID"), bb.cpid));
    res.push_back(std::make_pair(_("Interest"), RoundToString(bb.InterestSubsidy,8)));

    if (bb.ResearchAverageMagnitude > 0)
    {
        res.push_back(std::make_pair(_("Boinc Reward"), RoundToString(bb.ResearchSubsidy,8)));
        res.push_back(std::make_pair(_("Magnitude"), RoundToString(bb.Magnitude,8)));
        res.push_back(std::make_pair(_("Average Magnitude"), RoundToString(bb.ResearchAverageMagnitude, 8)));
        res.push_back(std::make_pair(_("Research Age"), RoundToString(bb.ResearchAge, 8)));
    }

    res.push_back(std::make_pair(_("Is Superblock"), (bb.superblock.length() >= 20 ? "Yes" : "No")));

    if(fDebug)
    {
        if (bb.superblock.length() >= 20)
            res.push_back(std::make_pair(_("Neural Contract Binary Size"), ToString(bb.superblock.length())));

        res.push_back(std::make_pair(_("Neural Hash"), bb.NeuralHash));
        res.push_back(std::make_pair(_("Current Neural Hash"), bb.CurrentNeuralHash));
        res.push_back(std::make_pair(_("Client Version"), bb.clientversion));
        res.push_back(std::make_pair(_("Organization"), bb.Organization));
        res.push_back(std::make_pair(_("Boinc Public Key"), bb.BoincPublicKey));
    }

    return res;
}

std::vector<std::pair<std::string, std::string>> GetTxNormalBoincHashInfo(const CMerkleTx& mtx)
{
    assert(!mtx.IsCoinStake() && !mtx.IsCoinBase());
    std::vector<std::pair<std::string, std::string>> res;

    try
    {
        const std::string &msg = mtx.hashBoinc;

        res.push_back(std::make_pair(_("Network Date"), TimestampToHRDate((double)mtx.nTime)));

        if (fDebug)
            res.push_back(std::make_pair(_("Message Length"), ToString(msg.length())));

        std::string sMessageType = ExtractXML(msg, "<MT>", "</MT>");
        std::string sTxMessage = ExtractXML(msg, "<MESSAGE>", "</MESSAGE>");
        std::string sRainMessage = ExtractXML(msg, "<NARR>", "</NARR>");

        if (sMessageType.length())
        {
            if (sMessageType == "beacon")
            {
                std::string sBeaconAction = ExtractXML(msg, "<MA>", "</MA>");
                std::string sBeaconCPID = ExtractXML(msg, "<MK>", "</MK>");

                if (sBeaconAction == "A")
                {
                    res.push_back(std::make_pair(_("Message Type"), _("Add Beacon Contract")));

                    std::string sBeaconEncodedContract = ExtractXML(msg, "<MV>", "</MV>");

                    if (sBeaconEncodedContract.length() < 256)
                    {
                        // If for whatever reason the contract is not a proper one and the average length does exceed this size; Without this a seg fault will occur on the DecodeBase64
                        // Another example is if an admin accidently uses add instead of delete in addkey to remove a beacon the 1 in <MV>1</MV> would cause a seg fault as well
                        res.push_back(std::make_pair(_("ERROR"), _("Contract length for beacon is less then 256 in length. Size: ") + ToString(sBeaconEncodedContract.length())));

                        if (fDebug)
                            res.push_back(std::make_pair(_("Message Data"), sBeaconEncodedContract));

                        return res;
                    }

                    std::string sBeaconDecodedContract = DecodeBase64(sBeaconEncodedContract);
                    std::vector<std::string> vBeaconContract = split(sBeaconDecodedContract.c_str(), ";");
                    std::string sBeaconAddress = vBeaconContract[2];
                    std::string sBeaconPublicKey = vBeaconContract[3];

                    res.push_back(std::make_pair(_("CPID"), sBeaconCPID));
                    res.push_back(std::make_pair(_("Address"), sBeaconAddress));
                    res.push_back(std::make_pair(_("Public Key"), sBeaconPublicKey));
                }

                else if (sBeaconAction == "D")
                {
                    res.push_back(std::make_pair(_("Message Type"), _("Delete Beacon Contract")));
                    res.push_back(std::make_pair(_("CPID"), sBeaconCPID));
                }
            }

            else if (sMessageType == "poll")
            {
                std::string sPollType = ExtractXML(msg, "<MK>", "</MK>");
                std::string sPollTitle = ExtractXML(msg, "<TITLE>", "</TITLE>");
                std::replace(sPollTitle.begin(), sPollTitle.end(), '_', ' ');
                std::string sPollDays = ExtractXML(msg, "<DAYS>", "</DAYS>");
                std::string sPollQuestion = ExtractXML(msg, "<QUESTION>", "</QUESTION>");
                std::string sPollAnswers = ExtractXML(msg, "<ANSWERS>", "</ANSWERS>");
                std::string sPollShareType = ExtractXML(msg, "<SHARETYPE>", "</SHARETYPE>");
                std::string sPollUrl = ExtractXML(msg, "<URL>", "</URL");
                std::string sPollExpiration = ExtractXML(msg, "<EXPIRATION>", "</EXPIRATION>");
                std::replace(sPollAnswers.begin(), sPollAnswers.end(), ';', ',');
                sPollShareType = GetShareType(std::stod(sPollShareType));

                if (Contains(sPollType, "[Foundation"))
                    res.push_back(std::make_pair(_("Message Type"), _("Add Foundation Poll")));

                else
                    res.push_back(std::make_pair(_("Message Type"), _("Add Poll")));

                res.push_back(std::make_pair(_("Title"), sPollTitle));
                res.push_back(std::make_pair(_("Question"), sPollQuestion));
                res.push_back(std::make_pair(_("Answers"), sPollAnswers));
                res.push_back(std::make_pair(_("Share Type"), sPollShareType));
                res.push_back(std::make_pair(_("URL"), sPollUrl));
                res.push_back(std::make_pair(_("Duration"), sPollDays + _(" days")));
                res.push_back(std::make_pair(_("Expires"), TimestampToHRDate(std::stod(sPollExpiration))));
            }

            else if (sMessageType == "vote")
            {
                std::string sVoteTitled = ExtractXML(msg, "<TITLE>", "</TITLE>");
                std::string sVoteShareType = GetPollXMLElementByPollTitle(sVoteTitled, "<SHARETYPE>", "</SHARETYPE>");
                std::string sVoteTitle = sVoteTitled;
                std::replace(sVoteTitle.begin(), sVoteTitle.end(), '_', ' ');
                std::string sVoteAnswer = ExtractXML(msg, "<ANSWER>", "</ANSWER>");
                std::replace(sVoteAnswer.begin(), sVoteAnswer.end(), ';', ',');

                res.push_back(std::make_pair(_("Message Type"), _("Vote")));
                res.push_back(std::make_pair(_("Title"), sVoteTitle));

                if (sVoteShareType.empty())
                {
                    res.push_back(std::make_pair(_("Share Type"), _("Unable to extract Share Type. Vote likely > 6 months old")));
                    res.push_back(std::make_pair(_("Answer"), sVoteAnswer));

                    if (fDebug)
                        res.push_back(std::make_pair(_("Share Type Debug"), sVoteShareType));

                    return res;
                }

                else
                    res.push_back(std::make_pair(_("Share Type"), GetShareType(std::stod(sVoteShareType))));

                res.push_back(std::make_pair(_("Answer"), sVoteAnswer));

                // Basic Variables for all poll types
                double dVoteWeight = 0;
                double dVoteMagnitude = 0;
                double dVoteBalance = 0;
                std::string sVoteMagnitude;
                std::string sVoteBalance;

                // Get voting magnitude and balance; These fields are always in vote contract
                if (!PollCreatedAfterSecurityUpgrade(sVoteTitled))
                {
                    sVoteMagnitude = ExtractXML(msg, "<MAGNITUDE>", "</MAGNITUDE>");
                    sVoteBalance = ExtractXML(msg, "<BALANCE>", "</BALANCE>");
                }

                else
                {
                    sVoteMagnitude = ExtractXML(msg, "<INNERMAGNITUDE>", "</INNERMAGNITUDE>");
                    sVoteBalance = ExtractXML(msg, "<TOTALVOTEDBALANCE>", "</TOTALVOTEDBALANCE>");
                }

                if (sVoteShareType == "1")
                    dVoteWeight = std::stod(sVoteMagnitude);

                else if (sVoteShareType == "2")
                    dVoteWeight = std::stod(sVoteBalance);

                else if (sVoteShareType == "3")
                {
                    // For voting mag for mag + balance polls we need to calculate total network magnitude from superblock before vote to use the correct data in formula.
                    // This gives us an accurate vote shares at that time. We like to keep wallet information as accurate as possible.
                    // Note during boosted superblocks we get unusual calculations for total network magnitude.
                    CBlockIndex* pblockindex = mapBlockIndex[mtx.hashBlock];
                    CBlock block;

                    if(pblockindex)
                    {
                        int nEndHeight = pblockindex->nHeight - (BLOCKS_PER_DAY*14);

                        // Incase; Why throw.
                        if (nEndHeight < 1)
                            nEndHeight = 1;

                        // Iterate back to find previous superblock
                        while (pblockindex->nHeight > nEndHeight && pblockindex->nIsSuperBlock == 0)
                            pblockindex = pblockindex->pprev;
                    }

                    if (pblockindex && pblockindex->nIsSuperBlock)
                    {
                        block.ReadFromDisk(pblockindex);

                        std::string sHashBoinc = block.vtx[0].hashBoinc;

                        MiningCPID vbb = DeserializeBoincBlock(sHashBoinc, block.nVersion);

                        std::string sUnpackedSuperblock = UnpackBinarySuperblock(vbb.superblock);
                        std::string sMagnitudeContract = ExtractXML(sUnpackedSuperblock, "<MAGNITUDES>", "</MAGNITUDES>");

                        // Since Superblockavg function gives avg for mags yes but total cpids we cannot use this function
                        // We need the rows_above_zero for Total Network Magnitude calculation with Money Supply Factor.
                        std::vector<std::string> vMagnitudeContract = split(sMagnitudeContract, ";");
                        int nRowsWithMag = 0;
                        double dTotalMag = 0;

                        for (auto const& sMag : vMagnitudeContract)
                        {
                            const std::vector<std::string>& vFields = split(sMag, ",");

                            if (vFields.size() < 2)
                                continue;

                            const std::string& sCPID = vFields[0];
                            double dMAG = std::stoi(vFields[1].c_str());

                            if (sCPID.length() > 10)
                            {
                                nRowsWithMag++;
                                dTotalMag += dMAG;
                            }
                        }

                        double dOutAverage = dTotalMag / ((double)nRowsWithMag + .01);
                        double dTotalNetworkMagnitude = (double)nRowsWithMag * dOutAverage;
                        double dMoneySupply = DoubleFromAmount(pblockindex->nMoneySupply);
                        double dMoneySupplyFactor = (dMoneySupply/dTotalNetworkMagnitude + .01);

                        dVoteMagnitude = RoundFromString(sVoteMagnitude,2);
                        dVoteBalance = RoundFromString(sVoteBalance,2);

                        if (dVoteMagnitude > 0)
                            dVoteWeight = ((dMoneySupplyFactor/5.67) * dVoteMagnitude) + std::stod(sVoteBalance);

                        else
                            dVoteWeight = std::stod(sVoteBalance);

                        res.push_back(std::make_pair(_("Magnitude"), RoundToString(dVoteMagnitude, 2)));
                        res.push_back(std::make_pair(_("Balance"), RoundToString(dVoteBalance, 2)));
                    }

                    else
                    {
                        res.push_back(std::make_pair(_("ERROR"), _("Unable to obtain superblock data before vote was made to calculate voting weight")));

                        dVoteWeight = -1;
                        res.push_back(std::make_pair(_("Magnitude"), RoundToString(dVoteMagnitude, 2)));
                        res.push_back(std::make_pair(_("Balance"), RoundToString(dVoteBalance, 2)));

                    }
                }

                else if (sVoteShareType == "4" || sVoteShareType == "5")
                    dVoteWeight = 1;

                res.push_back(std::make_pair(_("Weight"), RoundToString(dVoteWeight, 0)));
            }

            else if (sMessageType == "project")
            {
                std::string sProjectName = ExtractXML(msg, "<MK>", "</MK>");
                std::string sProjectURL = ExtractXML(msg, "<MV>", "</MV>");
                std::string sProjectAction = ExtractXML(msg, "<MA>", "</MA>");

                if (sProjectAction == "A")
                    res.push_back(std::make_pair(_("Message Type"), _("Add Project")));

                else if (sProjectAction == "D")
                    res.push_back(std::make_pair(_("Message Type"), _("Delete Project")));

                res.push_back(std::make_pair(_("Name"), sProjectName));

                if (sProjectAction == "A")
                    res.push_back(std::make_pair(_("URL"), sProjectURL));
            }

            else
            {
                res.push_back(std::make_pair(_("Message Type"), _("Unknown")));

                if (fDebug)
                    res.push_back(std::make_pair(_("Data"), msg));

                return res;
            }
        }

        else if (sTxMessage.length())
        {
            res.push_back(std::make_pair(_("Message Type"), _("Text Message")));
            res.push_back(std::make_pair(_("Message"), sTxMessage));
        }

        else if (sRainMessage.length())
        {
            res.push_back(std::make_pair(_("Message Type"), _("Text Rain Message")));
            res.push_back(std::make_pair(_("Message"), sRainMessage));
        }

        else if (sMessageType.empty() && sTxMessage.empty() && sRainMessage.empty())
            res.push_back(std::make_pair(_("Message Type"), _("No Attached Messages")));

        return res;
    }

    catch (const std::invalid_argument& e)
    {
        std::string sE(e.what());

        res.push_back(std::make_pair(_("ERROR"), _("Invalid argument exception while parsing Transaction Message -> ") + sE));

        return res;
    }

    catch (const std::out_of_range& e)
    {
        std::string sE(e.what());

        res.push_back(std::make_pair(_("ERROR"), _("Out of range exception while parsing Transaction Message -> ") + sE));

        return res;
    }
}

void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex)
{
    txnouttype type;
    vector<CTxDestination> addresses;
    int nRequired;

    out.pushKV("asm", scriptPubKey.ToString());

    if (fIncludeHex)
        out.pushKV("hex", HexStr(scriptPubKey.begin(), scriptPubKey.end()));

    if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired))
    {
        out.pushKV("type", GetTxnOutputType(type));
        return;
    }

    out.pushKV("reqSigs", nRequired);
    out.pushKV("type", GetTxnOutputType(type));

    UniValue a(UniValue::VARR);
    for (auto const& addr : addresses)
        a.push_back(CBitcoinAddress(addr).ToString());
    out.pushKV("addresses", a);
}

void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry)
{
    entry.pushKV("txid", tx.GetHash().GetHex());
    entry.pushKV("version", tx.nVersion);
    entry.pushKV("time", (int)tx.nTime);
    entry.pushKV("locktime", (int)tx.nLockTime);
    entry.pushKV("hashboinc", tx.hashBoinc);

    UniValue vin(UniValue::VARR);
    for (auto const& txin : tx.vin)
    {
        UniValue in(UniValue::VOBJ);

        if (tx.IsCoinBase())
            in.pushKV("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end()));

        else
        {
            in.pushKV("txid", txin.prevout.hash.GetHex());

            in.pushKV("vout", (int)txin.prevout.n);
            UniValue o(UniValue::VOBJ);
            o.pushKV("asm", txin.scriptSig.ToString());
            o.pushKV("hex", HexStr(txin.scriptSig.begin(), txin.scriptSig.end()));
            in.pushKV("scriptSig", o);
        }
        in.pushKV("sequence", (int)txin.nSequence);
        vin.push_back(in);
    }
    entry.pushKV("vin", vin);
    UniValue vout(UniValue::VARR);
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        UniValue out(UniValue::VOBJ);
        out.pushKV("value", ValueFromAmount(txout.nValue));
        out.pushKV("n", (int)i);
        UniValue o(UniValue::VOBJ);
        ScriptPubKeyToJSON(txout.scriptPubKey, o, true);
        out.pushKV("scriptPubKey", o);
        vout.push_back(out);
    }
    entry.pushKV("vout", vout);

    if (hashBlock != 0)
    {
        entry.pushKV("blockhash", hashBlock.GetHex());
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
        {
            CBlockIndex* pindex = (*mi).second;
            if (pindex->IsInMainChain())
            {
                entry.pushKV("confirmations", 1 + nBestHeight - pindex->nHeight);
                entry.pushKV("time", (int)pindex->nTime);
                entry.pushKV("blocktime", (int)pindex->nTime);
            }
            else
                entry.pushKV("confirmations", 0);
        }
    }
}

UniValue getrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getrawtransaction <txid> [verbose=bool]\n"
                "\n"
                "If verbose is false, returns a string that is\n"
                "serialized, hex-encoded data for <txid>.\n"
                "If verbose is true, returns an Object\n"
                "with information about <txid>\n");

    uint256 hash;
    hash.SetHex(params[0].get_str());

    // Accept either a bool (true) or a num (>=1) to indicate verbose output. Adapted from Bitcoin 20180820.
    bool fVerbose = false;
    if (!params[1].isNull())
    {
        fVerbose = params[1].isNum() ? (params[1].get_int() != 0) : params[1].get_bool();
    }
    
    LOCK(cs_main);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;
    string strHex = HexStr(ssTx.begin(), ssTx.end());

    if (!fVerbose)
        return strHex;

    UniValue result(UniValue::VOBJ);
    result.pushKV("hex", strHex);
    TxToJSON(tx, hashBlock, result);
    return result;
}

UniValue listunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
                "listunspent [minconf=1] [maxconf=9999999]  [\"address\",...]\n"
                "\n"
                "Returns array of unspent transaction outputs\n"
                "with between minconf and maxconf (inclusive) confirmations.\n"
                "Optionally filtered to only include txouts paid to specified addresses.\n"
                "Results are an array of Objects, each of which has:\n"
                "{txid, vout, scriptPubKey, amount, confirmations}\n");

    RPCTypeCheck(params, list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VARR));

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    set<CBitcoinAddress> setAddress;
    if (params.size() > 2)
    {
        UniValue inputs = params[2].get_array();
        for (unsigned int ix = 0; ix < inputs.size(); ix++)
        {
            CBitcoinAddress address(inputs[ix].get_str());
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+inputs[ix].get_str());
            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+inputs[ix].get_str());
           setAddress.insert(address);
        }
    }

    UniValue results(UniValue::VARR);

    vector<COutput> vecOutputs;

    pwalletMain->AvailableCoins(vecOutputs, false, NULL, false);

    LOCK(pwalletMain->cs_wallet);

    for (auto const& out : vecOutputs)
    {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;

        if(setAddress.size())
        {
            CTxDestination address;
            if(!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
                continue;

            if (!setAddress.count(address))
                continue;
        }

        int64_t nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("txid", out.tx->GetHash().GetHex());
        entry.pushKV("vout", out.i);
        CTxDestination address;
        if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
        {
            entry.pushKV("address", CBitcoinAddress(address).ToString());

            auto item = pwalletMain->mapAddressBook.find(address);

            if (item != pwalletMain->mapAddressBook.end())
            {
                entry.pushKV("label", item->second);

                if (GetBoolArg("-enableaccounts", false))
                    entry.pushKV("account", item->second);
            }
        }
        entry.pushKV("scriptPubKey", HexStr(pk.begin(), pk.end()));
        entry.pushKV("amount", ValueFromAmount(nValue));
        entry.pushKV("confirmations", out.nDepth);
        results.push_back(entry);
    }

    return results;
}



UniValue createrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
                "createrawtransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,\"data\":\"hex\",...}\n"
                "\nCreate a transaction spending the given inputs and creating new outputs.\n"
                "Outputs can be addresses or data.\n"
                "Returns hex-encoded raw transaction.\n"
                "Note that the transaction's inputs are not signed, and\n"
                "it is not stored in the wallet or transmitted to the network.\n"
                "\nArguments:\n"
                "1. \"transactions\"        (string, required) A json array of json objects\n"
                "     [\n"
                "       {\n"
                "         \"txid\":\"id\",    (string, required) The transaction id\n"
                "         \"vout\":n        (numeric, required) The output number\n"
                "       }\n"
                "       ,...\n"
                "     ]\n"
                "2. \"outputs\"             (string, required) a json object with outputs\n"
                "    {\n"
                "      \"address\": x.xxx   (numeric, required) The key is the bitcoin address, the value is the CURRENCY_UNIT amount\n"
                "      \"data\": \"hex\",     (string, required) The key is \"data\", the value is hex encoded data\n"
                "      ...\n"
                "    }\n"
                "\nResult:\n"
                "\"transaction\"            (string) hex string of the transaction\n"
                "\nExamples\n"
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01} "
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"data\\\":\\\"00010203\\\"} "
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01} "
                "createrawtransaction \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"data\\\":\\\"00010203\\\"} \n"
                );
    RPCTypeCheck(params, list_of(UniValue::VARR)(UniValue::VOBJ));

    UniValue inputs = params[0].get_array();
    UniValue sendTo = params[1].get_obj();

    CTransaction rawTx;

    for (unsigned int idx = 0; idx<inputs.size(); idx++)
    {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        const UniValue& txid_v = find_value(o, "txid");
        if (!txid_v.isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing txid key");
        string txid = txid_v.get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        CTxIn in(COutPoint(uint256(txid), nOutput));
        rawTx.vin.push_back(in);
    }

    set<CBitcoinAddress> setAddress;
    vector<string> addrList = sendTo.getKeys();
    for (auto const& name_: addrList)
    {
        CBitcoinAddress address(name_);
        if (name_ == "data")
        {
            std::vector<unsigned char> data = ParseHexV(sendTo[name_],"Data");
            CTxOut out(0, CScript() << OP_RETURN << data);
            rawTx.vout.push_back(out);
        }
        else
        {
            CBitcoinAddress address(name_);
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ")+name_);

            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+name_);
            setAddress.insert(address);

            CScript scriptPubKey;
            scriptPubKey.SetDestination(address.Get());
            int64_t nAmount = AmountFromValue(sendTo[name_]);

            CTxOut out(nAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
    }

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;
    return HexStr(ss.begin(), ss.end());
}

UniValue decoderawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "decoderawtransaction <hex string>\n"
                "\n"
                "Return a JSON object representing the serialized, hex-encoded transaction\n");

    RPCTypeCheck(params, list_of(UniValue::VSTR));

    vector<unsigned char> txData(ParseHex(params[0].get_str()));

    LOCK(cs_main);

    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    UniValue result(UniValue::VOBJ);
    TxToJSON(tx, 0, result);

    return result;
}

UniValue decodescript(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "decodescript <hex string>\n"
                "\n"
                "Decode a hex-encoded script.\n");

    RPCTypeCheck(params, list_of(UniValue::VSTR));

    UniValue r(UniValue::VOBJ);
    CScript script;
    if (params[0].get_str().size() > 0){
        vector<unsigned char> scriptData(ParseHexV(params[0], "argument"));
        script = CScript(scriptData.begin(), scriptData.end());
    } else {
        // Empty scripts are valid
    }
    ScriptPubKeyToJSON(script, r, false);

    r.pushKV("p2sh", CBitcoinAddress(script.GetID()).ToString());
    return r;
}

UniValue signrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
                "signrawtransaction <hex string> [{\"txid\":txid,\"vout\":n,\"scriptPubKey\":hex},...] [<privatekey1>,...] [sighashtype=\"ALL\"]\n"
                "\n"
                "Sign inputs for raw transaction (serialized, hex-encoded).\n"
                "Second optional argument (may be null) is an array of previous transaction outputs that\n"
                "this transaction depends on but may not yet be in the blockchain.\n"
                "Third optional argument (may be null) is an array of base58-encoded private\n"
                "keys that, if given, will be the only keys used to sign the transaction.\n"
                "Fourth optional argument is a string that is one of six values; ALL, NONE, SINGLE or\n"
                "ALL|ANYONECANPAY, NONE|ANYONECANPAY, SINGLE|ANYONECANPAY.\n"
                "Returns json object with keys:\n"
                "  hex : raw transaction with signature(s) (hex-encoded string)\n"
                "  complete : 1 if transaction has a complete set of signature (0 if not)\n"
                + HelpRequiringPassphrase());

    RPCTypeCheck(params, list_of(UniValue::VSTR)(UniValue::VARR)(UniValue::VARR)(UniValue::VSTR), true);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    vector<CTransaction> txVariants;
    while (!ssData.empty())
    {
        try {
            CTransaction tx;
            ssData >> tx;
            txVariants.push_back(tx);
        }
        catch (std::exception &e) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
        }
    }

    if (txVariants.empty())
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transaction");

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    CTransaction mergedTx(txVariants[0]);
    bool fComplete = true;

    // Fetch previous transactions (inputs):
    map<COutPoint, CScript> mapPrevOut;
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++)
    {
        CTransaction tempTx;
        MapPrevTx mapPrevTx;
        CTxDB txdb("r");
        map<uint256, CTxIndex> unused;
        bool fInvalid;

        // FetchInputs aborts on failure, so we go one at a time.
        tempTx.vin.push_back(mergedTx.vin[i]);
        tempTx.FetchInputs(txdb, unused, false, false, mapPrevTx, fInvalid);

        // Copy results into mapPrevOut:
        for (auto const& txin : tempTx.vin)
        {
            const uint256& prevHash = txin.prevout.hash;
            if (mapPrevTx.count(prevHash) && mapPrevTx[prevHash].second.vout.size()>txin.prevout.n)
                mapPrevOut[txin.prevout] = mapPrevTx[prevHash].second.vout[txin.prevout.n].scriptPubKey;
        }
    }

    // Add previous txouts given in the RPC call:
    if (params.size() > 1 && !params[1].isNull())
    {
        UniValue prevTxs = params[1].get_array();
        for (unsigned int idx = 0; idx < prevTxs.size(); idx++)
        {
            const UniValue&p = prevTxs[idx];
            if (!p.isObject())
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"txid'\",\"vout\",\"scriptPubKey\"}");

            UniValue prevOut = p.get_obj();

            RPCTypeCheckObj(prevOut, map_list_of("txid", UniValue::VSTR)("vout", UniValue::VNUM)("scriptPubKey", UniValue::VSTR));

            string txidHex = find_value(prevOut, "txid").get_str();
            if (!IsHex(txidHex))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "txid must be hexadecimal");
            uint256 txid;
            txid.SetHex(txidHex);

            int nOut = find_value(prevOut, "vout").get_int();
            if (nOut < 0)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "vout must be positive");

            string pkHex = find_value(prevOut, "scriptPubKey").get_str();
            if (!IsHex(pkHex))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "scriptPubKey must be hexadecimal");
            vector<unsigned char> pkData(ParseHex(pkHex));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            COutPoint outpoint(txid, nOut);
            if (mapPrevOut.count(outpoint))
            {
                // Complain if scriptPubKey doesn't match
                if (mapPrevOut[outpoint] != scriptPubKey)
                {
                    string err("Previous output scriptPubKey mismatch:\n");
                    err = err + mapPrevOut[outpoint].ToString() + "\nvs:\n"+
                        scriptPubKey.ToString();
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
                }
            }
            else
                mapPrevOut[outpoint] = scriptPubKey;
        }
    }

    bool fGivenKeys = false;
    CBasicKeyStore tempKeystore;
    if (params.size() > 2 && !params[2].isNull())
    {
        fGivenKeys = true;
        UniValue keys = params[2].get_array();
        for (unsigned int idx = 0; idx < keys.size(); idx++)
        {
            UniValue k = keys[idx];
            CBitcoinSecret vchSecret;
            bool fGood = vchSecret.SetString(k.get_str());
            if (!fGood)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,"Invalid private key");
            CKey key;
            bool fCompressed;
            CSecret secret = vchSecret.GetSecret(fCompressed);
            key.SetSecret(secret, fCompressed);
            tempKeystore.AddKey(key);
        }
    }
    else
        EnsureWalletIsUnlocked();

    const CKeyStore& keystore = (fGivenKeys ? tempKeystore : *pwalletMain);

    int nHashType = SIGHASH_ALL;
    if (params.size() > 3 && !params[3].isNull())
    {
        static map<string, int> mapSigHashValues =
            boost::assign::map_list_of
            (string("ALL"), int(SIGHASH_ALL))
            (string("ALL|ANYONECANPAY"), int(SIGHASH_ALL|SIGHASH_ANYONECANPAY))
            (string("NONE"), int(SIGHASH_NONE))
            (string("NONE|ANYONECANPAY"), int(SIGHASH_NONE|SIGHASH_ANYONECANPAY))
            (string("SINGLE"), int(SIGHASH_SINGLE))
            (string("SINGLE|ANYONECANPAY"), int(SIGHASH_SINGLE|SIGHASH_ANYONECANPAY))
            ;
        string strHashType = params[3].get_str();
        if (mapSigHashValues.count(strHashType))
            nHashType = mapSigHashValues[strHashType];
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid sighash param");
    }

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

    // Sign what we can:
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++)
    {
        CTxIn& txin = mergedTx.vin[i];
        if (mapPrevOut.count(txin.prevout) == 0)
        {
            fComplete = false;
            continue;
        }
        const CScript& prevPubKey = mapPrevOut[txin.prevout];

        txin.scriptSig.clear();
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mergedTx.vout.size()))
            SignSignature(keystore, prevPubKey, mergedTx, i, nHashType);

        // ... and merge in other signatures:
        for (auto const& txv : txVariants)
        {
            txin.scriptSig = CombineSignatures(prevPubKey, mergedTx, i, txin.scriptSig, txv.vin[i].scriptSig);
        }
        if (!VerifyScript(txin.scriptSig, prevPubKey, mergedTx, i, 0))
            fComplete = false;
    }

    UniValue result(UniValue::VOBJ);
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << mergedTx;
    result.pushKV("hex", HexStr(ssTx.begin(), ssTx.end()));
    result.pushKV("complete", fComplete);

    return result;
}

UniValue sendrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
                "sendrawtransaction <hex string>\n"
                "\n"
                "Submits raw transaction (serialized, hex-encoded) to local node and network\n");

    RPCTypeCheck(params, list_of(UniValue::VSTR));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // parse hex string from parameter
    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;

    // deserialize binary data stream
    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }
    uint256 hashTx = tx.GetHash();

    // See if the transaction is already in a block
    // or in the memory pool:
    CTransaction existingTx;
    uint256 hashBlock = 0;
    if (GetTransaction(hashTx, existingTx, hashBlock))
    {
        if (hashBlock != 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("transaction already in block ")+hashBlock.GetHex());
        // Not in block, but already in the memory pool; will drop
        // through to re-relay it.
    }
    else
    {
        // push to local node
        if (!AcceptToMemoryPool(mempool, tx, NULL))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX rejected");

        SyncWithWallets(tx, NULL, true);
    }
    RelayTransaction(tx, hashTx);

    return hashTx.GetHex();
}
