// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include "addrman.h"
#include "addrman_impl.h"
#include <algorithm>
#include <optional>
#include <string>
#include <fstream>
#include <iterator>
#include "util/strencodings.h"
#include "test/data/netaddress_19534_peers.bin.h"
#include "test/data/netaddress_19534_banlist.bin.h"
#include <boost/test/unit_test.hpp>

#ifndef WIN32
#include <sys/stat.h>
#endif
#include "gridcoin/scraper/scraper_net.h"
#include "hash.h"
#include "serialize.h"
#include "streams.h"
#include "net.h"
#include "net_processing.h"
#include "netbase.h"
#include "node/psgt_pool.h"
#include "node/shutdown.h"
#include "addrdb.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "gridcoin/contract/contract.h"
#include "gridcoin/scraper/fwd.h"
#include "gridcoin/scraper/scraper_registry.h"
#include "key.h"
#include "key_io.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "protocol.h"
#include "test/state_guard.h"
#include "util.h"
#include "util/system.h"
#include "util/time.h"
#include "validation.h"
#include "version.h"

using namespace std;

// Defined in validation.cpp; the VERSION handler feeds it the peer's height.
extern CMedianFilter<int> cPeerBlockCounts GUARDED_BY(cs_main);

// Defined in scraper.cpp; the complete manifests in the manifest map by scraper.
extern mmCSManifestsBinnedByScraper BinCScraperManifestsByScraper()
    EXCLUSIVE_LOCKS_REQUIRED(CScraperManifest::cs_mapManifest);

BOOST_AUTO_TEST_SUITE(net_tests)

BOOST_AUTO_TEST_CASE(cjdns_invalid_prefix)
{
    // Bitcoin master: Invalid CJDNS, wrong prefix. BIP155 decoding succeeds,
    // preserving NET_CJDNS; IsValid(), rather than the decoder, rejects it.
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    stream.write(MakeByteSpan(ParseHex("0610fd000001000200030004000500060007")));
    CNetAddr addr;
    stream >> addr;
    BOOST_CHECK(addr.IsCJDNS());
    BOOST_CHECK(!addr.HasCJDNSPrefix());
    BOOST_CHECK(!addr.IsValid());
    BOOST_CHECK(!addr.IsRoutable());
    BOOST_CHECK_EQUAL(addr.ToStringIP(), "fd00:1:2:3:4:5:6:7");
    BOOST_REQUIRE(stream.empty());
}


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


#ifndef WIN32
// A ban list and a peers file that are rewritten keep the mode they had, and are created
// owner-only when they are not there. Production change that would fail this: SerializeFileDB
// opening its temporary file with the default mode and renaming it over the old file without
// carrying the old mode.
BOOST_AUTO_TEST_CASE(addrdb_rewrite_keeps_the_mode_of_the_file)
{
    const auto mode_of = [](const fs::path& path) {
        struct stat st;
        BOOST_REQUIRE_EQUAL(::stat(path.string().c_str(), &st), 0);
        return static_cast<unsigned>(st.st_mode & 07777);
    };

    const fs::path ban_path = GetDataDir() / "keep_banlist.dat";
    CBanDB ban_db(ban_path);
    banmap_t bans;
    fs::remove(ban_path);
    BOOST_REQUIRE(ban_db.Write(bans));
    BOOST_CHECK_EQUAL(mode_of(ban_path), 0600U);
    BOOST_REQUIRE_EQUAL(::chmod(ban_path.string().c_str(), 0640), 0);
    BOOST_REQUIRE(ban_db.Write(bans));
    BOOST_CHECK_EQUAL(mode_of(ban_path), 0640U);
    fs::remove(ban_path);

    CAddrDB addr_db;
    AddrMan addrman;
    const fs::path peers_path = GetDataDir() / "peers.dat";
    fs::remove(peers_path);
    BOOST_REQUIRE(addr_db.Write(addrman));
    BOOST_CHECK_EQUAL(mode_of(peers_path), 0600U);
    BOOST_REQUIRE_EQUAL(::chmod(peers_path.string().c_str(), 0640), 0);
    BOOST_REQUIRE(addr_db.Write(addrman));
    BOOST_CHECK_EQUAL(mode_of(peers_path), 0640U);
    fs::remove(peers_path);
}
#endif

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
        // The handler answers with our own VERSION and a VERACK. With the
        // socket closed, SocketSendData() drops them rather than pass
        // INVALID_SOCKET to send(). Closing marks the node for disconnection,
        // and ProcessMessages() skips a node so marked, so clear the mark.
        node.CloseSocketDisconnect();
        node.fDisconnect = false;
        const SerializeData message = VersionMessage(received + 30);

        LOCK(node.cs_vRecvMsg);
        BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(message.data())), message.size()));

        SetMockTime(received + wait);
        g_peerman->ProcessMessages(&node);

        // The handler sets nVersion only once the time check has passed.
        BOOST_CHECK_MESSAGE(node.nVersion == PROTOCOL_VERSION, "wait " << wait << ": negotiated version " << node.nVersion.load());
        BOOST_CHECK_MESSAGE(node.nTimeOffset == 30, "wait " << wait << ": time offset " << node.nTimeOffset.load());
    }

    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
}

