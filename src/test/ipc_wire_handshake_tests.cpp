// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

// WIRE-level handshake tests: drive ipc::ClientHandshake against a served
// interfaces::Init over a REAL Cap'n Proto connection on a real AF_UNIX socket,
// using the SAME production transport the daemon and GUI use -- CapnpProtocol's
// listen()/connect() -- rather than the in-process FakeInit coverage in
// ipc_handshake_tests. This exercises, end to end over the wire: the served
// ServeInit's auth gate, the per-connection serve factory, and the client's
// schema/network compatibility checks (BuildInfo/NodeIdentity marshalled across
// the socket). Multiprocess-only (needs gridcoin_ipc and the generated capnp
// proxies); the whole file is gated on ENABLE_MULTIPROCESS in CMake.
//
// Using CapnpProtocol (not a hand-rolled EventLoop) matters: it runs each
// ConnectStream/ListenConnections on its own background loop thread and marshals
// calls cross-thread, exactly as production does. A test that constructs the loop
// and calls ConnectStream inline on the same thread (before loop()) deadlocks.

#include "gridcoin/accrual/snapshot.h"
#include "gridcoin/cpid.h"
#include "interfaces/file_transfer.h"
#include "interfaces/init.h"
#include "interfaces/ipc.h"
// Complete types for the unique_ptrs the gated factory methods return: the
// pre-auth test destroys those unique_ptrs (they never get a value, but the
// destructor is still instantiated).
#include "interfaces/gui_state.h"
#include "interfaces/handler.h"
#include "interfaces/node.h"
#include "interfaces/mrc.h"
#include "interfaces/psgt.h"
#include "interfaces/researcher.h"
#include "interfaces/sidestake.h"
#include "interfaces/staking.h"
#include "interfaces/voting.h"
#include "interfaces/wallet.h"
#include "interfaces/wallet_coin_source.h"
#include "interfaces/wallet_tx_source.h"
#include "chainparams.h"
#include "fs.h"
#include "ipc/capnp/common-types.h"
#include "ipc/capnp/protocol.h"
#include "ipc/capnp/wallet-types.h"
#include "ipc/connect.h"
#include "ipc/handshake.h"
#include "ipc/peercred.h"
#include "ipc/protocol.h"
#include "ipc/serve_init.h"
#include "key_io.h"
#include "logging.h"
#include "node/gui_cache.h"
#include "sync.h"
#include "test/autounlock_fake_store.h"
#include "test/gui_cache_fakes.h"
#include "test/lone_best_block.h"
#include "test/staking_test_peers.h"
#include "test/state_guard.h"
#include "test/wallet_file_placeholder.h"
#include "util.h"
#include "wallet/autounlock_setup.h"
#include "wallet/capability.h"
#include "wallet/wallet.h"

#include <boost/test/unit_test.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib> // mkdtemp
#include <cstring>
#include <functional>
#include <future>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {

//! Inner Init the served ServeInit delegates to; untouched by the handshake
//! (ServeInit answers authenticate()/getBuildInfo()/getIdentity() itself).
struct StubInner : interfaces::Init {};

//! Server double reporting a schema_major one greater than this build's, so a
//! genuine server-reported mismatch marshals over the wire. authenticate()
//! accepts any cookie so the handshake reaches the build-info compare.
struct SchemaMismatchServer : interfaces::Init {
    bool authenticate(const std::string&) override { return true; }
    interfaces::BuildInfo getBuildInfo() override
    {
        interfaces::BuildInfo b = ipc::GetLocalBuildInfo();
        b.schema_major += 1;
        return b;
    }
    interfaces::NodeIdentity getIdentity() override
    {
        interfaces::NodeIdentity id;
        id.network = "main";
        return id;
    }
};

//! Closes an fd when it goes out of scope, unless released, so a failed REQUIRE
//! does not leak it.
struct TestFd {
    int fd{-1};

    ~TestFd()
    {
        if (fd >= 0) ::close(fd);
    }

    int release()
    {
        const int released = fd;
        fd = -1;
        return released;
    }
};

//! A temporary AF_UNIX socket bound in a mkdtemp directory. CapnpProtocol::listen()
//! performs the ::listen(); connectClient() dials a fresh client fd.
struct UnixSocket {
    std::string dir;
    std::string path;
    int bound_fd = -1;

