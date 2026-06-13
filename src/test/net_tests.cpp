// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include "addrman.h"
#include "addrman_impl.h"
#include <string>
#include <boost/test/unit_test.hpp>
#include "hash.h"
#include "serialize.h"
#include "streams.h"
#include "net.h"
#include "netbase.h"
#include "addrdb.h"
#include "chainparams.h"
#include "clientversion.h"

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

BOOST_AUTO_TEST_SUITE_END()