// A message as a peer sends it, header and checksum included.
static SerializeData WireMessage(const char* command, const CDataStream& payload)
{
    CDataStream ss(SER_NETWORK, INIT_PROTO_VERSION);
    ss << CMessageHeader(command, 0);
    ss.write(Span{payload});

    const unsigned int size = ss.size() - CMessageHeader::HEADER_SIZE;
    memcpy(&ss[CMessageHeader::MESSAGE_SIZE_OFFSET], &size, sizeof(size));
    const uint256 hash = Hash(Span{ss}.subspan(CMessageHeader::HEADER_SIZE));
    memcpy(&ss[CMessageHeader::CHECKSUM_OFFSET], &hash, 4);

    return SerializeData(ss.begin(), ss.end());
}

// Takes the messages queued for a peer off its send queue, as the socket does,
// and gives the command and the payload of each in the order they were queued.
static std::vector<std::pair<std::string, SerializeData>> TakeSent(CNode& node)
{
    std::vector<std::pair<std::string, SerializeData>> sent;

    LOCK(node.cs_vSend);

    for (const SerializeData& message : node.vSendMsg) {
        BOOST_REQUIRE(message.size() >= CMessageHeader::HEADER_SIZE);

        const char* const command = CharCast(UCharCast(message.data())) + CMessageHeader::MESSAGE_START_SIZE;

        sent.emplace_back(std::string(command, strnlen(command, CMessageHeader::COMMAND_SIZE)),
                          SerializeData(message.begin() + CMessageHeader::HEADER_SIZE, message.end()));
    }

    node.vSendMsg.clear();
    node.nSendSize = 0;
    node.nSendOffset = 0;

    return sent;
}

// The entries of a getdata are served while the peer's send queue is under
// -maxsendbuffer. The rest stay on the peer and are served, in the order asked,
// as the queue is taken, and a message that came after the getdata is not
// handled until they are gone. The peer here has no socket, so what the
// handler queues for it stays in its send queue until the test takes it.
BOOST_AUTO_TEST_CASE(getdata_entries_wait_for_room_in_the_send_queue_in_the_order_asked)
{
    BOOST_REQUIRE(g_peerman);
    grc_test::StateGuard guard;
    const CMedianFilter<int> peer_block_counts = WITH_LOCK(cs_main, return cPeerBlockCounts);

    SetMockTime(1700000000);
    // In units of 1000 bytes.
    gArgs.ForceSetArg("-maxsendbuffer", "1");

    CNode node(INVALID_SOCKET, CAddress(CService(LookupNumeric("127.0.0.1", 0)), NODE_NONE), "", true);
    node.CloseSocketDisconnect();
    node.fDisconnect = false;

    LOCK(node.cs_vRecvMsg);

    const SerializeData version = VersionMessage(1700000000);
    BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(version.data())), version.size()));
    g_peerman->ProcessMessages(&node);
    BOOST_REQUIRE(node.nVersion == PROTOCOL_VERSION);
    TakeSent(node);

    // Transactions in relay memory, which a getdata is served from. Together
    // they are several times the send queue's limit.
    const size_t entries = 100;
    std::vector<CInv> request;
    std::vector<SerializeData> transactions;

    for (size_t i = 0; i < entries; ++i) {
        CMutableTransaction mtx;
        mtx.nLockTime = i + 1;
        const CTransaction tx(mtx);

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << tx;
        RelayTransaction(tx, tx.GetHash(), ss);

        request.emplace_back(MSG_TX, tx.GetHash());
        transactions.emplace_back(ss.begin(), ss.end());
    }

    const uint64_t nonce = 7;
    CDataStream getdata_payload(SER_NETWORK, INIT_PROTO_VERSION);
    getdata_payload << request;
    CDataStream ping_payload(SER_NETWORK, INIT_PROTO_VERSION);
    ping_payload << nonce;

    for (const SerializeData& message : {WireMessage(NetMsgType::GETDATA, getdata_payload),
                                         WireMessage(NetMsgType::PING, ping_payload)}) {
        BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(message.data())), message.size()));
    }

    // The first pass serves entries up to the limit. The rest are held, and
    // the ping waits behind them.
    g_peerman->ProcessMessages(&node);

    std::vector<std::pair<std::string, SerializeData>> sent = TakeSent(node);

    BOOST_CHECK(!sent.empty());
    BOOST_CHECK(sent.size() < entries);
    BOOST_CHECK_EQUAL(node.vRecvGetData.size(), entries - sent.size());
    BOOST_CHECK_EQUAL(node.vRecvMsg.size(), 1U);

    // A pass that serves nothing although the send queue has room: the entries
    // are still held, so the ping is still not handled.
    const size_t held = node.vRecvGetData.size();

    SetShutdownInProgress(true);
    g_peerman->ProcessMessages(&node);
    SetShutdownInProgress(false);

    BOOST_CHECK(TakeSent(node).empty());
    BOOST_CHECK_EQUAL(node.vRecvGetData.size(), held);
    BOOST_CHECK_EQUAL(node.vRecvMsg.size(), 1U);

    // Each later pass serves more as the queue is taken, and then the ping.
    for (size_t pass = 0; pass < entries && (!node.vRecvGetData.empty() || !node.vRecvMsg.empty()); ++pass) {
        g_peerman->ProcessMessages(&node);

        for (auto& message : TakeSent(node)) {
            sent.push_back(std::move(message));
        }
    }

    BOOST_CHECK(node.vRecvGetData.empty());
    BOOST_CHECK(node.vRecvMsg.empty());
    BOOST_REQUIRE_EQUAL(sent.size(), entries + 1);

    for (size_t i = 0; i < entries; ++i) {
        BOOST_CHECK_MESSAGE(sent[i].first == NetMsgType::TX && sent[i].second == transactions[i],
                            "reply " << i << " is not the transaction asked for in that place");
    }

    BOOST_CHECK_EQUAL(sent.back().first, NetMsgType::PONG);
    BOOST_CHECK(sent.back().second == SerializeData(ping_payload.begin(), ping_payload.end()));

    for (const CInv& inv : request) {
        RemoveFromRelayMemory(inv.hash);
    }

    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
}

