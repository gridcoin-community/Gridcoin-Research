// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <stdexcept>

#include "server.h"
#include "protocol.h"
#include "alert.h"
#include "wallet/wallet.h"
#include "wallet/db.h"
#include "streams.h"
#include "wallet/walletdb.h"
#include "net.h"
#include "banman.h"
#include "logging.h"

using namespace std;

extern std::map<uint256, CAlert> mapAlerts;
extern CCriticalSection cs_mapAlerts;

UniValue getconnectioncount(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "\n"
            "Returns the number of connections to other node\n.");

    LOCK(cs_vNodes);

    return (int)vNodes.size();
}

UniValue addnode(const UniValue& params, bool fHelp)
{
    string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
            (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw runtime_error(
                "addnode <node> <add|remove|onetry>\n"
                "\n"
                "Attempts add or remove <node> from the addnode list or try a connection to <node> once\n");

    string strNode = params[0].get_str();

    if (strCommand == "onetry")
    {
        CAddress addr;
        CNode* pnode= ConnectNode(addr, strNode.c_str());
        if(!pnode)
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node connection failed");
        UniValue result(UniValue::VOBJ);
        result.pushKV("result", "ok");
        return result;
    }

    LOCK(cs_vAddedNodes);
    vector<string>::iterator it = vAddedNodes.begin();
    for(; it != vAddedNodes.end(); it++)
        if (strNode == *it)
            break;

    if (strCommand == "add")
    {
        if (it != vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
        vAddedNodes.push_back(strNode);
    }
    else if(strCommand == "remove")
    {
        if (it == vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
        vAddedNodes.erase(it);
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("result", "ok");
    return result;
}

UniValue getnodeaddresses(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "getnodeaddresses [count]\n"
                "\nReturn known addresses which can potentially be used to find new nodes in the network\n"
                "count: How many addresses to return. Limited to the smaller of " + std::to_string(ADDRMAN_GETADDR_MAX) +
                " or " + std::to_string(ADDRMAN_GETADDR_MAX_PCT) + "% of all known addresses. (default = 1)\n");
    int count = 1;
    if(!params[0].isNull())
        count = params[0].get_int();

    if (count <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Address count out of range");

    // returns a shuffled list of CAddress
    std::vector<CAddress> vAddr = addrman.GetAddr();
    UniValue ret(UniValue::VARR);

    int address_return_count = std::min<int>(count, vAddr.size());
    for (int i = 0; i < address_return_count; ++i) {
        UniValue obj(UniValue::VOBJ);
        const CAddress& addr = vAddr[i];
        obj.pushKV("time", (int)addr.nTime);
        obj.pushKV("services", (uint64_t)addr.nServices);
        obj.pushKV("address", addr.ToStringIP());
        obj.pushKV("port", addr.GetPort());
        ret.push_back(obj);
    }
    return ret;
}

UniValue getaddednodeinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
                "getaddednodeinfo <dns> [node]\n"
                "\n"
                "Returns information about the given added node, or all added nodes\n"
                "(note that onetry addnodes are not listed here)\n"
                "If dns is false, only a list of added nodes will be provided,\n"
                "otherwise connected information will also be available\n");

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
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    if (!fDns)
    {
        UniValue ret(UniValue::VOBJ);
        for (auto const& strAddNode : laddedNodes)
            ret.pushKV("addednode", strAddNode);
        return ret;
    }

    UniValue ret(UniValue::VOBJ);

    list<pair<string, vector<CService> > > laddedAddresses(0);
    for (auto const& strAddNode : laddedNodes)
    {
        vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, GetDefaultPort(), fNameLookup, 0))
            laddedAddresses.push_back(make_pair(strAddNode, vservNode));
        else
        {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("addednode", strAddNode);
            obj.pushKV("connected", false);
            UniValue addresses(UniValue::VARR);
            obj.pushKV("addresses", addresses);
        }
    }

    LOCK(cs_vNodes);
    for (list<pair<string, vector<CService> > >::iterator it = laddedAddresses.begin(); it != laddedAddresses.end(); it++)
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("addednode", it->first);

        UniValue addresses(UniValue::VARR);
        bool fConnected = false;
        for (auto const& addrNode : it->second)
        {
            bool fFound = false;
            UniValue node(UniValue::VOBJ);
            node.pushKV("address", addrNode.ToString());
            for (auto const& pnode : vNodes)
                if (pnode->addr == addrNode)
                {
                    fFound = true;
                    fConnected = true;
                    node.pushKV("connected", pnode->fInbound ? "inbound" : "outbound");
                    break;
                }
            if (!fFound)
                node.pushKV("connected", "false");
            addresses.push_back(node);
        }
        obj.pushKV("connected", fConnected);
        obj.pushKV("addresses", addresses);
        ret.push_back(obj);
    }

    return ret;
}