    UnixSocket()
    {
        char tmpl[] = "/tmp/gcipcwire.XXXXXX";
        BOOST_REQUIRE(::mkdtemp(tmpl) != nullptr);
        dir = tmpl;
        path = dir + "/wire.sock";

        TestFd sock{::socket(AF_UNIX, SOCK_STREAM, 0)};
        BOOST_REQUIRE(sock.fd >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        BOOST_REQUIRE(path.size() < sizeof(addr.sun_path));
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        BOOST_REQUIRE_EQUAL(::bind(sock.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
        bound_fd = sock.release();
    }

    ~UnixSocket()
    {
        if (bound_fd >= 0) ::close(bound_fd);
        if (!path.empty()) ::unlink(path.c_str());
        if (!dir.empty()) ::rmdir(dir.c_str());
    }

    //! Hand the bound fd to CapnpProtocol::listen (which ::listen()s + owns it).
    int releaseBoundFd()
    {
        int fd = bound_fd;
        bound_fd = -1;
        return fd;
    }

    int connectClient() const
    {
        TestFd sock{::socket(AF_UNIX, SOCK_STREAM, 0)};
        BOOST_REQUIRE(sock.fd >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        BOOST_REQUIRE_EQUAL(::connect(sock.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
        return sock.release();
    }
};

//! Records which Init factory methods actually ARRIVED on the server. Every
//! factory returns nullptr -- what is under test is whether the call crossed the
//! wire at all, not what it produced, so no concrete interface implementations
//! are needed.
//!
//! This is the runtime half of the guard whose static half is
//! test/lint/lint-serve-init-complete.py. A virtual on interfaces::Init with no
//! ordinal in init.capnp gets no override on ProxyClient<Init>, so the client
//! call resolves to interfaces::Init's own default and returns nullptr WITHOUT
//! dispatching. Nothing fails to compile, nothing logs, and the capability
//! simply does not exist over IPC -- which is how the coin channel shipped
//! unmarshalled and left the multiprocess GUI unable to start. Flags are atomic
//! because the server answers on its own loop thread.
struct RecordingInner : interfaces::Init {
    struct Seen {
        std::atomic<bool> node{false};
        std::atomic<bool> staking{false};
        std::atomic<bool> wallet{false};
        std::atomic<bool> wallet_tx_source{false};
        std::atomic<bool> wallet_coin_source{false};
        std::atomic<bool> mrc{false};
        std::atomic<bool> voting{false};
        std::atomic<bool> researcher{false};
        std::atomic<bool> psgt{false};
        std::atomic<bool> sidestake{false};
        std::atomic<bool> gui_state{false};
        std::atomic<bool> core_ready{false};
    };

    explicit RecordingInner(Seen& seen) : m_seen(seen) {}

    bool isCoreReady() override { m_seen.core_ready = true; return true; }
    std::unique_ptr<interfaces::Node> makeNode() override { m_seen.node = true; return nullptr; }
    std::unique_ptr<interfaces::StakingStatus> makeStakingStatus() override { m_seen.staking = true; return nullptr; }
    std::unique_ptr<interfaces::Wallet> makeWallet() override { m_seen.wallet = true; return nullptr; }
    std::shared_ptr<interfaces::WalletTxSource> makeWalletTxSource() override { m_seen.wallet_tx_source = true; return nullptr; }
    std::shared_ptr<interfaces::WalletCoinSource> makeWalletCoinSource() override { m_seen.wallet_coin_source = true; return nullptr; }
    std::unique_ptr<interfaces::MRC> makeMRC() override { m_seen.mrc = true; return nullptr; }
    std::unique_ptr<interfaces::VotingManager> makeVotingManager() override { m_seen.voting = true; return nullptr; }
    std::unique_ptr<interfaces::ResearcherContext> makeResearcherContext() override { m_seen.researcher = true; return nullptr; }
    std::unique_ptr<interfaces::PSGTPoolContext> makePSGTPoolContext() override { m_seen.psgt = true; return nullptr; }
    std::unique_ptr<interfaces::SideStakeManager> makeSideStakeManager() override { m_seen.sidestake = true; return nullptr; }
    std::unique_ptr<interfaces::GuiState> makeGuiState() override { m_seen.gui_state = true; return nullptr; }

    Seen& m_seen;
};

//! Factory serving a fresh ServeInit (StubInner + the given cookie/identity) per
//! connection, matching how the daemon serves: every connection shares the
//! listener's one attach slot. The slot hook is empty; the daemon's ends the
//! wallet's IPC capability.
interfaces::MakeServeInitFn CookieServeFactory(std::string cookie, interfaces::NodeIdentity id)
{
    return [cookie = std::move(cookie), id = std::move(id),
            slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<StubInner>(), cookie, id, slot, {});
    };
}

//! Server + client CapnpProtocols wired over a real socket. connect() returns the
//! remote Init proxy for the test to hand to ClientHandshake. Teardown drops the
//! client proxy and both protocols (each joins its own loop thread) in order.
struct WireLink {
    UnixSocket sock;
    std::unique_ptr<ipc::Protocol> server;
    std::unique_ptr<ipc::Protocol> client_proto;
    std::unique_ptr<interfaces::Init> client;

    WireLink(interfaces::MakeServeInitFn factory)
    {
        server = ipc::capnp::MakeCapnpProtocol();
        server->listen(sock.releaseBoundFd(), "ipc-wire-server", std::move(factory));

        int fd = sock.connectClient();
        client_proto = ipc::capnp::MakeCapnpProtocol();
        client = client_proto->connect(fd, "ipc-wire-client", /*on_disconnect=*/{});
        BOOST_REQUIRE(client != nullptr);
    }

    ~WireLink()
    {
        client.reset();          // drop the remote proxy
        server->disconnectIncoming();
        client_proto.reset();    // joins the client loop thread
        server.reset();          // joins the server loop thread
    }
};

//! The server side of a GUI state subscription. The inner Init's makeGuiState
//! returns a StoringGuiState, which keeps here the fns the client registers, so
//! a case can push through them from the server, as the node's cache timer
//! does. It keeps them as the node's GuiState does: behind interfaces::GuardNotify.
//! A kept fn is a proxy for the client's callback, so a case clears the store
//! before the WireLink goes (the server's loop must outlive the proxy).
struct GuiStateStore {
    //! The node has destroyed the connection's Init: its side of the
    //! connection is torn down.
    std::atomic<bool> init_released{false};

    Mutex mutex;
    interfaces::GuiState::ChangedFn changed GUARDED_BY(mutex);
    interfaces::GuiState::MRCQuoteFn quote GUARDED_BY(mutex);
    interfaces::GuiStateSnapshot current GUARDED_BY(mutex);

    bool HasChanged()
    {
        LOCK(mutex);
        return static_cast<bool>(changed);
    }

    //! Call the client's changed fn, as the cache's timer does: synchronously,
    //! holding no lock (the fn is copied out first).
    void Push(const interfaces::GuiStateSnapshot& snapshot)
    {
        interfaces::GuiState::ChangedFn fn = WITH_LOCK(mutex, return changed);
        fn(snapshot);
    }

    void AnswerQuote(uint64_t request_id, const interfaces::MRCSnapshot& snapshot)
    {
        interfaces::GuiState::MRCQuoteFn fn = WITH_LOCK(mutex, return quote);
        fn(request_id, snapshot);
    }

    void Clear()
    {
        interfaces::GuiState::ChangedFn changed_fn;
        interfaces::GuiState::MRCQuoteFn quote_fn;
        {
            LOCK(mutex);
            std::swap(changed_fn, changed);
            std::swap(quote_fn, quote);
        }
    }
};

//! Clears a GuiStateStore at any exit of a case. Declared after the case's
//! WireLink, so it runs before the link goes: the callback proxies the store
//! keeps are released while the server's loop still runs, also when a REQUIRE
//! ends the case early.
class ClearAtExit
{
public:
    explicit ClearAtExit(GuiStateStore& store) : m_store(store) {}
    ~ClearAtExit() { m_store.Clear(); }

    ClearAtExit(const ClearAtExit&) = delete;
    ClearAtExit& operator=(const ClearAtExit&) = delete;

private:
    GuiStateStore& m_store;
};

struct StoringGuiState : interfaces::GuiState {
    explicit StoringGuiState(GuiStateStore& store) : m_store(store) {}

    interfaces::GuiStateSnapshot current() override { return WITH_LOCK(m_store.mutex, return m_store.current); }

    std::unique_ptr<interfaces::Handler> handleChanged(ChangedFn fn) override
    {
        LOCK(m_store.mutex);
        m_store.changed = interfaces::GuardNotify(std::move(fn));
        return interfaces::MakeCleanupHandler([] {});
    }

    void requestUpdate() override {}

    void requestMRCQuote(CAmount, uint64_t) override {}

    std::unique_ptr<interfaces::Handler> handleMRCQuote(MRCQuoteFn fn) override
    {
        LOCK(m_store.mutex);
        m_store.quote = interfaces::GuardNotify(std::move(fn));
        return interfaces::MakeCleanupHandler([] {});
    }

    GuiStateStore& m_store;
};

struct GuiStateInner : interfaces::Init {
    explicit GuiStateInner(GuiStateStore& store) : m_store(store) {}
    ~GuiStateInner() override { m_store.init_released = true; }

    std::unique_ptr<interfaces::GuiState> makeGuiState() override
    {
        return std::make_unique<StoringGuiState>(m_store);
    }

    GuiStateStore& m_store;
};

using namespace std::chrono_literals;

//! Long enough for the node's cache timer to have acted on anything it was
//! going to act on, at a 4 s interval on a manual clock: a wake and a settle.
constexpr auto TIMER_CHANCE = node::GuiCache::SETTLE + 250ms;

//! What the client's callbacks received, on the client's IPC worker thread.
template <typename T>
class Received
{
public:
    void Set(T value)
    {
        LOCK(m_mutex);
        m_value = std::move(value);
    }

    std::optional<T> Get()
    {
        LOCK(m_mutex);
        return m_value;
    }

private:
    Mutex m_mutex;
    std::optional<T> m_value GUARDED_BY(m_mutex);
};

//! A snapshot with a distinct value in a field of every part, so a field that
//! does not cross shows as a mismatch.
interfaces::GuiStateSnapshot DistinctSnapshot()
{
    interfaces::GuiStateSnapshot snap;
    snap.sequence = 7;
    snap.height = 123;
    snap.tip_time = 1700000000;
    snap.difficulty = 2.5;
    snap.initial_download = false;
    snap.out_of_sync = false;
    snap.peers_height = 124;
    snap.net_weight = 3.25;
    snap.staking = true;
    snap.coin_weight = 4.5;
    snap.miner_errors = "not staking";
    snap.has_balances = true;
    snap.balances.balance = 11;
    snap.balances.immature_balance = 12;
    snap.psgt_status.v15_enabled = true;
    snap.has_mrc = true;
    snap.mrc.reward = 5;
    snap.mrc.mature_balance = 13;
    snap.has_etts = true;
    snap.etts_seconds = 600;
    snap.has_researcher = true;
    snap.researcher.cpid = "abc";
    snap.researcher.has_cpid = true;
    snap.researcher.magnitude = 50.0;
    snap.researcher.has_magnitude = true;
    snap.researcher.has_active_beacon = true;
    return snap;
}

void CheckSameSnapshot(const interfaces::GuiStateSnapshot& got, const interfaces::GuiStateSnapshot& want)
{
    BOOST_CHECK_EQUAL(got.sequence, want.sequence);
    BOOST_CHECK_EQUAL(got.height, want.height);
    BOOST_CHECK_EQUAL(got.tip_time, want.tip_time);
    BOOST_CHECK_EQUAL(got.difficulty, want.difficulty);
    BOOST_CHECK_EQUAL(got.initial_download, want.initial_download);
    BOOST_CHECK_EQUAL(got.out_of_sync, want.out_of_sync);
    BOOST_CHECK_EQUAL(got.peers_height, want.peers_height);
    BOOST_CHECK_EQUAL(got.net_weight, want.net_weight);
    BOOST_CHECK_EQUAL(got.staking, want.staking);
    BOOST_CHECK_EQUAL(got.coin_weight, want.coin_weight);
    BOOST_CHECK_EQUAL(got.miner_errors, want.miner_errors);
    BOOST_CHECK_EQUAL(got.has_balances, want.has_balances);
    BOOST_CHECK_EQUAL(got.balances.balance, want.balances.balance);
    BOOST_CHECK_EQUAL(got.balances.immature_balance, want.balances.immature_balance);
    BOOST_CHECK_EQUAL(got.psgt_status.v15_enabled, want.psgt_status.v15_enabled);
    BOOST_CHECK_EQUAL(got.has_mrc, want.has_mrc);
    BOOST_CHECK_EQUAL(got.mrc.reward, want.mrc.reward);
    BOOST_CHECK_EQUAL(got.mrc.mature_balance, want.mrc.mature_balance);
    BOOST_CHECK_EQUAL(got.has_etts, want.has_etts);
    BOOST_CHECK_EQUAL(got.etts_seconds, want.etts_seconds);
    BOOST_CHECK_EQUAL(got.has_researcher, want.has_researcher);
    BOOST_CHECK_EQUAL(got.researcher.cpid, want.researcher.cpid);
    // The two presence flags sit beside a value of the same name in the schema,
    // where mpgen once read each as that value's companion and carried neither.
    BOOST_CHECK_EQUAL(got.researcher.has_cpid, want.researcher.has_cpid);
    BOOST_CHECK_EQUAL(got.researcher.magnitude, want.researcher.magnitude);
    BOOST_CHECK_EQUAL(got.researcher.has_magnitude, want.researcher.has_magnitude);
    BOOST_CHECK_EQUAL(got.researcher.has_active_beacon, want.researcher.has_active_beacon);
}

//! Counts, while alive, the notifications interfaces::GuardNotify drops as sent
//! to a disconnected client. Those are logged in the IPC category, which it
//! turns on for its lifetime.
class DroppedNotifications
{
public:
    DroppedNotifications()
        : m_was_enabled(LogInstance().WillLogCategory(BCLog::LogFlags::IPC))
        , m_callback(LogInstance().PushBackCallback([this](const std::string& line) {
              if (line.find("dropped a notification to a disconnected client") != std::string::npos) ++m_count;
          }))
    {
        LogInstance().EnableCategory(BCLog::LogFlags::IPC);
    }

    ~DroppedNotifications()
    {
        LogInstance().DeleteCallback(m_callback);
        if (!m_was_enabled) LogInstance().DisableCategory(BCLog::LogFlags::IPC);
    }

    DroppedNotifications(const DroppedNotifications&) = delete;
    DroppedNotifications& operator=(const DroppedNotifications&) = delete;

    int Count() const { return m_count.load(); }

private:
    const bool m_was_enabled;
    std::atomic<int> m_count{0};
    std::list<std::function<void(const std::string&)>>::iterator m_callback;
};

//! Serves the wallet interface over a wallet the case owns, so wallet calls
//! cross the real wire. No ServeInit: what is under test is the wallet's
//! methods, not the authentication gate.
struct WalletServingInit : interfaces::Init {
    explicit WalletServingInit(CWallet* wallet) : m_wallet(wallet) {}

    std::unique_ptr<interfaces::Wallet> makeWallet() override { return interfaces::MakeWallet(m_wallet); }

    CWallet* m_wallet{nullptr};
};

//! The in-process wallet, but answering setAutoUnlockSecret with a byte no
//! AutoUnlockSetupResult of this build has, as a core of a later build could. Every other
//! call is the wallet's own.
class UnknownSetupResultWallet final : public interfaces::Wallet
{
public:
    explicit UnknownSetupResultWallet(CWallet* wallet) : m_inner(interfaces::MakeWallet(wallet)) {}

    AutoUnlockSetupResult setAutoUnlockSecret(const SecureString& /*passphrase*/) override
    {
        return static_cast<AutoUnlockSetupResult>(200);
    }

    int64_t getBalance() override { return m_inner->getBalance(); }
    int64_t getStake() override { return m_inner->getStake(); }
    int64_t getUnconfirmedBalance() override { return m_inner->getUnconfirmedBalance(); }
    int64_t getImmatureBalance() override { return m_inner->getImmatureBalance(); }
    CAmount getTransactionFeeRate() override { return m_inner->getTransactionFeeRate(); }
    bool tryGetBalances(interfaces::WalletBalances& balances) override { return m_inner->tryGetBalances(balances); }
    int getNumTransactions() override { return m_inner->getNumTransactions(); }
    interfaces::WalletLockState getLockState() override { return m_inner->getLockState(); }
    bool isUnlockedForStakingOnly() override { return m_inner->isUnlockedForStakingOnly(); }
    bool getUnlockStakingOnlyFlag() override { return m_inner->getUnlockStakingOnlyFlag(); }
    bool encryptWallet(const SecureString& passphrase) override { return m_inner->encryptWallet(passphrase); }
    std::vector<std::string> getPreEncryptionBackups() override { return m_inner->getPreEncryptionBackups(); }
    bool lockWallet() override { return m_inner->lockWallet(); }
    bool restrictToStakingOnly() override { return m_inner->restrictToStakingOnly(); }
    bool elevateWallet(const SecureString& passphrase) override { return m_inner->elevateWallet(passphrase); }
    bool beginElevation(const SecureString& passphrase, CapabilityKind kind) override
    {
        return m_inner->beginElevation(passphrase, kind);
    }
    void endElevation() override { m_inner->endElevation(); }
    bool unlockWallet(const SecureString& passphrase, bool staking_only) override
    {
        return m_inner->unlockWallet(passphrase, staking_only);
    }
    bool changeWalletPassphrase(const SecureString& old_passphrase, const SecureString& new_passphrase) override
    {
        return m_inner->changeWalletPassphrase(old_passphrase, new_passphrase);
    }
    AutoUnlockSetupResult clearAutoUnlockSecret(const SecureString& passphrase) override
    {
        return m_inner->clearAutoUnlockSecret(passphrase);
    }
    AutoUnlockStoreStatus getAutoUnlockStatus() override { return m_inner->getAutoUnlockStatus(); }
    bool getPubKey(const CKeyID& address, CPubKey& pub_key_out) override { return m_inner->getPubKey(address, pub_key_out); }
    bool getKeyFromPool(const std::string& label, CPubKey& pub_key_out) override
    {
        return m_inner->getKeyFromPool(label, pub_key_out);
    }
    std::vector<interfaces::WalletOutput> getOutputs(const std::vector<COutPoint>& outpoints) override
    {
        return m_inner->getOutputs(outpoints);
    }
    interfaces::CoinControlSummary computeCoinControlSummary(const interfaces::WalletCoinControl& selection,
                                                             const std::vector<int64_t>& recipient_amounts,
                                                             bool subtract_fee_from_amount) override
    {
        return m_inner->computeCoinControlSummary(selection, recipient_amounts, subtract_fee_from_amount);
    }
    unsigned int getMaxConsolidationInputs() override { return m_inner->getMaxConsolidationInputs(); }
    interfaces::SendCoinsResult sendCoins(const std::vector<interfaces::WalletSendRecipient>& recipients,
                                          const std::optional<interfaces::WalletCoinControl>& coin_control,
                                          int64_t accepted_fee) override
    {
        return m_inner->sendCoins(recipients, coin_control, accepted_fee);
    }
    bool backupWallet(const std::string& dest) override { return m_inner->backupWallet(dest); }
    bool backupConfigFile(const std::string& dest) override { return m_inner->backupConfigFile(dest); }
    std::unique_ptr<interfaces::FileSource> beginBackup(uint8_t items, std::string& error_out) override
    {
        return m_inner->beginBackup(items, error_out);
    }
    std::unique_ptr<interfaces::FileSource> beginConsoleDownload(const std::string& method,
                                                                 const std::vector<std::string>& args,
                                                                 std::string& error_out) override
    {
        return m_inner->beginConsoleDownload(method, args, error_out);
    }
    std::unique_ptr<interfaces::FileSink> beginConsoleUpload(const std::string& method,
                                                             const std::vector<std::string>& args, uint64_t size,
                                                             std::string& error_out) override
    {
        return m_inner->beginConsoleUpload(method, args, size, error_out);
    }
    interfaces::MessageSignStatus signMessage(const std::string& address, const std::string& message,
                                              std::string& signature_out) override
    {
        return m_inner->signMessage(address, message, signature_out);
    }
    interfaces::MessageVerifyStatus verifyMessage(const std::string& address, const std::string& message,
                                                  const std::vector<unsigned char>& signature) override
    {
        return m_inner->verifyMessage(address, message, signature);
    }
    std::vector<interfaces::WalletAddress> getAddresses() override { return m_inner->getAddresses(); }
    bool getAddressLabel(const std::string& address, std::string& label_out) override
    {
        return m_inner->getAddressLabel(address, label_out);
    }
    bool isMine(const std::string& address) override { return m_inner->isMine(address); }
    void setAddressBook(const std::string& address, const std::string& label) override
    {
        m_inner->setAddressBook(address, label);
    }
    bool delAddressBook(const std::string& address) override { return m_inner->delAddressBook(address); }
    bool getNewReceiveAddress(std::string& address_out) override { return m_inner->getNewReceiveAddress(address_out); }
    bool getNewReceiveAddressWithLabel(const std::string& label, std::string& address_out) override
    {
        return m_inner->getNewReceiveAddressWithLabel(label, address_out);
    }
    std::vector<std::string> getUnbookedReceiveAddresses() override { return m_inner->getUnbookedReceiveAddresses(); }
    std::unique_ptr<interfaces::Handler> handleStatusChanged(StatusChangedFn fn) override
    {
        return m_inner->handleStatusChanged(std::move(fn));
    }
    std::unique_ptr<interfaces::Handler> handleAddressBookChanged(AddressBookChangedFn fn) override
    {
        return m_inner->handleAddressBookChanged(std::move(fn));
    }
    std::unique_ptr<interfaces::Handler> handleTransactionChanged(TransactionChangedFn fn) override
    {
        return m_inner->handleTransactionChanged(std::move(fn));
    }

private:
    std::unique_ptr<interfaces::Wallet> m_inner;
};

//! Serves an UnknownSetupResultWallet over \p wallet.
struct UnknownSetupResultServingInit : interfaces::Init {
    explicit UnknownSetupResultServingInit(CWallet* wallet) : m_wallet(wallet) {}

    std::unique_ptr<interfaces::Wallet> makeWallet() override
    {
        return std::make_unique<UnknownSetupResultWallet>(m_wallet);
    }

    CWallet* m_wallet{nullptr};
};

//! A daemon-shaped Ipc pair on <-ipcdir>/node.sock: the server checks each
//! accepted peer with ServerPeerPolicy as the daemon does, and the client
//! connects through Ipc::connectAddress as the GUI does. Teardown drops the
//! remote Init, disconnects the server's connections, then destroys both Ipcs
//! (each joins its loop thread) and removes the socket and the directory.
struct IpcPair {
    std::string dir;
    std::unique_ptr<interfaces::Ipc> server;
    std::unique_ptr<interfaces::Ipc> client;
    std::unique_ptr<interfaces::Init> init;

    IpcPair()
    {
        char tmpl[] = "/tmp/gcipcpair.XXXXXX";
        BOOST_REQUIRE(::mkdtemp(tmpl) != nullptr);
        dir = tmpl;
    }

    ~IpcPair()
    {
        init.reset();
        if (server) server->disconnectIncoming();
        client.reset();
        server.reset();
        ::unlink((dir + "/node.sock").c_str());
        ::rmdir(dir.c_str());
    }
};

//! What the client reports when another GUI holds the core's attach slot.
const std::string ALREADY_ATTACHED_MESSAGE{"Another Gridcoin GUI is already attached to this core. Only one GUI can "
                                           "attach at a time; close the other one first."};

//! A mkdtemp directory for ConnectToNode to dial, removed with everything in it
//! when the case ends.
struct TempIpcDir {
    fs::path dir;

    TempIpcDir()
    {
        char tmpl[] = "/tmp/gcipcconn.XXXXXX";
        BOOST_REQUIRE(::mkdtemp(tmpl) != nullptr);
        dir = tmpl;
    }

    ~TempIpcDir()
    {
        boost::system::error_code ec;
        fs::remove_all(dir, ec);
    }
};

//! An AF_UNIX socket bound at \p path, left for the caller to close. With
//! \p listen it also listens, with the core listener's backlog, and is never
//! accepted on: a core that does not answer.
int BindUnixSocket(const fs::path& path, bool listen)
{
    TestFd sock{::socket(AF_UNIX, SOCK_STREAM, 0)};
    BOOST_REQUIRE(sock.fd >= 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const std::string path_str = path.string();
    BOOST_REQUIRE(path_str.size() < sizeof(addr.sun_path));
    std::strncpy(addr.sun_path, path_str.c_str(), sizeof(addr.sun_path) - 1);
    BOOST_REQUIRE_EQUAL(::bind(sock.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    if (listen) BOOST_REQUIRE_EQUAL(::listen(sock.fd, 5), 0);
    return sock.release();
}

//! A core for ConnectToNode, in a TempIpcDir set as -ipcdir (the case's
//! StateGuard restores it): a fresh cookie, and a listener on node.sock. By
//! default it is daemon-shaped: it checks each peer with ServerPeerPolicy and
//! serves it a ServeInit, all sharing one attach slot, as the daemon serves.
//! Teardown disconnects the served connections and destroys the server, joining
//! its loop thread, before the directory is removed.
struct ServedCore {
    TempIpcDir ipcdir;
    std::unique_ptr<interfaces::Ipc> server;

    //! \p factory, if set, serves each connection instead of the daemon-shaped one.
    explicit ServedCore(interfaces::MakeServeInitFn factory = {})
    {
        gArgs.ForceSetArg("-ipcdir", ipcdir.dir.string());
        const std::string cookie = ipc::WriteCookie(ipcdir.dir);
        if (!factory) {
            interfaces::NodeIdentity id;
            id.network = Params().NetworkIDString();
            factory = [cookie, id, slot = ipc::MakeAttachSlot()](int peer_fd) -> std::unique_ptr<interfaces::Init> {
                const std::optional<ipc::PeerIdentity> peer = ipc::ReadPeerIdentity(peer_fd);
                if (!peer || ipc::ServerPeerPolicy(*peer) == ipc::PeerVerdict::Refuse) return nullptr;
                return ipc::MakeServeInit(std::make_unique<StubInner>(), cookie, id, slot, {});
            };
        }
        server = interfaces::MakeIpc("ipc-core", std::move(factory));
        std::string address = "unix";
        server->listenAddress(address);
    }

    ~ServedCore()
    {
        server->disconnectIncoming();
        server.reset();
    }
};

//! A core that takes the connection and answers the bootstrap but not the
//! handshake: authenticate() returns only when released, or after 20 s, which
//! only bounds a failing run.
struct StallingInit : interfaces::Init {
    explicit StallingInit(std::shared_future<void> release) : m_release(std::move(release)) {}

    bool authenticate(const std::string&) override
    {
        m_release.wait_for(std::chrono::seconds{20});
        return false;
    }

    std::shared_future<void> m_release;
};

//! Releases what it was given when it goes out of scope.
struct ReleaseOnExit {
    std::promise<void>& release;
    ~ReleaseOnExit() { release.set_value(); }
};

//! The core's transfer directory, removed when a case begins and ends.
struct TransferDirReset {
    TransferDirReset() { Remove(); }
    ~TransferDirReset() { Remove(); }

    static void Remove()
    {
        boost::system::error_code ec;
        fs::remove_all(GetDataDir() / "transfers", ec);
    }

    //! Files in it, or 0 when it does not exist.
    static size_t Count()
    {
        const fs::path dir = GetDataDir() / "transfers";
        if (!fs::exists(dir)) return 0;

        size_t n = 0;
        for (fs::directory_iterator it(dir); it != fs::directory_iterator(); ++it) {
            ++n;
        }
        return n;
    }
};

//! Serves pwalletMain through a ServeInit with its own attach slot, as the
//! daemon serves the wallet, so the wallet the client gets is bound to the
//! connection.
interfaces::MakeServeInitFn MainWalletServeFactory()
{
    interfaces::NodeIdentity id;
    id.network = "main";
    return [id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<WalletServingInit>(pwalletMain), "cookie", id, slot, {});
    };
}

} // namespace

BOOST_AUTO_TEST_SUITE(ipc_wire_handshake_tests)

// Happy path over the real wire: correct cookie + this build's info => ok, and the
// served identity/build round-trip back to the client.
BOOST_AUTO_TEST_CASE(wire_handshake_succeeds_with_correct_cookie)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    id.identity_token = "tok";
    WireLink link(CookieServeFactory("the-cookie", id));

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "the-cookie", "main");
    BOOST_CHECK(r.ok);
    BOOST_CHECK(r.error.empty());
    BOOST_CHECK_EQUAL(r.remote_ident.identity_token, "tok");
    BOOST_CHECK_EQUAL(r.remote_build.schema_major, ipc::IPC_SCHEMA_MAJOR);
}

// Wrong cookie: the served ServeInit's authenticate() returns false over the
// wire, so the client rejects.
//
// The error TEXT is asserted, not just its non-emptiness. "!ok && !error.empty()"
// cannot distinguish an authentication rejection from any other way the
// handshake can fail -- a dropped connection, a marshalling fault, the catch-all
// at the end of ClientHandshake -- all of which produce exactly the same shape.
// Matching the distinctive substring is what makes this test evidence that the
// cookie comparison ran and refused.
BOOST_AUTO_TEST_CASE(wire_handshake_rejects_wrong_cookie)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link(CookieServeFactory("right-cookie", id));

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "wrong-cookie", "main");
    BOOST_CHECK(!r.ok);
    BOOST_REQUIRE(!r.error.empty());
    BOOST_CHECK_MESSAGE(r.error.find("rejected the authentication cookie") != std::string::npos,
                        "handshake failed for the wrong reason: " << r.error);
}

// The auth gate over the REAL transport. ipc_serve_init_tests covers this
// in-process, which proves ServeInit throws but not that the throw survives the
// wire: the exception is raised inside a capnp server method, captured by
// kj::runCatchingExceptions, marshalled back as an RPC error and re-raised on the
// client. If any link in that chain swallowed it, the in-process test would still
// pass while an unauthenticated peer got a live capability.
BOOST_AUTO_TEST_CASE(wire_capabilities_are_refused_before_authentication)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    id.identity_token = "tok";
    WireLink link(CookieServeFactory("the-cookie", id));

    // No authenticate() call at all: every gated method must fail.
    BOOST_CHECK_THROW(link.client->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(link.client->getIdentity(), std::exception);
    BOOST_CHECK_THROW(link.client->isCoreReady(), std::exception);
    BOOST_CHECK_THROW(link.client->makeNode(), std::exception);
    BOOST_CHECK_THROW(link.client->makeWallet(), std::exception);

    // And the gate still opens afterwards -- the refusals above did not wedge the
    // connection, they just refused the calls.
    BOOST_CHECK(link.client->authenticate("the-cookie"));
    BOOST_CHECK_NO_THROW(link.client->getBuildInfo());
}

// A wrong cookie latches the connection: retrying with the CORRECT cookie on the
// same connection is refused. Over the wire this also confirms the latch lives on
// the server's per-connection ServeInit, not in client-side state.
BOOST_AUTO_TEST_CASE(wire_wrong_cookie_latches_the_connection)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link(CookieServeFactory("right-cookie", id));

    BOOST_CHECK(!link.client->authenticate("wrong-cookie"));
    BOOST_CHECK(!link.client->authenticate("right-cookie"));
    BOOST_CHECK_THROW(link.client->getBuildInfo(), std::exception);
}

// The authentication deadline, end to end: a peer that connects and never
// authenticates is dropped, and the connection slot it was occupying is returned
// to service.
//
// This is the assertion that matters for availability. max_connections is 2 (one
// attached GUI and one that is told another is attached) and accept() is not
// re-armed while both are taken, so without the deadline any local process could
// connect, say nothing, and lock the real GUI out for as long as it stayed alive.
//
// The squatter's disconnect is waited for explicitly rather than assumed after a
// fixed sleep: it is the same event that drives the server-side re-arm, so
// observing it means the second client's handshake below cannot hang waiting for a
// listener that never came back.
BOOST_AUTO_TEST_CASE(wire_unauthenticated_connection_is_dropped_and_slot_reclaimed)
{
    interfaces::NodeIdentity id;
    id.network = "main";

    UnixSocket sock;
    auto server = ipc::capnp::MakeCapnpProtocol();
    server->listen(sock.releaseBoundFd(), "ipc-wire-server", CookieServeFactory("cookie", id),
                   /*auth_deadline=*/std::chrono::seconds{1});

    // The squatter: connects and then does nothing at all.
    std::atomic<bool> squatter_dropped{false};
    auto squatter_proto = ipc::capnp::MakeCapnpProtocol();
    auto squatter = squatter_proto->connect(sock.connectClient(), "ipc-wire-squatter",
                                            [&squatter_dropped] { squatter_dropped = true; });
    BOOST_REQUIRE(squatter != nullptr);

    // Wait for the node to drop it. Generous cap: this asserts the drop happens,
    // not how promptly, and a loaded machine must not turn that into a flake.
    for (int i = 0; i < 200 && !squatter_dropped; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    BOOST_REQUIRE_MESSAGE(squatter_dropped,
                          "the node never dropped a connection that did not authenticate");

    // With the squatter gone, a real client connects and completes the full
    // handshake. (With the cap at two connections it could also have connected
    // beside the squatter; wire_slotless_connection_is_dropped_at_the_deadline
    // covers the cap being reclaimed.)
    auto gui_proto = ipc::capnp::MakeCapnpProtocol();
    auto gui = gui_proto->connect(sock.connectClient(), "ipc-wire-gui", /*on_disconnect=*/{});
    BOOST_REQUIRE(gui != nullptr);
    ipc::HandshakeResult r = ipc::ClientHandshake(*gui, "cookie", "main");
    BOOST_CHECK(r.ok);
    BOOST_CHECK(r.error.empty());

    // Teardown in the same order WireLink uses: proxies, then incoming
    // connections, then each protocol (which joins its own loop thread).
    gui.reset();
    squatter.reset();
    server->disconnectIncoming();
    gui_proto.reset();
    squatter_proto.reset();
    server.reset();
}

// Server reports a higher schema_major than this build => hard fail (the compare
// runs against a real value marshalled from the server).
BOOST_AUTO_TEST_CASE(wire_handshake_rejects_schema_major_mismatch)
{
    WireLink link([](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<SchemaMismatchServer>();
    });

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "any-cookie", "main");
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
}

// Network mismatch: server serves "main", client expects "test" => hard fail.
BOOST_AUTO_TEST_CASE(wire_handshake_rejects_network_mismatch)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link(CookieServeFactory("cookie", id));

    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "cookie", "test");
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
}