// A getdata is answered with at most PSGTPool::MAX_ENTRIES PSGTs. The count is
// kept on the peer with the entries held, so it runs on through the passes that
// serve one getdata, and it starts from zero for the next getdata.
BOOST_AUTO_TEST_CASE(psgt_entries_of_a_getdata_are_counted_across_passes_and_from_zero_for_the_next)
{
    BOOST_REQUIRE(g_peerman);
    BOOST_REQUIRE_EQUAL(g_psgt_pool.Size(), 0U);
    grc_test::StateGuard guard;
    const CMedianFilter<int> peer_block_counts = WITH_LOCK(cs_main, return cPeerBlockCounts);

    SetMockTime(1700000000);
    // A node that is out of sync serves no PSGT.
    g_previous_block_time.store(GetAdjustedTime());
    // In units of 1000 bytes.
    gArgs.ForceSetArg("-maxsendbuffer", "1");

    // The peer is not a local of this function. ThreadSanitizer knows a mutex
    // by its address and is not told when a local one goes out of scope, so a
    // local peer's locks can be taken for those of an object that an earlier
    // case had at the same place on the stack.
    const auto peer = std::make_unique<CNode>(INVALID_SOCKET,
                                              CAddress(CService(LookupNumeric("127.0.0.1", 0)), NODE_NONE),
                                              "", true);
    CNode& node = *peer;
    node.CloseSocketDisconnect();
    node.fDisconnect = false;

    LOCK(node.cs_vRecvMsg);

    const SerializeData version = VersionMessage(1700000000);
    BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(version.data())), version.size()));
    g_peerman->ProcessMessages(&node);
    BOOST_REQUIRE(node.nVersion == PROTOCOL_VERSION);
    TakeSent(node);

    // One pooled revision. Its bytes are not a PSGT: a getdata is served from
    // the bytes the pool stores, and they are not decoded on the way out. A few
    // replies of this size are over the send queue's limit.
    PSGTPoolEntry entry;
    entry.serialized.assign(400, 0x5a);
    entry.revision_hash = Hash(entry.serialized);
    entry.image = CScriptID(uint160(std::vector<unsigned char>(20, 0x21)));
    entry.tx_hash = uint256(std::vector<unsigned char>(32, 0x43));
    entry.time_received = GetAdjustedTime();
    entry.valid_keys_per_input.push_back({CKeyID(uint160(std::vector<unsigned char>(20, 0x65)))});
    entry.valid_sigs = 1;
    entry.sigs_required = 2;
    entry.sigs_total = 3;

    const CInv revision(MSG_PSGT, entry.revision_hash);

    CDataStream reply_payload(SER_NETWORK, PROTOCOL_VERSION);
    reply_payload << entry.serialized;
    const SerializeData reply(reply_payload.begin(), reply_payload.end());

    std::string reject_reason;
    BOOST_REQUIRE_MESSAGE(g_psgt_pool.Add(std::move(entry), reject_reason) == PSGTPoolAddResult::ACCEPTED_NEW,
                          reject_reason);

    const auto receive_getdata = [&node](const std::vector<CInv>& request) EXCLUSIVE_LOCKS_REQUIRED(node.cs_vRecvMsg) {
        CDataStream payload(SER_NETWORK, INIT_PROTO_VERSION);
        payload << request;

        const SerializeData message = WireMessage(NetMsgType::GETDATA, payload);
        BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(message.data())), message.size()));
    };

    const auto check_replies = [&reply](const std::vector<std::pair<std::string, SerializeData>>& sent) {
        for (size_t i = 0; i < sent.size(); ++i) {
            BOOST_CHECK_MESSAGE(sent[i].first == NetMsgType::PSGT && sent[i].second == reply,
                                "reply " << i << " is not the pooled revision");
        }
    };

    // More entries for the one revision than a getdata is answered with.
    const size_t limit = PSGTPool::MAX_ENTRIES;
    const size_t asked = limit + 50;

    receive_getdata(std::vector<CInv>(asked, revision));

    // The first pass serves entries up to the send queue's limit and holds
    // the rest. The count of those served is on the peer.
    g_peerman->ProcessMessages(&node);

    std::vector<std::pair<std::string, SerializeData>> sent = TakeSent(node);

    BOOST_CHECK(!sent.empty());
    BOOST_CHECK(sent.size() < limit);
    BOOST_CHECK_EQUAL(node.vRecvGetData.size(), asked - sent.size());
    BOOST_CHECK_EQUAL(node.nGetDataPsgtServed, sent.size());

    // The later passes go on from that count: the getdata is answered with
    // the limit and no more, however many passes it takes.
    for (size_t pass = 0; pass < asked && !node.vRecvGetData.empty(); ++pass) {
        g_peerman->ProcessMessages(&node);

        for (auto& message : TakeSent(node)) {
            sent.push_back(std::move(message));
        }
    }

    BOOST_CHECK(node.vRecvGetData.empty());
    BOOST_CHECK(node.vRecvMsg.empty());
    BOOST_CHECK_EQUAL(sent.size(), limit);
    check_replies(sent);

    // The next getdata is counted from zero.
    receive_getdata(std::vector<CInv>(2, revision));
    g_peerman->ProcessMessages(&node);

    sent = TakeSent(node);

    BOOST_CHECK(node.vRecvGetData.empty());
    BOOST_CHECK_EQUAL(sent.size(), 2U);
    check_replies(sent);

    g_psgt_pool.Clear();
    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
}

