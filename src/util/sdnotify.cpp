// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/sdnotify.h>

#include <logging.h>
#include <util/string.h>
#include <util/threadnames.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <utility>

#ifdef __linux__
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace sdnotify {

// Longest STATUS= text sent, in bytes.
static constexpr size_t MAX_STATUS_BYTES = 200;

namespace {
// Set from main() before any thread sends, or by the tests.
std::atomic<bool (*)()> g_shutdown_check{nullptr};

bool ShutdownCheckSaysStop()
{
    bool (*const check)() = g_shutdown_check.load();
    return check != nullptr && check();
}
} // namespace

#ifdef __linux__

namespace {
// Written by CaptureFromEnvironment() and ResetForTesting() only, which run while
// no other thread uses this module.
bool g_active = false;
sockaddr_un g_addr;
socklen_t g_addr_len = 0;
std::atomic<bool> g_failure_logged{false};
} // namespace

void CaptureFromEnvironment()
{
    g_active = false;
    g_addr_len = 0;
    g_failure_logged = false;

    const char* env = getenv("NOTIFY_SOCKET");
    if (env == nullptr) return;
    const std::string value(env);

    // The variable must not reach children.
    unsetenv("NOTIFY_SOCKET");

    if (value.empty() || (value[0] != '/' && value[0] != '@')) {
        LogPrintf("sdnotify: NOTIFY_SOCKET is neither an absolute path nor an abstract name, notifications disabled\n");
        return;
    }
    if (value.size() > sizeof(g_addr.sun_path) - 1) {
        LogPrintf("sdnotify: NOTIFY_SOCKET is too long, notifications disabled\n");
        return;
    }

    memset(&g_addr, 0, sizeof(g_addr));
    g_addr.sun_family = AF_UNIX;
    memcpy(g_addr.sun_path, value.data(), value.size());
    if (value[0] == '@') {
        // Abstract namespace. The length covers the NUL and the name, not the
        // whole structure.
        g_addr.sun_path[0] = '\0';
        g_addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + value.size());
    } else {
        g_addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + value.size() + 1);
    }
    g_active = true;
}

bool Active()
{
    return g_active;
}

bool Send(const std::string& state)
{
    if (!g_active) return false;

    const int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        if (!g_failure_logged.exchange(true)) {
            LogPrintf("sdnotify: socket() failed (errno %d), further failures not logged\n", errno);
        }
        return false;
    }

    const ssize_t n = sendto(fd, state.data(), state.size(), MSG_NOSIGNAL,
                             reinterpret_cast<const sockaddr*>(&g_addr), g_addr_len);
    const int saved_errno = errno;
    close(fd);

    if (n < 0 || static_cast<size_t>(n) != state.size()) {
        if (!g_failure_logged.exchange(true)) {
            LogPrintf("sdnotify: sendto() failed (errno %d), further failures not logged\n", saved_errno);
        }
        return false;
    }
    return true;
}

void ResetForTesting()
{
    g_active = false;
    g_addr_len = 0;
    g_failure_logged = false;
    g_shutdown_check = nullptr;
}

#else // !__linux__

void CaptureFromEnvironment() {}
bool Active() { return false; }
bool Send(const std::string&) { return false; }
void ResetForTesting()
{
    g_shutdown_check = nullptr;
}

#endif // __linux__

bool NotifyReady()
{
    return Send("READY=1\nSTATUS=Running");
}

bool NotifyStopping()
{
    return Send("STOPPING=1\nSTATUS=Shutting down");
}

bool NotifyStartupFailed()
{
    return Send("STATUS=Startup failed; see debug.log");
}

std::optional<std::string> DaemonizeRefusal(bool daemonize)
{
    if (!daemonize || !Active()) return std::nullopt;

    // The service manager takes readiness from the process it started, and the
    // daemonizing parent exits as soon as the child reports, which ends the unit.
    return std::string{"-daemon and -daemonwait cannot be used under a service manager that expects a readiness "
                       "notification (NOTIFY_SOCKET is set): the forked process cannot report it. Start with "
                       "-nodaemon -nodaemonwait, which also override daemon=1 in the configuration file."};
}

void SetShutdownCheck(bool (*shutdown_requested)())
{
    g_shutdown_check = shutdown_requested;
}

bool ExtendStartTimeout(const std::string& status, std::chrono::microseconds extension)
{
    if (!Active()) return false;
    if (ShutdownCheckSaysStop()) return false;

    // Cut on a UTF-8 code point boundary, so that a partial sequence is never sent.
    size_t length = std::min(status.size(), MAX_STATUS_BYTES);
    if (length < status.size()) {
        while (length > 0 && (static_cast<unsigned char>(status[length]) & 0xC0) == 0x80) --length;
    }

    // A control character would end or split the datagram line, so each is
    // replaced with a space.
    std::string text = status.substr(0, length);
    for (char& c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20 || byte == 0x7f) c = ' ';
    }
    return Send("EXTEND_TIMEOUT_USEC=" + ToString(extension.count()) + "\nSTATUS=" + text);
}

ScopedStartupHeartbeat::ScopedStartupHeartbeat(std::string status, std::chrono::milliseconds max_total,
                                               std::chrono::milliseconds interval)
    : m_status(std::move(status)), m_max_total(max_total), m_interval(interval)
{
    if (!Active()) return;
    m_thread = std::thread([this] { Run(); });
}

ScopedStartupHeartbeat::~ScopedStartupHeartbeat()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void ScopedStartupHeartbeat::Run()
{
    util::ThreadRename("sdnotify");

    using std::chrono::microseconds;
    const auto start = std::chrono::steady_clock::now();
    const microseconds max_total = std::chrono::duration_cast<microseconds>(m_max_total);

    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_stop) {
        // A stop requested during this step must run into the stop timeout.
        if (ShutdownCheckSaysStop()) return;

        // No extension reaches past max_total from the start of the step.
        const microseconds remaining =
            max_total - std::chrono::duration_cast<microseconds>(std::chrono::steady_clock::now() - start);
        if (remaining.count() <= 0) {
            LogPrintf("sdnotify: %s has run for %d seconds; the start timeout is no longer extended for it\n",
                      m_status, std::chrono::duration_cast<std::chrono::seconds>(m_max_total).count());
            return;
        }

        // Only Send() is called from here, never wallet or chain code.
        lock.unlock();
        ExtendStartTimeout(m_status, std::min(START_EXTENSION, remaining));
        lock.lock();
        m_cv.wait_for(lock, m_interval, [this] { return m_stop; });
    }
}

} // namespace sdnotify