// Every interfaces::Init factory must actually reach the server over the wire.
//
// This is the test that would have caught the coin channel shipping without a
// Cap'n Proto schema. makeWalletCoinSource existed on interfaces::Init, had an
// auth-gated ServeInit override, and its DTOs all carried
// INTERFACES_ASSERT_MARSHALABLE -- but with no ordinal in init.capnp, mpgen
// generated no override on ProxyClient<Init>. The client call resolved to the
// base default, returned nullptr without dispatching, and the multiprocess GUI
// threw at startup. Everything compiled and every existing lint passed.
//
// Asserting the SERVER saw each call is what makes this robust: a factory that
// legitimately returns nullptr (no wallet attached, say) is indistinguishable
// client-side from one that was never marshalled, so a client-side null check
// would prove nothing.
BOOST_AUTO_TEST_CASE(wire_every_init_factory_reaches_the_server)
{
    RecordingInner::Seen seen;

    interfaces::NodeIdentity id;
    id.network = "main";
    id.identity_token = "tok";

    WireLink link([&seen, &id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<RecordingInner>(seen), "the-cookie", id, slot, {});
    });

    // The Windows identity check's two methods are served before authentication.
    // Each crosses the wire like the factories below: without an ordinal the call
    // would resolve to the base default, which answers served=false. This
    // connection does not require the check, so none is begun or finished.
    const interfaces::AttestChallenge challenge = link.client->beginAttest();
    BOOST_CHECK_MESSAGE(challenge.served, "beginAttest did not reach the server");
    BOOST_CHECK(challenge.pipe_suffix.empty());
    BOOST_CHECK(challenge.nonce_hex.empty());
    BOOST_CHECK(!link.client->finishAttest());

    // ServeInit gates every factory behind RequireAuth() and the attach slot, so
    // authenticate first. The only connection holds the slot, and the answer
    // crosses the wire like the factories below.
    ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "the-cookie", "main");
    BOOST_REQUIRE(r.ok);
    BOOST_REQUIRE(link.client->holdsAttachSlot());

    // Each call returns nullptr by construction; the return is deliberately
    // ignored. What is asserted is that the invocation arrived server-side.
    link.client->isCoreReady();
    link.client->makeNode();
    link.client->makeStakingStatus();
    link.client->makeWallet();
    link.client->makeWalletTxSource();
    link.client->makeWalletCoinSource();
    link.client->makeMRC();
    link.client->makeVotingManager();
    link.client->makeResearcherContext();
    link.client->makePSGTPoolContext();
    link.client->makeSideStakeManager();
    link.client->makeGuiState();

    BOOST_CHECK_MESSAGE(seen.core_ready.load(), "isCoreReady did not reach the server");
    BOOST_CHECK_MESSAGE(seen.node.load(), "makeNode did not reach the server");
    BOOST_CHECK_MESSAGE(seen.staking.load(), "makeStakingStatus did not reach the server");
    BOOST_CHECK_MESSAGE(seen.wallet.load(), "makeWallet did not reach the server");
    BOOST_CHECK_MESSAGE(seen.wallet_tx_source.load(), "makeWalletTxSource did not reach the server");
    BOOST_CHECK_MESSAGE(seen.wallet_coin_source.load(),
                        "makeWalletCoinSource did not reach the server -- it is missing an ordinal "
                        "in src/ipc/capnp/init.capnp, so ProxyClient<Init> has no override and the "
                        "call never left the client process");
    BOOST_CHECK_MESSAGE(seen.mrc.load(), "makeMRC did not reach the server");
    BOOST_CHECK_MESSAGE(seen.voting.load(), "makeVotingManager did not reach the server");
    BOOST_CHECK_MESSAGE(seen.researcher.load(), "makeResearcherContext did not reach the server");
    BOOST_CHECK_MESSAGE(seen.psgt.load(), "makePSGTPoolContext did not reach the server");
    BOOST_CHECK_MESSAGE(seen.sidestake.load(), "makeSideStakeManager did not reach the server");
    BOOST_CHECK_MESSAGE(seen.gui_state.load(), "makeGuiState did not reach the server");
}

