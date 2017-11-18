// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h"
#include "bitcoinrpc.h"
#include "alert.h"
#include "wallet.h"
#include "db.h"
#include "walletdb.h"

using namespace json_spirit;
using namespace std;
extern std::string NeuralRequest(std::string MyNeuralRequest);
extern bool RequestSupermajorityNeuralData();
std::string GetCurrentNeuralNetworkSupermajorityHash(double& out_popularity);
extern void GatherNeuralHashes();
extern bool AsyncNeuralRequest(std::string command_name,std::string cpid,int NodeLimit);


Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "Returns the number of connections to other nodes.");

    LOCK(cs_vNodes);
    return (int)vNodes.size();
}


std::string NeuralRequest(std::string MyNeuralRequest)
{
    // Find a Neural Network Node that is free
    LOCK(cs_vNodes);
    for (auto const& pNode : vNodes)
    {
        if (Contains(pNode->strSubVer,"1999"))
        {
            //printf("Node is a neural participant \r\n");
            std::string reqid = "reqid";
            pNode->PushMessage("neural", MyNeuralRequest, reqid);
            if (fDebug3) printf(" PUSH ");
        }
    }
    return "";
}


void GatherNeuralHashes()
{
    // Find a Neural Network Node that is free
    LOCK(cs_vNodes);
    for (auto const& pNode : vNodes)
    {
        if (Contains(pNode->strSubVer,"1999"))
        {
            std::string reqid = "reqid";
            std::string command_name="neural_hash";
            pNode->PushMessage("neural", command_name, reqid);
            if (fDebug10) printf(" Pushed ");
        }
    }
}


bool RequestSupermajorityNeuralData()
{
    LOCK(cs_vNodes);
    double dCurrentPopularity = 0;
    std::string sCurrentNeuralSupermajorityHash = GetCurrentNeuralNetworkSupermajorityHash(dCurrentPopularity);
    std::string reqid = DefaultWalletAddress();
            
    for (auto const& pNode : vNodes)
    {
        if (!pNode->NeuralHash.empty() && !sCurrentNeuralSupermajorityHash.empty() && pNode->NeuralHash == sCurrentNeuralSupermajorityHash)
        {
            std::string command_name="neural_data";
            pNode->PushMessage("neural", command_name, reqid);
            return true;
        }
    }
    return false;
}

Value addnode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw runtime_error(
            "addnode <node> <add|remove|onetry>\n"
            "Attempts add or remove <node> from the addnode list or try a connection to <node> once.");

    string strNode = params[0].get_str();

    if (strCommand == "onetry")
    {
        CAddress addr;
        ConnectNode(addr, strNode.c_str());
        return Value::null;
    }

    LOCK(cs_vAddedNodes);
    vector<string>::iterator it = vAddedNodes.begin();
    for(; it != vAddedNodes.end(); it++)
        if (strNode == *it)
            break;

    if (strCommand == "add")
    {
        if (it != vAddedNodes.end())
            throw JSONRPCError(-23, "Error: Node already added");
        vAddedNodes.push_back(strNode);
    }
    else if(strCommand == "remove")
    {
        if (it == vAddedNodes.end())
            throw JSONRPCError(-24, "Error: Node has not been added.");
        vAddedNodes.erase(it);
    }

    return Value::null;
}