UniValue setban(const UniValue& params, bool fHelp)
{
    std::string strCommand;

    if (!params[1].isNull())
        strCommand = params[1].get_str();

    if (fHelp || params.size() < 2 || params.size() > 4 || (strCommand != "add" && strCommand != "remove"))
    {
        throw runtime_error(
                    "setban <ip or subnet> <command> [bantime] [absolute]\n"
                    "\n"
                    "add or remove an IP/Subnet from the banned list.\n"
                    "subnet: The IP/Subnet (see getpeerinfo for nodes IP) with an optional netmask (default is /32 = single IP) \n"
                    "command: 'add' to add an IP/Subnet to the list, 'remove' to remove an IP/Subnet from the list \n"
                    "bantime: time in seconds how long (or until when if [absolute] is set) the IP is banned \n"
                    "         (0 or empty means using the default time of 24h which can also be overwritten by the -bantime startup argument)\n"
                    "absolute: Defaults to false. If set, the bantime must be an absolute timestamp in seconds since epoch (Jan 1 1970 GMT).\n"
                    );
    }

    if (!g_banman) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Error: Ban database not loaded");
    }

    CSubNet subNet;
    CNetAddr netAddr;
    bool isSubnet = false;

    if (params[0].get_str().find('/') != std::string::npos)
        isSubnet = true;

    if (!isSubnet) {
        std::vector<CNetAddr> resolved;
        if (LookupHost(params[0].get_str().c_str(), resolved, 1, false)) {
            netAddr = resolved[0];
        }
    }
    else
        LookupSubNet(params[0].get_str().c_str(), subNet);

    if (! (isSubnet ? subNet.IsValid() : netAddr.IsValid()) )
        throw JSONRPCError(RPC_CLIENT_INVALID_IP_OR_SUBNET, "Error: Invalid IP/Subnet");

    if (strCommand == "add")
    {
        if (isSubnet ? g_banman->IsBanned(subNet) : g_banman->IsBanned(netAddr)) {
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: IP/Subnet already banned");
        }

        int64_t banTime = 0; //use standard bantime if not specified
        if (!params[2].isNull())
            banTime = params[2].get_int64();

        bool absolute = false;
        if (params[3].isTrue())
            absolute = true;

        if (isSubnet) {
            g_banman->Ban(subNet, BanReasonManuallyAdded, banTime, absolute);
            CNode::DisconnectNode(subNet);
        } else {
            g_banman->Ban(netAddr, BanReasonManuallyAdded, banTime, absolute);
            CNode::DisconnectNode(netAddr);
        }
    }
    else if(strCommand == "remove")
    {
        if (!( isSubnet ? g_banman->Unban(subNet) : g_banman->Unban(netAddr) )) {
            throw JSONRPCError(RPC_CLIENT_INVALID_IP_OR_SUBNET, "Error: Unban failed. Requested address/subnet was not previously banned.");
        }
    }
    return NullUniValue;
}

UniValue listbanned(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
    {
        throw runtime_error(
            "listbanned\n"
            "\n"
            "List all banned IPs/subnets.\n"
            );
    }

    if(!g_banman) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Error: Ban database not loaded");
    }

    banmap_t banMap;
    g_banman->GetBanned(banMap);
    const int64_t current_time{GetTime()};

    UniValue bannedAddresses(UniValue::VARR);
    for (const auto& entry : banMap)
    {
        const CBanEntry& banEntry = entry.second;
        UniValue rec(UniValue::VOBJ);
        rec.pushKV("address", entry.first.ToString());
        rec.pushKV("ban_created", banEntry.nCreateTime);
        rec.pushKV("banned_until", banEntry.nBanUntil);
        rec.pushKV("ban_duration", (banEntry.nBanUntil - banEntry.nCreateTime));
        rec.pushKV("time_remaining", (banEntry.nBanUntil - current_time));
        rec.pushKV("ban_reason", banEntry.banReasonToString());

        bannedAddresses.push_back(rec);
    }

    return bannedAddresses;
}

UniValue clearbanned(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "clearbanned\n"
            "\n"
            "Clear all banned IPs.\n"
            );

    if (!g_banman) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Error: Ban database not loaded");
    }

    g_banman->ClearBanned();

    return NullUniValue;
}

UniValue ping(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "ping\n"
                "\n"
                "Requests that a ping be sent to all other nodes, to measure ping time.\n"
                "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
                "Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping\n");

    // Request that each node send a ping during next message processing pass
    LOCK(cs_vNodes);
    for (auto const& pNode : vNodes) {
        pNode->fPingQueued = true;
    }

    return NullUniValue;
}

