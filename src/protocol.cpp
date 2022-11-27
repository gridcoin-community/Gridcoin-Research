// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "netbase.h"
#include "protocol.h"
#include "util.h"

#ifndef WIN32
# include <arpa/inet.h>
#endif

namespace NetMsgType {
    const char *VERSION="version";
    const char *VERACK="verack";
    const char *ADDR="addr";
    const char *INV="inv";
    const char *GETDATA="getdata";
    const char *GETBLOCKS="getblocks";
    const char *GETHEADERS="getheaders";
    const char *TX="tx";
    const char *HEADERS="headers";
    const char *BLOCK="block";
    const char *GETADDR="getaddr";
    const char *MEMPOOL="mempool";
    const char *PING="ping";
    const char *PONG="pong";
    const char *ALERT="alert";

    // Gridcoin aliases (to be removed)
    const char *ENCRYPT="encrypt";
    const char *GRIDADDR="gridaddr";
    const char *ARIES="aries";

    // Gridcoin specific
    const char *SCRAPERINDEX="scraperindex";
    const char *PART="part";
}

static const char* ppszTypeName[] =
{
    "ERROR", // Should never occur
    NetMsgType::TX,
    NetMsgType::BLOCK,
    NetMsgType::PART,
    NetMsgType::SCRAPERINDEX,
};

/** All known message types. Keep this in the same order as the list of
 * messages above and in protocol.h.
 */
const static std::string allNetMessageTypes[] = {
    NetMsgType::VERSION,
    NetMsgType::VERACK,
    NetMsgType::ADDR,
    NetMsgType::INV,
    NetMsgType::GETDATA,
    NetMsgType::GETBLOCKS,
    NetMsgType::GETHEADERS,
    NetMsgType::TX,
    NetMsgType::HEADERS,
    NetMsgType::BLOCK,
    NetMsgType::GETADDR,
    NetMsgType::MEMPOOL,
    NetMsgType::PING,
    NetMsgType::PONG,
    NetMsgType::ALERT,

    // Gridcoin aliases (to be removed)
    NetMsgType::ENCRYPT,
    NetMsgType::GRIDADDR,
    NetMsgType::ARIES,

    // Gridcoin specific
    NetMsgType::SCRAPERINDEX,
    NetMsgType::PART,
};

const static std::vector<std::string> allNetMessageTypesVec(std::begin(allNetMessageTypes), std::end(allNetMessageTypes));

CMessageHeader::CMessageHeader(const char* pszCommand, unsigned int nMessageSizeIn)
{
    memcpy(pchMessageStart, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE);

    // Copy the command name
    size_t i = 0;
    for (; i < COMMAND_SIZE && pszCommand[i] != 0; ++i) pchCommand[i] = pszCommand[i];
    assert(pszCommand[i] == 0); // Assert that the command name passed in is not longer than COMMAND_SIZE

    nMessageSize = nMessageSizeIn;
}

std::string CMessageHeader::GetCommand() const
{
    if (pchCommand[COMMAND_SIZE-1] == 0)
        return std::string(pchCommand, pchCommand + strlen(pchCommand));
    else
        return std::string(pchCommand, pchCommand + COMMAND_SIZE);
}

bool CMessageHeader::IsValid() const
{
    // Check start string
    if (memcmp(pchMessageStart, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE) != 0)
        return false;

    // Check the command string for errors
    for (const char* p1 = pchCommand; p1 < pchCommand + COMMAND_SIZE; p1++)
    {
        if (*p1 == 0)
        {
            // Must be all zeros after the first zero
            for (; p1 < pchCommand + COMMAND_SIZE; p1++)
                if (*p1 != 0)
                    return false;
        }
        else if (*p1 < ' ' || *p1 > 0x7E)
            return false;
    }

    // Message size
    if (nMessageSize > MAX_SIZE)
    {
        LogPrintf("CMessageHeader::IsValid() : (%s, %u bytes) nMessageSize > MAX_SIZE", GetCommand(), nMessageSize);
        return false;
    }

    return true;
}

CInv::CInv()
{
    type = 0;
    hash.SetNull();
}

CInv::CInv(int typeIn, const uint256& hashIn) : type(typeIn), hash(hashIn) {}

CInv::CInv(const std::string& strType, const uint256& hashIn)
{
    unsigned int i;
    for (i = 1; i < std::size(ppszTypeName); i++)
    {
        if (strType == ppszTypeName[i])
        {
            type = i;
            break;
        }
    }
    if (i == std::size(ppszTypeName))
        throw std::out_of_range(strprintf("CInv::CInv(string, uint256) : unknown type '%s'", strType));
    hash = hashIn;
}

bool operator<(const CInv& a, const CInv& b)
{
    return (a.type < b.type || (a.type == b.type && a.hash < b.hash));
}

bool CInv::IsKnownType() const
{
    return (type >= 1 && type < (int)std::size(ppszTypeName));
}

const char* CInv::GetCommand() const
{
    if (!IsKnownType())
        throw std::out_of_range(strprintf("CInv::GetCommand() : type=%d unknown type", type));
    return ppszTypeName[type];
}

std::string CInv::ToString() const
{
    return strprintf("%s %s", GetCommand(), hash.ToString().substr(0,20));
}

void CInv::print() const
{
    LogPrintf("CInv(%s)", ToString());
}

const std::vector<std::string> &getAllNetMessageTypes()
{
    return allNetMessageTypesVec;
}