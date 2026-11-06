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
#include "net_processing.h"
#include "netbase.h"
#include "addrdb.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "primitives/block.h"
#include "test/state_guard.h"
#include "util.h"
#include "util/time.h"
#include "version.h"

using namespace std;

// Defined in validation.cpp; the VERSION handler feeds it the peer's height.
extern CMedianFilter<int> cPeerBlockCounts GUARDED_BY(cs_main);

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

// A VERSION message as a peer sends it, header and checksum included, built as
// CNode::EndMessage() builds one, in the stream version a new connection
// starts with. The peer passes the handshake's other checks: a client (not
// NODE_NETWORK, so it is not asked for blocks) at height 1 (so -autoban lets
// it through), with a non-routable address and the nonce 1, which skips the
// check for a connection to ourselves.
static SerializeData VersionMessage(int64_t peer_time)
{
    CDataStream ss(SER_NETWORK, INIT_PROTO_VERSION);
    ss << CMessageHeader(NetMsgType::ARIES, 0);
    ss << PROTOCOL_VERSION << (uint64_t)NODE_NONE << peer_time << CAddress() << CAddress() << (uint64_t)1
       << std::string("/net_tests/") << 1;

    const unsigned int size = ss.size() - CMessageHeader::HEADER_SIZE;
    memcpy(&ss[CMessageHeader::MESSAGE_SIZE_OFFSET], &size, sizeof(size));
    const uint256 hash = Hash(Span{ss}.subspan(CMessageHeader::HEADER_SIZE));
    memcpy(&ss[CMessageHeader::CHECKSUM_OFFSET], &hash, 4);

    return SerializeData(ss.begin(), ss.end());
}

// A peer's time is compared with ours when its VERSION was received, not when
// the message handler gets to it: while a node catches up, a VERSION can wait
// behind a block backlog, and the wait must not count as a difference between
// the clocks. The peer's clock is 30 s ahead. The message is received, the
// clock moves on by the wait, and then the message is processed. The 600 s
// wait is longer than the 8-minute window; the peer is accepted because the
// check uses the receive time.
BOOST_AUTO_TEST_CASE(version_time_checks_use_the_receive_time)
{
    BOOST_REQUIRE(g_peerman);
    grc_test::StateGuard guard;
    const CMedianFilter<int> peer_block_counts = WITH_LOCK(cs_main, return cPeerBlockCounts);

    const int64_t received = 1700000000;

    for (const int64_t wait : {0, 300, 600}) {
        SetMockTime(received);
        CNode node(INVALID_SOCKET, CAddress(CService(LookupNumeric("127.0.0.1", 0)), NODE_NONE), "", true);
        const SerializeData message = VersionMessage(received + 30);

        LOCK(node.cs_vRecvMsg);
        BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(message.data())), message.size()));

        SetMockTime(received + wait);
        g_peerman->ProcessMessages(&node);

        // The handler sets nVersion only once the time check has passed. The
        // node cannot send its own VERSION back without a socket, so it is
        // marked for disconnection either way.
        BOOST_CHECK_MESSAGE(node.nVersion == PROTOCOL_VERSION, "wait " << wait << ": negotiated version " << node.nVersion.load());
        BOOST_CHECK_MESSAGE(node.nTimeOffset == 30, "wait " << wait << ": time offset " << node.nTimeOffset.load());
    }

    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
}

BOOST_AUTO_TEST_SUITE_END()
