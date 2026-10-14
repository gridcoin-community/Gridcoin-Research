// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include "addrman.h"
#include "addrman_impl.h"
#include <string>
#include <fstream>
#include <iterator>
#include "util/strencodings.h"
#include "test/data/netaddress_19534_peers.bin.h"
#include "test/data/netaddress_19534_banlist.bin.h"
#include <boost/test/unit_test.hpp>
#include "hash.h"
#include "serialize.h"
#include "streams.h"
#include "net.h"
#include "net_processing.h"
#include "netbase.h"
#include "addrdb.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "primitives/block.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(net_tests)

// Serialize a (deterministic) AddrMan into a peers.dat-style stream, prefixed
// with the network message-start bytes (issue #2558 PR 6b: the test no longer
// subclasses AddrMan; it uses the public interface + a deterministic AddrMan).
static CDataStream AddrmanToStream(const AddrMan& addrman)
{
    CDataStream ssPeersIn(SER_DISK, CLIENT_VERSION);
    ssPeersIn << Params().MessageStart();
    ssPeersIn << addrman;
    std::string str = ssPeersIn.str();
    vector<unsigned char> vchData(str.begin(), str.end());
    return CDataStream(vchData, SER_DISK, CLIENT_VERSION);
}

// Build a deliberately corrupt peers.dat-style stream: it claims addrman has
// 10 "new" and 10 "tried" entries but contains only one addr. Byte-identical to
// the pre-pimpl AddrManCorrupted serialization mock (null key included).
static CDataStream MakeCorruptAddrmanStream()
{
    CDataStream ssPeersIn(SER_DISK, CLIENT_VERSION);
    ssPeersIn << Params().MessageStart();

    unsigned char nVersion = 1;
    ssPeersIn << nVersion;
    ssPeersIn << ((unsigned char)32);
    uint256 nKey; // null key (the old mock was MakeDeterministic'd to a null key)
    ssPeersIn << nKey;
    ssPeersIn << 10; // nNew
    ssPeersIn << 10; // nTried

    int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
    ssPeersIn << nUBuckets;

    CService serv;
    Lookup("252.1.1.1", serv, 7777, false);
    CAddress addr = CAddress(serv, NODE_NONE);
    CNetAddr resolved;
    LookupHost("252.2.2.2", resolved, false);
    AddrInfo info = AddrInfo(addr, resolved);
    ssPeersIn << info;

    std::string str = ssPeersIn.str();
    vector<unsigned char> vchData(str.begin(), str.end());
    return CDataStream(vchData, SER_DISK, CLIENT_VERSION);
}

BOOST_AUTO_TEST_CASE(caddrdb_read)
{
    AddrMan addrmanUncorrupted{/*deterministic=*/true};

    CService addr1, addr2, addr3;
    Lookup("250.7.1.1", addr1, 8333, false);
    Lookup("250.7.2.2", addr2, 9999, false);
    Lookup("250.7.3.3", addr3, 9999, false);

    // Add three addresses to new table.
    CService source;
    Lookup("252.5.1.1", source, 8333, false);
    addrmanUncorrupted.Add(CAddress(addr1, NODE_NONE), source);
    addrmanUncorrupted.Add(CAddress(addr2, NODE_NONE), source);
    addrmanUncorrupted.Add(CAddress(addr3, NODE_NONE), source);

    // Test that the de-serialization does not throw an exception.
    CDataStream ssPeers1 = AddrmanToStream(addrmanUncorrupted);
    bool exceptionThrown = false;
    AddrMan addrman1;

    BOOST_CHECK(addrman1.size() == 0);
    try {
        unsigned char pchMsgTmp[4];
        ssPeers1 >> pchMsgTmp;
        ssPeers1 >> addrman1;
    } catch (const std::exception& e) {
        exceptionThrown = true;
    }

    BOOST_CHECK(addrman1.size() == 3);
    BOOST_CHECK(exceptionThrown == false);

    // Test that CAddrDB::Read creates an addrman with the correct number of addrs.
    CDataStream ssPeers2 = AddrmanToStream(addrmanUncorrupted);

    AddrMan addrman2;
    CAddrDB adb;
    BOOST_CHECK(addrman2.size() == 0);
    adb.Read(addrman2, ssPeers2);
    BOOST_CHECK(addrman2.size() == 3);
}