// A request queued for a scraper part is sent only while the part is needed: a
// manifest here references it and its data has not arrived. The requests here
// have no record in the map of request times. The request for the needed part
// is sent all the same, and the other two are taken off the queue unsent.
BOOST_AUTO_TEST_CASE(a_queued_request_for_a_part_is_sent_only_while_the_part_is_needed)
{
    BOOST_REQUIRE(g_peerman);
    grc_test::StateGuard guard;
    const CMedianFilter<int> peer_block_counts = WITH_LOCK(cs_main, return cPeerBlockCounts);

    SetMockTime(1700000000);

    // Allocated, not a local, for the reason given in the case above.
    const auto peer = std::make_unique<CNode>(INVALID_SOCKET,
                                              CAddress(CService(LookupNumeric("127.0.0.1", 0)), NODE_NONE),
                                              "", true);
    CNode& node = *peer;
    node.CloseSocketDisconnect();
    node.fDisconnect = false;

    {
        LOCK(node.cs_vRecvMsg);

        const SerializeData version = VersionMessage(1700000000);
        BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(version.data())), version.size()));
        g_peerman->ProcessMessages(&node);
        BOOST_REQUIRE(node.nVersion == PROTOCOL_VERSION);
    }

    auto manifest = std::shared_ptr<CScraperManifest>(new CScraperManifest());

    // A part the manifest references whose data has not arrived.
    const uint256 missing = Hash(std::string("a part that has not arrived"));
    manifest->addPart(missing);

    // A part the manifest references whose data is here.
    CDataStream data(SER_NETWORK, PROTOCOL_VERSION);
    data << std::string("a part that is here");
    BOOST_REQUIRE_EQUAL(manifest->addPartData(std::move(data)), 1);

    const uint256 held = WITH_LOCK(CSplitBlob::cs_mapParts, return WITH_LOCK(manifest->cs_manifest,
                                                                             return manifest->vParts[1]->hash));

    // A part that nothing references.
    const uint256 unreferenced = Hash(std::string("a part nothing references"));

    BOOST_REQUIRE(CSplitBlob::PartNeeded(missing));
    BOOST_REQUIRE(!CSplitBlob::PartNeeded(held));
    BOOST_REQUIRE(!CSplitBlob::PartNeeded(unreferenced));

    // One request for each, due now.
    for (const uint256& hash : {held, unreferenced, missing}) {
        node.mapAskFor.insert(std::make_pair(int64_t{0}, CInv(MSG_PART, hash)));
    }

    {
        // The locks the message handler calls SendMessages with.
        LOCK2(cs_main, node.cs_vSend);

        TakeSent(node);
        g_peerman->SendMessages(&node, false);
    }

    BOOST_CHECK(node.mapAskFor.empty());

    std::vector<CInv> asked;

    for (const auto& message : TakeSent(node)) {
        if (message.first != NetMsgType::GETDATA) continue;

        std::vector<CInv> entries;
        CDataStream payload(SER_NETWORK, PROTOCOL_VERSION);
        payload.write(Span{message.second});
        payload >> entries;

        asked.insert(asked.end(), entries.begin(), entries.end());
    }

    BOOST_REQUIRE_EQUAL(asked.size(), 1U);
    BOOST_CHECK(asked.front().type == MSG_PART);
    BOOST_CHECK(asked.front().hash == missing);

    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
}

