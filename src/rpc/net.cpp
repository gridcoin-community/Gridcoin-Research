// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <stdexcept>

#include "server.h"
#include "protocol.h"
#include <rpc/util.h>
#include "alert.h"
#include "wallet/wallet.h"
#include "wallet/db.h"
#include "streams.h"
#include "wallet/walletdb.h"
#include "net.h"
#include "banman.h"
#include "logging.h"

using namespace std;

extern CCriticalSection cs_mapAlerts;
extern std::map<uint256, CAlert> mapAlerts GUARDED_BY(cs_mapAlerts);

static const RPCHelpMan getconnectioncount_help{
    "getconnectioncount",
    "Returns the number of connections to other nodes.",
    {},
    RPCResult{RPCResult::Type::NUM, "", "The connection count."},
    RPCExamples{
        HelpExampleCli("getconnectioncount", "") +
        HelpExampleRpc("getconnectioncount", "")},
};
const RPCHelpMan& getconnectioncount_helpman() { return getconnectioncount_help; }

UniValue getconnectioncount(const UniValue& params)
{
    // Peer count via the CConnman node-access API (issue #2558 PR 9b).
    return g_connman ? (int)g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) : 0;
}

static const RPCHelpMan addnode_help{
    "addnode",
    "Attempts to add or remove a node from the addnode list, or try a connection to a node once.",
    {
        {"node", RPCArg::Type::STR, RPCArg::Optional::NO,
            "The node (see getpeerinfo for nodes), as host[:port]."},
        {"command", RPCArg::Type::STR, RPCArg::Optional::NO,
            "'add' to add a node to the list, 'remove' to remove a node from the list, "
            "'onetry' to try a connection to the node once."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "result", "always \"ok\" on success"},
        }},
    RPCExamples{
        HelpExampleCli("addnode", "\"192.168.0.6:32749\" \"onetry\"") +
        HelpExampleRpc("addnode", "\"192.168.0.6:32749\", \"onetry\"")},
};
const RPCHelpMan& addnode_helpman() { return addnode_help; }

UniValue addnode(const UniValue& params)
{
    const string strCommand = params[1].get_str();
    if (strCommand != "onetry" && strCommand != "add" && strCommand != "remove") {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "command must be one of: \"add\", \"remove\", \"onetry\"");
    }

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

    // add/remove operate on the CConnman added-node list (issue #2558 PR 9b2).
    // Report a null connection manager (e.g. the brief shutdown window after
    // g_connman.reset()) distinctly, so the ADDED / NOT_ADDED codes stay
    // reserved for genuine list-operation results.
    if (!g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }
    if (strCommand == "add")
    {
        if (!g_connman->AddNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
    }
    else if(strCommand == "remove")
    {
        if (!g_connman->RemoveAddedNode(strNode))
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("result", "ok");
    return result;
}

static const RPCHelpMan getnodeaddresses_help{
    "getnodeaddresses",
    "Return known addresses, which can potentially be used to find new nodes in the network.",
    {
        {"count", RPCArg::Type::NUM, RPCArg::Optional::OMITTED,
            strprintf("How many addresses to return. Limited to the smaller of %d or %d%% "
                      "of all known addresses (default: 1).",
                      ADDRMAN_GETADDR_MAX, ADDRMAN_GETADDR_MAX_PCT)},
    },
    RPCResult{RPCResult::Type::ARR, "", "",
        {
            {RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::NUM_TIME, "time", "The address timestamp (seconds since epoch)"},
                    {RPCResult::Type::NUM, "services", "Service flags advertised by the address"},
                    {RPCResult::Type::STR, "address", "The IP address"},
                    {RPCResult::Type::NUM, "port", "The port"},
                }},
        }},
    RPCExamples{
        HelpExampleCli("getnodeaddresses", "8") +
        HelpExampleRpc("getnodeaddresses", "8")},
};
const RPCHelpMan& getnodeaddresses_helpman() { return getnodeaddresses_help; }

