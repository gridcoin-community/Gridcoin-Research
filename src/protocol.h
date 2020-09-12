// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __cplusplus
# error This header can only be compiled as C++.
#endif

#ifndef __INCLUDED_PROTOCOL_H__
#define __INCLUDED_PROTOCOL_H__

#include "netbase.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"

#include <string>

extern bool fTestNet;
static inline unsigned short GetDefaultPort(const bool testnet = fTestNet)
{
    return testnet ? 32748 : 32749;
}


extern unsigned char pchMessageStart[4];

/** Message header.
 * (4) message start.
 * (12) command.
 * (4) size.
 * (4) checksum.
 */
class CMessageHeader
{
    public:
        static constexpr size_t MESSAGE_START_SIZE = 4;
        static constexpr size_t COMMAND_SIZE = 12;
        static constexpr size_t MESSAGE_SIZE_SIZE = 4;
        static constexpr size_t CHECKSUM_SIZE = 4;
        static constexpr size_t MESSAGE_SIZE_OFFSET = MESSAGE_START_SIZE + COMMAND_SIZE;
        static constexpr size_t CHECKSUM_OFFSET = MESSAGE_SIZE_OFFSET + MESSAGE_SIZE_SIZE;
        static constexpr size_t HEADER_SIZE = MESSAGE_START_SIZE + COMMAND_SIZE + MESSAGE_SIZE_SIZE + CHECKSUM_SIZE;
        typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

        CMessageHeader();
        CMessageHeader(const char* pszCommand, unsigned int nMessageSizeIn);

        std::string GetCommand() const;
        bool IsValid() const;

        SERIALIZE_METHODS(CMessageHeader, obj) { READWRITE(obj.pchMessageStart, obj.pchCommand, obj.nMessageSize, obj.pchChecksum); }

        char pchMessageStart[MESSAGE_START_SIZE];
        char pchCommand[COMMAND_SIZE];
        uint32_t nMessageSize;
        uint8_t pchChecksum[CHECKSUM_SIZE];
};

/** nServices flags */
enum ServiceFlags : uint64_t {
    // NODE_NETWORK means that the node is capable of serving the complete block chain.
    NODE_NETWORK = (1 << 0),
};

/** A CService with information about it as peer */
class CAddress : public CService
{
    static constexpr uint32_t TIME_INIT{100000000};

    public:
        CAddress() : CService{} {};
        explicit CAddress(CService ipIn, ServiceFlags nServicesIn=NODE_NETWORK): CService{ipIn}, nServices{nServicesIn} {};

        SERIALIZE_METHODS(CAddress, obj)
        {
            SER_READ(obj, obj.nTime = TIME_INIT);
            int nVersion = s.GetVersion();
            if (s.GetType() & SER_DISK) {
                READWRITE(nVersion);
            }
            if ((s.GetType() & SER_DISK) ||
                (nVersion >= INIT_PROTO_VERSION && !(s.GetType() & SER_GETHASH))) {
                READWRITE(obj.nTime);
            }
            READWRITE(Using<CustomUintFormatter<8>>(obj.nServices));
            READWRITEAS(CService, obj);
        }

        void print() const;

        ServiceFlags nServices{NODE_NETWORK};
        // disk and network only
        uint32_t nTime{TIME_INIT};
        // memory only
        int64_t nLastTry = 0;
};

/** inv message data */
class CInv
{
    public:
        CInv();
        CInv(int typeIn, const uint256& hashIn);
        CInv(const std::string& strType, const uint256& hashIn);

        SERIALIZE_METHODS(CInv, obj) { READWRITE(obj.type, obj.hash); }

        friend bool operator<(const CInv& a, const CInv& b);

        bool IsKnownType() const;
        const char* GetCommand() const;
        std::string ToString() const;
        void print() const;

        int type;
        uint256 hash;
};


#endif // __INCLUDED_PROTOCOL_H__