Value getaddednodeinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getaddednodeinfo <dns> [node]\n"
            "Returns information about the given added node, or all added nodes\n"
            "(note that onetry addnodes are not listed here)\n"
            "If dns is false, only a list of added nodes will be provided,\n"
            "otherwise connected information will also be available.");

    bool fDns = params[0].get_bool();

    list<string> laddedNodes(0);
    if (params.size() == 1)
    {
        LOCK(cs_vAddedNodes);
        for (auto const& strAddNode : vAddedNodes)
            laddedNodes.push_back(strAddNode);
    }
    else
    {
        string strNode = params[1].get_str();
        LOCK(cs_vAddedNodes);
        for (auto const& strAddNode : vAddedNodes)
            if (strAddNode == strNode)
            {
                laddedNodes.push_back(strAddNode);
                break;
            }
        if (laddedNodes.size() == 0)
            throw JSONRPCError(-24, "Error: Node has not been added.");
    }

    if (!fDns)
    {
        Object ret;
        for (auto const& strAddNode : laddedNodes)
            ret.push_back(Pair("addednode", strAddNode));
        return ret;
    }

    Array ret;

    list<pair<string, vector<CService> > > laddedAddreses(0);
    for (auto const& strAddNode : laddedNodes)
    {
        vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, GetDefaultPort(), fNameLookup, 0))
            laddedAddreses.push_back(make_pair(strAddNode, vservNode));
        else
        {
            Object obj;
            obj.push_back(Pair("addednode", strAddNode));
            obj.push_back(Pair("connected", false));
            Array addresses;
            obj.push_back(Pair("addresses", addresses));
        }
    }

    LOCK(cs_vNodes);
    for (list<pair<string, vector<CService> > >::iterator it = laddedAddreses.begin(); it != laddedAddreses.end(); it++)
    {
        Object obj;
        obj.push_back(Pair("addednode", it->first));

        Array addresses;
        bool fConnected = false;
        for (auto const& addrNode : it->second)
        {
            bool fFound = false;
            Object node;
            node.push_back(Pair("address", addrNode.ToString()));
            for (auto const& pnode : vNodes)
                if (pnode->addr == addrNode)
                {
                    fFound = true;
                    fConnected = true;
                    node.push_back(Pair("connected", pnode->fInbound ? "inbound" : "outbound"));
                    break;
                }
            if (!fFound)
                node.push_back(Pair("connected", "false"));
            addresses.push_back(node);
        }
        obj.push_back(Pair("connected", fConnected));
        obj.push_back(Pair("addresses", addresses));
        ret.push_back(obj);
    }

    return ret;
}


bool AsyncNeuralRequest(std::string command_name,std::string cpid,int NodeLimit)
{
    // Find a Neural Network Node that is free
    LOCK(cs_vNodes);
    int iContactCount = 0;
    msNeuralResponse="";
    for (auto const& pNode : vNodes)
    {
        if (Contains(pNode->strSubVer,"1999"))
        {
            std::string reqid = cpid;
            pNode->PushMessage("neural", command_name, reqid);
            //if (fDebug3) printf("Requested command %s \r\n",command_name.c_str());
            iContactCount++;
            if (iContactCount >= NodeLimit) return true;
        }
    }
    if (iContactCount==0) 
    {
        printf("No neural network nodes online.");
        return false;
    }
    return true;
}



Value ping(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "ping\n"
            "Requests that a ping be sent to all other nodes, to measure ping time.\n"
            "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
            "Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.");

    // Request that each node send a ping during next message processing pass
    LOCK(cs_vNodes);
    for (auto const& pNode : vNodes) {
        pNode->fPingQueued = true;
    }

    return Value::null;
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    for (auto const& pnode : vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}

Value getpeerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpeerinfo\n"
            "Returns data about each connected network node.");

    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    Array ret;
    GatherNeuralHashes();
    
    for (auto const& stats : vstats) {
        Object obj;

        obj.push_back(Pair("addr", stats.addrName));

          if (!(stats.addrLocal.empty()))
            obj.push_back(Pair("addrlocal", stats.addrLocal));

        obj.push_back(Pair("services", strprintf("%08" PRIx64, stats.nServices)));
        obj.push_back(Pair("lastsend", stats.nLastSend));
        obj.push_back(Pair("lastrecv", stats.nLastRecv));
        obj.push_back(Pair("conntime", stats.nTimeConnected));
        obj.push_back(Pair("pingtime", stats.dPingTime));
        if (stats.dPingWait > 0.0)
            obj.push_back(Pair("pingwait", stats.dPingWait));
        obj.push_back(Pair("version", stats.nVersion));
        obj.push_back(Pair("subver", stats.strSubVer));
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        obj.push_back(Pair("nTrust", stats.nTrust));
        obj.push_back(Pair("banscore", stats.nMisbehavior));
        bool bNeural = false;
        bNeural = Contains(stats.strSubVer, "1999");
        obj.push_back(Pair("Neural Network", bNeural));
        if (bNeural)
        {
            obj.push_back(Pair("Neural Hash", stats.NeuralHash));
            obj.push_back(Pair("Neural Participant", IsNeuralNodeParticipant(stats.sGRCAddress, GetAdjustedTime())));

        }
        ret.push_back(obj);
    }

    return ret;
}



Value getnettotals(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "getnettotals\n"
            "Returns information about network traffic, including bytes in, bytes out,\n"
            "and current time.");

    Object obj;
    obj.push_back(Pair("totalbytesrecv", CNode::GetTotalBytesRecv()));
    obj.push_back(Pair("totalbytessent", CNode::GetTotalBytesSent()));
    obj.push_back(Pair("timemillis", GetTimeMillis()));
    return obj;
}