UniValue getnodeaddresses(const UniValue& params)
{
    int count = 1;
    if (params.size() > 0 && !params[0].isNull())
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

static const RPCHelpMan getaddednodeinfo_help{
    "getaddednodeinfo",
    "Returns information about the given added node, or all added nodes\n"
    "(note that onetry addnodes are not listed here).\n"
    "If dns is false, only a list of added nodes will be provided,\n"
    "otherwise connected information will also be available.",
    {
        {"dns", RPCArg::Type::BOOL, RPCArg::Optional::NO,
            "If false, only a list of added nodes will be provided, otherwise connection info too."},
        {"node", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
            "If provided, return information about this specific added node, otherwise all are returned."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::ELISION, "",
                "Shape depends on the 'dns' argument: a map of \"addednode\" -> node string when dns=false, "
                "or nested per-node connection detail when dns=true."},
        }},
    RPCExamples{
        HelpExampleCli("getaddednodeinfo", "true") +
        HelpExampleCli("getaddednodeinfo", "true \"192.168.0.6:32749\"") +
        HelpExampleRpc("getaddednodeinfo", "true, \"192.168.0.6:32749\"")},
};
const RPCHelpMan& getaddednodeinfo_helpman() { return getaddednodeinfo_help; }

UniValue getaddednodeinfo(const UniValue& params)
{
    bool fDns = params[0].get_bool();

    // Snapshot the added-node list via the CConnman API (issue #2558 PR 9b2).
    std::vector<std::string> vAdded = g_connman ? g_connman->GetAddedNodes() : std::vector<std::string>();

    list<string> laddedNodes(0);
    if (params.size() == 1)
    {
        for (auto const& strAddNode : vAdded)
            laddedNodes.push_back(strAddNode);
    }
    else
    {
        string strNode = params[1].get_str();
        for (auto const& strAddNode : vAdded)
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

    // Snapshot connected peers (address + direction) via the node-access API
    // (issue #2558 PR 9b2), then match the resolved added-node addresses
    // against it without holding cs_vNodes across the result build.
    std::vector<std::pair<CService, bool>> vConnected; // (addr, fInbound)
    if (g_connman) {
        g_connman->ForEachNode([&vConnected](CNode* pnode) {
            vConnected.emplace_back(static_cast<CService>(pnode->addr), pnode->fInbound);
        });
    }

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
            for (auto const& c : vConnected)
                if (c.first == addrNode)
                {
                    fFound = true;
                    fConnected = true;
                    node.pushKV("connected", c.second ? "inbound" : "outbound");
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

static const RPCHelpMan setban_help{
    "setban",
    "Attempts to add or remove an IP/Subnet from the banned list.",
    {
        {"subnet", RPCArg::Type::STR, RPCArg::Optional::NO,
            "The IP/Subnet (see getpeerinfo for nodes IP) with an optional netmask "
            "(default is /32 = single IP)."},
        {"command", RPCArg::Type::STR, RPCArg::Optional::NO,
            "'add' to add an IP/Subnet to the list, 'remove' to remove an IP/Subnet from the list."},
        {"bantime", RPCArg::Type::NUM, RPCArg::Optional::OMITTED,
            "Time in seconds how long (or until when if [absolute] is set) the IP is banned. "
            "0 or empty means using the default time of 24h which can also be overwritten "
            "by the -bantime startup argument."},
        {"absolute", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,
            "If set, the bantime must be an absolute timestamp in seconds since epoch (Jan 1 1970 GMT). "
            "Defaults to false."},
    },
    RPCResult{RPCResult::Type::NONE, "", ""},
    RPCExamples{
        HelpExampleCli("setban", "\"192.168.0.6\" \"add\" 86400") +
        HelpExampleCli("setban", "\"192.168.0.0/24\" \"add\"") +
        HelpExampleRpc("setban", "\"192.168.0.6\", \"add\", 86400")},
};
const RPCHelpMan& setban_helpman() { return setban_help; }

UniValue setban(const UniValue& params)
{
    const std::string strCommand = params[1].get_str();
    if (strCommand != "add" && strCommand != "remove") {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
            "command must be \"add\" or \"remove\"");
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
            if (g_connman) g_connman->DisconnectNode(subNet);
        } else {
            g_banman->Ban(netAddr, BanReasonManuallyAdded, banTime, absolute);
            if (g_connman) g_connman->DisconnectNode(netAddr);
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

static const RPCHelpMan listbanned_help{
    "listbanned",
    "List all banned IPs/subnets.",
    {},
    RPCResult{RPCResult::Type::ARR, "", "",
        {
            {RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::STR, "address", "The banned IP or subnet"},
                    {RPCResult::Type::NUM_TIME, "ban_created", "Timestamp when the ban was created"},
                    {RPCResult::Type::NUM_TIME, "banned_until", "Timestamp when the ban expires"},
                    {RPCResult::Type::NUM, "ban_duration", "Configured duration in seconds"},
                    {RPCResult::Type::NUM, "time_remaining", "Seconds remaining until the ban expires"},
                    {RPCResult::Type::STR, "ban_reason", "Reason text for the ban"},
                }},
        }},
    RPCExamples{
        HelpExampleCli("listbanned", "") +
        HelpExampleRpc("listbanned", "")},
};
const RPCHelpMan& listbanned_helpman() { return listbanned_help; }

UniValue listbanned(const UniValue& params)
{
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

static const RPCHelpMan clearbanned_help{
    "clearbanned",
    "Clear all banned IPs.",
    {},
    RPCResult{RPCResult::Type::NONE, "", ""},
    RPCExamples{
        HelpExampleCli("clearbanned", "") +
        HelpExampleRpc("clearbanned", "")},
};
const RPCHelpMan& clearbanned_helpman() { return clearbanned_help; }

UniValue clearbanned(const UniValue& params)
{
    if (!g_banman) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Error: Ban database not loaded");
    }

    g_banman->ClearBanned();

    return NullUniValue;
}

static const RPCHelpMan ping_help{
    "ping",
    "Requests that a ping be sent to all other nodes, to measure ping time.\n"
    "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
    "Ping command is handled in queue with all other commands, so it measures processing backlog,\n"
    "not just network ping.",
    {},
    RPCResult{RPCResult::Type::NONE, "", ""},
    RPCExamples{
        HelpExampleCli("ping", "") +
        HelpExampleRpc("ping", "")},
};
const RPCHelpMan& ping_helpman() { return ping_help; }

UniValue ping(const UniValue& params)
{
    // Request that each node send a ping during next message processing pass
    // (issue #2558 PR 9b: iterate via the CConnman node-access API).
    if (g_connman) {
        g_connman->ForEachNode([](CNode* pNode) {
            pNode->fPingQueued = true;
        });
    }

    return NullUniValue;
}

static const RPCHelpMan getpeerinfo_help{
    "getpeerinfo",
    "Returns data about each connected network node.",
    {},
    RPCResult{RPCResult::Type::ARR, "", "",
        {
            {RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::NUM, "id", "Peer index"},
                    {RPCResult::Type::STR, "addr", "The IP address and port of the peer"},
                    {RPCResult::Type::STR, "addrlocal", /*optional=*/true,
                        "Local address as reported by the peer (if any)"},
                    {RPCResult::Type::STR_HEX, "services", "Hex-encoded service flags"},
                    {RPCResult::Type::NUM_TIME, "lastsend", "Timestamp of the last send"},
                    {RPCResult::Type::NUM_TIME, "lastrecv", "Timestamp of the last receive"},
                    {RPCResult::Type::NUM, "bytessent", "Total bytes sent"},
                    {RPCResult::Type::NUM, "bytesrecv", "Total bytes received"},
                    {RPCResult::Type::NUM_TIME, "conntime", "Connection time as a UNIX epoch"},
                    {RPCResult::Type::NUM, "timeoffset", "The time offset in seconds"},
                    {RPCResult::Type::NUM, "pingtime", "Ping time in seconds"},
                    {RPCResult::Type::NUM, "minping", /*optional=*/true,
                        "Minimum observed ping time in seconds (if measured)"},
                    {RPCResult::Type::NUM, "pingwait", /*optional=*/true,
                        "Seconds spent waiting for a ping response (if a ping is in flight)"},
                    {RPCResult::Type::NUM, "version", "Peer protocol version"},
                    {RPCResult::Type::STR, "subver", "Peer subversion string"},
                    {RPCResult::Type::BOOL, "inbound", "True if connection is inbound"},
                    {RPCResult::Type::NUM, "startingheight", "Starting block height reported by the peer"},
                    {RPCResult::Type::NUM, "nTrust", "Peer trust score"},
                    {RPCResult::Type::NUM, "banscore", "Misbehavior score; nodes are banned once this hits the threshold"},
                }},
        }},
    RPCExamples{
        HelpExampleCli("getpeerinfo", "") +
        HelpExampleRpc("getpeerinfo", "")},
};
const RPCHelpMan& getpeerinfo_helpman() { return getpeerinfo_help; }

