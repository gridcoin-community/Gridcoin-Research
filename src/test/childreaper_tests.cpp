// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#ifdef __linux__

#include <config/gridcoin-config.h>
#include <test/umask_guard.h>
#include <tinyformat.h>
#include <util/childreaper.h>
#include <util/dir_permissions.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
//! Whether this process can be made a child subreaper: reading the setting and
//! setting it to the value it has changes nothing. Older qemu-user refuses
//! PR_SET_CHILD_SUBREAPER; Linux has had it since 3.4.
bool CanBeSubreaper()
{
    int current = 0;
    if (prctl(PR_GET_CHILD_SUBREAPER, &current, 0, 0, 0) != 0) return false;
    return prctl(PR_SET_CHILD_SUBREAPER, current, 0, 0, 0) == 0;
}

//! Makes this process the reaper of its orphaned descendants and runs the orphan
//! reaper, and on every exit path stops the reaper and restores the previous
//! subreaper setting.
class ReaperGuard
{
public:
    ReaperGuard()
    {
        int old = 0;
        if (prctl(PR_GET_CHILD_SUBREAPER, &old, 0, 0, 0) == 0) m_old_subreaper = old;
        m_subreaper_set = prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) == 0;
        util::StartOrphanReaper(true);
    }

    ~ReaperGuard()
    {
        util::StopOrphanReaper();
        if (m_subreaper_set) prctl(PR_SET_CHILD_SUBREAPER, m_old_subreaper, 0, 0, 0);
    }

    bool SubreaperSet() const { return m_subreaper_set; }

private:
    int m_old_subreaper = 0;
    bool m_subreaper_set = false;
};

//! The pids of all children of this process, from every thread's children file.
std::vector<long> ChildPids()
{
    std::vector<long> pids;
    DIR* dir = opendir("/proc/self/task");
    if (dir == nullptr) return pids;
    std::vector<std::string> tids;
    while (const dirent* entry = readdir(dir)) {
        if (entry->d_name[0] != '.') tids.emplace_back(entry->d_name);
    }
    closedir(dir);

    for (const std::string& tid : tids) {
        std::ifstream children("/proc/self/task/" + tid + "/children");
        long pid = 0;
        while (children >> pid) pids.push_back(pid);
    }
    return pids;
}

//! The process state letter from /proc/<pid>/stat, or '?' when it cannot be read.
char ProcessState(long pid)
{
    std::ifstream stat(strprintf("/proc/%ld/stat", pid));
    std::string line;
    std::getline(stat, line);
    // The command name is in parentheses and may contain anything, so the state
    // follows the last closing parenthesis.
    const size_t close = line.rfind(')');
    if (close == std::string::npos || close + 2 >= line.size()) return '?';
    return line[close + 2];
}

//! Kills and reaps every child that was not present at construction, on every exit
//! path, so that a case never leaves a process behind.
class ChildCleanup
{
public:
    ChildCleanup() : m_baseline(ChildPids()) {}

    ~ChildCleanup()
    {
        for (const long pid : ChildPids()) {
            if (std::find(m_baseline.begin(), m_baseline.end(), pid) != m_baseline.end()) continue;
            kill(static_cast<pid_t>(pid), SIGKILL);
            waitpid(static_cast<pid_t>(pid), nullptr, 0);
        }
    }

private:
    std::vector<long> m_baseline;
};

bool AnyZombieChild()
{
    for (const long pid : ChildPids()) {
        if (ProcessState(pid) == 'Z') return true;
    }
    return false;
}
} // namespace

BOOST_AUTO_TEST_SUITE(childreaper_tests)

BOOST_AUTO_TEST_CASE(run_shell_command_returns_exit_status)
{
    const int status = util::RunShellCommand("exit 3");
    BOOST_REQUIRE(status != -1);
    BOOST_CHECK(WIFEXITED(status));
    BOOST_CHECK_EQUAL(WEXITSTATUS(status), 3);

    const int ok = util::RunShellCommand("true");
    BOOST_CHECK(WIFEXITED(ok) && WEXITSTATUS(ok) == 0);

    const int killed = util::RunShellCommand("kill -9 $$");
    BOOST_REQUIRE(killed != -1);
    BOOST_CHECK(WIFSIGNALED(killed));
    BOOST_CHECK_EQUAL(WTERMSIG(killed), 9);
}