BOOST_AUTO_TEST_CASE(caddrdb_read_corrupted)
{
    // Test that the de-serialization of corrupted addrman throws an exception.
    CDataStream ssPeers1 = MakeCorruptAddrmanStream();
    bool exceptionThrown = false;
    AddrMan addrman1;
    BOOST_CHECK(addrman1.size() == 0);
    try {
        unsigned char pchMsgTmp[4];
        ssPeers1 >> pchMsgTmp;
        ssPeers1 >> addrman1;
    } catch (const std::exception& e) {
        exceptionThrown = true;
    }
    // Even through de-serialization failed addrman is not left in a clean state.
    BOOST_CHECK(addrman1.size() == 1);
    BOOST_CHECK(exceptionThrown);

    // Test that CAddrDB::Read leaves addrman in a clean state if de-serialization fails.
    CDataStream ssPeers2 = MakeCorruptAddrmanStream();

    AddrMan addrman2;
    CAddrDB adb;
    BOOST_CHECK(addrman2.size() == 0);
    adb.Read(addrman2, ssPeers2);
    BOOST_CHECK(addrman2.size() == 0);
}

// The PeerManager tip-relay handler (issue #3125 C8) must no-op safely on a
// quiescent node: a null tip (defensive guard) and a non-best tip -- the
// best-chain gate that suppresses relay when SetBestChain's trust-regression
// path reorganizes back to the previous tip yet still emits UpdatedBlockTip
// with the losing block. The fixture's g_connman has zero nodes, so a passing
// gate would also no-op; what this exercises is the gate/guard path itself
// under the handler's cs_main lock annotation.
BOOST_AUTO_TEST_CASE(peerman_updatedblocktip_relay_gates)
{
    BOOST_REQUIRE(g_peerman);

    LOCK(cs_main);

    g_peerman->UpdatedBlockTip(nullptr, nullptr, false);

    const uint256 not_best_hash = uint256S("0xdeadbeef");
    CBlockIndex index;
    index.phashBlock = &not_best_hash;
    BOOST_REQUIRE(index.GetBlockHash() != hashBestChain);

    g_peerman->UpdatedBlockTip(&index, nullptr, false);
}

//! DeferredRelay exists so an announcement can be decided while cs_wallet is
//! held and made after it unlocks (#3391). The lock ordering itself is not
//! reachable from a unit test; the bookkeeping that makes it safe is.
//!
//! Flushing must empty the queue rather than merely walk it, because the
//! destructor flushes as well: a caller that flushes explicitly -- as
//! ResendWalletTransactions does, to announce before its erase pass retakes
//! cs_wallet -- would otherwise announce everything a second time on scope exit.
BOOST_AUTO_TEST_CASE(deferred_relay_empties_on_flush_and_is_idempotent)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256S("0x01"), 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 1 * COIN;
    const CTransaction tx(mtx);

    DeferredRelay relay;
    BOOST_CHECK(relay.empty());

    relay.AddTransaction(tx, tx.GetHash());
    BOOST_CHECK(!relay.empty());

    relay.Flush();
    BOOST_CHECK(relay.empty());

    // Flush() announces, which caches the serialized transaction in the
    // process-global mapRelay. This suite has no state guard, so drop it again
    // rather than leave it available to a later getdata case.
    RemoveFromRelayMemory(tx.GetHash());

    // The second flush stands in for the destructor running after the explicit
    // one above.
    relay.Flush();
    BOOST_CHECK(relay.empty());

    relay.AddPSGT(uint256S("0x02"));
    BOOST_CHECK(!relay.empty());

    relay.Flush();
    BOOST_CHECK(relay.empty());
}