UniValue getpeerinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getpeerinfo\n"
                "\n"
                "Returns data about each connected network node.");

    vector<CNodeStats> vstats;
    UniValue ret(UniValue::VARR);

    {
        LOCK(cs_vNodes);

        CNode::CopyNodeStats(vstats);
    }

    for (auto const& stats : vstats) {
        UniValue obj(UniValue::VOBJ);

        obj.pushKV("id", stats.id);
        obj.pushKV("addr", stats.addrName);

          if (!(stats.addrLocal.empty()))
            obj.pushKV("addrlocal", stats.addrLocal);

        obj.pushKV("services", strprintf("%08" PRIx64, stats.nServices));
        obj.pushKV("lastsend", stats.nLastSend);
        obj.pushKV("lastrecv", stats.nLastRecv);
        obj.pushKV("bytessent", stats.nSendBytes);
        obj.pushKV("bytesrecv", stats.nRecvBytes);
        obj.pushKV("conntime", stats.nTimeConnected);
        obj.pushKV("timeoffset", stats.nTimeOffset);
        obj.pushKV("pingtime", stats.dPingTime);
        if (stats.dMinPing < static_cast<double>(std::numeric_limits<int64_t>::max())/1e6)
            obj.pushKV("minping", stats.dMinPing);
        if (stats.dPingWait > 0.0)
            obj.pushKV("pingwait", stats.dPingWait);
        obj.pushKV("version", stats.nVersion);
        obj.pushKV("subver", stats.strSubVer);
        obj.pushKV("inbound", stats.fInbound);
        obj.pushKV("startingheight", stats.nStartingHeight);
        obj.pushKV("nTrust", stats.nTrust);
        obj.pushKV("banscore", stats.nMisbehavior);

        ret.push_back(obj);
    }

    return ret;
}

UniValue getnettotals(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "getnettotals\n"
                "\n"
                "Returns information about network traffic, including bytes in, bytes out,\n"
                "and current time\n");

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("totalbytesrecv", CNode::GetTotalBytesRecv());
    obj.pushKV("totalbytessent", CNode::GetTotalBytesSent());
    obj.pushKV("timemillis", GetTimeMillis());
    return obj;
}

UniValue listalerts(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error(
                "listalerts\n"
                "\n"
                "Returns information about alerts.\n");

    LOCK(cs_mapAlerts);

    UniValue result(UniValue::VOBJ);
    UniValue alerts(UniValue::VARR);


    for (const auto& iter_alert : mapAlerts) {
        UniValue alert(UniValue::VOBJ);

        alert.pushKV("version", iter_alert.second.nVersion);
        alert.pushKV("relay_until", DateTimeStrFormat(iter_alert.second.nRelayUntil));
        alert.pushKV("expiration", DateTimeStrFormat(iter_alert.second.nExpiration));
        alert.pushKV("id", iter_alert.second.nID);
        alert.pushKV("cancel_upto", iter_alert.second.nCancel);

        UniValue set_cancel(UniValue::VARR);
        for (const auto& cancel : iter_alert.second.setCancel) {
            set_cancel.push_back(cancel);
        }
        alert.pushKV("cancels", set_cancel);

        alert.pushKV("minimum_version", iter_alert.second.nMinVer);
        alert.pushKV("maximum_version", iter_alert.second.nMaxVer);

        UniValue set_subver(UniValue::VARR);
        for (const auto& subver : iter_alert.second.setSubVer) {
            set_subver.push_back(subver);
        }
        alert.pushKV("subversions", set_subver);

        alert.pushKV("priority", iter_alert.second.nPriority);

        alert.pushKV("comment", iter_alert.second.strComment);
        alert.pushKV("status_bar", iter_alert.second.strStatusBar);
        alert.pushKV("reserved", iter_alert.second.strReserved);

        alert.pushKV("hash", iter_alert.second.GetHash().GetHex());
        alert.pushKV("in_effect", iter_alert.second.IsInEffect());
        alert.pushKV("applies_to_me", iter_alert.second.AppliesToMe());

        alerts.push_back(alert);
    }

    result.pushKV("alerts", alerts);

    return result;
}