// The node runs with an owner-only umask (util::SetOwnerOnlyUmask). A command it
// runs for the user, as the *notify options do, gets the umask the process
// started with back, so a script makes its files as it would when run by hand.
BOOST_AUTO_TEST_CASE(a_shell_command_gets_the_umask_the_process_started_with)
{
    const grc_test::UmaskGuard started_with{022};
    util::SetOwnerOnlyUmask();

    const int status = util::RunShellCommand("[ \"$(umask)\" = 0022 ]");
    BOOST_REQUIRE(status != -1);
    BOOST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

BOOST_AUTO_TEST_CASE(stop_without_start_and_repeated_calls_are_safe)
{
    // Compared with the children present now, so a child left by another suite
    // does not matter.
    const std::vector<long> baseline = ChildPids();

    util::StopOrphanReaper();
    util::StopOrphanReaper();

    // Not running as init and not forced, so nothing starts and stop is still safe.
    util::StartOrphanReaper();
    util::StopOrphanReaper();

    util::StartOrphanReaper(true);
    util::StartOrphanReaper(true);
    util::StopOrphanReaper();
    util::StopOrphanReaper();

    // The reaper can be started again after a stop.
    util::StartOrphanReaper(true);
    util::StopOrphanReaper();
    BOOST_CHECK(ChildPids() == baseline);
}

BOOST_AUTO_TEST_CASE(orphans_are_reaped)
{
    if (!CanBeSubreaper()) {
        BOOST_TEST_MESSAGE("this process cannot be made a child subreaper (older qemu-user); skipping");
        return;
    }
    ReaperGuard guard;
    BOOST_REQUIRE(guard.SubreaperSet());

    // The shell exits at once and the background sleep is orphaned to this process.
    const int status = util::RunShellCommand("sleep 0.2 >/dev/null 2>&1 &");
    BOOST_REQUIRE(status != -1);
    BOOST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    // Give the sleep time to end and the reaper time to run, bounded for a loaded machine.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!ChildPids().empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    BOOST_CHECK(!AnyZombieChild());
    BOOST_CHECK(ChildPids().empty());
}

BOOST_AUTO_TEST_CASE(spawned_status_not_stolen)
{
    ReaperGuard guard;

    for (int i = 0; i < 50; ++i) {
        const int status = util::RunShellCommand("exit 7");
        BOOST_REQUIRE_MESSAGE(status != -1, "call " << i << " failed to spawn");
        BOOST_REQUIRE_MESSAGE(WIFEXITED(status) && WEXITSTATUS(status) == 7, "call " << i << " returned status " << status);
    }

    // The same from several threads at once, which is how the notify commands run.
    // Failures are counted and checked after the join, since Boost.Test assertions
    // are not thread safe.
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&failures] {
            for (int i = 0; i < 200; ++i) {
                const int status = util::RunShellCommand("exit 7");
                if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 7) ++failures;
            }
        });
    }
    for (std::thread& thread : threads) thread.join();
    BOOST_CHECK_EQUAL(failures.load(), 0);
}

BOOST_AUTO_TEST_CASE(stop_returns_with_a_live_orphan)
{
    ChildCleanup cleanup;
    if (!CanBeSubreaper()) {
        BOOST_TEST_MESSAGE("this process cannot be made a child subreaper (older qemu-user); skipping");
        return;
    }
    ReaperGuard guard;
    BOOST_REQUIRE(guard.SubreaperSet());

    // The shell exits at once and the sleep stays alive as an orphan of this process,
    // so the reaper has a live child and nothing else to wake it.
    const int status = util::RunShellCommand("sleep 5 >/dev/null 2>&1 &");
    BOOST_REQUIRE(status != -1);

    const auto start = std::chrono::steady_clock::now();
    util::StopOrphanReaper();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    BOOST_CHECK_MESSAGE(elapsed < std::chrono::seconds(3), "stop took longer than the live orphan's remaining run time");
}

BOOST_AUTO_TEST_CASE(spawned_shell_does_not_inherit_descriptors)
{
#if HAVE_POSIX_SPAWN_CLOSEFROM
    // Opened without close-on-exec, as the core's own sockets and files may be.
    const int fd = open("/dev/null", O_RDONLY);
    BOOST_REQUIRE(fd >= 3);
    const int status = util::RunShellCommand(strprintf("test ! -e /proc/self/fd/%d", fd));
    close(fd);
    BOOST_REQUIRE(status != -1);
    BOOST_CHECK(WIFEXITED(status));
    BOOST_CHECK_EQUAL(WEXITSTATUS(status), 0);
#else
    BOOST_TEST_MESSAGE("posix_spawn_file_actions_addclosefrom_np is not available, case skipped");
#endif
}

// The shell starts with SIGPIPE at its default action and no signal blocked, whatever
// the core has: the daemon ignores SIGPIPE, and a notify command's pipeline must see
// the default behaviour.
BOOST_AUTO_TEST_CASE(spawned_shell_has_default_sigpipe_and_an_empty_mask)
{
    // Puts back SIGPIPE's disposition and this thread's signal mask however the case ends.
    struct SignalStateGuard {
        struct sigaction pipe_action{};
        sigset_t mask{};
        SignalStateGuard()
        {
            sigaction(SIGPIPE, nullptr, &pipe_action);
            pthread_sigmask(SIG_SETMASK, nullptr, &mask);
        }
        ~SignalStateGuard()
        {
            pthread_sigmask(SIG_SETMASK, &mask, nullptr);
            sigaction(SIGPIPE, &pipe_action, nullptr);
        }
    } const restore;

    struct sigaction ignore;
    memset(&ignore, 0, sizeof(ignore));
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    BOOST_REQUIRE_EQUAL(sigaction(SIGPIPE, &ignore, nullptr), 0);

    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR2);
    BOOST_REQUIRE_EQUAL(pthread_sigmask(SIG_BLOCK, &block, nullptr), 0);

    // Exit status 0 only when the shell does not ignore SIGPIPE (bit 12 of SigIgn) and
    // does not block SIGUSR2 (bit 11 of SigBlk).
    const int status = util::RunShellCommand(
        "ign=$(sed -n 's/^SigIgn:[[:space:]]*//p' /proc/$$/status); "
        "blk=$(sed -n 's/^SigBlk:[[:space:]]*//p' /proc/$$/status); "
        "[ $(( 0x$ign & 0x1000 )) -eq 0 ] && [ $(( 0x$blk & 0x800 )) -eq 0 ]");

    BOOST_REQUIRE(status != -1);
    BOOST_CHECK(WIFEXITED(status));
    BOOST_CHECK_EQUAL(WEXITSTATUS(status), 0);
}

BOOST_AUTO_TEST_SUITE_END()

#else

BOOST_AUTO_TEST_SUITE(childreaper_tests)
BOOST_AUTO_TEST_CASE(linux_only) {}
BOOST_AUTO_TEST_SUITE_END()

#endif // __linux__