// The GUI state push, end to end over the wire: the client registers its fn
// through the remote GuiState, the server calls the fn it was handed (the node's
// cache timer does this on every publish), and the client receives the snapshot
// with every part intact. The snapshot is a struct argument of a callback, which
// no other notification in this tree carries, and nests the MRC, researcher,
// PSGT and wallet structs.
BOOST_AUTO_TEST_CASE(wire_gui_state_push_reaches_the_client)
{
    GuiStateStore store;
    {
        LOCK(store.mutex);
        store.current = DistinctSnapshot();
    }

    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&store, &id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<GuiStateInner>(store), "the-cookie", id, slot, {});
    });
    ClearAtExit clear_store(store);
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    // current() returns the snapshot as a result.
    CheckSameSnapshot(gui_state->current(), DistinctSnapshot());

    Received<interfaces::GuiStateSnapshot> received;
    std::unique_ptr<interfaces::Handler> handler =
        gui_state->handleChanged([&received](const interfaces::GuiStateSnapshot& snap) { received.Set(snap); });
    BOOST_REQUIRE(store.HasChanged());

    // The call returns once the client's callback has run.
    interfaces::GuiStateSnapshot pushed = DistinctSnapshot();
    pushed.sequence = 8;
    store.Push(pushed);

    const std::optional<interfaces::GuiStateSnapshot> got = received.Get();
    BOOST_REQUIRE(got);
    BOOST_CHECK_EQUAL(got->height, 123);
    BOOST_CHECK(got->has_mrc);
    BOOST_CHECK_EQUAL(got->mrc.reward, 5);
    CheckSameSnapshot(*got, pushed);
}

