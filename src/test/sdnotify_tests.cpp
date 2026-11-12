// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <logging.h>
#include <util/sdnotify.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <tinyformat.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#include <cstdlib>
#include <cstring>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
//! Restores NOTIFY_SOCKET and clears the module state on every exit path.
class NotifyEnvGuard
{
public:
    NotifyEnvGuard()
    {
        const char* old = getenv("NOTIFY_SOCKET");
        if (old != nullptr) {
            m_had_value = true;
            m_old_value = old;
        }
        unsetenv("NOTIFY_SOCKET");
        sdnotify::ResetForTesting();
    }

    ~NotifyEnvGuard()
    {
        sdnotify::ResetForTesting();
        if (m_had_value) {
            setenv("NOTIFY_SOCKET", m_old_value.c_str(), 1);
        } else {
            unsetenv("NOTIFY_SOCKET");
        }
    }

    NotifyEnvGuard(const NotifyEnvGuard&) = delete;
    NotifyEnvGuard& operator=(const NotifyEnvGuard&) = delete;

private:
    bool m_had_value = false;
    std::string m_old_value;
};

//! A temporary directory, removed with its socket file at scope end.
class TempDir
{
public:
    TempDir()
    {
        char tmpl[] = "/tmp/grc-sdnotify-XXXXXX";
        const char* dir = mkdtemp(tmpl);
        if (dir != nullptr) m_path = dir;
    }

    ~TempDir()
    {
        if (m_path.empty()) return;
        unlink((m_path + "/notify").c_str());
        rmdir(m_path.c_str());
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::string& Path() const { return m_path; }

private:
    std::string m_path;
};

//! A bound datagram socket with a bounded receive timeout.
class Listener
{
public:
    //! name starts with '/' for a path socket or '@' for the abstract namespace.
    explicit Listener(const std::string& name)
    {
        m_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (m_fd < 0) return;

        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, name.data(), name.size());
        if (name[0] == '@') addr.sun_path[0] = '\0';
        const socklen_t len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + name.size());

        if (bind(m_fd, reinterpret_cast<const sockaddr*>(&addr), len) != 0) {
            close(m_fd);
            m_fd = -1;
            return;
        }

        timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    ~Listener()
    {
        if (m_fd >= 0) close(m_fd);
    }

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    bool Ok() const { return m_fd >= 0; }

    //! Sets how long Receive() waits for a datagram.
    void SetTimeout(std::chrono::milliseconds timeout)
    {
        timeval tv;
        tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
        tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
        setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    //! One datagram, or the empty string on timeout.
    std::string Receive() const
    {
        char buf[1024];
        const ssize_t n = recv(m_fd, buf, sizeof(buf), 0);
        if (n < 0) return std::string();
        return std::string(buf, static_cast<size_t>(n));
    }

private:
    int m_fd = -1;
};
//! What the shutdown check under test returns.
std::atomic<bool> g_test_shutdown{false};

bool TestShutdownRequested()
{
    return g_test_shutdown.load();
}

//! Installs TestShutdownRequested as the shutdown check, with no shutdown requested,
//! and clears it at scope end.
class ShutdownCheckGuard
{
public:
    ShutdownCheckGuard()
    {
        g_test_shutdown = false;
        sdnotify::SetShutdownCheck(TestShutdownRequested);
    }

    ~ShutdownCheckGuard()
    {
        sdnotify::SetShutdownCheck(nullptr);
        g_test_shutdown = false;
    }

    ShutdownCheckGuard(const ShutdownCheckGuard&) = delete;
    ShutdownCheckGuard& operator=(const ShutdownCheckGuard&) = delete;
};

//! The EXTEND_TIMEOUT_USEC value of an extension datagram, or -1 when it is not one.
long long ExtensionUsec(const std::string& message)
{
    const std::string key = "EXTEND_TIMEOUT_USEC=";
    if (message.compare(0, key.size(), key) != 0) return -1;
    // The value ends at the line break before STATUS=.
    const size_t end = message.find('\n', key.size());
    return ToIntegral<long long>(message.substr(key.size(), end - key.size())).value_or(-1);
}
} // namespace
#endif // __linux__

BOOST_AUTO_TEST_SUITE(sdnotify_tests)

#ifdef __linux__

BOOST_AUTO_TEST_CASE(capture_unsets_environment)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(getenv("NOTIFY_SOCKET") == nullptr);
    BOOST_CHECK(sdnotify::Active());
}