// A key the scraper registry holds as an authorized scraper, until the object
// goes.
class AuthorizedScraper
{
public:
    AuthorizedScraper()
    {
        m_key.MakeNewKey(true);

        CMutableTransaction mtx;
        mtx.vContracts.push_back(GRC::MakeContract<GRC::ScraperEntryPayload>(
            uint32_t {3}, GRC::ContractAction::ADD, uint32_t {2}, m_key.GetPubKey().GetID(),
            GRC::ScraperEntryStatus::AUTHORIZED));
        m_tx = CTransaction(mtx);

        LOCK(cs_main);
        GRC::GetScraperRegistry().Add({m_tx.vContracts[0], m_tx, &m_index});
    }

    AuthorizedScraper(const AuthorizedScraper&) = delete;
    AuthorizedScraper& operator=(const AuthorizedScraper&) = delete;

    CKey& Key() { return m_key; }

private:
    grc_test::RegistryReset m_scrapers{GRC::ContractType::SCRAPER};
    CKey m_key;
    CTransaction m_tx;
    CBlockIndex m_index{};
};

// A manifest published as a scraper publishes one: each part goes in as data
// while the publish is in progress, and addManifest() signs the manifest and
// puts it in the manifest map. The manifest is taken out of the map when the
// object goes. The constructor checks nothing, so that it always completes and
// the destructor always runs; a case checks Published() first. The manifest is
// named as a scraper names one, with the address of its key, unless the case
// gives another name.
class PublishedManifest
{
public:
    PublishedManifest(
        CKey& key,
        const std::vector<std::string>& parts,
        const int64_t time,
        const std::optional<std::string>& name = std::nullopt)
        : m_manifest(std::make_shared<CScraperManifest>())
    {
        bool parts_added = true;

        for (const std::string& text : parts) {
            CDataStream part(SER_NETWORK, 1);
            part << text;

            if (m_manifest->addPartData(std::move(part), true) < 0) parts_added = false;
        }

        {
            LOCK(m_manifest->cs_manifest);

            m_manifest->sCManifestName = name
                ? *name
                : EncodeDestination(CTxDestination(key.GetPubKey().GetID()));
            m_manifest->nTime = time;
            m_manifest->BeaconList = 0;
            m_manifest->BeaconList_c = 0;

            for (unsigned int n = 1; n < parts.size(); ++n) {
                CScraperManifest::dentry project;
                project.project = "project " + ToString(n);
                project.part1 = n;
                project.LastModified = time;
                project.current = true;
                m_manifest->projects.push_back(project);
            }
        }

        LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

        m_published = parts_added && CScraperManifest::addManifest(m_manifest, key);

        if (m_published) m_hash = WITH_LOCK(m_manifest->cs_manifest, return *m_manifest->phash);
    }

    ~PublishedManifest()
    {
        if (!m_published) return;

        LOCK(CScraperManifest::cs_mapManifest);
        CScraperManifest::DeleteManifest(m_hash, true);
    }

    PublishedManifest(const PublishedManifest&) = delete;
    PublishedManifest& operator=(const PublishedManifest&) = delete;

    bool Published() const { return m_published; }
    //! The hash the manifest map holds the manifest under.
    const uint256& Hash() const { return m_hash; }
    const std::shared_ptr<CScraperManifest>& Manifest() const { return m_manifest; }

