// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifdef HAVE_CONFIG_H
#include "config/gridcoin-config.h"
#endif

#include "netaddress.h"
#include "prevector.h"
#include "crypto/sha3.h"
#include "crypto/common.h"
#include "hash.h"
#include "util/strencodings.h"
#include "util/string.h"
#include "tinyformat.h"

#include <algorithm>
#include <array>
#include <ios>
#include <iterator>
#include <tuple>

CNetAddr::BIP155Network CNetAddr::GetBIP155Network() const
{
    switch (m_net) {
    case NET_IPV4:
        return BIP155Network::IPV4;
    case NET_IPV6:
        return BIP155Network::IPV6;
    case NET_TOR:
        switch (m_addr.size()) {
        case ADDR_TORV2_SIZE:
            return BIP155Network::TORV2;
        case ADDR_TORV3_SIZE:
            return BIP155Network::TORV3;
        default:
            assert(false); // LINT-OK-ASSERT: NET_TOR m_addr is only ever set to the TORv2 or TORv3 size
        }
    case NET_CJDNS:
        return BIP155Network::CJDNS;
    case NET_INTERNAL:   // should have been handled before calling this function
    case NET_UNROUTABLE: // m_net is never and should not be set to NET_UNROUTABLE
    case NET_MAX:        // m_net is never and should not be set to NET_MAX
        assert(false); // LINT-OK-ASSERT: m_net is never CJDNS/UNROUTABLE/MAX; INTERNAL is handled by every caller first
    } // no default case, so the compiler can warn about missing cases

    assert(false); // LINT-OK-ASSERT: unreachable, the switch above covers every enum value
}

bool CNetAddr::SetNetFromBIP155Network(uint8_t possible_bip155_net, size_t address_size)
{
    switch (possible_bip155_net) {
    case BIP155Network::IPV4:
        if (address_size == ADDR_IPV4_SIZE) {
            m_net = NET_IPV4;
            return true;
        }
        throw std::ios_base::failure(
            strprintf("BIP155 IPv4 address with length %u (should be %u)", address_size,
                      ADDR_IPV4_SIZE));
    case BIP155Network::IPV6:
        if (address_size == ADDR_IPV6_SIZE) {
            m_net = NET_IPV6;
            return true;
        }
        throw std::ios_base::failure(
            strprintf("BIP155 IPv6 address with length %u (should be %u)", address_size,
                      ADDR_IPV6_SIZE));
    case BIP155Network::TORV2:
        if (address_size == ADDR_TORV2_SIZE) {
            m_net = NET_TOR;
            return true;
        }
        throw std::ios_base::failure(
            strprintf("BIP155 TORv2 address with length %u (should be %u)", address_size,
                      ADDR_TORV2_SIZE));
    case BIP155Network::TORV3:
        if (address_size == ADDR_TORV3_SIZE) {
            m_net = NET_TOR;
            return true;
        }
        throw std::ios_base::failure(
            strprintf("BIP155 TORv3 address with length %u (should be %u)", address_size,
                      ADDR_TORV3_SIZE));
    case BIP155Network::I2P:
        // Gridcoin adaptation: no I2P support (yet). Treat like an unknown id
        // (silently skip) instead of upstream's size-check/accept.
        return false;
    case BIP155Network::CJDNS:
        if (address_size == ADDR_CJDNS_SIZE) {
            m_net = NET_CJDNS;
            return true;
        }
        throw std::ios_base::failure(
            strprintf("BIP155 CJDNS address with length %u (should be %u)", address_size,
                      ADDR_CJDNS_SIZE));
    }

    // Don't throw on addresses with unknown network ids (maybe from the future).
    // Instead silently drop them and have the unserialization code consume
    // subsequent ones which may be known to us.
    return false;
}

void CNetAddr::Init()
{
    m_net = NET_IPV6;
    m_addr.assign(ADDR_IPV6_SIZE, 0x0);
    scopeId = 0;
}

void CNetAddr::SetIP(const CNetAddr& ipIn)
{
    // Size check.
    switch (ipIn.m_net) {
    case NET_IPV4:
        assert(ipIn.m_addr.size() == ADDR_IPV4_SIZE); // LINT-OK-ASSERT: m_addr size always matches m_net, enforced by every setter
        break;
    case NET_IPV6:
        assert(ipIn.m_addr.size() == ADDR_IPV6_SIZE); // LINT-OK-ASSERT: m_addr size always matches m_net, enforced by every setter
        break;
    case NET_TOR:
        assert(ipIn.m_addr.size() == ADDR_TORV2_SIZE || ipIn.m_addr.size() == ADDR_TORV3_SIZE); // LINT-OK-ASSERT: m_addr size always matches m_net, enforced by every setter
        break;
    case NET_INTERNAL:
        assert(ipIn.m_addr.size() == ADDR_INTERNAL_SIZE); // LINT-OK-ASSERT: m_addr size always matches m_net, enforced by every setter
        break;
    case NET_CJDNS:
        assert(ipIn.m_addr.size() == ADDR_CJDNS_SIZE);
        break;
    case NET_UNROUTABLE:
    case NET_MAX:
        assert(false); // LINT-OK-ASSERT: m_net is never set to CJDNS/UNROUTABLE/MAX
    } // no default case, so the compiler can warn about missing cases

    m_net = ipIn.m_net;
    m_addr = ipIn.m_addr;
}