BOOST_AUTO_TEST_CASE(ready_sends_ready_and_no_mainpid)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(sdnotify::NotifyReady());
    const std::string msg = listener.Receive();
    BOOST_CHECK_EQUAL(msg, "READY=1\nSTATUS=Running");
    BOOST_CHECK(msg.compare(0, 7, "READY=1") == 0);
    BOOST_CHECK(msg.find("MAINPID=") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(stopping_message)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(sdnotify::NotifyStopping());
    BOOST_CHECK_EQUAL(listener.Receive(), "STOPPING=1\nSTATUS=Shutting down");
}

BOOST_AUTO_TEST_CASE(abstract_address_is_supported)
{
    NotifyEnvGuard env;
    const std::string name = strprintf("@grc-sdnotify-test-%d", static_cast<int>(getpid()));
    Listener listener(name);
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", name.c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_REQUIRE(sdnotify::Active());
    BOOST_CHECK(sdnotify::NotifyReady());
    BOOST_CHECK_EQUAL(listener.Receive(), "READY=1\nSTATUS=Running");
}

BOOST_AUTO_TEST_CASE(invalid_address_is_inactive)
{
    NotifyEnvGuard env;

    setenv("NOTIFY_SOCKET", "relative/path", 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(getenv("NOTIFY_SOCKET") == nullptr);
    BOOST_CHECK(!sdnotify::Active());
    BOOST_CHECK(!sdnotify::NotifyReady());
}

BOOST_AUTO_TEST_CASE(overlong_address_is_inactive)
{
    NotifyEnvGuard env;

    const std::string name = "/" + std::string(sizeof(sockaddr_un::sun_path), 'a');
    setenv("NOTIFY_SOCKET", name.c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(!sdnotify::Active());
    BOOST_CHECK(!sdnotify::NotifyReady());
}

BOOST_AUTO_TEST_CASE(nothing_captured_is_inactive)
{
    NotifyEnvGuard env;

    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(!sdnotify::Active());
    BOOST_CHECK(!sdnotify::Send("READY=1"));
}

BOOST_AUTO_TEST_CASE(extend_sanitizes_status)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(sdnotify::ExtendStartTimeout("a\nb"));
    BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=1800000000\nSTATUS=a b");

    // Every control character is replaced, including tab, carriage return, escape and DEL.
    BOOST_CHECK(sdnotify::ExtendStartTimeout(std::string("a\tb\rc\x1b" "d\x7f" "e\x01" "f", 11)));
    BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=1800000000\nSTATUS=a b c d e f");

    // The cut at 200 bytes never leaves a partial UTF-8 sequence. The two byte
    // character straddles the limit, so it is dropped whole.
    BOOST_CHECK(sdnotify::ExtendStartTimeout(std::string(199, 'x') + "\xc3\xa9" + "yyy"));
    BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=1800000000\nSTATUS=" + std::string(199, 'x'));

    // A character that ends exactly at the limit is kept.
    BOOST_CHECK(sdnotify::ExtendStartTimeout(std::string(198, 'x') + "\xc3\xa9" + "yyy"));
    BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=1800000000\nSTATUS=" + std::string(198, 'x') + "\xc3\xa9");
}

BOOST_AUTO_TEST_CASE(extend_truncates_status)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(sdnotify::ExtendStartTimeout(std::string(500, 'x')));
    BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=1800000000\nSTATUS=" + std::string(200, 'x'));
}

BOOST_AUTO_TEST_CASE(heartbeat_sends_immediately)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    {
        // Each beat extends by three beat intervals: 3 minutes for the default 60 seconds.
        sdnotify::ScopedStartupHeartbeat heartbeat("Loading block index");
        BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=180000000\nSTATUS=Loading block index");
    }
}

BOOST_AUTO_TEST_CASE(startup_failed_status)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(sdnotify::NotifyStartupFailed());
    const std::string msg = listener.Receive();
    BOOST_CHECK_EQUAL(msg, "STATUS=Startup failed; see debug.log");
    BOOST_CHECK(msg.find("READY=1") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(startup_stopped_status)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(sdnotify::NotifyStartupStopped());
    const std::string msg = listener.Receive();
    BOOST_CHECK_EQUAL(msg, "STATUS=Startup stopped");
    BOOST_CHECK(msg.find("READY=1") == std::string::npos);
}