UniValue getpeerinfo(const UniValue& params)
{
    vector<CNodeStats> vstats;
    UniValue ret(UniValue::VARR);

    // Peer stats via the CConnman node-access API (issue #2558 PR 9b; leaves
    // vstats empty when g_connman is not up).
    if (g_connman) g_connman->GetNodeStats(vstats);

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

static const RPCHelpMan getnettotals_help{
    "getnettotals",
    "Returns information about network traffic, including bytes in, bytes out, and current time.",
    {},
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::NUM, "totalbytesrecv", "Total cumulative bytes received"},
            {RPCResult::Type::NUM, "totalbytessent", "Total cumulative bytes sent"},
            {RPCResult::Type::NUM_TIME, "timemillis", "Current system time in milliseconds since epoch"},
        }},
    RPCExamples{
        HelpExampleCli("getnettotals", "") +
        HelpExampleRpc("getnettotals", "")},
};
const RPCHelpMan& getnettotals_helpman() { return getnettotals_help; }

UniValue getnettotals(const UniValue& params)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("totalbytesrecv", CNode::GetTotalBytesRecv());
    obj.pushKV("totalbytessent", CNode::GetTotalBytesSent());
    obj.pushKV("timemillis", GetTimeMillis());
    return obj;
}

static const RPCHelpMan listalerts_help{
    "listalerts",
    "Returns information about alerts.",
    {},
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::ARR, "alerts", "",
                {
                    {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::NUM, "version", "Alert version"},
                            {RPCResult::Type::STR, "relay_until", "Timestamp until which the alert is relayed"},
                            {RPCResult::Type::STR, "expiration", "Timestamp at which the alert expires"},
                            {RPCResult::Type::NUM, "id", "Alert ID"},
                            {RPCResult::Type::NUM, "cancel_upto", "Cancels all alert IDs up to this number"},
                            {RPCResult::Type::ARR, "cancels", "",
                                {{RPCResult::Type::NUM, "id", "An alert ID this alert cancels"}}},
                            {RPCResult::Type::NUM, "minimum_version", "Minimum applicable client version"},
                            {RPCResult::Type::NUM, "maximum_version", "Maximum applicable client version"},
                            {RPCResult::Type::ARR, "subversions", "",
                                {{RPCResult::Type::STR, "subversion", "A peer subversion string this alert targets"}}},
                            {RPCResult::Type::NUM, "priority", "Alert priority"},
                            {RPCResult::Type::STR, "comment", "Comment text"},
                            {RPCResult::Type::STR, "status_bar", "Status bar text"},
                            {RPCResult::Type::STR, "reserved", "Reserved field"},
                            {RPCResult::Type::STR_HEX, "hash", "Hex-encoded alert hash"},
                            {RPCResult::Type::BOOL, "in_effect", "Whether the alert is currently in effect"},
                            {RPCResult::Type::BOOL, "applies_to_me", "Whether the alert applies to this node"},
                        }},
                }},
        }},
    RPCExamples{
        HelpExampleCli("listalerts", "") +
        HelpExampleRpc("listalerts", "")},
};
const RPCHelpMan& listalerts_helpman() { return listalerts_help; }

