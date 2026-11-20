// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifdef __linux__

#include "init.h"
#include "node/shutdown.h"
#include "test/state_guard.h"
#include "util/system.h"
#include "wallet/autounlock.h"
#include "wallet/autounlock_socket.h"

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
//! A mkdtemp directory under /tmp, which keeps the socket path within the sockaddr_un limit, removed at the end.
struct TempDir
{
    fs::path path;

    TempDir()
    {
        std::string tmpl = "/tmp/autounlocksock_XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        BOOST_REQUIRE(::mkdtemp(buf.data()) != nullptr);
        path = fs::path(buf.data());
    }

    ~TempDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

//! Requests a shutdown while it lives and clears the request however the case exits.
struct ShutdownForThisCase
{
    ShutdownForThisCase() { SetShutdownRequested(); }
    ~ShutdownForThisCase() { AbortShutdown(); }
};

//! The challenge the stand-in sends, CHALLENGE_BYTES long.
const std::string CHALLENGE{"challenge-012345"};

//! A helper stand-in. It binds the socket after bind_delay and accepts connections. To each one it sends the
//! challenge, waits up to two seconds for it to come back, and then writes the reply and closes the connection; an
//! answer that is missing or differs gets no reply. With an empty challenge it writes the reply at once. It stops
//! after max_connections or when asked to. With a drip interval it instead sends one byte per interval and never
//! closes, until the client goes away. It sets flags only, which the main thread checks, because Boost.Test
//! assertions are not safe on this thread.
class Listener
{
public:
    Listener(const fs::path& path, std::string reply, int max_connections,
             std::chrono::milliseconds bind_delay = std::chrono::milliseconds(0),
             std::chrono::milliseconds drip_interval = std::chrono::milliseconds(0), std::string challenge = CHALLENGE)
        : m_path(path), m_reply(std::move(reply)), m_max(max_connections), m_bind_delay(bind_delay),
          m_drip_interval(drip_interval), m_challenge(std::move(challenge))
    {
        if (bind_delay.count() == 0) {
            Bind();
        }

        m_thread = std::thread([this]() { Run(); });
    }

    ~Listener()
    {
        m_stop = true;
        m_thread.join();

        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }

    int Accepted() const { return m_accepted; }
    bool Bound() const { return m_bound; }
    //! The number of connections that sent the challenge back.
    int Answered() const { return m_answered; }

private:
    void Bind()
    {
        m_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (m_fd < 0) {
            return;
        }

        sockaddr_un address;
        std::memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, m_path.c_str(), m_path.native().size());
        m_bound = ::bind(m_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 && ::listen(m_fd, 8) == 0;
    }

    void Run()
    {
        if (m_bind_delay.count() > 0) {
            std::this_thread::sleep_for(m_bind_delay);
            Bind();
        }

        while (m_bound && !m_stop && m_accepted < m_max) {
            pollfd pfd{m_fd, POLLIN, 0};

            if (::poll(&pfd, 1, 20) <= 0) {
                continue;
            }

            const int conn = ::accept4(m_fd, nullptr, nullptr, SOCK_CLOEXEC);

            if (conn < 0) {
                continue;
            }

            ++m_accepted;

            if (m_drip_interval.count() == 0 && !m_challenge.empty()) {
                if (!Challenge(conn)) {
                    ::close(conn);
                    continue;
                }
                ++m_answered;
            }

            if (m_drip_interval.count() > 0) {
                while (!m_stop && ::send(conn, "x", 1, MSG_NOSIGNAL) == 1) {
                    std::this_thread::sleep_for(m_drip_interval);
                }
            } else if (!m_reply.empty()) {
                size_t sent = 0;

                while (sent < m_reply.size()) {
                    const ssize_t n = ::send(conn, m_reply.data() + sent, m_reply.size() - sent, MSG_NOSIGNAL);

                    if (n <= 0) {
                        break;
                    }

                    sent += static_cast<size_t>(n);
                }
            }

            ::close(conn);
        }
    }

    //! Sends the challenge and reads the answer: true when the same bytes came back within two seconds.
    bool Challenge(int conn)
    {
        const ssize_t sent = ::send(conn, m_challenge.data(), m_challenge.size(), MSG_NOSIGNAL);

        if (sent != static_cast<ssize_t>(m_challenge.size())) {
            return false;
        }

        std::string answer;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);