// A daemonizing start cannot report readiness to a notify unit, so it is refused
// when a notification address was captured, with a text that names -nodaemon. A
// start that does not daemonize, or one with no address, is not refused.
BOOST_AUTO_TEST_CASE(daemonize_refused_under_a_notify_socket)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    BOOST_CHECK(!sdnotify::DaemonizeRefusal(true));

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();
    BOOST_REQUIRE(sdnotify::Active());

    const std::optional<std::string> refusal = sdnotify::DaemonizeRefusal(true);
    BOOST_REQUIRE(refusal);
    BOOST_CHECK_MESSAGE(refusal->find("-nodaemon") != std::string::npos, *refusal);
    BOOST_CHECK(!sdnotify::DaemonizeRefusal(false));
}

// Once a shutdown has been requested no extension is sent, from a progress message
// or from a heartbeat, so a stop during startup runs into the stop timeout.
BOOST_AUTO_TEST_CASE(no_extension_once_a_shutdown_is_requested)
{
    NotifyEnvGuard env;
    ShutdownCheckGuard check;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());
    listener.SetTimeout(std::chrono::milliseconds{300});

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    BOOST_CHECK(sdnotify::ExtendStartTimeout("Loading wallet"));
    BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=1800000000\nSTATUS=Loading wallet");

    {
        sdnotify::ScopedStartupHeartbeat heartbeat("Rescanning", std::chrono::hours{1}, std::chrono::milliseconds{20});
        BOOST_CHECK_EQUAL(listener.Receive(), "EXTEND_TIMEOUT_USEC=60000\nSTATUS=Rescanning");

        g_test_shutdown = true;
        // A beat that was already past the check may still arrive; nothing comes after it.
        listener.Receive();
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        listener.SetTimeout(std::chrono::milliseconds{50});
        while (!listener.Receive().empty()) {}
        listener.SetTimeout(std::chrono::milliseconds{300});
        BOOST_CHECK_EQUAL(listener.Receive(), "");
    }

    BOOST_CHECK(!sdnotify::ExtendStartTimeout("Loading wallet"));
    BOOST_CHECK_EQUAL(listener.Receive(), "");

    {
        sdnotify::ScopedStartupHeartbeat heartbeat("Rescanning", std::chrono::hours{1}, std::chrono::milliseconds{20});
        BOOST_CHECK_EQUAL(listener.Receive(), "");
    }
}

// A heartbeat moves the start timeout no further than its cap from its own start:
// each extension is at most what is left of the cap, and none is sent after it.
BOOST_AUTO_TEST_CASE(heartbeat_extension_is_capped)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    constexpr auto cap = std::chrono::milliseconds{400};
    constexpr auto interval = std::chrono::milliseconds{50};

    std::vector<long long> extensions;
    {
        sdnotify::ScopedStartupHeartbeat heartbeat("Loading block index", cap, interval);
        listener.SetTimeout(std::chrono::milliseconds{300});
        for (std::string message = listener.Receive(); !message.empty(); message = listener.Receive()) {
            extensions.push_back(ExtensionUsec(message));
            BOOST_CHECK_MESSAGE(message.find("\nSTATUS=Loading block index") != std::string::npos, message);
        }
        // The heartbeat is still in scope here, so it stopped by itself.
    }

    BOOST_REQUIRE(!extensions.empty());
    // At most one beat per interval while the cap lasts, with some slack for scheduling.
    BOOST_CHECK_LE(extensions.size(), static_cast<size_t>(cap / interval) + 2);
    long long previous = 0;
    for (const long long usec : extensions) {
        BOOST_CHECK_GT(usec, 0);
        BOOST_CHECK_LE(usec, std::chrono::duration_cast<std::chrono::microseconds>(cap).count());
        // A beat extends by at most three beat intervals, so one that passed the stop
        // check just before a stop extends the stop by little.
        BOOST_CHECK_LE(usec, std::chrono::duration_cast<std::chrono::microseconds>(3 * interval).count());
        if (previous != 0) BOOST_CHECK_LE(usec, previous);
        previous = usec;
    }
}