    //! The message a peer is sent for the manifest.
    CDataStream Message() const
    {
        LOCK2(CScraperManifest::cs_mapManifest, CSplitBlob::cs_mapParts);

        return CScraperManifest::SerializeForSend(m_manifest);
    }

private:
    std::shared_ptr<CScraperManifest> m_manifest;
    bool m_published{false};
    uint256 m_hash;
};

// A scraper keeps the manifest it publishes under the hash of the message a
// peer is sent for it, which is the hash the peer keeps the manifest under. So
// the publisher has the manifest already when a peer sends the message back.
BOOST_AUTO_TEST_CASE(a_published_manifest_is_kept_under_the_hash_of_the_message_sent_for_it)
{
    BOOST_REQUIRE(g_peerman);
    grc_test::StateGuard guard;
    const CMedianFilter<int> peer_block_counts = WITH_LOCK(cs_main, return cPeerBlockCounts);

    SetMockTime(1700000000);

    // Allocated, not a local, for the reason given in the cases above.
    const auto peer = std::make_unique<CNode>(INVALID_SOCKET,
                                              CAddress(CService(LookupNumeric("127.0.0.1", 0)), NODE_NONE),
                                              "", true);
    CNode& node = *peer;
    node.CloseSocketDisconnect();
    node.fDisconnect = false;

    const size_t manifests_before = WITH_LOCK(CScraperManifest::cs_mapManifest,
                                              return CScraperManifest::mapManifest.size());

    {
        AuthorizedScraper scraper;
        const PublishedManifest published(scraper.Key(), {"a beacon list part", "a project part"}, GetAdjustedTime());

        BOOST_REQUIRE(published.Published());

        CDataStream message = published.Message();
        const uint256 message_hash = Hash(message);

        BOOST_CHECK(published.Hash() == message_hash);

        {
            LOCK(CScraperManifest::cs_mapManifest);

            BOOST_CHECK_EQUAL(CScraperManifest::mapManifest.size(), manifests_before + 1);

            const auto entry = CScraperManifest::mapManifest.find(message_hash);

            BOOST_REQUIRE(entry != CScraperManifest::mapManifest.end());
            BOOST_CHECK(entry->second == published.Manifest());
        }

        // A peer sends the manifest back. The node has it, and takes no second
        // copy of it.
        BOOST_CHECK(!CScraperManifest::RecvManifest(&node, message));

        BOOST_CHECK_EQUAL(WITH_LOCK(CScraperManifest::cs_mapManifest, return CScraperManifest::mapManifest.size()),
                          manifests_before + 1);
    }

    BOOST_CHECK_EQUAL(WITH_LOCK(CScraperManifest::cs_mapManifest, return CScraperManifest::mapManifest.size()),
                      manifests_before);

    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
}

// A scraper's manifest mostly repeats the parts of its last one. The manifest
// is complete as published, and the node counts it once for its scraper along
// with the one before.
BOOST_AUTO_TEST_CASE(a_published_manifest_that_repeats_a_held_part_is_complete)
{
    grc_test::StateGuard guard;

    SetMockTime(1700000000);

    AuthorizedScraper scraper;
    const std::string scraper_id = EncodeDestination(CTxDestination(scraper.Key().GetPubKey().GetID()));

    const PublishedManifest first(scraper.Key(), {"a beacon list part", "a project part"},
                                  GetAdjustedTime() - 600);
    const PublishedManifest second(scraper.Key(), {"a beacon list part", "a later project part"},
                                   GetAdjustedTime());

    BOOST_REQUIRE(first.Published());
    BOOST_REQUIRE(second.Published());

    BOOST_CHECK(WITH_LOCK(first.Manifest()->cs_manifest, return first.Manifest()->isComplete()));
    BOOST_CHECK(WITH_LOCK(second.Manifest()->cs_manifest, return second.Manifest()->isComplete()));

    LOCK(CScraperManifest::cs_mapManifest);

    const mmCSManifestsBinnedByScraper binned = BinCScraperManifestsByScraper();
    const auto bin = binned.find(scraper_id);

    BOOST_REQUIRE(bin != binned.end());
    BOOST_REQUIRE_EQUAL(bin->second.size(), 2U);

    // Newest first.
    auto entry = bin->second.begin();
    BOOST_CHECK(entry->second.first == second.Hash());
    ++entry;
    BOOST_CHECK(entry->second.first == first.Hash());
}

