// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_UTIL_SDNOTIFY_H
#define GRIDCOIN_UTIL_SDNOTIFY_H

#include <condition_variable>
#include <mutex>
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

//! Asks the service manager to extend the start timeout, with a status line.
bool ExtendStartTimeout(const std::string& status);

//! Sends an extension immediately and then every 60 seconds until destroyed, so
//! that one long startup step does not run into the start timeout. The thread
//! only calls Send().
class ScopedStartupHeartbeat
{
public:
    explicit ScopedStartupHeartbeat(std::string status);
    ~ScopedStartupHeartbeat();

    ScopedStartupHeartbeat(const ScopedStartupHeartbeat&) = delete;
    ScopedStartupHeartbeat& operator=(const ScopedStartupHeartbeat&) = delete;

private:
    void Run();

    const std::string m_status;
    // Leaf lock, taken only around the stop flag.
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
    std::thread m_thread;
};

//! Clears the captured address and the failure latch.
void ResetForTesting();

} // namespace sdnotify

#endif // GRIDCOIN_UTIL_SDNOTIFY_H