// The quote answer over the wire: a request id and an MRC snapshot.
BOOST_AUTO_TEST_CASE(wire_gui_state_quote_reaches_the_client)
{
    GuiStateStore store;
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&store, &id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<GuiStateInner>(store), "the-cookie", id, slot, {});
    });
    ClearAtExit clear_store(store);
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    Received<std::pair<uint64_t, interfaces::MRCSnapshot>> received;
    std::unique_ptr<interfaces::Handler> handler =
        gui_state->handleMRCQuote([&received](uint64_t request_id, const interfaces::MRCSnapshot& quote) {
            received.Set({request_id, quote});
        });

    interfaces::MRCSnapshot quote;
    quote.fee = 33;
    quote.min_fee = 28;
    quote.boosted_fee_error = true;
    quote.boosted_fee_error_msg = "too much";
    store.AnswerQuote(9, quote);

    const auto got = received.Get();
    BOOST_REQUIRE(got);
    BOOST_CHECK_EQUAL(got->first, 9U);
    BOOST_CHECK_EQUAL(got->second.fee, 33);
    BOOST_CHECK_EQUAL(got->second.min_fee, 28);
    BOOST_CHECK(got->second.boosted_fee_error);
    BOOST_CHECK_EQUAL(got->second.boosted_fee_error_msg, "too much");
}

// A GUI that goes away while the node still holds its fn: the node's next push
// returns to the caller (the cache's timer) without throwing, and is dropped as
// a notification to a disconnected client, not as some other failure.
BOOST_AUTO_TEST_CASE(push_to_a_disconnected_gui_is_dropped)
{
    GuiStateStore store;
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&store, &id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<GuiStateInner>(store), "the-cookie", id, slot, {});
    });
    ClearAtExit clear_store(store);
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    std::atomic<int> calls{0};
    std::unique_ptr<interfaces::Handler> handler =
        gui_state->handleChanged([&calls](const interfaces::GuiStateSnapshot&) { ++calls; });
    BOOST_REQUIRE(store.HasChanged());

    store.Push(DistinctSnapshot());
    BOOST_CHECK_EQUAL(calls.load(), 1);

    // The GUI exits: its proxies go, then its connection (joining its loop). The
    // node then tears its side down on its own; the case waits for that, so
    // that the IPC category, on for the push alone, logs the push and nothing
    // else.
    handler.reset();
    gui_state.reset();
    link.client.reset();
    link.client_proto.reset();
    BOOST_REQUIRE(gui_cache_test::WaitUntil([&store] { return store.init_released.load(); }));

    {
        DroppedNotifications dropped;
        BOOST_CHECK_NO_THROW(store.Push(DistinctSnapshot()));
        BOOST_CHECK_EQUAL(calls.load(), 1);
        BOOST_CHECK_EQUAL(dropped.Count(), 1);
    }
}

// A GUI subscribed through the node's own GuiState (ServeInit over the
// monolithic Init, over a published cache) whose connection drops with the
// subscription alive: the node releases the subscription with the connection,
// so its cache stops computing for it.
BOOST_AUTO_TEST_CASE(a_dropped_gui_connection_ends_its_subscription)
{
    gui_cache_test::FakeCompute compute;
    gui_cache_test::Deliveries deliveries;
    gui_cache_test::PublishedFakeCache published(compute, 4000ms, 30000ms);
    node::GuiCache& cache = published.Cache();

    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([&id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(interfaces::MakeGridcoinInit(), "the-cookie", id, slot, {});
    });
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::GuiState> gui_state = link.client->makeGuiState();
    BOOST_REQUIRE(gui_state);

    // The subscription's forced update reaches the client over the wire, and so
    // does the next due update.
    std::unique_ptr<interfaces::Handler> handler = gui_state->handleChanged(deliveries.Make());
    BOOST_REQUIRE(deliveries.WaitFor([](const interfaces::GuiStateSnapshot& snap) { return snap.sequence == 1; }));

    compute.Advance(4000ms);
    gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), 2);
    BOOST_REQUIRE(deliveries.WaitFor([](const interfaces::GuiStateSnapshot& snap) { return snap.sequence == 2; }));

    // Only the subscription is left alive on the client; then the connection
    // drops under it.
    gui_state.reset();
    link.client.reset();
    link.server->disconnectIncoming();

    // The node cleans up the GUI's objects on its own: once it has, a due block
    // computes nothing.
    BOOST_REQUIRE_MESSAGE(gui_cache_test::WaitUntil([&] {
        const int before = compute.chain_calls.load();
        compute.Advance(4000ms);
        gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
        return compute.chain_calls.load() == before;
    }), "the cache kept computing for a GUI whose connection dropped");

    // And it stays stopped, on the timer too.
    std::this_thread::sleep_for(TIMER_CHANCE);
    const int chain_calls = compute.chain_calls.load();
    const int runs = compute.cs_main_runs.load();
    compute.Advance(4000ms);
    gui_cache_test::InScope(cache, node::GuiCache::CHAIN);
    std::this_thread::sleep_for(TIMER_CHANCE);
    BOOST_CHECK_EQUAL(compute.chain_calls.load(), chain_calls);
    BOOST_CHECK_EQUAL(compute.cs_main_runs.load(), runs);
}

// A connection served with the identity check required refuses the cookie over the
// real wire when no check admitted the peer, as it does on POSIX, where the check
// cannot run.
BOOST_AUTO_TEST_CASE(wire_attestation_required_refuses_the_cookie)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    WireLink link([id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        ipc::ServeInitOptions options;
        options.require_attestation = true;
        return ipc::MakeServeInit(std::make_unique<StubInner>(), "the-cookie", id, slot, {}, options);
    });

    const ipc::HandshakeResult r = ipc::ClientHandshake(*link.client, "the-cookie", "main");
    BOOST_CHECK(!r.ok);
    BOOST_CHECK_MESSAGE(r.error.find("rejected the authentication cookie") != std::string::npos, r.error);
}

// A capability over the real wire: the passphrase crosses as Data, the kind as a
// byte, and the server's wallet holds the capability in its IPC slot, where the
// signing call that follows on the same connection finds it. The wallet stays
// unlocked for staking only throughout.
BOOST_AUTO_TEST_CASE(wire_wallet_elevation_round_trip)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    WireLink link([&fx](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<WalletServingInit>(&fx.wallet);
    });

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    const std::string address = EncodeDestination(fx.id);
    std::string signature;

    BOOST_CHECK(wallet->signMessage(address, "over the wire", signature)
                == interfaces::MessageSignStatus::Refused);

    BOOST_CHECK(!wallet->beginElevation(SecureString("not the passphrase"), CapabilityKind::SignMessage));
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    // The GUI checks the passphrase on a thread made for the one call and
    // ended after it (AskPassphraseDialog). Such a call crosses like any other,
    // the thread ends cleanly, and the capability it began serves the calls
    // the GUI then makes from its own thread.
    bool begun = false;
    std::thread checker([&] {
        begun = wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::SignMessage);
    });
    checker.join();

    BOOST_REQUIRE(begun);
    BOOST_CHECK(fx.wallet.IpcCapability({}).has_value());

    BOOST_CHECK(wallet->signMessage(address, "over the wire", signature) == interfaces::MessageSignStatus::OK);
    BOOST_CHECK(!signature.empty());

    wallet->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    std::string after;
    BOOST_CHECK(wallet->signMessage(address, "over the wire", after) == interfaces::MessageSignStatus::Refused);
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());

    wallet.reset();
}

// A kind byte outside the enumeration is refused when the server reads it: the
// call fails, no capability is issued, and the connection goes on serving.
BOOST_AUTO_TEST_CASE(wire_capability_kind_out_of_range_is_refused)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);

    WireLink link([&fx](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<WalletServingInit>(&fx.wallet);
    });

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    BOOST_CHECK_THROW(wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, static_cast<CapabilityKind>(200)),
                      std::exception);
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());
    BOOST_CHECK_EQUAL(fx.wallet.FailedUnlockCount(), 0U);

    BOOST_CHECK(wallet->getLockState().unlocked_for_staking_only);
    BOOST_CHECK(wallet->beginElevation(grc_test::EncryptedWallet::PASSPHRASE, CapabilityKind::Send));
    wallet->endElevation();
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    wallet.reset();
}

// Autounlock setup over the real wire: the passphrase crosses as Data, the result as a byte
// and the status as a struct. It is checked in-band, so the connection's capability slot
// stays empty throughout.
BOOST_AUTO_TEST_CASE(wire_autounlock_setup_round_trip)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    grc_test::FakeStoreInstalled fake(/*configured=*/false);

    WireLink link([&fx](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<WalletServingInit>(&fx.wallet);
    });

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    const AutoUnlockStoreStatus status = wallet->getAutoUnlockStatus();
    BOOST_CHECK(status.supported);
    BOOST_CHECK(!status.configured);
    BOOST_CHECK_EQUAL(status.scope, "machine");
    BOOST_CHECK_EQUAL(status.location, grc_test::FakeAutoUnlockStore::LOCATION);
    BOOST_CHECK(status.problem.empty());

    BOOST_CHECK(wallet->setAutoUnlockSecret(grc_test::EncryptedWallet::PASSPHRASE) == AutoUnlockSetupResult::Ok);
    BOOST_CHECK(fake.calls->kept == grc_test::EncryptedWallet::PASSPHRASE);
    BOOST_CHECK(wallet->getAutoUnlockStatus().configured);
    BOOST_CHECK(fx.wallet.IsUnlockedForStakingOnly());
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    BOOST_CHECK(wallet->clearAutoUnlockSecret(grc_test::EncryptedWallet::PASSPHRASE) == AutoUnlockSetupResult::Ok);
    BOOST_CHECK(!fake.calls->kept.has_value());
    BOOST_CHECK(!wallet->getAutoUnlockStatus().configured);
    BOOST_CHECK(fx.wallet.IsLocked());
    BOOST_CHECK(!fx.wallet.IpcCapability({}).has_value());

    wallet.reset();
}

