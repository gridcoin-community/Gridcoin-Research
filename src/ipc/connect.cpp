// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/connect.h"

#include "chainparams.h"
#include "interfaces/init.h"
#include "interfaces/ipc.h"
#include "ipc/handshake.h"
#include "logging.h"
#include "sync.h"
#include "threadsafety.h"
#include "tinyformat.h"
#include "util/threadnames.h"
#include "util/syserror.h"

#include <condition_variable>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#ifndef WIN32
#include <sys/socket.h> // shutdown
#include <unistd.h>     // close
#else
#include <winsock2.h> // shutdown, closesocket
#endif

namespace ipc {
namespace {

//! Holds a GUI's on_disconnect back until its handshake has succeeded. Until
//! then a drop -- the core going away during the handshake, or the watchdog
//! shutting a silent connection down -- is the failure ConnectToNode reports,
//! not a lost session, so it must not reach the GUI's handler, which quits the
//! GUI. The mutex is a leaf: the handler runs after it is released.
class DisconnectGate
{
public:
    explicit DisconnectGate(std::function<void()> on_disconnect) : m_on_disconnect(std::move(on_disconnect)) {}

    //! The connection dropped. Runs on the IPC event-loop thread.
    void Dropped() LOCKS_EXCLUDED(m_mutex)
    {
        std::function<void()> on_disconnect;
        {
            LOCK(m_mutex);
            m_dropped = true;
            if (!m_armed) return;
            on_disconnect = std::move(m_on_disconnect);
        }
        if (on_disconnect) on_disconnect();
    }

    //! Pass a later drop on to the handler. Returns false, arming nothing, when
    //! the connection has already dropped.
    bool Arm() LOCKS_EXCLUDED(m_mutex)
    {
        LOCK(m_mutex);
        if (m_dropped) return false;
        m_armed = true;
        return true;
    }

private:
    Mutex m_mutex;
    bool m_armed GUARDED_BY(m_mutex){false};
    bool m_dropped GUARDED_BY(m_mutex){false};
    std::function<void()> m_on_disconnect GUARDED_BY(m_mutex);
};

//! Shut the socket \p fd down in both directions. The connection's reader then
//! sees the end of the stream, so a call waiting on the connection, the
//! bootstrap included, fails as a disconnect.
void ShutdownSocket(int fd)
{
#ifdef WIN32
    ::shutdown(static_cast<SOCKET>(fd), SD_BOTH);
#else
    ::shutdown(fd, SHUT_RDWR);
#endif
}

//! Owns the connection's shutdown handle (ConnectInfo::shutdown_fd) once
//! adopted, and closes it when this goes out of scope.
class ShutdownFd
{
public:
    ShutdownFd() = default;
    ~ShutdownFd() { Close(); }
    ShutdownFd(const ShutdownFd&) = delete;
    ShutdownFd& operator=(const ShutdownFd&) = delete;

    void Adopt(int fd) { m_fd = fd; }
    bool Valid() const { return m_fd != -1; }

    void Close()
    {
        if (m_fd == -1) return;
#ifdef WIN32
        // An lvalue, as FdGuard in ipc/interfaces.cpp passes: where compat.h is
        // in view, its closesocket() takes a SOCKET&.
        SOCKET s = static_cast<SOCKET>(m_fd);
        closesocket(s);
#else
        ::close(m_fd);
#endif
        m_fd = -1;
    }

private:
    int m_fd{-1};
};

//! Shuts a connection down if connecting to the core and the handshake have not
//! finished in time.
//!
//! The clock starts before the socket exists, because the first round trip, the
//! transport's bootstrap (Init.construct), is made inside connectAddress. The
//! socket arrives through Watch() from inside connectAddress, before that round
//! trip, and is shut down at the deadline, or at once if the deadline has passed.
//!
//! The IPC calls are made by the thread that calls ConnectToNode, not by this
//! one. libmultiprocess keeps per-thread client state for each connection a
//! thread has called on, and that state is torn down by two paths that do not
//! exclude each other: by the event loop when it deletes the connection, and by
//! the thread itself when it exits. The watchdog's own shutdown makes the loop
//! delete the connection, so a thread that made the calls must not exit then.
//! The calling thread outlives the connection on every failure (ConnectToNode
//! destroys the connection before it returns), and this thread makes no IPC
//! calls, so its exit has nothing to tear down. Its mutex is a leaf.
class HandshakeWatchdog
{
public:
    explicit HandshakeWatchdog(std::chrono::seconds timeout)
        : m_thread([this, timeout] {
              util::ThreadRename("ipc-watchdog");
              Run(timeout);
          })
    {
    }
    ~HandshakeWatchdog() { Finish(); }
    HandshakeWatchdog(const HandshakeWatchdog&) = delete;
    HandshakeWatchdog& operator=(const HandshakeWatchdog&) = delete;

