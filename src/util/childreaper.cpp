// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/childreaper.h>

#ifndef WIN32

#include <config/gridcoin-config.h>
#include <logging.h>
#include <util/threadnames.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <initializer_list>
#include <mutex>
#include <set>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace util {

namespace {
// Leaf mutex. Guards g_active_spawns, and is held across posix_spawn and the
// insertion that follows it, never across a wait for a spawned process.
std::mutex g_spawn_mutex;
// Processes whose spawner waits for them, so the reaper leaves them alone.
std::set<pid_t> g_active_spawns;

// Leaf mutex for the reaper thread state and its sleeps. Never held together with
// g_spawn_mutex.
std::mutex g_reaper_mutex;
std::condition_variable g_reaper_cv;
std::thread g_reaper_thread;
std::atomic<bool> g_reaper_stop{false};

// The first nonzero result of the calls that set up a spawn, or 0.
int FirstError(std::initializer_list<int> results)
{
    for (const int result : results) {
        if (result != 0) return result;
    }
    return 0;
}

// Spawns path with argv, waits for it and returns its status, or -1 on failure.
int SpawnAndWait(const char* path, char* const argv[])
{
    // The child starts with SIGPIPE at its default action and no signal blocked,
    // whatever this process has: the daemon ignores SIGPIPE, and a pipeline in a
    // notify command must end as it would from a terminal.
    posix_spawnattr_t attr;
    int spawn_error = posix_spawnattr_init(&attr);
    if (spawn_error != 0) {
        LogPrintf("RunShellCommand error: posix_spawnattr_init failed with errno %d\n", spawn_error);
        return -1;
    }
    sigset_t defaults;
    sigset_t mask;
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE);
    sigemptyset(&mask);
    // posix_spawnattr_setflags takes a short; the flags fit.
    short flags = static_cast<short>(POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_t* used_actions = nullptr;
#if HAVE_POSIX_SPAWN_CLOSEFROM
    // The shell gets none of the descriptors the core holds open.
    spawn_error = posix_spawn_file_actions_init(&actions);
    if (spawn_error == 0) {
        used_actions = &actions;
        spawn_error = posix_spawn_file_actions_addclosefrom_np(&actions, 3);
    }
#elif defined(__APPLE__) && defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
    // Every descriptor not inherited explicitly is closed in the shell, so it gets its
    // standard input, output and error and nothing the core holds open. Only those of
    // the three that are open are named: naming a closed one makes posix_spawn fail
    // with EBADF.
    flags = static_cast<short>(flags | POSIX_SPAWN_CLOEXEC_DEFAULT);
    spawn_error = posix_spawn_file_actions_init(&actions);
    if (spawn_error == 0) {
        used_actions = &actions;
        for (const int fd : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
            if (spawn_error == 0 && ::fcntl(fd, F_GETFD) != -1) {
                spawn_error = posix_spawn_file_actions_addinherit_np(&actions, fd);
            }
        }
    }
#else
    // Where neither closefrom nor POSIX_SPAWN_CLOEXEC_DEFAULT is available (glibc
    // before 2.34, for one) the shell inherits every descriptor that is not
    // close-on-exec, as with system(). The core opens its sockets close-on-exec; a
    // file opened without it reaches the shell.
#endif
    if (spawn_error == 0) {
        spawn_error = FirstError({posix_spawnattr_setsigdefault(&attr, &defaults),
                                  posix_spawnattr_setsigmask(&attr, &mask), posix_spawnattr_setflags(&attr, flags)});
    }

    pid_t pid = 0;
    if (spawn_error == 0) {
        std::lock_guard<std::mutex> lock(g_spawn_mutex);
        spawn_error = posix_spawn(&pid, path, used_actions, &attr, argv, environ);
        if (spawn_error == 0) g_active_spawns.insert(pid);
    }
    if (used_actions != nullptr) posix_spawn_file_actions_destroy(used_actions);
    posix_spawnattr_destroy(&attr);

    if (spawn_error != 0) {
        LogPrintf("RunShellCommand error: posix_spawn(%s) failed with errno %d\n", path, spawn_error);
        return -1;
    }

    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    const int wait_errno = errno;

    {
        std::lock_guard<std::mutex> lock(g_spawn_mutex);
        g_active_spawns.erase(pid);
    }

    if (waited != pid) {
        LogPrintf("RunShellCommand error: waitpid(%d) failed with errno %d\n", static_cast<int>(pid), wait_errno);
        return -1;
    }
    return status;
}

// Sleeps for the duration, or until the reaper is asked to stop.
void ReaperSleep(std::chrono::milliseconds duration)
{
    std::unique_lock<std::mutex> lock(g_reaper_mutex);
    g_reaper_cv.wait_for(lock, duration, [] { return g_reaper_stop.load(); });
}

void ReaperLoop()
{
    util::ThreadRename("childreaper");

    while (!g_reaper_stop) {
        // Wait for any child to become reapable without reaping it.
        siginfo_t info;
        info.si_pid = 0;
        if (waitid(P_ALL, 0, &info, WEXITED | WNOWAIT) != 0) {
            // ECHILD when there are no children at all, or an interruption.
            if (errno != EINTR) ReaperSleep(std::chrono::seconds(1));
            continue;
        }
        if (info.si_pid == 0) continue;

        std::unique_lock<std::mutex> lock(g_spawn_mutex);
        if (g_active_spawns.count(info.si_pid) != 0) {
            // Its spawner reaps it.
            lock.unlock();
            ReaperSleep(std::chrono::milliseconds(10));
            continue;
        }
        waitpid(info.si_pid, nullptr, WNOHANG);
    }
}
} // namespace