// A node announces a manifest to a peer that connects, and sends it when asked,
// only when it holds every part of it: the peer asks the node it took a manifest
// from for the parts. The node here holds one manifest it published, with its
// parts, and one it received with no part of it here.
BOOST_AUTO_TEST_CASE(a_manifest_is_announced_and_sent_only_with_all_of_its_parts_here)
{
    BOOST_REQUIRE(g_peerman);
    grc_test::StateGuard guard;
    const CMedianFilter<int> peer_block_counts = WITH_LOCK(cs_main, return cPeerBlockCounts);

    SetMockTime(1700000000);

    AuthorizedScraper scraper;

    // The message for a manifest whose publisher's copy is gone, and its parts
    // with it.
    CDataStream message(SER_NETWORK, PROTOCOL_VERSION);
    {
        const PublishedManifest gone(scraper.Key(), {"a beacon list part not here", "a project part not here"},
                                     GetAdjustedTime() - 600);
        BOOST_REQUIRE(gone.Published());

        message = gone.Message();
    }

    const uint256 incomplete_hash = Hash(message);

    // Takes the received manifest out of the map when the case ends, however it
    // ends.
    struct Removal
    {
        const uint256 hash;

        ~Removal()
        {
            LOCK(CScraperManifest::cs_mapManifest);
            CScraperManifest::DeleteManifest(hash, true);
        }
    } removal{incomplete_hash};

    BOOST_REQUIRE(CScraperManifest::RecvManifest(nullptr, message));

    const PublishedManifest complete(scraper.Key(), {"a beacon list part", "a project part"}, GetAdjustedTime());
    BOOST_REQUIRE(complete.Published());
    BOOST_REQUIRE(complete.Hash() != incomplete_hash);

    {
        LOCK(CScraperManifest::cs_mapManifest);

        const auto incomplete = CScraperManifest::mapManifest.find(incomplete_hash);

        BOOST_REQUIRE(incomplete != CScraperManifest::mapManifest.end());
        BOOST_REQUIRE(!WITH_LOCK(incomplete->second->cs_manifest, return incomplete->second->isComplete()));
        BOOST_REQUIRE(WITH_LOCK(complete.Manifest()->cs_manifest, return complete.Manifest()->isComplete()));
    }

    // A node that is out of sync sends no manifest.
    g_previous_block_time.store(GetAdjustedTime());

    // Allocated, not a local, for the reason given in the cases above.
    const auto peer = std::make_unique<CNode>(INVALID_SOCKET,
                                              CAddress(CService(LookupNumeric("127.0.0.1", 0)), NODE_NONE),
                                              "", true);
    CNode& node = *peer;
    node.CloseSocketDisconnect();
    node.fDisconnect = false;

    LOCK(node.cs_vRecvMsg);

    // The peer connects, and is announced what the node holds.
    const SerializeData version = VersionMessage(1700000000);
    BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(version.data())), version.size()));
    g_peerman->ProcessMessages(&node);
    BOOST_REQUIRE(node.nVersion == PROTOCOL_VERSION);
    TakeSent(node);

    {
        LOCK(node.cs_inventory);

        const auto announced = [&node](const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(node.cs_inventory) {
            return std::count_if(node.vInventoryToSend.begin(), node.vInventoryToSend.end(), [&hash](const CInv& inv) {
                return inv.type == MSG_SCRAPERINDEX && inv.hash == hash;
            });
        };

        BOOST_CHECK_EQUAL(announced(complete.Hash()), 1);
        BOOST_CHECK_EQUAL(announced(incomplete_hash), 0);
    }

    // The peer asks for both.
    CDataStream getdata_payload(SER_NETWORK, INIT_PROTO_VERSION);
    getdata_payload << std::vector<CInv>{CInv(MSG_SCRAPERINDEX, incomplete_hash),
                                         CInv(MSG_SCRAPERINDEX, complete.Hash())};

    const SerializeData getdata = WireMessage(NetMsgType::GETDATA, getdata_payload);
    BOOST_REQUIRE(node.ReceiveMsgBytes(CharCast(UCharCast(getdata.data())), getdata.size()));
    g_peerman->ProcessMessages(&node);

    std::vector<uint256> sent;

    for (const auto& reply : TakeSent(node)) {
        if (reply.first == NetMsgType::SCRAPERINDEX) sent.push_back(Hash(reply.second));
    }

    // The message sent hashes to the hash the peer asked for.
    BOOST_REQUIRE_EQUAL(sent.size(), 1U);
    BOOST_CHECK(sent.front() == complete.Hash());

    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
}