// A step that reports its progress (a rescan) is extended only while the progress advances:
// each beat that finds it changed extends, by three intervals, and once it stops advancing the
// beats send nothing, so the extension runs out within about three intervals. Such a step has
// no cap.
BOOST_AUTO_TEST_CASE(heartbeat_with_progress_extends_only_while_advancing)
{
    NotifyEnvGuard env;
    TempDir dir;
    BOOST_REQUIRE(!dir.Path().empty());
    Listener listener(dir.Path() + "/notify");
    BOOST_REQUIRE(listener.Ok());

    setenv("NOTIFY_SOCKET", (dir.Path() + "/notify").c_str(), 1);
    sdnotify::CaptureFromEnvironment();

    // The heartbeat logs one line when a beat finds no progress; that is the moment the
    // stall is seen, and nothing is sent after it while the progress stays still. The
    // callback runs on the heartbeat's thread.
    std::atomic<bool> stall_logged{false};
    const auto callback = LogInstance().PushBackCallback([&stall_logged](const std::string& line) {
        if (line.find("Rescanning made no progress") != std::string::npos) stall_logged = true;
    });
    struct CallbackRemover {
        decltype(callback) it;
        ~CallbackRemover() { LogInstance().DeleteCallback(it); }
    } const remove_callback{callback};

    constexpr auto interval = std::chrono::milliseconds{30};
    // Generous, so that a heartbeat thread that is late under load cannot fail the case.
    constexpr auto deadline = std::chrono::seconds{10};
    std::atomic<long long> height{0};
    std::atomic<bool> advancing{true};
    std::thread scanner([&] {
        while (advancing) {
            ++height;
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
    });
    // Stops and joins the scanner however the case ends: a joinable thread destroyed
    // during unwinding would end the whole run.
    struct ScannerJoiner {
        std::atomic<bool>& advancing;
        std::thread& thread;
        ~ScannerJoiner()
        {
            advancing = false;
            if (thread.joinable()) thread.join();
        }
    } const join_scanner{advancing, scanner};

    {
        sdnotify::ScopedStartupHeartbeat heartbeat("Rescanning", [&height] { return height.load(); }, interval);

        // Advancing: every beat extends, by three intervals.
        listener.SetTimeout(std::chrono::seconds{2});
        std::vector<long long> extensions;
        const auto give_up = std::chrono::steady_clock::now() + deadline;
        while (extensions.size() < 4 && std::chrono::steady_clock::now() < give_up) {
            const std::string message = listener.Receive();
            BOOST_REQUIRE_MESSAGE(!message.empty(), "no extension while the progress advanced");
            extensions.push_back(ExtensionUsec(message));
        }
        BOOST_REQUIRE_GE(extensions.size(), 4U);
        for (const long long usec : extensions) {
            BOOST_CHECK_EQUAL(usec, std::chrono::duration_cast<std::chrono::microseconds>(3 * interval).count());
        }
        BOOST_CHECK(!stall_logged);

        // Stalled: wait until a beat has seen the stall, then drain what was sent before it.
        // Nothing may follow while the progress stays still.
        advancing = false;
        scanner.join();
        const auto stall_deadline = std::chrono::steady_clock::now() + deadline;
        while (!stall_logged && std::chrono::steady_clock::now() < stall_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        BOOST_REQUIRE_MESSAGE(stall_logged, "the stall was not seen");
        listener.SetTimeout(std::chrono::milliseconds{1});
        while (!listener.Receive().empty()) {}
        listener.SetTimeout(5 * interval);
        BOOST_CHECK_EQUAL(listener.Receive(), "");

        // Advancing again: extended again.
        ++height;
        listener.SetTimeout(std::chrono::seconds{2});
        BOOST_CHECK(!listener.Receive().empty());
    }
}

#endif // __linux__

//! A conf file with daemon=1 and daemonwait=1 must lose to explicit -nodaemon
//! and -nodaemonwait on the command line.
BOOST_AUTO_TEST_CASE(command_line_nodaemon_overrides_conf)
{
    class TestArgsManager : public ArgsManager
    {
    public:
        using ArgsManager::ReadConfigStream;
    };

    TestArgsManager args;
    for (const char* a : {"-daemon", "-daemonwait"}) {
        args.AddArg(a, "", ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    }

    const char* argv[] = {"x", "-nodaemon", "-nodaemonwait"};
    std::string err;
    BOOST_REQUIRE(args.ParseParameters(3, argv, err));

    std::istringstream conf("daemon=1\ndaemonwait=1\n");
    BOOST_REQUIRE(args.ReadConfigStream(conf, "test.conf", err, true));

    BOOST_CHECK(!args.GetBoolArg("-daemon", true));
    BOOST_CHECK(!args.GetBoolArg("-daemonwait", true));
}

BOOST_AUTO_TEST_SUITE_END()