UniValue listalerts(const UniValue& params)
{
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
// Variadic: legacy behavior accepted any params.size() >= 6, with the
// 7th positional (cancelupto) optionally consumed. MarkVariadic() keeps
// the dispatcher off the upper-bound check so trailing-ignored args keep
// working; the body still requires the 6 mandatory positions.
static const RPCHelpMan sendalert_help = RPCHelpMan{
    "sendalert",
    "Sign and broadcast a network alert. Requires the WIF-encoded alert master private key.",
    {
        {"message", RPCArg::Type::STR, RPCArg::Optional::NO, "Alert text message."},
        {"privatekey", RPCArg::Type::STR, RPCArg::Optional::NO,
            "WIF-encoded alert master private key."},
        {"minver", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Minimum applicable internal client version."},
        {"maxver", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Maximum applicable internal client version."},
        {"priority", RPCArg::Type::NUM, RPCArg::Optional::NO, "Integer priority number."},
        {"id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Alert ID."},
        {"cancelupto", RPCArg::Type::NUM, RPCArg::Optional::OMITTED,
            "Cancels all alert IDs up to this number."},
    },
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "strStatusBar", "Status bar text relayed"},
            {RPCResult::Type::NUM, "nVersion", "Alert protocol version"},
            {RPCResult::Type::NUM, "nMinVer", "Minimum applicable client version"},
            {RPCResult::Type::NUM, "nMaxVer", "Maximum applicable client version"},
            {RPCResult::Type::NUM, "nPriority", "Alert priority"},
            {RPCResult::Type::NUM, "nID", "Alert ID"},
            {RPCResult::Type::NUM, "nCancel", /*optional=*/true,
                "Cancel-upto value (only present when nonzero)"},
        }},
    RPCExamples{
        HelpExampleCli("sendalert", "\"network outage\" \"<wif>\" 1000000 2000000 100 1") +
        HelpExampleRpc("sendalert", "\"network outage\", \"<wif>\", 1000000, 2000000, 100, 1")},
}.MarkVariadic();
const RPCHelpMan& sendalert_helpman() { return sendalert_help; }