void CNetAddr::SetLegacyIPv6(const uint8_t ipv6[16])
{
    size_t skip{0};
    if (std::equal(IPV4_IN_IPV6_PREFIX.begin(), IPV4_IN_IPV6_PREFIX.end(), ipv6)) {
        m_net = NET_IPV4;
        skip = IPV4_IN_IPV6_PREFIX.size();
    } else if (std::equal(TORV2_IN_IPV6_PREFIX.begin(), TORV2_IN_IPV6_PREFIX.end(), ipv6)) {
        m_net = NET_TOR;
        skip = TORV2_IN_IPV6_PREFIX.size();
    } else if (std::equal(INTERNAL_IN_IPV6_PREFIX.begin(), INTERNAL_IN_IPV6_PREFIX.end(), ipv6)) {
        m_net = NET_INTERNAL;
        skip = INTERNAL_IN_IPV6_PREFIX.size();
    } else {
        m_net = NET_IPV6;
    }
    m_addr.assign(ipv6 + skip, ipv6 + ADDR_IPV6_SIZE);
}

bool CNetAddr::SetInternal(const std::string& name)
{
    if (name.empty()) return false;
    m_net = NET_INTERNAL;
    unsigned char hash[32] = {};
    CSHA256().Write(reinterpret_cast<const unsigned char*>(name.data()), name.size()).Finalize(hash);
    m_addr.assign(hash, hash + ADDR_INTERNAL_SIZE);
    return true;
}

namespace torv3 {
// https://gitweb.torproject.org/torspec.git/tree/rend-spec-v3.txt#n2135
static constexpr size_t CHECKSUM_LEN = 2;
static const unsigned char VERSION[] = {3};
static constexpr size_t TOTAL_LEN = ADDR_TORV3_SIZE + CHECKSUM_LEN + sizeof(VERSION);

static void Checksum(Span<const uint8_t> addr_pubkey, uint8_t (&checksum)[CHECKSUM_LEN])
{
    // TORv3 CHECKSUM = H(".onion checksum" | PUBKEY | VERSION)[:2]
    static const unsigned char prefix[] = ".onion checksum";
    static constexpr size_t prefix_len = 15;

    SHA3_256 hasher;

    hasher.Write(Span<const unsigned char>{prefix, prefix_len});
    hasher.Write(addr_pubkey);
    hasher.Write(VERSION);

    uint8_t checksum_full[SHA3_256::OUTPUT_SIZE];

    hasher.Finalize(checksum_full);

    memcpy(checksum, checksum_full, sizeof(checksum));
}

}; // namespace torv3

bool CNetAddr::SetSpecial(const std::string& str)
{
    static const char* suffix{".onion"};
    static constexpr size_t suffix_len{6};

    if (!ValidAsCString(str) || str.size() <= suffix_len ||
        str.substr(str.size() - suffix_len) != suffix) {
        return false;
    }

    bool invalid;
    const auto& input = DecodeBase32(str.substr(0, str.size() - suffix_len).c_str(), &invalid);

    if (invalid) {
        return false;
    }

    switch (input.size()) {
    case ADDR_TORV2_SIZE:
        m_net = NET_TOR;
        m_addr.assign(input.begin(), input.end());
        return true;
    case torv3::TOTAL_LEN: {
        Span<const uint8_t> input_pubkey{input.data(), ADDR_TORV3_SIZE};
        Span<const uint8_t> input_checksum{input.data() + ADDR_TORV3_SIZE, torv3::CHECKSUM_LEN};
        Span<const uint8_t> input_version{input.data() + ADDR_TORV3_SIZE + torv3::CHECKSUM_LEN, sizeof(torv3::VERSION)};

        uint8_t calculated_checksum[torv3::CHECKSUM_LEN];
        torv3::Checksum(input_pubkey, calculated_checksum);

        if (input_checksum != calculated_checksum || input_version != torv3::VERSION) {
            return false;
        }

        m_net = NET_TOR;
        m_addr.assign(input_pubkey.begin(), input_pubkey.end());
        return true;
    }
    }

    return false;
}