// Over the real wire, a setup result the client's build does not know is refused as the
// client reads it, by the generated proxy's read of the result (the hook in wallet-types.h):
// the call fails with that hook's message, and the connection goes on serving.
BOOST_AUTO_TEST_CASE(wire_autounlock_result_out_of_range_is_refused)
{
    grc_test::EncryptedWallet fx;
    fx.UnlockFor(UnlockScope::StakingOnly);
    grc_test::FakeStoreInstalled fake(/*configured=*/true);

    WireLink link([&fx](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<UnknownSetupResultServingInit>(&fx.wallet);
    });

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    BOOST_CHECK_EXCEPTION(wallet->setAutoUnlockSecret(grc_test::EncryptedWallet::PASSPHRASE), std::runtime_error,
                          [](const std::runtime_error& e) {
                              return std::string(e.what()).find("AutoUnlockSetupResult IPC field out of range")
                                     != std::string::npos;
                          });

    BOOST_CHECK(wallet->getLockState().unlocked_for_staking_only);
    BOOST_CHECK(wallet->clearAutoUnlockSecret(grc_test::EncryptedWallet::PASSPHRASE) == AutoUnlockSetupResult::Ok);
    BOOST_CHECK(!fake.calls->kept.has_value());
    BOOST_CHECK(fx.wallet.IsLocked());

    wallet.reset();
}

// A setup result byte outside the enumeration is refused when the client reads it, rather
// than read as an outcome the client would misreport.
BOOST_AUTO_TEST_CASE(autounlock_result_read_hook_refuses_unknown_values)
{
    struct Input {
        uint8_t m_value;
        uint8_t get() const { return m_value; }
    };

    // The hook never touches the context, so a connection is not needed to call it.
    alignas(std::max_align_t) static char unused_connection[1];
    mp::InvokeContext invoke_context{*reinterpret_cast<mp::Connection*>(unused_connection)};

    const AutoUnlockSetupResult last = mp::CustomReadField(mp::TypeList<AutoUnlockSetupResult>(), mp::Priority<2>(),
                                                           invoke_context, Input{5},
                                                           mp::ReadDestTemp<AutoUnlockSetupResult>());
    BOOST_CHECK(last == AutoUnlockSetupResult::OkUnverified);

    for (const uint8_t value : {uint8_t{6}, uint8_t{200}, uint8_t{255}}) {
        BOOST_CHECK_THROW(mp::CustomReadField(mp::TypeList<AutoUnlockSetupResult>(), mp::Priority<2>(), invoke_context,
                                              Input{value}, mp::ReadDestTemp<AutoUnlockSetupResult>()),
                          std::runtime_error);
    }
}

// The automatic backups made before an encryption come back over the real wire, so the GUI
// of the split build can name them. Nothing is deleted.
BOOST_AUTO_TEST_CASE(wire_encryptwallet_carries_the_pre_encryption_backups)
{
    // Served without a ServeInit, the wallet is bound to no connection, so the encryption
    // stops nothing; the real stop is replaced anyway, should that change.
    interfaces::SetEncryptionShutdownFnForTesting([] {});
    struct RestoreShutdownFn {
        ~RestoreShutdownFn() { interfaces::SetEncryptionShutdownFnForTesting({}); }
    } restore_shutdown_fn;

    const TempIpcDir backup_dir;
    grc_test::ForcedArgGuard backupdir_arg("backupdir", backup_dir.dir.string());

    const fs::path first = backup_dir.dir / "wallet.dat-2026-01-01T00-00-00Z";
    const fs::path second = backup_dir.dir / "wallet.dat-2026-02-01T00-00-00Z-beacon";
    for (const fs::path& path : {first, second}) {
        fsbridge::ofstream stream(path);
        stream << "ipc_wire_handshake_tests\n";
    }

    CWallet plain;
    {
        CKey key;
        key.MakeNewKey(true);
        LOCK(plain.cs_wallet);
        BOOST_REQUIRE(plain.AddKey(key));
    }

    WireLink link([&plain](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<WalletServingInit>(&plain);
    });

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    BOOST_REQUIRE(wallet->encryptWallet(SecureString("over the wire")));
    BOOST_CHECK(plain.IsCrypted());

    const std::vector<std::string> backups = wallet->getPreEncryptionBackups();

    BOOST_REQUIRE_EQUAL(backups.size(), 2U);
    BOOST_CHECK_EQUAL(backups[0], first.string());
    BOOST_CHECK_EQUAL(backups[1], second.string());
    BOOST_CHECK(fs::exists(first));
    BOOST_CHECK(fs::exists(second));

    wallet.reset();
}

// A short passphrase read off the wire lands in a secure buffer of its own, and the wire bytes
// are cleansed.
BOOST_AUTO_TEST_CASE(secure_string_read_hook_reserves_secure_capacity)
{
    std::string wire = "short";
    struct Input {
        ::capnp::Data::Reader m_data;
        ::capnp::Data::Reader get() const { return m_data; }
    } input{::capnp::Data::Reader(reinterpret_cast<const ::kj::byte*>(wire.data()), wire.size())};

    // The hook never touches the context, so a connection is not needed to call it.
    alignas(std::max_align_t) static char unused_connection[1];
    mp::InvokeContext invoke_context{*reinterpret_cast<mp::Connection*>(unused_connection)};
    SecureString result = mp::CustomReadField(mp::TypeList<SecureString>(), mp::Priority<1>(), invoke_context, input,
                                              mp::ReadDestTemp<SecureString>());

    BOOST_CHECK_EQUAL(std::string(result.begin(), result.end()), "short");
    BOOST_CHECK_GE(result.capacity(), 100U);
    BOOST_CHECK_EQUAL(wire, std::string(5, '\0'));
}

// The passphrases of a console command cross as raw bytes, whitespace and all.
BOOST_AUTO_TEST_CASE(wire_secret_console_command_carries_passphrases)
{
    grc_test::EncryptedWallet fx;

    interfaces::NodeIdentity id;
    id.network = "main";
    id.identity_token = "tok";

    struct NodeServingInner : StubInner {
        std::unique_ptr<interfaces::Node> makeNode() override { return interfaces::MakeNode(); }
    };

    WireLink link([&id, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
        return ipc::MakeServeInit(std::make_unique<NodeServingInner>(), "the-cookie", id, slot, {});
    });

    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "the-cookie", "main").ok);

    std::unique_ptr<interfaces::Node> node = link.client->makeNode();
    BOOST_REQUIRE(node != nullptr);

    const SecureString fresh(" lead and trail ");
    const interfaces::RpcConsoleResult result = node->executeRpcConsoleSecretCommand(
        "walletpassphrasechange", {grc_test::EncryptedWallet::PASSPHRASE, fresh}, {});
    BOOST_CHECK_MESSAGE(result.ok, result.output);

    BOOST_CHECK(fx.wallet.Unlock(fresh));

    node.reset();
}

// A backup over the real wire: the core snapshots the wallet file into its own
// transfer directory, and the client streams the copy through the FileSource
// proxy into a file of its choosing, verified by the trailer. The snapshot is
// gone once it has been streamed.
BOOST_AUTO_TEST_CASE(wire_backup_streams_the_wallet_file)
{
    const TransferDirReset transfers;
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();
    const TempIpcDir dest_dir;

    WireLink link(MainWalletServeFactory());
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "cookie", "main").ok);

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    std::string error;
    std::unique_ptr<interfaces::FileSource> source =
        wallet->beginBackup(static_cast<uint8_t>(interfaces::BackupItem::Wallet), error);
    BOOST_REQUIRE_MESSAGE(source != nullptr, error);

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 1U);
    BOOST_CHECK_EQUAL(infos[0].name, pwalletMain->strWalletFile);

    const fs::path dest = dest_dir.dir / "wallet-backup.dat";
    const std::atomic<bool> cancel{false};
    const interfaces::TransferOutcome outcome = interfaces::DownloadEntry(*source, 0, dest, {}, cancel);
    BOOST_CHECK_MESSAGE(outcome.status == interfaces::TransferStatus::Ok, outcome.message);

    std::string copied;
    {
        fsbridge::ifstream in(dest, std::ios::binary);
        copied.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    BOOST_CHECK_EQUAL(copied, grc_test::WalletFilePlaceholder::CONTENT);
    BOOST_CHECK_EQUAL(TransferDirReset::Count(), 0U);

    source.reset();
    wallet.reset();
}

// Review Focus 2 over the real wire: a GUI that goes away part way through a
// backup leaves no snapshot in the core. The client reads one short chunk, then
// drops its Init proxy, which destroys its connection; the server tears the
// connection down, and the snapshot is gone once it has.
BOOST_AUTO_TEST_CASE(wire_backup_snapshot_is_gone_after_the_client_goes)
{
    const TransferDirReset transfers;
    const grc_test::WalletFilePlaceholder wallet_file;
    grc_test::RequireWalletFileNotInUse();

    WireLink link(MainWalletServeFactory());
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "cookie", "main").ok);

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    std::string error;
    std::unique_ptr<interfaces::FileSource> source =
        wallet->beginBackup(static_cast<uint8_t>(interfaces::BackupItem::Wallet), error);
    BOOST_REQUIRE_MESSAGE(source != nullptr, error);

    const interfaces::FileChunk chunk = source->read(0, 4);
    BOOST_REQUIRE_MESSAGE(chunk.error.empty(), chunk.error);
    BOOST_CHECK_EQUAL(chunk.data.size(), 4U);
    BOOST_CHECK(!chunk.eof);
    BOOST_CHECK_EQUAL(TransferDirReset::Count(), 1U);

    // The proxies made through the connection are left without one.
    link.client.reset();

    // The cap only limits a failing run.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
    while (TransferDirReset::Count() != 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    BOOST_CHECK_MESSAGE(TransferDirReset::Count() == 0,
                        "the snapshot was still in the core's transfer directory after the client went away");

    source.reset();
    wallet.reset();
}