        while (answer.size() < m_challenge.size() && std::chrono::steady_clock::now() < deadline) {
            pollfd pfd{conn, POLLIN, 0};

            if (::poll(&pfd, 1, 20) <= 0) {
                continue;
            }

            char buffer[64];
            const ssize_t n = ::recv(conn, buffer, std::min(sizeof(buffer), m_challenge.size() - answer.size()), 0);

            if (n <= 0) {
                return false;
            }

            answer.append(buffer, static_cast<size_t>(n));
        }

        return answer == m_challenge;
    }

    fs::path m_path;
    std::string m_reply;
    int m_max;
    std::chrono::milliseconds m_bind_delay;
    std::chrono::milliseconds m_drip_interval;
    std::string m_challenge;
    int m_fd{-1};
    std::atomic<bool> m_bound{false};
    std::atomic<int> m_accepted{0};
    std::atomic<int> m_answered{0};
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
};

autounlock::SocketFetchParams QuickParams()
{
    autounlock::SocketFetchParams params;
    params.first_delay = std::chrono::milliseconds(10);
    params.receive_timeout = std::chrono::seconds(1);
    return params;
}
} // namespace

BOOST_AUTO_TEST_SUITE(autounlock_socket_tests)

BOOST_AUTO_TEST_CASE(fetch_reads_passphrase_to_eof)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "secret", 1);
    BOOST_REQUIRE(listener.Bound());

    const auto value = autounlock::FetchFromSocket(p, QuickParams());
    BOOST_REQUIRE(value.has_value());
    BOOST_CHECK(*value == "secret");
    BOOST_CHECK_EQUAL(listener.Answered(), 1);
}

BOOST_AUTO_TEST_CASE(challenge_is_the_documented_length)
{
    BOOST_CHECK_EQUAL(CHALLENGE.size(), autounlock::CHALLENGE_BYTES);
}

BOOST_AUTO_TEST_CASE(helper_that_closes_before_its_challenge_is_a_refusal)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "", 100, std::chrono::milliseconds(0), std::chrono::milliseconds(0), /*challenge=*/"");

    BOOST_CHECK(!autounlock::FetchFromSocket(p, QuickParams()).has_value());
    BOOST_CHECK_EQUAL(listener.Accepted(), 5);
}

BOOST_AUTO_TEST_CASE(connected_fd_is_cloexec)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "secret", 1);
    BOOST_REQUIRE(listener.Bound());

    std::string err;
    const int fd = autounlock::ConnectAutoUnlockSocket(p, err);
    BOOST_REQUIRE_MESSAGE(fd >= 0, err);
    BOOST_CHECK(::fcntl(fd, F_GETFD) & FD_CLOEXEC);
    ::close(fd);
}

BOOST_AUTO_TEST_CASE(zero_bytes_is_refusal)
{
    // The challenge is answered, and the helper then closes without a passphrase.
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "", 100);

    BOOST_CHECK(!autounlock::FetchFromSocket(p, QuickParams()).has_value());
    BOOST_CHECK_EQUAL(listener.Accepted(), 5);
    BOOST_CHECK_EQUAL(listener.Answered(), 5);
}

BOOST_AUTO_TEST_CASE(oversize_reply_refused)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, std::string(1025, 'x'), 100);

    BOOST_CHECK(!autounlock::FetchFromSocket(p, QuickParams()).has_value());
    BOOST_CHECK_EQUAL(listener.Accepted(), 5);
}

BOOST_AUTO_TEST_CASE(relative_path_refused)
{
    TempDir dir;
    Listener listener(dir.path / "au.sock", "secret", 100);

    BOOST_CHECK(!autounlock::FetchFromSocket("au.sock", QuickParams()).has_value());
    BOOST_CHECK_EQUAL(listener.Accepted(), 0);
}

BOOST_AUTO_TEST_CASE(fetch_succeeds_when_socket_appears_late)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";

    // The socket is bound only after 200 ms. The direct connect below asserts that nothing is there before that
    // (connect fails with errno 2, ENOENT), and the waits between attempts (50, 100, 200 ms) outlast the bind delay,
    // so the value can arrive only through a retry.
    autounlock::SocketFetchParams params = QuickParams();
    params.first_delay = std::chrono::milliseconds(50);

    std::string err;
    BOOST_CHECK_EQUAL(autounlock::ConnectAutoUnlockSocket(p, err), -1);
    BOOST_CHECK(err.find("connect failed") == 0);
    BOOST_CHECK(err.find("(2)") != std::string::npos || err.find("No such file") != std::string::npos);

    Listener listener(p, "secret", 1, std::chrono::milliseconds(200));

    const auto value = autounlock::FetchFromSocket(p, params);
    BOOST_CHECK(listener.Bound());
    BOOST_CHECK_EQUAL(listener.Accepted(), 1);
    BOOST_REQUIRE(value.has_value());
    BOOST_CHECK(*value == "secret");
}

