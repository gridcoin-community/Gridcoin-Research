// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include "addrman.h"
#include "addrman_impl.h"
#include <string>
#include <boost/test/unit_test.hpp>

#ifndef WIN32
#include <sys/stat.h>
#endif
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
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "protocol.h"
#include "test/state_guard.h"
#include "util.h"
#include "util/system.h"
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

BOOST_AUTO_TEST_SUITE_END()
