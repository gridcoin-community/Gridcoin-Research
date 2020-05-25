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
	
        CMessageHeader();
        CMessageHeader(const char* pszCommand, unsigned int nMessageSizeIn);

        std::string GetCommand() const;
        bool IsValid() const;

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
             READWRITE(pchMessageStart);
             READWRITE(pchCommand);
             READWRITE(nMessageSize);
             READWRITE(nChecksum);
        }

        char pchMessageStart[MESSAGE_START_SIZE];
        char pchCommand[COMMAND_SIZE];
        uint32_t nMessageSize;
        unsigned int nChecksum;
};

/** nServices flags */
enum
{
    NODE_NETWORK = (1 << 0),
};

/** A CService with information about it as peer */
class CAddress : public CService
{
    public:
        CAddress();
        explicit CAddress(CService ipIn, uint64_t nServicesIn=NODE_NETWORK);

        void Init();

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            if (ser_action.ForRead()) {
                Init();
            }

            int nVersion = s.GetVersion();
            if (s.GetType() & SER_DISK) {
                READWRITE(nVersion);
            }

            if ((s.GetType() & SER_DISK)
                || (nVersion >= CADDR_TIME_VERSION && !(s.GetType() & SER_GETHASH)))
            {
                READWRITE(nTime);
            }

            READWRITE(nServices);
            READWRITEAS(CService, *this);
        }

        void print() const;

    // TODO: make private (improves encapsulation)
    public:
        uint64_t nServices;

        // disk and network only
        unsigned int nTime;

        // memory only
        int64_t nLastTry;
};

/** inv message data */
class CInv
{
    public:
        CInv();
        CInv(int typeIn, const uint256& hashIn);
        CInv(const std::string& strType, const uint256& hashIn);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action)
        {
            READWRITE(type);
            READWRITE(hash);
        }

        friend bool operator<(const CInv& a, const CInv& b);

        bool IsKnownType() const;
        const char* GetCommand() const;
        std::string ToString() const;
        void print() const;

    // TODO: make private (improves encapsulation)
    public:
        int type;
        uint256 hash;
};

#endif // __INCLUDED_PROTOCOL_H__