BOOST_AUTO_TEST_CASE(slow_drip_cannot_extend_the_deadline)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "", 100, std::chrono::milliseconds(0), std::chrono::milliseconds(300));

    autounlock::SocketFetchParams params = QuickParams();
    params.attempts = 2;

    const auto start = std::chrono::steady_clock::now();
    const auto value = autounlock::FetchFromSocket(p, params);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    BOOST_CHECK(listener.Bound());
    BOOST_CHECK(!value.has_value());
    BOOST_CHECK_EQUAL(listener.Accepted(), 2);
    // two attempts of 1 s each and one 10 ms wait, with room for a slow machine
    BOOST_CHECK(elapsed < std::chrono::seconds(4));
    BOOST_CHECK(elapsed >= std::chrono::seconds(2));
}

BOOST_AUTO_TEST_CASE(dispatcher_uses_the_socket_option)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "secret", 100);

    // without the option there is no backend
    BOOST_CHECK(!FetchAutoUnlockPassphrase());
    BOOST_CHECK_EQUAL(listener.Accepted(), 0);

    {
        grc_test::ForcedArgGuard guard("autounlocksocket", p.string());
        const auto value = FetchAutoUnlockPassphrase();
        BOOST_REQUIRE(value.has_value());
        BOOST_CHECK(*value == "secret");
    }

    BOOST_CHECK(!FetchAutoUnlockPassphrase());
}

BOOST_AUTO_TEST_CASE(path_too_long_refused)
{
    TempDir dir;
    Listener listener(dir.path / "au.sock", "secret", 100);

    const fs::path longpath = fs::path("/tmp") / std::string(sizeof(sockaddr_un::sun_path), 'a');

    std::string err;
    BOOST_CHECK_EQUAL(autounlock::ConnectAutoUnlockSocket(longpath, err), -1);
    BOOST_CHECK_EQUAL(err, "path too long");
    BOOST_CHECK(!autounlock::FetchFromSocket(longpath, QuickParams()).has_value());
    BOOST_CHECK_EQUAL(listener.Accepted(), 0);
}

BOOST_AUTO_TEST_CASE(shutdown_makes_no_attempt)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "secret", 100);

    {
        ShutdownForThisCase shutdown;
        BOOST_CHECK(!autounlock::FetchFromSocket(p, QuickParams()).has_value());
    }

    BOOST_CHECK_EQUAL(listener.Accepted(), 0);
}

BOOST_AUTO_TEST_CASE(shutdown_during_the_read_ends_the_fetch)
{
    TempDir dir;
    const fs::path p = dir.path / "au.sock";
    Listener listener(p, "", 100, std::chrono::milliseconds(0), std::chrono::milliseconds(300));

    // a long receive timeout, so that only the shutdown can end the read quickly
    autounlock::SocketFetchParams params = QuickParams();
    params.receive_timeout = std::chrono::seconds(30);

    std::thread requester([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        SetShutdownRequested();
    });

    const auto start = std::chrono::steady_clock::now();
    const auto value = autounlock::FetchFromSocket(p, params);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    requester.join();
    AbortShutdown();

    BOOST_CHECK(!value.has_value());
    // no further attempt after the shutdown
    BOOST_CHECK_EQUAL(listener.Accepted(), 1);
    BOOST_CHECK(elapsed < std::chrono::seconds(3));
}

BOOST_AUTO_TEST_CASE(option_is_command_line_only)
{
    gArgs.ClearArgs();
    struct ClearOnExit { ~ClearOnExit() { gArgs.ClearArgs(); } } clear_on_exit;
    SetupServerArgs();

    const auto flags = gArgs.GetRegisteredArgFlags();
    BOOST_REQUIRE(flags.count("-autounlocksocket"));
    BOOST_CHECK(flags.at("-autounlocksocket") & ArgsManager::COMMAND_LINE_ONLY);

    // An ArgsManager with the registered flags ignores the key in a configuration file.
    struct Args : ArgsManager
    {
        using ArgsManager::ReadConfigStream;
    } args;
    args.AddArg("-autounlocksocket=<path>", "", flags.at("-autounlocksocket"), OptionsCategory::OPTIONS);

    std::istringstream conf("autounlocksocket=/x\n");
    std::string error;
    BOOST_REQUIRE(args.ReadConfigStream(conf, "", error));
    BOOST_CHECK(!args.IsArgSet("-autounlocksocket"));
}

BOOST_AUTO_TEST_SUITE_END()

#endif // __linux__
