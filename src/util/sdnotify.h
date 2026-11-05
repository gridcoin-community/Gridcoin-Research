// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_SDNOTIFY_H
#define GRIDCOIN_UTIL_SDNOTIFY_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

//! Minimal service manager notification (the sd_notify datagram protocol) with no
//! dependency on libsystemd.
//!
//! Every function is a no-op returning false off Linux, or when no usable
//! NOTIFY_SOCKET was captured. MAINPID is never sent.
namespace sdnotify {

//! Reads, validates and removes NOTIFY_SOCKET from the environment. A value that
//! starts with '/' is a filesystem path, one that starts with '@' is in the
//! abstract namespace. Anything else, or a value that does not fit sun_path,
//! leaves the module inactive with one log line. Must be called while the process
//! is single-threaded, early in main().
void CaptureFromEnvironment();

//! True when a usable notification address was captured.
bool Active();

//! Sends one datagram. Logs the first failure only.
bool Send(const std::string& state);

//! "READY=1" with a status line.
bool NotifyReady();

//! "STOPPING=1" with a status line.
bool NotifyStopping();

//! "STATUS=Startup failed; see debug.log", for a start that failed before READY=1.
//! The daemon sends it once, before Shutdown().
bool NotifyStartupFailed();

//! The startup error for a start that would daemonize (\p daemonize: -daemon or
//! -daemonwait) while a notification address was captured, or nullopt. A notify
//! unit cannot work with a forking daemon: under NotifyAccess=main the forked
//! child's READY=1 is dropped, and the parent's exit ends the unit. The text names
//! -nodaemon.
std::optional<std::string> DaemonizeRefusal(bool daemonize);

//! How far one extension moves the start timeout, counted from when it is sent: 30
//! minutes.
static constexpr std::chrono::microseconds START_EXTENSION{std::chrono::minutes{30}};

//! The longest a ScopedStartupHeartbeat keeps the start timeout away, counted from
//! the start of its step: 4 hours. Loading the block index of a full mainnet chain
//! (4.13 to 4.15 million blocks) took 17 to 30 seconds in ten starts of one node in
//! October 2026, and doc/block_corruption_recovery_design.md puts a -reindex at 10 to
//! 20 minutes on a hard disk. Four hours leaves room for much slower hardware, while a
//! step that hangs still ends the start.
static constexpr std::chrono::milliseconds MAX_STARTUP_STEP_EXTENSION{std::chrono::hours{4}};

//! Sets the function that says whether a shutdown has been requested. Once it
//! returns true no extension is sent, by ExtendStartTimeout or by a heartbeat, so a
//! stop requested during startup runs into the stop timeout instead of being
//! extended with the start. Set it once in main(), before any thread can send;
//! nullptr clears it.
void SetShutdownCheck(bool (*shutdown_requested)());

//! Asks the service manager to extend the start timeout by \p extension from now,
//! with a status line. Sends nothing once a shutdown has been requested.
bool ExtendStartTimeout(const std::string& status, std::chrono::microseconds extension = START_EXTENSION);

//! Sends an extension immediately and then every \p interval until destroyed, so
//! that one long startup step does not run into the start timeout. Each extension
//! is three intervals (3 minutes for the default interval), and at most what is left
//! of \p max_total, counted from construction, and none is
//! sent once that is used up or a shutdown has been requested: a step that hangs
//! ends the start at most \p max_total after it began. The steps it covers have no
//! progress counter another thread can read safely, so it does not wait for one.
//! The thread only calls Send().
class ScopedStartupHeartbeat
{
public:
    explicit ScopedStartupHeartbeat(std::string status,
                                    std::chrono::milliseconds max_total = MAX_STARTUP_STEP_EXTENSION,
                                    std::chrono::milliseconds interval = std::chrono::seconds{60});
    ~ScopedStartupHeartbeat();

    ScopedStartupHeartbeat(const ScopedStartupHeartbeat&) = delete;
    ScopedStartupHeartbeat& operator=(const ScopedStartupHeartbeat&) = delete;

private:
    void Run();

    const std::string m_status;
    const std::chrono::milliseconds m_max_total;
    const std::chrono::milliseconds m_interval;
    // Leaf lock, taken only around the stop flag.
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
    std::thread m_thread;
};

//! Clears the captured address, the failure latch and the shutdown check.
void ResetForTesting();

} // namespace sdnotify

#endif // GRIDCOIN_UTIL_SDNOTIFY_H