// A scraper names a manifest with the address of the key it signs the manifest
// with. A node takes a manifest that is named so and refuses one that is not,
// in sync and out of sync. It does not score the peer that sent it. The key of
// a manifest is tested before its name.
BOOST_AUTO_TEST_CASE(a_manifest_is_taken_only_when_named_with_the_address_of_its_key)
{
    BOOST_REQUIRE(g_peerman);
    grc_test::StateGuard guard;
    const CMedianFilter<int> peer_block_counts = WITH_LOCK(cs_main, return cPeerBlockCounts);

    SetMockTime(1700000000);

    // Allocated, not a local, for the reason given in the cases above. The
    // address is not one of the node's own, so a score for the peer is kept.
    const auto peer = std::make_unique<CNode>(INVALID_SOCKET,
                                              CAddress(CService(LookupNumeric("192.0.2.1", 0)), NODE_NONE),
                                              "", true);
    CNode& node = *peer;
    node.CloseSocketDisconnect();
    node.fDisconnect = false;

    BOOST_REQUIRE(!node.addr.IsLocal());

    AuthorizedScraper scraper;

    CKey another_key;
    another_key.MakeNewKey(true);

    // The messages for three manifests, named as the case describes. The
    // publisher's copy of each is gone, and its parts with it.
    const auto message_for = [](CKey& key, const std::string& text, const std::optional<std::string>& name) {
        const PublishedManifest gone(key, {"a beacon list part " + text, "a project part " + text},
                                     GetAdjustedTime(), name);
        BOOST_REQUIRE(gone.Published());

        return gone.Message();
    };

    const std::string address = EncodeDestination(CTxDestination(scraper.Key().GetPubKey().GetID()));
    const std::string another_address = EncodeDestination(CTxDestination(another_key.GetPubKey().GetID()));

    const CDataStream named_with_its_address = message_for(scraper.Key(), "named with its address", std::nullopt);
    const CDataStream named_with_another_address = message_for(
        scraper.Key(), "named with another address", another_address);
    const CDataStream named_with_its_address_and_a_space = message_for(
        scraper.Key(), "named with its address and a space", address + " ");
    const CDataStream named_with_no_address = message_for(scraper.Key(), "named with no address", "a scraper");

    // A manifest of a key that is not an authorized scraper's.
    const CDataStream of_another_key = message_for(another_key, "of another key", address);

    const uint256 taken_hash = Hash(named_with_its_address);

    // Takes the received manifest out of the map when the case ends, however it
    // ends.
    struct Removal
    {
        const uint256 hash;

        ~Removal()
        {
            LOCK(CScraperManifest::cs_mapManifest);
            CScraperManifest::DeleteManifest(hash, true);
        }
    } removal{taken_hash};

    const auto manifests_held = [] {
        return WITH_LOCK(CScraperManifest::cs_mapManifest, return CScraperManifest::mapManifest.size());
    };
    const auto parts_held = [] {
        return WITH_LOCK(CSplitBlob::cs_mapParts, return CSplitBlob::mapParts.size());
    };

    const size_t manifests_before = manifests_held();
    const size_t parts_before = parts_held();

    for (const bool in_sync : {false, true}) {
        // A node that is in sync also tests that the key is an authorized
        // scraper's, which this one is.
        g_previous_block_time.store(in_sync ? GetAdjustedTime() : 0);
        BOOST_REQUIRE_EQUAL(OutOfSyncByAge(), !in_sync);

        for (const CDataStream* const message : {&named_with_another_address,
                                                 &named_with_its_address_and_a_space,
                                                 &named_with_no_address}) {
            CDataStream received(*message);

            BOOST_CHECK(!CScraperManifest::RecvManifest(&node, received));
            BOOST_CHECK_EQUAL(manifests_held(), manifests_before);
            BOOST_CHECK_EQUAL(parts_held(), parts_before);
            BOOST_CHECK_EQUAL(node.GetMisbehavior(), 0);
        }
    }

    // The node is in sync here.
    CDataStream received(named_with_its_address);

    BOOST_CHECK(CScraperManifest::RecvManifest(nullptr, received));
    BOOST_CHECK_EQUAL(manifests_held(), manifests_before + 1);
    BOOST_CHECK_EQUAL(parts_held(), parts_before + 2);

    {
        LOCK(CScraperManifest::cs_mapManifest);

        BOOST_CHECK_EQUAL(CScraperManifest::mapManifest.count(taken_hash), 1U);
    }

    // IsManifestAuthorized() sets the score for a manifest of a key that is not
    // an authorized scraper's, whatever the manifest is named.
    {
        CDataStream stream(of_another_key);
        const auto manifest = std::make_shared<CScraperManifest>();
        const unsigned int not_set = 12345;
        unsigned int banscore = not_set;

        LOCK2(CScraperManifest::cs_mapManifest, manifest->cs_manifest);

        BOOST_CHECK(!manifest->UnserializeCheck(stream, banscore));
        BOOST_CHECK_NE(banscore, not_set);
    }

    WITH_LOCK(cs_main, cPeerBlockCounts = peer_block_counts);
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