CNetAddr::CNetAddr()
{
    Init();
}

CNetAddr::CNetAddr(const struct in_addr& ipv4Addr)
{
    m_net = NET_IPV4;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&ipv4Addr);
    m_addr.assign(ptr, ptr + ADDR_IPV4_SIZE);
}

CNetAddr::CNetAddr(const struct in6_addr& ipv6Addr, const uint32_t scope)
{
    SetLegacyIPv6(reinterpret_cast<const uint8_t*>(&ipv6Addr));
    scopeId = scope;
}

bool CNetAddr::IsIPv4() const { return m_net == NET_IPV4; }

bool CNetAddr::IsIPv6() const { return m_net == NET_IPV6; }

bool CNetAddr::IsRFC1918() const
{
    return IsIPv4() && (
        m_addr[0] == 10 ||
        (m_addr[0] == 192 && m_addr[1] == 168) ||
        (m_addr[0] == 172 && m_addr[1] >= 16 && m_addr[1] <= 31));
}

bool CNetAddr::IsRFC2544() const
{
    return IsIPv4() && m_addr[0] == 198 && (m_addr[1] == 18 || m_addr[1] == 19);
}

bool CNetAddr::IsRFC3927() const
{
    return IsIPv4() && HasPrefix(m_addr, std::array<uint8_t, 2>{169, 254});
}

bool CNetAddr::IsRFC6598() const
{
    return IsIPv4() && m_addr[0] == 100 && m_addr[1] >= 64 && m_addr[1] <= 127;
}

bool CNetAddr::IsRFC5737() const
{
    return IsIPv4() && (HasPrefix(m_addr, std::array<uint8_t, 3>{192, 0, 2}) ||
                        HasPrefix(m_addr, std::array<uint8_t, 3>{198, 51, 100}) ||
                        HasPrefix(m_addr, std::array<uint8_t, 3>{203, 0, 113}));
}

bool CNetAddr::IsRFC3849() const
{
    return IsIPv6() && HasPrefix(m_addr, std::array<uint8_t, 4>{0x20, 0x01, 0x0D, 0xB8});
}

bool CNetAddr::IsRFC3964() const
{
    return IsIPv6() && HasPrefix(m_addr, std::array<uint8_t, 2>{0x20, 0x02});
}