UniValue sendalert(const UniValue& params)
{
    // Variadic positional: legacy minimum was 6 args (7th cancelupto optional, extras ignored).
    // The dispatcher pre-check is skipped via MarkVariadic(), so retain a body-level lower-bound
    // check here for the underflow case.
    if (params.size() < 6)
        throw runtime_error(sendalert_helpman().ToString());

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

    key = DecodeSecret(params[0].get_str());

    if (!key.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    if (!key.Sign(Hash(alert.vchMsg), alert.vchSig))
        throw runtime_error(
            "Unable to sign alert, check private key?\n");
    if(!alert.ProcessAlert())
        throw runtime_error(
            "Failed to process alert.\n");
    // Relay alert (issue #2558 PR 9b: iterate via the CConnman node-access API).
    if (g_connman) {
        g_connman->ForEachNode([&alert](CNode* pnode) {
            alert.RelayTo(pnode);
        });
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

static const RPCHelpMan sendalert2_help{
    "sendalert2",
    "Sign and broadcast a network alert with subversion/cancel list targeting.",
    {
        {"privatekey", RPCArg::Type::STR, RPCArg::Optional::NO,
            "WIF-encoded alert master private key."},
        {"id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Unique alert number."},
        {"subverlist", RPCArg::Type::STR, RPCArg::Optional::NO,
            "Comma-separated list of subversion strings the alert applies to (empty for all)."},
        {"cancellist", RPCArg::Type::STR, RPCArg::Optional::NO,
            "Comma-separated alert IDs to cancel (empty for none)."},
        {"expire", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Alert expiration window in days."},
        {"priority", RPCArg::Type::NUM, RPCArg::Optional::NO,
            "Integer priority; values >1000 are user-visible."},
        {"message", RPCArg::Type::STR, RPCArg::Optional::NO, "Alert text message."},
    },
    RPCResult{RPCResult::Type::ANY, "", "Summary of what was done; see source for the exact shape."},
    RPCExamples{
        HelpExampleCli("sendalert2", "\"<wif>\" 1 \"\" \"\" 7 1500 \"network upgrade required\"") +
        HelpExampleRpc("sendalert2", "\"<wif>\", 1, \"\", \"\", 7, 1500, \"network upgrade required\"")},
};
const RPCHelpMan& sendalert2_helpman() { return sendalert2_help; }

UniValue sendalert2(const UniValue& params)
{
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

    key = DecodeSecret(params[0].get_str());

    if (!key.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    if (!key.Sign(Hash(alert.vchMsg), alert.vchSig))
        throw runtime_error(
            "Unable to sign alert, check private key?\n");
    if(!alert.ProcessAlert())
        throw runtime_error(
            "Failed to process alert.\n");
    // Relay alert (issue #2558 PR 9b: iterate via the CConnman node-access API).
    if (g_connman) {
        g_connman->ForEachNode([&alert](CNode* pnode) {
            alert.RelayTo(pnode);
        });
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("Content", alert.ToString());
    result.pushKV("Success", true);
    return result;
}

static const RPCHelpMan getnetworkinfo_help{
    "getnetworkinfo",
    "Displays network related information.",
    {},
    RPCResult{RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "version", "The full Gridcoin version string"},
            {RPCResult::Type::NUM, "minor_version", "Client minor version"},
            {RPCResult::Type::NUM, "protocolversion", "P2P protocol version"},
            {RPCResult::Type::NUM, "timeoffset", "Time offset against the network (seconds)"},
            {RPCResult::Type::NUM, "connections", "Number of connections to other nodes"},
            {RPCResult::Type::STR_AMOUNT, "paytxfee", "The fee paid per transaction"},
            {RPCResult::Type::STR_AMOUNT, "mininput", "Minimum input value"},
            {RPCResult::Type::STR, "proxy", "host:port of any active SOCKS proxy, or empty string if none"},
            {RPCResult::Type::STR, "ip", "Best-effort guess at this node's external IP"},
            {RPCResult::Type::ARR, "localaddresses", "",
                {
                    {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR, "address", "A locally bound or advertised address"},
                            {RPCResult::Type::NUM, "port", "The port number"},
                            {RPCResult::Type::NUM, "score", "Local-address selection score"},
                        }},
                }},
            {RPCResult::Type::STR, "errors", "Any current warnings"},
        }},
    RPCExamples{
        HelpExampleCli("getnetworkinfo", "") +
        HelpExampleRpc("getnetworkinfo", "")},
};
const RPCHelpMan& getnetworkinfo_helpman() { return getnetworkinfo_help; }

UniValue getnetworkinfo(const UniValue& params)
{
    UniValue res(UniValue::VOBJ);

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    // Connection count via the CConnman node-access API (issue #2558 PR 9b).
    int connections = g_connman ? (int)g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) : 0;

    // Peer-reported external IP via the CConnman API (issue #2558 PR 9d).
    std::string addr_seen_by_peer_ip = g_connman ? g_connman->GetAddrSeenByPeer().ToStringIP() : "";

    LOCK(cs_main);

    res.pushKV("version",         FormatFullVersion());
    res.pushKV("minor_version",   CLIENT_VERSION_MINOR);
    res.pushKV("protocolversion", PROTOCOL_VERSION);
    res.pushKV("timeoffset",      GetTimeOffset());
    res.pushKV("connections",     connections);
    res.pushKV("paytxfee",        ValueFromAmount(nTransactionFee));
    res.pushKV("mininput",        ValueFromAmount(nMinimumInputValue));
    res.pushKV("proxy",           (proxy.IsValid() ? proxy.ToStringIPPort() : string()));
    res.pushKV("ip",              addr_seen_by_peer_ip);

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
