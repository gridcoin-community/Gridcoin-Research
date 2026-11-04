// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <util/childreaper.h>

#ifndef WIN32

#include <logging.h>
#include <util/threadnames.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>

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

// Spawns path with argv, waits for it and returns its status, or -1 on failure.
int SpawnAndWait(const char* path, char* const argv[])
{
    pid_t pid = 0;
    int spawn_error = 0;
    {
        std::lock_guard<std::mutex> lock(g_spawn_mutex);
        spawn_error = posix_spawn(&pid, path, nullptr, nullptr, argv, environ);
        if (spawn_error == 0) g_active_spawns.insert(pid);
    }
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

    // The reaper may be blocked in waitid() on a child that is still running. A
    // child that ends at once wakes it, and the stop flag is then seen.
    char true_name[] = "true";
    char* const argv[] = {true_name, nullptr};
    if (SpawnAndWait("/bin/true", argv) == -1) {
        // Without a way to wake it the thread cannot be joined safely. It uses only
        // static state and ends at the next child event.
        LogPrintf("StopOrphanReaper error: could not wake the reaper thread, leaving it to end on its own\n");
        thread.detach();
        return;
    }
    thread.join();
}

} // namespace util

#endif // !WIN32
