// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef __cplusplus
# error This header can only be compiled as C++.
#endif

#ifndef BITCOIN_PROTOCOL_H
#define BITCOIN_PROTOCOL_H

#include "netbase.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"

#include <limits>
#include <string>

extern bool fTestNet;
static inline unsigned short GetDefaultPort(const bool testnet = fTestNet)
{
    return testnet ? 32748 : 32749;
}


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

        CMessageHeader() = default;
        CMessageHeader(const char* pszCommand, unsigned int nMessageSizeIn);

        std::string GetCommand() const;
        bool IsValid() const;

        SERIALIZE_METHODS(CMessageHeader, obj) { READWRITE(obj.pchMessageStart, obj.pchCommand, obj.nMessageSize, obj.pchChecksum); }

        char pchMessageStart[MESSAGE_START_SIZE]{};
        char pchCommand[COMMAND_SIZE]{};
        uint32_t nMessageSize{std::numeric_limits<uint32_t>::max()};
        uint8_t pchChecksum[CHECKSUM_SIZE]{};
};

/** nServices flags */
enum ServiceFlags : uint64_t {
    // NOTE: When adding here, be sure to update serviceFlagToStr too
    // Nothing
    NODE_NONE = 0,
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

/**
 * Bitcoin protocol message types. When adding new message types, don't forget
 * to update allNetMessageTypes in protocol.cpp.
 */
namespace NetMsgType {

    /**
    * The version message provides information about the transmitting node to the
    * receiving node at the beginning of a connection.
    * @see https://bitcoin.org/en/developer-reference#version
    */
    extern const char *VERSION;

    /**
    * The verack message acknowledges a previously-received version message,
    * informing the connecting node that it can begin to send other messages.
    * @see https://bitcoin.org/en/developer-reference#verack
    */
    extern const char *VERACK;

    /**
    * The addr (IP address) message relays connection information for peers on the
    * network.
    * @see https://bitcoin.org/en/developer-reference#addr
    */
    extern const char *ADDR;

    /**
    * The inv message (inventory message) transmits one or more inventories of
    * objects known to the transmitting peer.
    * @see https://bitcoin.org/en/developer-reference#inv
    */
    extern const char *INV;

    /**
    * The getdata message requests one or more data objects from another node.
    * @see https://bitcoin.org/en/developer-reference#getdata
    */
    extern const char *GETDATA;

    /**
    * The getblocks message requests an inv message that provides block header
    * hashes starting from a particular point in the block chain.
    * @see https://bitcoin.org/en/developer-reference#getblocks
    */
    extern const char *GETBLOCKS;

    /**
    * The getheaders message requests a headers message that provides block
    * headers starting from a particular point in the block chain.
    * @since protocol version 31800.
    * @see https://bitcoin.org/en/developer-reference#getheaders
    */
    extern const char *GETHEADERS;

    /**
    * The tx message transmits a single transaction.
    * @see https://bitcoin.org/en/developer-reference#tx
    */
    extern const char *TX;

    /**
    * The headers message sends one or more block headers to a node which
    * previously requested certain headers with a getheaders message.
    * @since protocol version 31800.
    * @see https://bitcoin.org/en/developer-reference#headers
    */
    extern const char *HEADERS;

    /**
    * The block message transmits a single serialized block.
    * @see https://bitcoin.org/en/developer-reference#block
    */
    extern const char *BLOCK;

    /**
    * The ping message is sent periodically to help confirm that the receiving
    * peer is still connected.
    * @see https://bitcoin.org/en/developer-reference#ping
    */
    extern const char *PING;

    /**
    * The pong message replies to a ping message, proving to the pinging node that
    * the ponging node is still alive.
    * @since protocol version 60001 as described by BIP31.
    * @see https://bitcoin.org/en/developer-reference#pong
    */
    extern const char *PONG;

    /**
    * The alert message warns nodes of problems that may affect them or the rest
    * of the network.
    * @since protocol version 311.
    * @see https://bitcoin.org/en/developer-reference#alert
    */
    extern const char *ALERT;

    /**
    * The getaddr message requests an addr message from the receiving node,
    * preferably one with lots of IP addresses of other receiving nodes.
    * @see https://bitcoin.org/en/developer-reference#getaddr
    */
    extern const char *GETADDR;

    /**
    * The mempool message requests the TXIDs of transactions that the receiving
    * node has verified as valid but which have not yet appeared in a block.
    * @since protocol version 60002.
    * @see https://bitcoin.org/en/developer-reference#mempool
    */
    extern const char *MEMPOOL;

    /**
    * Gridcoin alias for block message (will be removed)
    */
    extern const char *ENCRYPT;

    /**
    * Gridcoin alias for addr message (will be removed)
    */
    extern const char *GRIDADDR;

    /**
    * Gridcoin specific message
    */
    extern const char *SCRAPERINDEX;

    /**
    * Gridcoin specific message
    */
    extern const char *PART;

    /**
    * Gridcoin alias for version message (will be removed)
    */
    extern const char *ARIES;
};
/* Get a vector of all valid message types (see above) */
const std::vector<std::string> &getAllNetMessageTypes();


#endif // BITCOIN_PROTOCOL_H