// ppcoin: send alert.
// There is a known deadlock situation with ThreadMessageHandler
// ThreadMessageHandler: holds cs_vSend and acquiring cs_main in SendMessages()
// ThreadRPCServer: holds cs_main and acquiring cs_vSend in alert.RelayTo()/PushMessage()/BeginMessage()
UniValue sendalert(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 6)
        throw runtime_error(
            "sendalert <message> <privatekey> <minver> <maxver> <priority> <id> [cancelupto]\n"
            "\n"
            "<message> ----> is the alert text message\n"
            "<privatekey> -> is WIF encoded alert master private key\n"
            "<minver> -----> is the minimum applicable internal client version\n"
            "<maxver> -----> is the maximum applicable internal client version\n"
            "<priority> ---> is integer priority number\n"
            "<id> ---------> is the alert id\n"
            "[cancelupto] -> cancels all alert ids up to this number\n"
            "\n"
            "Returns true or false\n");

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
    alert.vchMsg = vector<unsigned char>((unsigned char*)&sMsg.begin()[0], (unsigned char*)&sMsg.end()[0]);

    CBitcoinSecret vchSecret;

    if (!vchSecret.SetString(params[0].get_str())) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    bool fCompressed;
    CSecret secret = vchSecret.GetSecret(fCompressed);
    key.Set(secret.begin(), secret.end(), fCompressed);

    if (!key.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    if (!key.Sign(Hash(alert.vchMsg), alert.vchSig))
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

    UniValue result(UniValue::VOBJ);
    result.pushKV("strStatusBar", alert.strStatusBar);
    result.pushKV("nVersion", alert.nVersion);
    result.pushKV("nMinVer", alert.nMinVer);
    result.pushKV("nMaxVer", alert.nMaxVer);
    result.pushKV("nPriority", alert.nPriority);
    result.pushKV("nID", alert.nID);
    if (alert.nCancel > 0)
        result.pushKV("nCancel", alert.nCancel);
    return result;
}

UniValue sendalert2(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 7)
        throw runtime_error(
            //          0            1    2            3            4        5          6
            "sendalert2 <privatekey> <id> <subverlist> <cancellist> <expire> <priority> <message>\n"
            "\n"
            "<privatekey> -> is WIF encoded alert master private key\n"
            "<id> ---------> is the unique alert number\n"
            "<subverlist> -> comma separated list of versions warning applies to\n"
            "<cancellist> -> comma separated ids of alerts to cancel\n"
            "<expire> -----> alert expiration in days\n"
            "<priority> ---> integer, >1000->visible\n"
            "<message> ---->is the alert text message\n"
            "\n"
            "Returns summary of what was done.");

    CAlert alert;
    CKey key;

    alert.strStatusBar = params[6].get_str();
    alert.nMinVer = PROTOCOL_VERSION;
    alert.nMaxVer = PROTOCOL_VERSION;
    alert.nPriority = params[5].get_int();
    alert.nID = params[1].get_int();
    alert.nVersion = PROTOCOL_VERSION;
    alert.nRelayUntil = alert.nExpiration = GetAdjustedTime() + 24*60*60*params[4].get_int();

    if(params[2].get_str().length())
    {
        std::vector<std::string> split_subver = split(params[2].get_str(), ",");
        alert.setSubVer.insert(split_subver.begin(),split_subver.end());
    }

    if(params[3].get_str().length())
    {
        for(std::string &s : split(params[3].get_str(), ","))
        {
            int aver = RoundFromString(s, 0);
            alert.setCancel.insert(aver);
        }
    }

    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedAlert)alert;
    alert.vchMsg = vector<unsigned char>((unsigned char*)&sMsg.begin()[0], (unsigned char*)&sMsg.end()[0]);

    CBitcoinSecret vchSecret;

    if (!vchSecret.SetString(params[0].get_str())) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    bool fCompressed;
    CSecret secret = vchSecret.GetSecret(fCompressed);
    key.Set(secret.begin(), secret.end(), fCompressed);

    if (!key.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    if (!key.Sign(Hash(alert.vchMsg), alert.vchSig))
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

    UniValue result(UniValue::VOBJ);
    result.pushKV("Content", alert.ToString());
    result.pushKV("Success", true);
    return result;
}

UniValue getnetworkinfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getnetworkinfo\n"
                "\n"
                "Displays network related information\n");

    UniValue res(UniValue::VOBJ);

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    LOCK(cs_main);

    res.pushKV("version",         FormatFullVersion());
    res.pushKV("minor_version",   CLIENT_VERSION_MINOR);
    res.pushKV("protocolversion", PROTOCOL_VERSION);
    res.pushKV("timeoffset",      GetTimeOffset());
    res.pushKV("connections",     (int)vNodes.size());
    res.pushKV("paytxfee",        ValueFromAmount(nTransactionFee));
    res.pushKV("mininput",        ValueFromAmount(nMinimumInputValue));
    res.pushKV("proxy",           (proxy.IsValid() ? proxy.ToStringIPPort() : string()));
    res.pushKV("ip",              addrSeenByPeer.ToStringIP());

    UniValue localAddresses(UniValue::VARR);
    {
        LOCK(cs_mapLocalHost);
        for (const std::pair<const CNetAddr, LocalServiceInfo> &item : mapLocalHost)
        {
            UniValue rec(UniValue::VOBJ);
            rec.pushKV("address", item.first.ToString());
            rec.pushKV("port", item.second.nPort);
            rec.pushKV("score", item.second.nScore);
            localAddresses.push_back(rec);
        }
    }

    res.pushKV("localaddresses", localAddresses);
    res.pushKV("errors",          GetWarnings("statusbar"));

    return res;
}