// These files were generated by the fixed-size #19534 implementation before
// this port. They include mainnet magic and checksum, and pin the disk format
// independently of the new serializer.
static std::vector<unsigned char> ReadLegacyFile(const fs::path& path)
{
    std::ifstream file(path.string(), std::ios::binary);
    BOOST_REQUIRE(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static void WriteLegacyFile(const fs::path& path, const std::vector<unsigned char>& bytes)
{
    std::ofstream file(path.string(), std::ios::binary | std::ios::trunc);
    BOOST_REQUIRE(file.is_open());
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    BOOST_REQUIRE(file.good());
}

static void PopulateLegacyPeers(AddrMan& peers)
{
    CNetAddr source;
    BOOST_REQUIRE(source.SetInternal("baseline-seed"));
    for (const char* name : {"250.7.1.1", "2001:470:abcd::1", "5wyqrzbvrdsumnok.onion"}) {
        CService service;
        BOOST_REQUIRE(Lookup(name, service, 32749, false));
        CAddress address(service, NODE_NETWORK);
        address.nTime = 1700000000;
        BOOST_REQUIRE(peers.Add(address, source));
    }
    CService tried;
    BOOST_REQUIRE(Lookup("250.7.1.1", tried, 32749, false));
    peers.Good(tried, 1700000001);
}

BOOST_AUTO_TEST_CASE(cnetaddr_legacy_wire)
{
    struct Example { const char* name; const char* hex; };
    const Example examples[] = {
        {"1.2.3.4", "00000000000000000000ffff01020304"},
        {"1122:3344:5566:7788:9900:aabb:ccdd:eeff", "11223344556677889900aabbccddeeff"},
        {"5wyqrzbvrdsumnok.onion", "fd87d87eeb43edb108e43588e54635ca"},
        {nullptr, "fd6b88c08724ca978112ca1bbdcafac2"},
        {"0.0.0.0", "00000000000000000000ffff00000000"},
        {"255.255.255.255", "00000000000000000000ffffffffffff"},
        {"::", "00000000000000000000000000000000"},
    };
    CNetAddr reused;
    for (const auto& example : examples) {
        CNetAddr addr;
        if (example.name) {
            BOOST_REQUIRE(LookupHost(example.name, addr, false));
        } else {
            BOOST_REQUIRE(addr.SetInternal("a"));
        }
        CNetAddr copied;
        copied.SetIP(addr);
        BOOST_CHECK(copied == addr);
        for (int type : {SER_NETWORK, SER_DISK}) {
            CDataStream stream(type, PROTOCOL_VERSION);
            stream << addr;
            BOOST_CHECK_EQUAL(stream.size(), 16U);
            BOOST_CHECK_EQUAL(HexStr(stream), example.hex);
            BOOST_CHECK_EQUAL(HexStr(addr.GetAddrBytes()), example.hex);
            stream >> reused;
            BOOST_CHECK(reused == addr);
            BOOST_CHECK_EQUAL(reused.GetNetwork(), addr.GetNetwork());
            BOOST_CHECK_EQUAL(reused.ToString(), addr.ToString());
            BOOST_CHECK(stream.empty());

            CService service(addr, 32749);
            stream << service;
            BOOST_CHECK_EQUAL(stream.size(), 18U);
            BOOST_CHECK_EQUAL(HexStr(stream), std::string(example.hex) + "7fed");
            BOOST_CHECK_EQUAL(HexStr(service.GetKey()), std::string(example.hex) + "7fed");
            CService restored(reused, 1);
            stream >> restored;
            BOOST_CHECK(restored == service);
            BOOST_CHECK_EQUAL(restored.GetPort(), 32749);
            BOOST_CHECK(stream.empty());
        }
    }
    reused.Init();
    BOOST_CHECK(!reused.IsValid());
    BOOST_CHECK(reused.IsIPv6());
    CDataStream empty(SER_NETWORK, PROTOCOL_VERSION);
    empty << reused;
    BOOST_CHECK_EQUAL(HexStr(empty), "00000000000000000000000000000000");
}

BOOST_AUTO_TEST_CASE(cnetaddr_network_specific_properties)
{
    CNetAddr addr;
    BOOST_REQUIRE(LookupHost("239.1.2.3", addr, false));
    BOOST_CHECK(addr.IsMulticast());
    BOOST_REQUIRE(LookupHost("ff02::1", addr, false));
    BOOST_CHECK(addr.IsMulticast());
    BOOST_REQUIRE(addr.SetSpecial("7777777777777777.onion"));
    BOOST_CHECK(addr.IsTor());
    BOOST_CHECK(!addr.IsMulticast());
    BOOST_CHECK_EQUAL(addr.ToString(), "7777777777777777.onion");
    BOOST_REQUIRE(addr.SetInternal("esffpp"));
    BOOST_CHECK(addr.IsInternal());
    BOOST_CHECK(!addr.IsValid());
    BOOST_CHECK_EQUAL(addr.ToString(), "esffpvrt3wpeaygy.internal");

    struct in_addr ipv4;
    struct in6_addr ipv6;
    BOOST_REQUIRE(LookupHost("12.34.56.78", addr, false));
    BOOST_REQUIRE(addr.GetInAddr(&ipv4));
    BOOST_CHECK(CNetAddr(ipv4) == addr);
    BOOST_CHECK(!addr.GetIn6Addr(&ipv6));
    BOOST_REQUIRE(LookupHost("1122:3344:5566:7788:9900:aabb:ccdd:eeff", addr, false));
    BOOST_REQUIRE(addr.GetIn6Addr(&ipv6));
    BOOST_CHECK(CNetAddr(ipv6) == addr);
    BOOST_CHECK(!addr.GetInAddr(&ipv4));
}

BOOST_AUTO_TEST_CASE(csubnet_legacy_wire)
{
    const char* names[] = {"1.2.3.4", "1122:3344:5566:7788:9900:aabb:ccdd:eeff"};
    const char* hex[] = {"00000000000000000000ffff01020304", "11223344556677889900aabbccddeeff"};
    for (size_t family = 0; family < 2; ++family) {
        CNetAddr addr;
        BOOST_REQUIRE(LookupHost(names[family], addr, false));
        const int offset = family == 0 ? 12 : 0;
        const int bits = family == 0 ? 32 : 128;
        for (int cidr = 0; cidr <= bits; ++cidr) {
            // Build the expected 16-byte format from the old prefix and CIDR
            // definition, rather than serializing the new in-memory mask.
            auto network = ParseHex(hex[family]);
            std::vector<unsigned char> mask(16, 0xFF);
            for (int bit = offset * 8 + cidr; bit < 128; ++bit) {
                mask[bit / 8] &= ~(1 << (7 - bit % 8));
            }
            for (size_t i = 0; i < network.size(); ++i) network[i] &= mask[i];
            std::vector<unsigned char> expected = network;
            expected.insert(expected.end(), mask.begin(), mask.end());
            expected.push_back(1);

            CSubNet subnet(addr, cidr);
            BOOST_REQUIRE(subnet.IsValid());
            for (int type : {SER_NETWORK, SER_DISK}) {
                CDataStream stream(type, PROTOCOL_VERSION);
                stream << subnet;
                BOOST_CHECK_EQUAL(HexStr(stream), HexStr(expected));
                CSubNet restored;
                stream >> restored;
                BOOST_CHECK(restored == subnet);
                BOOST_CHECK(restored.Match(addr));
                BOOST_CHECK_EQUAL(restored.ToString(), subnet.ToString());
                BOOST_CHECK(stream.empty());
            }
        }
        CSubNet host(addr);
        CSubNet fullmask(addr, bits);
        BOOST_CHECK(host == fullmask);
        CSubNet invalid(addr, bits + 1);
        BOOST_CHECK(!invalid.IsValid());
    }
    CSubNet invalid;
    CDataStream stream(SER_DISK, PROTOCOL_VERSION);
    stream << invalid;
    BOOST_CHECK_EQUAL(HexStr(stream), std::string(66, '0'));
}

BOOST_AUTO_TEST_CASE(caddrdb_19534_compatibility)
{
    const std::vector<unsigned char> fixture(std::begin(netaddress_19534_peers_bin),
                                             std::end(netaddress_19534_peers_bin));
    const fs::path path = GetDataDir() / "peers.dat";
    WriteLegacyFile(path, fixture);
    CAddrDB db;
    AddrMan restored{/*deterministic=*/true};
    BOOST_REQUIRE(db.Read(restored)); // Includes verification of the old checksum.
    BOOST_CHECK_EQUAL(restored.size(), 3U);
    for (const char* name : {"250.7.1.1", "2001:470:abcd::1", "5wyqrzbvrdsumnok.onion"}) {
        CService service;
        BOOST_REQUIRE(Lookup(name, service, 32749, false));
        const AddrInfo* info = restored.Find(service);
        BOOST_REQUIRE(info);
        BOOST_CHECK_EQUAL(info->ToString(), service.ToString());
        BOOST_CHECK_EQUAL(info->nTime, 1700000000);
    }
    BOOST_REQUIRE(db.Write(restored));
    const auto upgraded = ReadLegacyFile(path);
    BOOST_REQUIRE(upgraded.size() > 6U);
    BOOST_CHECK_EQUAL(upgraded[4], 3U);
    BOOST_CHECK_EQUAL(upgraded[5], 35U);
    BOOST_CHECK(upgraded != fixture);
    AddrMan reread{/*deterministic=*/true};
    BOOST_REQUIRE(db.Read(reread)); // Verifies the upgraded file checksum.
    BOOST_REQUIRE_EQUAL(reread.size(), restored.size());
    for (const char* name : {"250.7.1.1", "2001:470:abcd::1", "5wyqrzbvrdsumnok.onion"}) {
        CService service;
        BOOST_REQUIRE(Lookup(name, service, 32749, false));
        const AddrInfo* before = restored.Find(service);
        const AddrInfo* after = reread.Find(service);
        BOOST_REQUIRE(before && after);
        BOOST_CHECK(static_cast<const CAddress&>(*before) == static_cast<const CAddress&>(*after));
        CDataStream before_info(SER_DISK, CLIENT_VERSION | ADDRV2_FORMAT);
        CDataStream after_info(SER_DISK, CLIENT_VERSION | ADDRV2_FORMAT);
        before_info << *before;
        after_info << *after;
        BOOST_CHECK_EQUAL(HexStr(after_info), HexStr(before_info));
    }

    AddrMan fresh{/*deterministic=*/true};
    PopulateLegacyPeers(fresh);
    BOOST_REQUIRE(db.Write(fresh));
    BOOST_CHECK(ReadLegacyFile(path) == upgraded);

    // bitcoin#23077: CJDNS survives a checksummed peers.dat round trip.
    CDataStream cjdns_stream(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    cjdns_stream.write(MakeByteSpan(ParseHex("0610fc000001000200030004000500060007")));
    CNetAddr cjdns;
    cjdns_stream >> cjdns;
    BOOST_REQUIRE(cjdns.IsCJDNS());
    CAddress cjdns_address(CService(cjdns, 32749), NODE_NETWORK, 1700000000);
    BOOST_REQUIRE(fresh.Add(cjdns_address, CNetAddr(LookupNumeric("252.2.2.2", 0))));
    BOOST_REQUIRE(db.Write(fresh));
    AddrMan cjdns_restored{/*deterministic=*/true};
    BOOST_REQUIRE(db.Read(cjdns_restored));
    BOOST_REQUIRE_EQUAL(cjdns_restored.size(), 4U);
    const AddrInfo* cjdns_info = cjdns_restored.Find(cjdns);
    BOOST_REQUIRE(cjdns_info);
    BOOST_CHECK(static_cast<const CAddress&>(*cjdns_info) == cjdns_address);
    BOOST_CHECK(cjdns_info->IsCJDNS());
    BOOST_CHECK_EQUAL(cjdns_info->ToString(), "[fc00:1:2:3:4:5:6:7]:32749");

    auto corrupt = fixture;
    corrupt.back() ^= 1;
    WriteLegacyFile(path, corrupt);
    AddrMan damaged{/*deterministic=*/true};
    BOOST_CHECK(!db.Read(damaged));
    fs::remove(path);
}

BOOST_AUTO_TEST_CASE(cbandb_19534_compatibility)
{
    const std::vector<unsigned char> fixture(std::begin(netaddress_19534_banlist_bin),
                                             std::end(netaddress_19534_banlist_bin));
    const fs::path path = GetDataDir() / "legacy-banlist.dat";
    WriteLegacyFile(path, fixture);
    CBanDB db(path);
    banmap_t restored;
    BOOST_REQUIRE(db.Read(restored)); // Includes verification of the old checksum.
    BOOST_REQUIRE_EQUAL(restored.size(), 4U);
    BOOST_REQUIRE(db.Write(restored));
    BOOST_CHECK(ReadLegacyFile(path) == fixture);

    CNetAddr addr;
    BOOST_REQUIRE(LookupHost("1.2.3.4", addr, false));
    bool found_non_cidr = false, found_tor = false;
    for (const auto& ban : restored) {
        BOOST_CHECK_EQUAL(ban.second.nCreateTime, 1700000000);
        BOOST_CHECK_EQUAL(ban.second.nBanUntil, 1700003600);
        BOOST_CHECK_EQUAL(ban.second.banReason, BanReasonManuallyAdded);
        if (ban.first.ToString() == "1.2.0.0/255.255.232.0") {
            BOOST_CHECK(ban.first.Match(addr));
            found_non_cidr = true;
        } else if (ban.first.ToString() == "5wyqrzbvrdsumnok.onion/128") {
            CNetAddr tor;
            BOOST_REQUIRE(tor.SetSpecial("5wyqrzbvrdsumnok.onion"));
            BOOST_CHECK(ban.first.Match(tor));
            found_tor = true;
        }
    }
    BOOST_CHECK(found_non_cidr);
    BOOST_CHECK(found_tor);

    CNetAddr tor;
    BOOST_REQUIRE(tor.SetSpecial("5wyqrzbvrdsumnok.onion"));
    CSubNet tor_host(tor);
    BOOST_REQUIRE(tor_host.IsValid());
    const auto old_tor = restored.find(tor_host);
    BOOST_REQUIRE(old_tor != restored.end());
    BOOST_CHECK(tor_host.Match(tor));
    CDataStream tor_stream(SER_DISK, CLIENT_VERSION), legacy_tor_stream(SER_DISK, CLIENT_VERSION);
    tor_stream << tor_host;
    legacy_tor_stream << old_tor->first;
    BOOST_CHECK_EQUAL(HexStr(tor_stream), HexStr(legacy_tor_stream));

    // Newly constructed IP subnets must compare and serialize identically to
    // their legacy counterparts, not merely round-trip the old files.
    banmap_t fresh, canonical;
    for (const char* name : {"1.2.3.4/24", "2001:470:abcd::1/36"}) {
        CSubNet subnet;
        BOOST_REQUIRE(LookupSubNet(name, subnet));
        const auto old = restored.find(subnet);
        BOOST_REQUIRE(old != restored.end());
        fresh.emplace(subnet, old->second);
        canonical.insert(*old);
    }
    CDataStream fresh_stream(SER_DISK, CLIENT_VERSION), old_stream(SER_DISK, CLIENT_VERSION);
    fresh_stream << fresh;
    old_stream << canonical;
    BOOST_CHECK_EQUAL(HexStr(fresh_stream), HexStr(old_stream));

    auto corrupt = fixture;
    corrupt.back() ^= 1;
    WriteLegacyFile(path, corrupt);
    banmap_t damaged;
    BOOST_CHECK(!db.Read(damaged));
    fs::remove(path);
}

BOOST_AUTO_TEST_SUITE_END()