// ppcoin: send alert.  
// There is a known deadlock situation with ThreadMessageHandler
// ThreadMessageHandler: holds cs_vSend and acquiring cs_main in SendMessages()
// ThreadRPCServer: holds cs_main and acquiring cs_vSend in alert.RelayTo()/PushMessage()/BeginMessage()
Value sendalert(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 6)
        throw runtime_error(
            "sendalert <message> <privatekey> <minver> <maxver> <priority> <id> [cancelupto]\n"
            "<message> is the alert text message\n"
            "<privatekey> is hex string of alert master private key\n"
            "<minver> is the minimum applicable internal client version\n"
            "<maxver> is the maximum applicable internal client version\n"
            "<priority> is integer priority number\n"
            "<id> is the alert id\n"
            "[cancelupto] cancels all alert id's up to this number\n"
            "Returns true or false.");

    CAlert alert;
    CKey key;

    alert.strStatusBar = params[0].get_str();
    alert.nMinVer = params[2].get_int();
    alert.nMaxVer = params[3].get_int();
    alert.nPriority = params[4].get_int();
    alert.nID = params[5].get_int();
    if (params.size() > 6)
        alert.nCancel = params[6].get_int();
    alert.nVersion = PROTOCOL_VERSION;
    alert.nRelayUntil = GetAdjustedTime() + 365*24*60*60;
    alert.nExpiration = GetAdjustedTime() + 365*24*60*60;

    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedAlert)alert;
    alert.vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());

    vector<unsigned char> vchPrivKey = ParseHex(params[1].get_str());
    key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
        throw runtime_error(
            "Unable to sign alert, check private key?\n");  
    if(!alert.ProcessAlert()) 
        throw runtime_error(
            "Failed to process alert.\n");
    // Relay alert
    {
        LOCK(cs_vNodes);
        for (auto const& pnode : vNodes)
            alert.RelayTo(pnode);
    }

    Object result;
    result.push_back(Pair("strStatusBar", alert.strStatusBar));
    result.push_back(Pair("nVersion", alert.nVersion));
    result.push_back(Pair("nMinVer", alert.nMinVer));
    result.push_back(Pair("nMaxVer", alert.nMaxVer));
    result.push_back(Pair("nPriority", alert.nPriority));
    result.push_back(Pair("nID", alert.nID));
    if (alert.nCancel > 0)
        result.push_back(Pair("nCancel", alert.nCancel));
    return result;
}

Value sendalert2(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 7)
        throw runtime_error(
            //          0            1    2            3            4        5          6
            "sendalert <privatekey> <id> <subverlist> <cancellist> <expire> <priority> <message>\n"
            "<message> is the alert text message\n"
            "<privatekey> is hex string of alert master private key\n"
            "<subverlist> comma separated list of versions warning applies to\n"
            "<priority> integer, >1000->visible\n"
            "<id> is the unique alert number\n"
            "<cancellist> comma separated ids of alerts to cancel\n"
            "<expire> alert expiration in days\n"
            "Returns true or false.");

    CAlert alert;
    CKey key;

    alert.strStatusBar = params[6].get_str();
    alert.nMinVer = PROTOCOL_VERSION;
    alert.nMaxVer = PROTOCOL_VERSION;
    alert.nPriority = params[5].get_int();
    alert.nID = params[1].get_int();
    alert.nVersion = PROTOCOL_VERSION;
    alert.nRelayUntil = alert.nExpiration = GetAdjustedTime() + 24*60*60*params[4].get_int();

    std::vector<std::string> split_subver = split(params[2].get_str(), ",");
    alert.setSubVer.insert(split_subver.begin(),split_subver.end());

    std::vector<std::string> split_cancel = split(params[3].get_str(), ",");
    for(std::string &s : split_cancel)
    {
        int aver = RoundFromString(s, 0);
        alert.setCancel.insert(aver);
    }

    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedAlert)alert;
    alert.vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());

    vector<unsigned char> vchPrivKey = ParseHex(params[0].get_str());
    key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
        throw runtime_error(
            "Unable to sign alert, check private key?\n");  
    if(!alert.ProcessAlert()) 
        throw runtime_error(
            "Failed to process alert.\n");
    // Relay alert
    {
        LOCK(cs_vNodes);
        for (auto const& pnode : vNodes)
            alert.RelayTo(pnode);
    }

    Object result;
    result.push_back(Pair("Content", alert.ToString()));
    result.push_back(Pair("Success", true));
    return result;
}