    //! The socket to shut down. The caller keeps it open until Finish() has
    //! returned.
    void Watch(int fd) LOCKS_EXCLUDED(m_mutex)
    {
        LOCK(m_mutex);
        m_fd = fd;
        if (m_expired) ShutdownOnce();
    }

    //! Stop the clock and join the thread. Returns whether the connection was
    //! shut down.
    bool Finish() LOCKS_EXCLUDED(m_mutex)
    {
        {
            LOCK(m_mutex);
            m_finished = true;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) m_thread.join();
        LOCK(m_mutex);
        return m_shut_down;
    }

private:
    void Run(std::chrono::seconds timeout) LOCKS_EXCLUDED(m_mutex)
    {
        WAIT_LOCK(m_mutex, lock);
        if (m_cv.wait_for(lock, timeout, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return m_finished; })) return;
        m_expired = true;
        ShutdownOnce();
    }

    void ShutdownOnce() EXCLUSIVE_LOCKS_REQUIRED(m_mutex)
    {
        if (m_fd == -1 || m_shut_down) return;
        ShutdownSocket(m_fd);
        m_shut_down = true;
    }

    Mutex m_mutex;
    std::condition_variable m_cv;
    bool m_finished GUARDED_BY(m_mutex){false};
    bool m_expired GUARDED_BY(m_mutex){false};
    bool m_shut_down GUARDED_BY(m_mutex){false};
    int m_fd GUARDED_BY(m_mutex){-1};
    //! Last, so the members above exist before the thread starts.
    std::thread m_thread;
};

} // namespace