// A console file command that writes a file, over the real wire: its arguments
// cross as a List(Text), the core puts a file of its own where the command takes
// one (dumpcontracts takes it second, and txids_only follows it), and the output
// streams through the FileSource proxy with the command's reply as the entry's
// note. The wallet is unencrypted, so no capability is needed.
BOOST_AUTO_TEST_CASE(wire_console_download_streams_a_command_file)
{
    const TransferDirReset transfers;
    const grc_test::LoneBestBlock tip;
    const TempIpcDir dest_dir;

    WireLink link(MainWalletServeFactory());
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "cookie", "main").ok);

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);

    std::string error;
    std::unique_ptr<interfaces::FileSource> source =
        wallet->beginConsoleDownload("dumpcontracts", {"beacon", "true"}, error);
    BOOST_REQUIRE_MESSAGE(source != nullptr, error);

    const std::vector<interfaces::FileEntryInfo> infos = source->entries();
    BOOST_REQUIRE_EQUAL(infos.size(), 1U);
    BOOST_CHECK_MESSAGE(infos[0].note.find("\"contract_type\": \"beacon\"") != std::string::npos, infos[0].note);

    const fs::path dest = dest_dir.dir / "contracts.txt";
    const std::atomic<bool> cancel{false};
    const interfaces::TransferOutcome outcome = interfaces::DownloadEntry(*source, 0, dest, {}, cancel);
    BOOST_CHECK_MESSAGE(outcome.status == interfaces::TransferStatus::Ok, outcome.message);

    // The argument after the file chose the text form.
    std::string text;
    {
        fsbridge::ifstream in(dest, std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    BOOST_CHECK_MESSAGE(text.rfind("high_height_time,low_height,high_height,num_blocks\n", 0) == 0, text);
    BOOST_CHECK_EQUAL(TransferDirReset::Count(), 0U);

    source.reset();
    wallet.reset();
}

// A console file command that reads a file, over the real wire: the upload goes
// through the FileSink proxy in small writes, the command reads it from a core
// temporary file at finish, and its reply comes back as the output. The client
// holds a FileTransfer capability, as a GUI on an encrypted wallet does; the
// command reaches no key, so the capability is ended as the upload begins.
BOOST_AUTO_TEST_CASE(wire_console_upload_runs_a_command_on_the_file)
{
    const TransferDirReset transfers;
    const TempIpcDir local_dir;

    const fs::path local = local_dir.dir / "5000000";
    {
        GRC::AccrualSnapshotWriter writer(local);
        writer.WriteHeader(5000000);
        writer.WriteRecord(GRC::Cpid::Parse("f5d8234352e5a5ae3915debba7258294"), 12345);
    }

    WireLink link(MainWalletServeFactory());
    BOOST_REQUIRE(ipc::ClientHandshake(*link.client, "cookie", "main").ok);

    std::unique_ptr<interfaces::Wallet> wallet = link.client->makeWallet();
    BOOST_REQUIRE(wallet != nullptr);
    BOOST_REQUIRE(wallet->beginElevation(SecureString{}, CapabilityKind::FileTransfer));

    std::string error;
    std::unique_ptr<interfaces::FileSink> sink =
        wallet->beginConsoleUpload("parseaccrualsnapshotfile", {}, fs::file_size(local), error);
    BOOST_REQUIRE_MESSAGE(sink != nullptr, error);

    BOOST_CHECK(!pwalletMain->IpcCapability({}).has_value());

    const std::atomic<bool> cancel{false};
    const interfaces::FileSinkResult result = interfaces::UploadFile(*sink, local, {}, cancel, 7);
    BOOST_CHECK_MESSAGE(result.ok, result.error);
    BOOST_CHECK_MESSAGE(result.output.find("\"height\": 5000000") != std::string::npos, result.output);
    BOOST_CHECK_EQUAL(TransferDirReset::Count(), 0U);

    // An upload above its command's cap is refused as it begins.
    error.clear();
    BOOST_CHECK(!wallet->beginConsoleUpload("importwallet", {}, 64ULL * 1024 * 1024 + 1, error));
    BOOST_CHECK_MESSAGE(error.find("too large") != std::string::npos, error);

    sink.reset();
    wallet->endElevation();
    wallet.reset();
}

// Ipc::connectAddress end to end over -ipcdir: the client reads the listener's
// identity on the connected socket, checks it with ClientPeerPolicy against the
// socket's directory, and reports through ConnectInfo that the core runs as
// this account. The server admits the client through ServerPeerPolicy.
BOOST_FIXTURE_TEST_CASE(wire_connect_address_reports_same_account, grc_test::StateGuard)
{
    // Declared before the pair, so it outlives the server's factory.
    std::atomic<int> served{0};
    IpcPair pair;
    gArgs.ForceSetArg("-ipcdir", pair.dir);

    interfaces::NodeIdentity id;
    id.network = "main";
    auto factory = [&served, id, slot = ipc::MakeAttachSlot()](int peer_fd) -> std::unique_ptr<interfaces::Init> {
        const std::optional<ipc::PeerIdentity> peer = ipc::ReadPeerIdentity(peer_fd);
        if (!peer || ipc::ServerPeerPolicy(*peer) == ipc::PeerVerdict::Refuse) return nullptr;
        ++served;
        return ipc::MakeServeInit(std::make_unique<StubInner>(), "the-cookie", id, slot, {});
    };
    pair.server = interfaces::MakeIpc("ipc-pair-server", std::move(factory));
    std::string listen_address = "unix";
    pair.server->listenAddress(listen_address);

    pair.client = interfaces::MakeIpc("ipc-pair-client", {});
    std::string address = "unix";
    interfaces::ConnectInfo info;
    // Set to the opposite of the expected answer, so the check shows it was written.
    info.server_other_account = true;
    // The shutdown handle is the caller's to close once it is handed over, also
    // if connectAddress then throws, so it is taken through the hook.
    TestFd shutdown_fd;
    info.on_shutdown_fd = [&shutdown_fd](int fd) { shutdown_fd.fd = fd; };
    pair.init = pair.client->connectAddress(address, {}, &info);
    BOOST_REQUIRE(pair.init != nullptr);
    BOOST_CHECK_EQUAL(address, "unix:" + pair.dir + "/node.sock");
    BOOST_CHECK(!info.server_other_account);
    // A second, close-on-exec descriptor of the connection's socket, handed
    // over through the hook and also left in shutdown_fd.
    BOOST_CHECK(shutdown_fd.fd >= 0);
    BOOST_CHECK_EQUAL(info.shutdown_fd, shutdown_fd.fd);
    const int fd_flags = ::fcntl(shutdown_fd.fd, F_GETFD);
    BOOST_CHECK(fd_flags != -1 && (fd_flags & FD_CLOEXEC));

    ipc::HandshakeResult r = ipc::ClientHandshake(*pair.init, "the-cookie", "main");
    BOOST_CHECK(r.ok);
    BOOST_CHECK_EQUAL(served.load(), 1);
}

// A connection that authenticates while another GUI holds the attach slot is
// dropped at the deadline like one that never authenticated, while the attached
// GUI is kept; and the place it held is reclaimed, so a third client is accepted.
// The listener keeps a connection past the deadline only when it is
// authenticated and holds the slot, so of its two places, one holds the attached
// GUI and the other is free again within the deadline.
BOOST_AUTO_TEST_CASE(wire_slotless_connection_is_dropped_at_the_deadline)
{
    interfaces::NodeIdentity id;
    id.network = "main";

    UnixSocket sock;
    auto server = ipc::capnp::MakeCapnpProtocol();
    // Long enough that both handshakes finish inside it on a loaded machine; the
    // case asserts what happens at the deadline, not how soon.
    server->listen(sock.releaseBoundFd(), "ipc-wire-server", CookieServeFactory("cookie", id),
                   /*auth_deadline=*/std::chrono::seconds{3});

    std::atomic<bool> a_dropped{false};
    auto a_proto = ipc::capnp::MakeCapnpProtocol();
    auto a = a_proto->connect(sock.connectClient(), "ipc-wire-gui-a", [&a_dropped] { a_dropped = true; });
    BOOST_REQUIRE(a != nullptr);
    BOOST_REQUIRE_MESSAGE(ipc::ClientHandshake(*a, "cookie", "main").ok,
                          "GUI A's handshake failed; it must finish inside the 3 s authentication deadline");
    BOOST_REQUIRE(a->holdsAttachSlot());

    std::atomic<bool> b_dropped{false};
    auto b_proto = ipc::capnp::MakeCapnpProtocol();
    auto b = b_proto->connect(sock.connectClient(), "ipc-wire-gui-b", [&b_dropped] { b_dropped = true; });
    BOOST_REQUIRE(b != nullptr);
    const ipc::HandshakeResult rb = ipc::ClientHandshake(*b, "cookie", "main");
    BOOST_REQUIRE_MESSAGE(rb.already_attached,
                          "GUI B's handshake did not report the attached GUI (" << rb.error
                              << "); it must finish inside the 3 s authentication deadline");
    BOOST_REQUIRE(!b->holdsAttachSlot());

    // Generous cap: this asserts the drop happens, not how promptly.
    for (int i = 0; i < 400 && !b_dropped; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    BOOST_REQUIRE_MESSAGE(b_dropped, "the node kept an authenticated connection that holds no attach slot");

    // A's deadline came first (it connected first), and A was kept: it still
    // answers, and still holds the slot.
    BOOST_CHECK(!a_dropped);
    bool a_holds = false;
    BOOST_CHECK_NO_THROW(a_holds = a->holdsAttachSlot());
    BOOST_CHECK(a_holds);

    // B's place is reclaimed: a third client is accepted and served the
    // handshake, which tells it about the GUI that still holds the slot.
    auto c_proto = ipc::capnp::MakeCapnpProtocol();
    auto c = c_proto->connect(sock.connectClient(), "ipc-wire-gui-c", /*on_disconnect=*/{});
    BOOST_REQUIRE(c != nullptr);
    const ipc::HandshakeResult rc = ipc::ClientHandshake(*c, "cookie", "main");
    BOOST_REQUIRE_MESSAGE(rc.already_attached,
                          "client C's handshake did not report the attached GUI (" << rc.error
                              << "); it must finish inside the 3 s authentication deadline");
    BOOST_CHECK(!c->holdsAttachSlot());

    c.reset();
    b.reset();
    a.reset();
    server->disconnectIncoming();
    c_proto.reset();
    b_proto.reset();
    a_proto.reset();
    server.reset();
}

// The attached GUI's teardown hook finishes before the core serves another GUI.
//
// The slot hook sleeps before it records that it ran, so a second GUI that could
// claim the slot while the hook was still running would see it unrecorded. GUI B
// retries on a fresh connection until it holds the slot; the moment it does, the
// hook must already have finished. ClientHandshake succeeds only for a GUI that
// holds the slot.
BOOST_AUTO_TEST_CASE(wire_disconnect_runs_slot_hook_before_next_gui)
{
    interfaces::NodeIdentity id;
    id.network = "main";

    // Declared before the server, so it outlives every hook the server runs.
    std::atomic<bool> dropped_a{false};

    UnixSocket sock;
    auto server = ipc::capnp::MakeCapnpProtocol();
    server->listen(sock.releaseBoundFd(), "ipc-wire-server",
                   [id, &dropped_a, slot = ipc::MakeAttachSlot()](int) -> std::unique_ptr<interfaces::Init> {
                       return ipc::MakeServeInit(std::make_unique<StubInner>(), "cookie", id, slot, [&dropped_a] {
                           std::this_thread::sleep_for(std::chrono::milliseconds{200});
                           dropped_a = true;
                       });
                   });

    // GUI A attaches, then goes away: its proxy and its protocol are destroyed.
    {
        auto a_proto = ipc::capnp::MakeCapnpProtocol();
        auto a = a_proto->connect(sock.connectClient(), "ipc-wire-gui-a", /*on_disconnect=*/{});
        BOOST_REQUIRE(a != nullptr);
        ipc::HandshakeResult r = ipc::ClientHandshake(*a, "cookie", "main");
        BOOST_REQUIRE(r.ok);
        BOOST_REQUIRE(a->holdsAttachSlot());
        a.reset();
        a_proto.reset();
    }

    // GUI B tries again on a new connection each time until it holds the slot.
    // The cap only limits a failing run.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
    bool attached = false;
    bool dropped_when_attached = false;
    int attempts = 0;
    while (!attached && std::chrono::steady_clock::now() < deadline) {
        ++attempts;
        auto b_proto = ipc::capnp::MakeCapnpProtocol();
        auto b = b_proto->connect(sock.connectClient(), "ipc-wire-gui-b", /*on_disconnect=*/{});
        BOOST_REQUIRE(b != nullptr);
        ipc::HandshakeResult r = ipc::ClientHandshake(*b, "cookie", "main");
        if (r.ok) {
            // At that moment, with no further wait.
            dropped_when_attached = dropped_a.load();
            attached = true;
        }
        b.reset();
        b_proto.reset();
        // Paces the retries; not part of what is asserted.
        if (!attached) std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    BOOST_REQUIRE_MESSAGE(attached, "GUI B never attached after GUI A went away (" << attempts << " attempts)");
    BOOST_CHECK_MESSAGE(dropped_when_attached,
                        "GUI B attached before GUI A's teardown hook had finished");

    server->disconnectIncoming();
    server.reset();
}

// A second GUI that reaches a core another GUI is attached to passes the
// compatibility checks and is then told, over the wire, that the other GUI is
// attached. The attached GUI keeps the slot.
BOOST_AUTO_TEST_CASE(wire_second_gui_is_told_already_attached)
{
    interfaces::NodeIdentity id;
    id.network = "main";

    UnixSocket sock;
    auto server = ipc::capnp::MakeCapnpProtocol();
    server->listen(sock.releaseBoundFd(), "ipc-wire-server", CookieServeFactory("cookie", id));

    auto a_proto = ipc::capnp::MakeCapnpProtocol();
    auto a = a_proto->connect(sock.connectClient(), "ipc-wire-gui-a", /*on_disconnect=*/{});
    BOOST_REQUIRE(a != nullptr);
    const ipc::HandshakeResult ra = ipc::ClientHandshake(*a, "cookie", "main");
    BOOST_CHECK(ra.ok);
    BOOST_CHECK(!ra.already_attached);

    auto b_proto = ipc::capnp::MakeCapnpProtocol();
    auto b = b_proto->connect(sock.connectClient(), "ipc-wire-gui-b", /*on_disconnect=*/{});
    BOOST_REQUIRE(b != nullptr);
    const ipc::HandshakeResult rb = ipc::ClientHandshake(*b, "cookie", "main");
    BOOST_CHECK(!rb.ok);
    BOOST_CHECK(rb.already_attached);
    BOOST_CHECK_EQUAL(rb.error, ALREADY_ATTACHED_MESSAGE);

    bool a_holds = false;
    BOOST_CHECK_NO_THROW(a_holds = a->holdsAttachSlot());
    BOOST_CHECK(a_holds);

    b.reset();
    a.reset();
    server->disconnectIncoming();
    b_proto.reset();
    a_proto.reset();
    server.reset();
}

// No cookie in the directory: nothing to authenticate with and nothing to dial.
// That is how a core that is still starting looks, so it is retried.
BOOST_AUTO_TEST_CASE(wire_connect_without_cookie_is_no_cookie)
{
    TempIpcDir ipcdir;
    ipc::ConnectFailure failure;
    BOOST_CHECK(!ipc::ConnectToNode(ipcdir.dir, failure).has_value());
    BOOST_CHECK_MESSAGE(failure.kind == ipc::ConnectFailureKind::NoCookie, "failure: " << failure.message);
    BOOST_CHECK(ipc::NextConnectStep(failure, std::chrono::seconds{0}) == ipc::ConnectStep::Retry);
}

// A cookie but nothing listening, with no socket file or with one a crashed core
// left behind: the core is not serving yet, which is retried too.
BOOST_AUTO_TEST_CASE(wire_connect_without_listener_is_not_listening)
{
    TempIpcDir ipcdir;
    ipc::WriteCookie(ipcdir.dir);

    ipc::ConnectFailure absent;
    BOOST_CHECK(!ipc::ConnectToNode(ipcdir.dir, absent).has_value());
    BOOST_CHECK_MESSAGE(absent.kind == ipc::ConnectFailureKind::NotListening, "no socket file: " << absent.message);

    // Bound and closed without listening: the file stays, and a connect to it is
    // refused.
    ::close(BindUnixSocket(ipcdir.dir / "node.sock", /*listen=*/false));
    ipc::ConnectFailure stale;
    BOOST_CHECK(!ipc::ConnectToNode(ipcdir.dir, stale).has_value());
    BOOST_CHECK_MESSAGE(stale.kind == ipc::ConnectFailureKind::NotListening, "stale socket file: " << stale.message);
    BOOST_CHECK(ipc::NextConnectStep(stale, std::chrono::seconds{0}) == ipc::ConnectStep::Retry);
}

// A core that takes the connection into its queue but never answers, not even
// the transport's bootstrap round trip inside connectAddress: the client's
// watchdog gives up after the timeout and says so, and the drop it causes is not
// reported to the GUI as a lost session.
BOOST_AUTO_TEST_CASE(wire_connect_times_out_against_a_mute_listener)
{
    TempIpcDir ipcdir;
    ipc::WriteCookie(ipcdir.dir);
    const int listen_fd = BindUnixSocket(ipcdir.dir / "node.sock", /*listen=*/true);

    // Declared before the attempt, so it outlives the connection that could call it.
    std::atomic<bool> disconnect_reported{false};
    const auto start = std::chrono::steady_clock::now();
    std::future<std::pair<bool, ipc::ConnectFailure>> attempt = std::async(std::launch::async, [&] {
        ipc::ConnectFailure f;
        const bool connected = ipc::ConnectToNode(ipcdir.dir, f, [&disconnect_reported] { disconnect_reported = true; },
                                                  /*handshake_timeout=*/std::chrono::seconds{2})
                                   .has_value();
        return std::make_pair(connected, f);
    });
    const bool returned = attempt.wait_for(std::chrono::seconds{10}) == std::future_status::ready;
    const auto took = std::chrono::steady_clock::now() - start;

    // Closing the listener resets the connection still in its queue. That also
    // ends an attempt that is still waiting, so the case finishes either way.
    ::close(listen_fd);
    const std::pair<bool, ipc::ConnectFailure> result = attempt.get();
    const ipc::ConnectFailure& failure = result.second;

    BOOST_CHECK_MESSAGE(returned, "ConnectToNode had not returned 10 s after a 2 s handshake timeout");
    BOOST_CHECK(took >= std::chrono::seconds{2});
    BOOST_CHECK(!result.first);
    BOOST_CHECK_MESSAGE(failure.kind == ipc::ConnectFailureKind::Timeout, "failure: " << failure.message);
    BOOST_CHECK_EQUAL(failure.message, "The Gridcoin core did not answer within 2 seconds. It may be busy, or another "
                                       "program may be holding its IPC connection.");
    BOOST_CHECK(ipc::NextConnectStep(failure, std::chrono::seconds{0}) == ipc::ConnectStep::GiveUp);
    BOOST_CHECK(!disconnect_reported);
}

// A core that serves the connection but does not answer the handshake: the
// watchdog shuts the connection down during authenticate(), the call fails, and
// the result is the timeout. The connection was set up, so its disconnect
// handler was registered, and the drop the watchdog causes must not reach it.
BOOST_FIXTURE_TEST_CASE(wire_connect_times_out_against_a_stalled_handshake, grc_test::StateGuard)
{
    std::promise<void> release;
    const std::shared_future<void> released = release.get_future().share();
    ServedCore core([released](int) -> std::unique_ptr<interfaces::Init> {
        return std::make_unique<StallingInit>(released);
    });
    // Declared after the core, so the stalled call is released before the core
    // is torn down.
    ReleaseOnExit release_on_exit{release};

    std::atomic<bool> disconnect_reported{false};
    const auto start = std::chrono::steady_clock::now();
    ipc::ConnectFailure failure;
    const bool connected = ipc::ConnectToNode(core.ipcdir.dir, failure,
                                              [&disconnect_reported] { disconnect_reported = true; },
                                              /*handshake_timeout=*/std::chrono::seconds{2})
                               .has_value();
    const auto took = std::chrono::steady_clock::now() - start;

    BOOST_CHECK(!connected);
    BOOST_CHECK_MESSAGE(failure.kind == ipc::ConnectFailureKind::Timeout, "failure: " << failure.message);
    BOOST_CHECK(took >= std::chrono::seconds{2});
    BOOST_CHECK_MESSAGE(took < std::chrono::seconds{10}, "ConnectToNode took "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(took).count()
                        << " ms with a 2 s handshake timeout");
    BOOST_CHECK(!disconnect_reported);
}

// ConnectToNode end to end against a daemon-shaped core. The first GUI attaches
// and learns that the core runs as this account. A second is told the first is
// attached, and its attempt reports no lost session. The attached GUI's
// disconnect handler is armed: when the core drops it, the handler runs.
BOOST_FIXTURE_TEST_CASE(wire_connect_to_node_attaches_one_gui, grc_test::StateGuard)
{
    ServedCore core;

    std::atomic<bool> a_dropped{false};
    ipc::ConnectFailure a_failure;
    std::optional<ipc::GuiConnection> a =
        ipc::ConnectToNode(core.ipcdir.dir, a_failure, [&a_dropped] { a_dropped = true; });
    BOOST_REQUIRE_MESSAGE(a.has_value(), "the first GUI did not attach: " << a_failure.message);
    BOOST_CHECK(a->handshake.ok);
    BOOST_CHECK(!a->core_other_account);
    bool a_holds = false;
    BOOST_CHECK_NO_THROW(a_holds = a->init->holdsAttachSlot());
    BOOST_CHECK(a_holds);

    std::atomic<bool> b_dropped{false};
    ipc::ConnectFailure b_failure;
    BOOST_CHECK(!ipc::ConnectToNode(core.ipcdir.dir, b_failure, [&b_dropped] { b_dropped = true; }).has_value());
    BOOST_CHECK_MESSAGE(b_failure.kind == ipc::ConnectFailureKind::AlreadyAttached, "failure: " << b_failure.message);
    BOOST_CHECK_EQUAL(b_failure.message, ALREADY_ATTACHED_MESSAGE);
    BOOST_CHECK(ipc::NextConnectStep(b_failure, std::chrono::seconds{0}) == ipc::ConnectStep::GiveUp);
    BOOST_CHECK(!b_dropped);
    BOOST_CHECK(!a_dropped);

    core.server->disconnectIncoming();
    // Generous cap: this asserts the handler runs, not how promptly.
    for (int i = 0; i < 400 && !a_dropped; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    BOOST_CHECK_MESSAGE(a_dropped, "the attached GUI's disconnect handler did not run when the core dropped it");

    a.reset();
}

BOOST_AUTO_TEST_SUITE_END()