bool CNetAddr::IsRFC6052() const
{
    return IsIPv6() &&
           HasPrefix(m_addr, std::array<uint8_t, 12>{0x00, 0x64, 0xFF, 0x9B, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
}

bool CNetAddr::IsRFC4380() const
{
    return IsIPv6() && HasPrefix(m_addr, std::array<uint8_t, 4>{0x20, 0x01, 0x00, 0x00});
}

bool CNetAddr::IsRFC4862() const
{
    return IsIPv6() && HasPrefix(m_addr, std::array<uint8_t, 8>{0xFE, 0x80, 0x00, 0x00,
                                                                0x00, 0x00, 0x00, 0x00});
}

bool CNetAddr::IsRFC4193() const
{
    return IsIPv6() && (m_addr[0] & 0xFE) == 0xFC;
}

bool CNetAddr::IsRFC6145() const
{
    return IsIPv6() &&
           HasPrefix(m_addr, std::array<uint8_t, 12>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00});
}

bool CNetAddr::IsRFC4843() const
{
    return IsIPv6() && HasPrefix(m_addr, std::array<uint8_t, 3>{0x20, 0x01, 0x00}) &&
           (m_addr[3] & 0xF0) == 0x10;
}

bool CNetAddr::IsTor() const { return m_net == NET_TOR; }

/**
 * Check whether this object represents a CJDNS address.
 * Set from BIP155 addrv2 (network id 6) or by MaybeFlipIPv6toCJDNS() for
 * fc00::/8 addresses when -cjdnsreachable is set.
 */
bool CNetAddr::IsCJDNS() const { return m_net == NET_CJDNS; }

bool CNetAddr::IsLocal() const
{
    // IPv4 loopback
   if (IsIPv4() && (m_addr[0] == 127 || m_addr[0] == 0))
       return true;

   // IPv6 loopback (::1/128)
   static const unsigned char pchLocal[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
   if (IsIPv6() && memcmp(m_addr.data(), pchLocal, sizeof(pchLocal)) == 0)
       return true;

   return false;
}

bool CNetAddr::IsMulticast() const
{
    return    (IsIPv4() && (m_addr[0] & 0xF0) == 0xE0)
           || (IsIPv6() && m_addr[0] == 0xFF);
}

bool CNetAddr::IsValid() const
{
    // Cleanup 3-byte shifted addresses caused by garbage in size field
    // of addr messages from versions before 0.2.9 checksum.
    // Two consecutive addr messages look like this:
    // header20 vectorlen3 addr26 addr26 addr26 header20 vectorlen3 addr26 addr26 addr26...
    // so if the first length field is garbled, it reads the second batch
    // of addr misaligned by 3 bytes.
    if (IsIPv6() && memcmp(m_addr.data(), IPV4_IN_IPV6_PREFIX.data() + 3, IPV4_IN_IPV6_PREFIX.size() - 3) == 0)
        return false;

    // unspecified IPv6 address (::/128)
    unsigned char ipNone[16] = {};
    if (IsIPv6() && memcmp(m_addr.data(), ipNone, sizeof(ipNone)) == 0)
        return false;

    // documentation IPv6 address
    if (IsRFC3849())
        return false;

    if (IsInternal())
        return false;

    if (IsCJDNS() && !HasCJDNSPrefix())
        return false;

    if (IsIPv4()) {
        const uint32_t addr = ReadBE32(m_addr.data());
        if (addr == INADDR_ANY || addr == INADDR_NONE) {
            return false;
        }
    }

    return true;
}

bool CNetAddr::IsRoutable() const
{
        return IsValid() && !(IsRFC1918() || IsRFC2544() || IsRFC3927() || IsRFC4862() || IsRFC6598() || IsRFC5737() || (IsRFC4193() && !IsTor()) || IsRFC4843() || IsLocal() || IsInternal());
}

bool CNetAddr::IsInternal() const
{
   return m_net == NET_INTERNAL;
}

bool CNetAddr::IsAddrV1Compatible() const
{
    switch (m_net) {
    case NET_IPV4:
    case NET_IPV6:
    case NET_INTERNAL:
        return true;
    case NET_TOR:
        // NET_TOR covers both Tor v2 (10 bytes, fits OnionCat) and Tor v3 (32 bytes, does not).
        return m_addr.size() == ADDR_TORV2_SIZE;
    case NET_CJDNS:
        return false;
    case NET_UNROUTABLE: // m_net is never and should not be set to NET_UNROUTABLE
    case NET_MAX:        // m_net is never and should not be set to NET_MAX
        assert(false); // LINT-OK-ASSERT: m_net is never set to UNROUTABLE/MAX
    } // no default case, so the compiler can warn about missing cases

    assert(false); // LINT-OK-ASSERT: unreachable, the switch above covers every enum value
}

enum Network CNetAddr::GetNetwork() const
{
    if (IsInternal())
        return NET_INTERNAL;

    if (!IsRoutable())
        return NET_UNROUTABLE;

    return m_net;
}

static std::string IPv6ToString(Span<const uint8_t> a)
{
    assert(a.size() == ADDR_IPV6_SIZE); // LINT-OK-ASSERT: callers only pass a 16-byte IPv6 or CJDNS m_addr
    // clang-format off
    return strprintf("%x:%x:%x:%x:%x:%x:%x:%x",
                     ReadBE16(&a[0]),
                     ReadBE16(&a[2]),
                     ReadBE16(&a[4]),
                     ReadBE16(&a[6]),
                     ReadBE16(&a[8]),
                     ReadBE16(&a[10]),
                     ReadBE16(&a[12]),
                     ReadBE16(&a[14]));
    // clang-format on
}

std::string CNetAddr::ToStringIP() const
{
    switch (m_net) {
    case NET_IPV4:
    case NET_IPV6: {
        CService serv(*this, 0);
        struct sockaddr_storage sockaddr;
        socklen_t socklen = sizeof(sockaddr);
        if (serv.GetSockAddr((struct sockaddr*)&sockaddr, &socklen)) {
            char name[1025] = "";
            if (!getnameinfo((const struct sockaddr*)&sockaddr, socklen, name,
                             sizeof(name), nullptr, 0, NI_NUMERICHOST))
                return std::string(name);
        }
        if (m_net == NET_IPV4) {
            return strprintf("%u.%u.%u.%u", m_addr[0], m_addr[1], m_addr[2], m_addr[3]);
        }
        return IPv6ToString(Span<const uint8_t>{m_addr.data(), m_addr.size()});
    }
    case NET_TOR:
        switch (m_addr.size()) {
        case ADDR_TORV2_SIZE:
            return EncodeBase32(m_addr.data(), m_addr.size()) + ".onion";
        case ADDR_TORV3_SIZE: {

            uint8_t checksum[torv3::CHECKSUM_LEN];
            torv3::Checksum(Span<const uint8_t>{m_addr.data(), m_addr.size()}, checksum);

            // TORv3 onion_address = base32(PUBKEY | CHECKSUM | VERSION) + ".onion"
            prevector<torv3::TOTAL_LEN, uint8_t> address{m_addr.begin(), m_addr.end()};
            address.insert(address.end(), checksum, checksum + torv3::CHECKSUM_LEN);
            address.insert(address.end(), torv3::VERSION, torv3::VERSION + sizeof(torv3::VERSION));

            return EncodeBase32(address.data(), address.size()) + ".onion";
        }
        default:
            assert(false); // LINT-OK-ASSERT: NET_TOR m_addr is only ever set to the TORv2 or TORv3 size
        }
    case NET_CJDNS:
        return IPv6ToString(Span<const uint8_t>{m_addr.data(), m_addr.size()});
    case NET_INTERNAL:
        return EncodeBase32(m_addr.data(), m_addr.size()) + ".internal";
    case NET_UNROUTABLE: // m_net is never and should not be set to NET_UNROUTABLE
    case NET_MAX:        // m_net is never and should not be set to NET_MAX
        assert(false); // LINT-OK-ASSERT: m_net is never set to UNROUTABLE/MAX
    } // no default case, so the compiler can warn about missing cases

    assert(false); // LINT-OK-ASSERT: unreachable, the switch above covers every enum value
}

std::string CNetAddr::ToString() const
{
    return ToStringIP();
}

bool operator==(const CNetAddr& a, const CNetAddr& b)
{
    return a.m_net == b.m_net && a.m_addr == b.m_addr;
}

bool operator!=(const CNetAddr& a, const CNetAddr& b)
{
    return !(a == b);
}

bool operator<(const CNetAddr& a, const CNetAddr& b)
{
    return std::tie(a.m_net, a.m_addr) < std::tie(b.m_net, b.m_addr);
}

bool CNetAddr::GetInAddr(struct in_addr* pipv4Addr) const
{
    if (!IsIPv4())
        return false;
    assert(sizeof(*pipv4Addr) == m_addr.size()); // LINT-OK-ASSERT: IsIPv4() implies a 4-byte m_addr
    memcpy(pipv4Addr, m_addr.data(), m_addr.size());
    return true;
}

bool CNetAddr::GetIn6Addr(struct in6_addr* pipv6Addr) const
{
    if (!IsIPv6() && !IsCJDNS()) {
        return false;
    }
    assert(sizeof(*pipv6Addr) == m_addr.size()); // LINT-OK-ASSERT: IsIPv6() implies a 16-byte m_addr
    memcpy(pipv6Addr, m_addr.data(), m_addr.size());
    return true;
}

// get canonical identifier of an address' group
// no two connections will be attempted to addresses with the same group
std::vector<unsigned char> CNetAddr::GetGroup() const
{
    std::vector<unsigned char> vchRet;
    int nClass = NET_IPV6;
    int nStartByte = 0;
    int nBits = 16;

    // all local addresses belong to the same group
    if (IsLocal())
    {
        nClass = 255;
        nBits = 0;
    }
    // all internal-usage addresses get their own group
    if (IsInternal())
    {
        nClass = NET_INTERNAL;
        nStartByte = 0;
        nBits = ADDR_INTERNAL_SIZE * 8;
    }
    // all other unroutable addresses belong to the same group
    else if (!IsRoutable())
    {
        nClass = NET_UNROUTABLE;
        nBits = 0;
    }
    // for IPv4 addresses, '1' + the 16 higher-order bits of the IP
    // includes mapped IPv4, SIIT translated IPv4, and the well-known prefix
    else if (IsIPv4() || IsRFC6145() || IsRFC6052())
    {
        nClass = NET_IPV4;
        nStartByte = IsIPv4() ? 0 : 12;
    }
    // for 6to4 tunnelled addresses, use the encapsulated IPv4 address
    else if (IsRFC3964())
    {
        nClass = NET_IPV4;
        nStartByte = 2;
    }
    // for Teredo-tunnelled IPv6 addresses, use the encapsulated IPv4 address
    else if (IsRFC4380())
    {
        vchRet.push_back(NET_IPV4);
        vchRet.push_back(m_addr[12] ^ 0xFF);
        vchRet.push_back(m_addr[13] ^ 0xFF);
        return vchRet;
    }
    else if (IsTor())
    {
        nClass = NET_TOR;
        nStartByte = 0;
        nBits = 4;
    }
    else if (IsCJDNS())
    {
        // Treat in the same way as Tor because the address is "random" bytes
        // (derived from a public key). However in CJDNS the first byte is a
        // constant 0xfc, so the random bytes come after it. Thus skip the
        // constant 8 bits at the start.
        nClass = NET_CJDNS;
        nStartByte = 0;
        nBits = 12;
    }
    // for he.net, use /36 groups
    else if (IsIPv6() && HasPrefix(m_addr, std::array<uint8_t, 4>{0x20, 0x01, 0x04, 0x70}))
        nBits = 36;
    // for the rest of the IPv6 network, use /32 groups
    else
        nBits = 32;

    vchRet.push_back(nClass);
    while (nBits >= 8)
    {
        vchRet.push_back(m_addr[nStartByte]);
        nStartByte++;
        nBits -= 8;
    }
    if (nBits > 0)
        vchRet.push_back(m_addr[nStartByte] | ((1 << (8 - nBits)) - 1));

    return vchRet;
}

std::vector<unsigned char> CNetAddr::GetAddrBytes() const
{
    if (IsAddrV1Compatible()) {
        uint8_t serialized[V1_SERIALIZATION_SIZE];
        SerializeV1Array(serialized);
        return {std::begin(serialized), std::end(serialized)};
    }
    return std::vector<unsigned char>(m_addr.begin(), m_addr.end());
}

uint64_t CNetAddr::GetHash() const
{
    uint256 hash = Hash(m_addr);
    uint64_t result;
    memcpy(&result, &hash, sizeof(result));
    return result;
}

// private extensions to enum Network, only returned by GetExtNetwork,
// and only used in GetReachabilityFrom
static const int NET_UNKNOWN = NET_MAX + 0;
static const int NET_TEREDO  = NET_MAX + 1;
int static GetExtNetwork(const CNetAddr *addr)
{
    if (addr == nullptr)
        return NET_UNKNOWN;
    if (addr->IsRFC4380())
        return NET_TEREDO;
    return addr->GetNetwork();
}

/** Calculates a metric for how reachable (*this) is from a given partner */
int CNetAddr::GetReachabilityFrom(const CNetAddr *paddrPartner) const
{
    enum Reachability {
        REACH_UNREACHABLE,
        REACH_DEFAULT,
        REACH_TEREDO,
        REACH_IPV6_WEAK,
        REACH_IPV4,
        REACH_IPV6_STRONG,
        REACH_PRIVATE
    };

    if (!IsRoutable() || IsInternal())
        return REACH_UNREACHABLE;

    int ourNet = GetExtNetwork(this);
    int theirNet = GetExtNetwork(paddrPartner);
    bool fTunnel = IsRFC3964() || IsRFC6052() || IsRFC6145();

    switch(theirNet) {
    case NET_IPV4:
        switch(ourNet) {
        default:       return REACH_DEFAULT;
        case NET_IPV4: return REACH_IPV4;
        }
    case NET_IPV6:
        switch(ourNet) {
        default:         return REACH_DEFAULT;
        case NET_TEREDO: return REACH_TEREDO;
        case NET_IPV4:   return REACH_IPV4;
        case NET_IPV6:   return fTunnel ? REACH_IPV6_WEAK : REACH_IPV6_STRONG; // only prefer giving our IPv6 address if it's not tunnelled
        }
    case NET_TOR:
        switch(ourNet) {
        default:         return REACH_DEFAULT;
        case NET_IPV4:   return REACH_IPV4; // Tor users can connect to IPv4 as well
        case NET_TOR:    return REACH_PRIVATE;
        }
    case NET_CJDNS:
        switch(ourNet) {
        default:         return REACH_DEFAULT;
        case NET_CJDNS:  return REACH_PRIVATE;
        }
    case NET_TEREDO:
        switch(ourNet) {
        default:          return REACH_DEFAULT;
        case NET_TEREDO:  return REACH_TEREDO;
        case NET_IPV6:    return REACH_IPV6_WEAK;
        case NET_IPV4:    return REACH_IPV4;
        }
    case NET_UNKNOWN:
    case NET_UNROUTABLE:
    default:
        switch(ourNet) {
        default:          return REACH_DEFAULT;
        case NET_TEREDO:  return REACH_TEREDO;
        case NET_IPV6:    return REACH_IPV6_WEAK;
        case NET_IPV4:    return REACH_IPV4;
        case NET_TOR:     return REACH_PRIVATE; // either from Tor, or don't care about our address
        }
    }
}

void CService::Init()
{
    port = 0;
}

CService::CService()
{
    Init();
}

CService::CService(const CNetAddr& cip, unsigned short portIn) : CNetAddr(cip), port(portIn)
{
}

CService::CService(const struct in_addr& ipv4Addr, unsigned short portIn) : CNetAddr(ipv4Addr), port(portIn)
{
}

CService::CService(const struct in6_addr& ipv6Addr, unsigned short portIn) : CNetAddr(ipv6Addr), port(portIn)
{
}

CService::CService(const struct sockaddr_in& addr) : CNetAddr(addr.sin_addr), port(ntohs(addr.sin_port))
{
    assert(addr.sin_family == AF_INET);
}

CService::CService(const struct sockaddr_in6 &addr) : CNetAddr(addr.sin6_addr), port(ntohs(addr.sin6_port))
{
   assert(addr.sin6_family == AF_INET6);
}

bool CService::SetSockAddr(const struct sockaddr *paddr)
{
    switch (paddr->sa_family) {
    case AF_INET:
        *this = CService(*(const struct sockaddr_in*)paddr);
        return true;
    case AF_INET6:
        *this = CService(*(const struct sockaddr_in6*)paddr);
        return true;
    default:
        return false;
    }
}

unsigned short CService::GetPort() const
{
    return port;
}

bool operator==(const CService& a, const CService& b)
{
    return (CNetAddr)a == (CNetAddr)b && a.port == b.port;
}

bool operator!=(const CService& a, const CService& b)
{
    return (CNetAddr)a != (CNetAddr)b || a.port != b.port;
}

bool operator<(const CService& a, const CService& b)
{
    return (CNetAddr)a < (CNetAddr)b || ((CNetAddr)a == (CNetAddr)b && a.port < b.port);
}

bool CService::GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const
{
    if (IsIPv4()) {
        if (*addrlen < (socklen_t)sizeof(struct sockaddr_in))
            return false;
        *addrlen = sizeof(struct sockaddr_in);
        struct sockaddr_in *paddrin = (struct sockaddr_in*)paddr;
        memset(paddrin, 0, *addrlen);
        if (!GetInAddr(&paddrin->sin_addr))
            return false;
        paddrin->sin_family = AF_INET;
        paddrin->sin_port = htons(port);
        return true;
    }
    if (IsIPv6() || IsCJDNS()) {
        if (*addrlen < (socklen_t)sizeof(struct sockaddr_in6))
            return false;
        *addrlen = sizeof(struct sockaddr_in6);
        struct sockaddr_in6 *paddrin6 = (struct sockaddr_in6*)paddr;
        memset(paddrin6, 0, *addrlen);
        if (!GetIn6Addr(&paddrin6->sin6_addr))
            return false;
        paddrin6->sin6_family = AF_INET6;
        paddrin6->sin6_port = htons(port);
        return true;
    }
    return false;
}

std::vector<unsigned char> CService::GetKey() const
{
    std::vector<unsigned char> key = GetAddrBytes();
    key.push_back(port >> 8);
    key.push_back(port & 0xFF);
    return key;
}

std::string CService::ToStringPort() const
{
    return strprintf("%u", port);
}

std::string CService::ToStringIPPort() const
{
    if (IsIPv4() || IsTor() || IsInternal()) {
        return ToStringIP() + ":" + ToStringPort();
    } else {
        return "[" + ToStringIP() + "]:" + ToStringPort();
    }
}

std::string CService::ToString() const
{
    return ToStringIPPort();
}

void CService::SetPort(unsigned short portIn)
{
    port = portIn;
}

CSubNet::CSubNet():
    valid(false)
{
    memset(netmask, 0, sizeof(netmask));
}

CSubNet::CSubNet(const CNetAddr& addr, uint8_t mask) : CSubNet()
{
    valid = (addr.IsIPv4() && mask <= ADDR_IPV4_SIZE * 8) ||
            (addr.IsIPv6() && mask <= ADDR_IPV6_SIZE * 8);
    if (!valid) {
        return;
    }

    assert(mask <= sizeof(netmask) * 8); // LINT-OK-ASSERT: mask was bounded by the address size just above

    network = addr;
    if (network.IsIPv4()) {
        std::fill(netmask + ADDR_IPV4_SIZE, std::end(netmask), 0xFF);
    }

    uint8_t n = mask;
    for (size_t i = 0; i < network.m_addr.size(); ++i) {
        const uint8_t bits = n < 8 ? n : 8;
        netmask[i] = (uint8_t)((uint8_t)0xFF << (8 - bits)); // Set first bits.
        network.m_addr[i] &= netmask[i]; // Normalize network according to netmask.
        n -= bits;
    }
}

/**
 * @returns The number of 1-bits in the prefix of the specified subnet mask. If
 *          the specified subnet mask is not a valid one, -1.
 */
static inline int NetmaskBits(uint8_t x)
{
    switch(x) {
    case 0x00: return 0;
    case 0x80: return 1;
    case 0xc0: return 2;
    case 0xe0: return 3;
    case 0xf0: return 4;
    case 0xf8: return 5;
    case 0xfc: return 6;
    case 0xfe: return 7;
    case 0xff: return 8;
    default: return -1;
    }
}

CSubNet::CSubNet(const CNetAddr& addr, const CNetAddr& mask) : CSubNet()
{
    valid = (addr.IsIPv4() || addr.IsIPv6()) && addr.m_net == mask.m_net;
    if (!valid) {
        return;
    }
    // Check if `mask` contains 1-bits after 0-bits (which is an invalid netmask).
    bool zeros_found = false;
    for (auto b : mask.m_addr) {
        const int num_bits = NetmaskBits(b);
        if (num_bits == -1 || (zeros_found && num_bits != 0)) {
            valid = false;
            return;
        }
        if (num_bits < 8) {
            zeros_found = true;
        }
    }

    assert(mask.m_addr.size() <= sizeof(netmask)); // LINT-OK-ASSERT: mask is IPv4 or IPv6, at most 16 bytes

    memcpy(netmask, mask.m_addr.data(), mask.m_addr.size());

    network = addr;
    if (network.IsIPv4()) {
        std::fill(netmask + ADDR_IPV4_SIZE, std::end(netmask), 0xFF);
    }

    // Normalize network according to netmask
    for (size_t x = 0; x < network.m_addr.size(); ++x) {
        network.m_addr[x] &= netmask[x];
    }
}

CSubNet::CSubNet(const CNetAddr& addr) : CSubNet()
{
    // Gridcoin BanMan uses single-address subnets for Tor host bans.
    // Retain v2 host bans; v3 cannot fit the legacy subnet mask.
    // CIDR and explicit netmasks remain IP-only.
    valid = addr.IsIPv4() || addr.IsIPv6() ||
            (addr.IsTor() && addr.m_addr.size() == ADDR_TORV2_SIZE);
    if (!valid) {
        return;
    }

    assert(addr.m_addr.size() <= sizeof(netmask)); // LINT-OK-ASSERT: addr is IPv4, IPv6 or TORv2 here, at most 16 bytes

    memset(netmask, 0xFF, sizeof(netmask));

    network = addr;
    if (network.IsIPv4()) {
        std::fill(netmask + ADDR_IPV4_SIZE, std::end(netmask), 0xFF);
    }
}

/**
 * @returns True if this subnet is valid, the specified address is valid, and
 *          the specified address belongs in this subnet.
 */
bool CSubNet::Match(const CNetAddr &addr) const
{
    if (!valid || !addr.IsValid() || network.m_net != addr.m_net ||
        network.m_addr.size() != addr.m_addr.size() ||
        addr.m_addr.size() > sizeof(netmask))
        return false;
    for (size_t x = 0; x < addr.m_addr.size(); ++x) {
        if ((addr.m_addr[x] & netmask[x]) != network.m_addr[x]) {
            return false;
        }
    }
    return true;
}

std::string CSubNet::ToString() const
{
    uint8_t legacy_mask[ADDR_IPV6_SIZE];
    GetLegacyNetmask(legacy_mask);

    /* Parse binary 1{n}0{N-n} to see if mask can be represented as /n */
    int cidr = 0;
    bool valid_cidr = true;
    int n = network.IsIPv4() ? 12 : 0;
    for (; n < 16 && legacy_mask[n] == 0xff; ++n)
        cidr += 8;
    if (n < 16) {
        int bits = NetmaskBits(legacy_mask[n]);
        if (bits < 0)
            valid_cidr = false;
        else
            cidr += bits;
        ++n;
    }
    for (; n < 16 && valid_cidr; ++n)
        if (legacy_mask[n] != 0x00)
            valid_cidr = false;

    /* Format output */
    std::string strNetmask;
    if (valid_cidr) {
        strNetmask = strprintf("%u", cidr);
    } else {
        if (network.IsIPv4())
            strNetmask = strprintf("%u.%u.%u.%u", legacy_mask[12], legacy_mask[13], legacy_mask[14], legacy_mask[15]);
        else
            strNetmask = strprintf("%x:%x:%x:%x:%x:%x:%x:%x",
                             legacy_mask[0] << 8 | legacy_mask[1], legacy_mask[2] << 8 | legacy_mask[3],
                             legacy_mask[4] << 8 | legacy_mask[5], legacy_mask[6] << 8 | legacy_mask[7],
                             legacy_mask[8] << 8 | legacy_mask[9], legacy_mask[10] << 8 | legacy_mask[11],
                             legacy_mask[12] << 8 | legacy_mask[13], legacy_mask[14] << 8 | legacy_mask[15]);
    }

    return network.ToString() + "/" + strNetmask;
}

bool CSubNet::IsValid() const
{
    return valid;
}

bool operator==(const CSubNet& a, const CSubNet& b)
{
    return a.valid == b.valid && a.network == b.network && !memcmp(a.netmask, b.netmask, 16);
}

bool operator!=(const CSubNet& a, const CSubNet& b)
{
    return !(a==b);
}

bool operator<(const CSubNet& a, const CSubNet& b)
{
    return (a.network < b.network || (a.network == b.network && memcmp(a.netmask, b.netmask, 16) < 0));
}