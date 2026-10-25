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
#include "streams.h"
#include "uint256.h"
#include "version.h"

#include <limits>
#include <string>


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

    /** bitcoin#20516: the first field of the CAddress disk serialization used to be
     *  the CLIENT_VERSION, optionally OR'ed with ADDRV2_FORMAT. It is now:
     *  - low bits (DISK_VERSION_IGNORE_MASK): a fixed DISK_VERSION_INIT, ignored on read;
     *  - high bits: real format info. Only 0 or DISK_VERSION_ADDRV2 are valid.
     *
     *  Gridcoin adaptation: upstream's mask is 19 bits (Bitcoin CLIENT_VERSION < 2^19),
     *  but Gridcoin's CLIENT_VERSION is 1000000*MAJOR+... (5.x => ~5,0xx,xxx > 2^22),
     *  and existing peers.dat files store that value here. The mask therefore covers
     *  every bit below ADDRV2_FORMAT (bits 0..28) so old files still read as V1. */
    static constexpr uint32_t DISK_VERSION_INIT{5000000};
    static constexpr uint32_t DISK_VERSION_IGNORE_MASK{0x1FFFFFFF};
    /** Must be exactly 1<<29 (= ADDRV2_FORMAT) for compatibility with what
     *  the 19954 network code and upstream use. */
    static constexpr uint32_t DISK_VERSION_ADDRV2{1 << 29};
    static_assert((DISK_VERSION_INIT & ~DISK_VERSION_IGNORE_MASK) == 0, "DISK_VERSION_INIT must be covered by DISK_VERSION_IGNORE_MASK");
    static_assert((DISK_VERSION_ADDRV2 & DISK_VERSION_IGNORE_MASK) == 0, "DISK_VERSION_ADDRV2 must not be covered by DISK_VERSION_IGNORE_MASK");
    static_assert(DISK_VERSION_ADDRV2 == ADDRV2_FORMAT, "DISK_VERSION_ADDRV2 must equal ADDRV2_FORMAT");

    public:
        CAddress() : CService{} {};
        explicit CAddress(CService ipIn, ServiceFlags nServicesIn=NODE_NETWORK): CService{ipIn}, nServices{nServicesIn} {};
        CAddress(CService ipIn, ServiceFlags nServicesIn, uint32_t nTimeIn) : CService{ipIn}, nServices{nServicesIn}, nTime{nTimeIn} {};

        SERIALIZE_METHODS(CAddress, obj)
        {
            bool use_v2;
            bool store_time;
            if (s.GetType() & SER_DISK) {
                // bitcoin#20516: in the disk format the encoding (v1 or v2) is
                // determined by a flag stored in the serialization itself.
                // ADDRV2_FORMAT in the stream version only permits/selects v2.
                uint32_t stored_format_version = DISK_VERSION_INIT;
                if (s.GetVersion() & ADDRV2_FORMAT) stored_format_version |= DISK_VERSION_ADDRV2;
                READWRITE(stored_format_version);
                stored_format_version &= ~DISK_VERSION_IGNORE_MASK; // ignore low bits
                if (stored_format_version == 0) {
                    use_v2 = false;
                } else if (stored_format_version == DISK_VERSION_ADDRV2 && (s.GetVersion() & ADDRV2_FORMAT)) {
                    // Only support v2 deserialization if ADDRV2_FORMAT is set.
                    use_v2 = true;
                } else {
                    throw std::ios_base::failure("Unsupported CAddress disk format version");
                }
                store_time = true;
            } else {
                // Network format: encoding chosen by ADDRV2_FORMAT in the stream
                // version, nothing is embedded. Gridcoin keeps its historical
                // nTime rule (omitted below INIT_PROTO_VERSION and for SER_GETHASH).
                use_v2 = s.GetVersion() & ADDRV2_FORMAT;
                store_time = (s.GetVersion() >= INIT_PROTO_VERSION && !(s.GetType() & SER_GETHASH));
            }

            SER_READ(obj, obj.nTime = TIME_INIT);
            if (store_time) READWRITE(obj.nTime);
            // nServices is serialized as CompactSize in V2; as uint64_t in V1.
            if (use_v2) {
                uint64_t services_tmp;
                SER_WRITE(obj, services_tmp = obj.nServices);
                READWRITE(Using<CompactSizeFormatter<false>>(services_tmp));
                SER_READ(obj, obj.nServices = static_cast<ServiceFlags>(services_tmp));
            } else {
                READWRITE(Using<CustomUintFormatter<8>>(obj.nServices));
            }
            // Invoke V1/V2 serializer for the CService parent object.
            OverrideStream<Stream> os(&s, s.GetType(), use_v2 ? ADDRV2_FORMAT : 0);
            SerReadWriteMany(os, ser_action, ReadWriteAsHelper<CService>(obj));
        }

        friend bool operator==(const CAddress& a, const CAddress& b)
        {
            return a.nTime == b.nTime &&
                   a.nServices == b.nServices &&
                   static_cast<const CService&>(a) == static_cast<const CService&>(b);
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
    * The addrv2 message relays connection information for peers on the network just
    * like the addr message, but is extended to allow gossiping of longer node
    * addresses (see BIP155).
    */
    extern const char *ADDRV2;

    /**
    * The sendaddrv2 message signals support for receiving ADDRV2 messages (BIP155).
    * It also implies that its sender can encode as ADDRV2 and would send ADDRV2
    * instead of ADDR to a peer that has signaled ADDRV2 support by sending SENDADDRV2.
    */
    extern const char *SENDADDRV2;

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
    * Gridcoin specific message: a partially signed transaction for the
    * PSGT pool (#2910). Payload is the raw binary PSGT wire bytes.
    */
    extern const char *PSGT;

    /**
    * Gridcoin alias for version message (will be removed)
    */
    extern const char *ARIES;
};
/* Get a vector of all valid message types (see above) */
const std::vector<std::string> &getAllNetMessageTypes();


#endif // BITCOIN_PROTOCOL_H