int RunShellCommand(const std::string& command)
{
    char sh[] = "sh";
    char dash_c[] = "-c";
    // posix_spawn takes a non-const argv but does not modify the strings.
    char* const argv[] = {sh, dash_c, const_cast<char*>(command.c_str()), nullptr};
    return SpawnAndWait("/bin/sh", argv);
}

void StartOrphanReaper(bool force)
{
    if (!force && getpid() != 1) return;

    std::lock_guard<std::mutex> lock(g_reaper_mutex);
    if (g_reaper_thread.joinable()) return;

    g_reaper_stop = false;
    g_reaper_thread = std::thread(ReaperLoop);
}

void StopOrphanReaper()
{
    std::thread thread;
    {
        std::lock_guard<std::mutex> lock(g_reaper_mutex);
        if (!g_reaper_thread.joinable()) return;
        g_reaper_stop = true;
        thread = std::move(g_reaper_thread);
    }
    g_reaper_cv.notify_all();

    // The reaper may be blocked in waitid() while a child is still running. A
    // short-lived child that nobody waits for wakes it: the reaper sees an
    // unregistered zombie, reaps it, loops and sees the stop flag. It must not be
    // registered or waited for here, or this thread could take the exit the reaper
    // is waiting for.
    char true_name[] = "true";
    char* const argv[] = {true_name, nullptr};
    pid_t wake_pid = 0;
    const int spawn_error = posix_spawn(&wake_pid, "/bin/true", nullptr, nullptr, argv, environ);
    if (spawn_error != 0) {
        // Without a way to wake it the thread cannot be joined safely. It uses only
        // static state and ends at the next child event.
        LogPrintf("StopOrphanReaper error: posix_spawn(/bin/true) failed with errno %d, leaving the reaper thread to end on its own\n", spawn_error);
        thread.detach();
        return;
    }

    // The reaper may end before it reaps the wake child, so wait until the child has
    // ended, without reaping it, and take it after the join if it is still there.
    // The wait returns at once with ECHILD when the reaper already took it.
    siginfo_t info;
    while (waitid(P_PID, static_cast<id_t>(wake_pid), &info, WEXITED | WNOWAIT) != 0 && errno == EINTR) {}

    thread.join();

    std::lock_guard<std::mutex> lock(g_spawn_mutex);
    if (g_active_spawns.count(wake_pid) == 0) waitpid(wake_pid, nullptr, WNOHANG);
}

} // namespace util

#endif // !WIN32