std::optional<GuiConnection> ConnectToNode(const fs::path& ipc_dir, ConnectFailure& failure_out,
                                           std::function<void()> on_disconnect,
                                           std::chrono::seconds handshake_timeout)
{
    // 1. The node writes a fresh cookie on every -multiprocess startup and
    //    deletes it on a clean shutdown; its absence means no node is serving
    //    this datadir (none is running with -multiprocess, one is still
    //    starting, or the last one shut down cleanly), so there is nothing to
    //    dial.
    const CookieRead cookie_read = ReadCookie(ipc_dir);
    if (cookie_read.status == CookieStatus::Unreadable) {
        // The core is there, and this account may not read what lets it in. NextConnectStep tries
        // again for a moment, for a cookie the core is replacing; a denial that lasts needs signing
        // in again, for an account just added to the attach group.
        failure_out = {ConnectFailureKind::CookieDenied,
                       strprintf("the Gridcoin core is there, but this account may not read its IPC cookie %s (%s). "
                                 "If this account was just added to the group allowed to attach, sign out and sign in "
                                 "again: group membership is read at sign-in.",
                                 (ipc_dir / "ipc.cookie").string(), cookie_read.error)};
        return std::nullopt;
    }
    if (!cookie_read.cookie) {
        failure_out = {ConnectFailureKind::NoCookie,
                       strprintf("no IPC cookie in %s -- is the Gridcoin daemon running with -multiprocess?",
                                 ipc_dir.string())};
        return std::nullopt;
    }
    const std::string& cookie = *cookie_read.cookie;

    GuiConnection conn;
    // A connect-only GUI never listens, so it serves no Init: pass an empty
    // serve-init factory (MakeIpc only ever calls it on the listen path).
    conn.ipc = interfaces::MakeIpc("gridcoinresearch", {});

    // From here to the end of the handshake the watchdog bounds how long the
    // core may take. The shutdown handle is closed when this returns, after the
    // watchdog's thread has been joined (it is declared first, so it is
    // destroyed last).
    ShutdownFd shutdown_fd;
    std::optional<HandshakeWatchdog> watchdog;
    try {
        watchdog.emplace(handshake_timeout);
    } catch (const std::system_error& e) {
        failure_out = {ConnectFailureKind::Handshake,
                       strprintf("could not start the IPC handshake watchdog: %s", AsciiWhat(e))};
        return std::nullopt;
    }

    // 2. Connect to <ipc_dir>/node.sock. connectAddress checks the process at
    //    the other end before anything is sent to it, and hands the watchdog
    //    the socket before its bootstrap round trip.
    std::string address = "unix:" + (ipc_dir / "node.sock").string();
    const auto gate = std::make_shared<DisconnectGate>(std::move(on_disconnect));
    interfaces::ConnectInfo info;
    // The duplicate is adopted as it is handed over, so it is closed on every
    // path, whatever connectAddress then throws.
    info.on_shutdown_fd = [&shutdown_fd, &watchdog](int fd) {
        shutdown_fd.Adopt(fd);
        watchdog->Watch(fd);
    };
    std::optional<ConnectFailure> failure;
    try {
        conn.init = conn.ipc->connectAddress(address, [gate] { gate->Dropped(); }, &info);
    } catch (const interfaces::PeerRefusedError& e) {
        failure = ConnectFailure{ConnectFailureKind::PeerRefused,
                                 strprintf("could not connect to the daemon on %s: %s", address, AsciiWhat(e))};
    } catch (const std::system_error& e) {
        // Nothing listens on the path: there is no socket file, or a stopped
        // core left one behind.
        const bool not_listening = e.code() == std::errc::connection_refused ||
                                   e.code() == std::errc::no_such_file_or_directory;
        failure = ConnectFailure{not_listening ? ConnectFailureKind::NotListening : ConnectFailureKind::Handshake,
                                 strprintf("could not connect to the daemon on %s: %s", address, AsciiWhat(e))};
    } catch (const std::exception& e) {
        failure = ConnectFailure{ConnectFailureKind::Handshake,
                                 strprintf("could not connect to the daemon on %s: %s", address, AsciiWhat(e))};
    }
    if (!failure && !conn.init) {
        failure = ConnectFailure{ConnectFailureKind::NotListening,
                                 strprintf("no daemon is listening on %s", address)};
    }
    if (!failure && !shutdown_fd.Valid()) {
        LogPrintf("WARN: IPC: the connection on %s cannot be shut down from another thread, so its handshake "
                  "is not time-bounded", address);
    }

    // 3. Authenticate, verify compatibility (schema/protocol/network) and the
    //    attach slot, and fetch the node's build + identity. Identity *binding*
    //    (vs the GUI's stored expectation) and the git_commit soft-warn banner
    //    are the caller's GUI-side steps, driven off conn.handshake.
    HandshakeResult hr;
    if (!failure) hr = ClientHandshake(*conn.init, cookie, Params().NetworkIDString());
    const bool timed_out = watchdog->Finish();
    shutdown_fd.Close();

    // A failure the watchdog's shutdown caused is reported as the timeout.
    if (timed_out) {
        failure_out = {ConnectFailureKind::Timeout,
                       strprintf("The Gridcoin core did not answer within %d seconds. It may be busy, or another "
                                 "program may be holding its IPC connection.",
                                 handshake_timeout.count())};
        return std::nullopt;
    }
    if (failure) {
        failure_out = std::move(*failure);
        return std::nullopt;
    }
    if (!hr.ok) {
        failure_out = {hr.already_attached ? ConnectFailureKind::AlreadyAttached : ConnectFailureKind::Handshake,
                       hr.error};
        return std::nullopt;
    }
    // From here on a drop is a lost session, for the caller's handler. One that
    // came before it is this call's failure instead.
    if (!gate->Arm()) {
        failure_out = {ConnectFailureKind::Handshake, "Lost connection to the daemon right after the handshake."};
        return std::nullopt;
    }
    conn.handshake = std::move(hr);
    conn.core_other_account = info.server_other_account;

    return conn;
}

} // namespace ipc
