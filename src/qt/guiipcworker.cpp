// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "qt/guiipcworker.h"

#include "util.h" /* for LogPrintf */
#include "util/threadnames.h"

#include <atomic>
#include <exception>
#include <optional>
#include <system_error>
#include <utility>

namespace {
//! The process's workers, by role: GuiIpcWorker::Current().
std::atomic<GuiIpcWorker*> g_modal_worker{nullptr};
std::atomic<GuiIpcWorker*> g_coin_load_worker{nullptr};

std::atomic<GuiIpcWorker*>& Registered(GuiIpcWorker::Role role)
{
    return role == GuiIpcWorker::Role::Modal ? g_modal_worker : g_coin_load_worker;
}

const char* RoleName(GuiIpcWorker::Role role)
{
    return role == GuiIpcWorker::Role::Modal ? "modal" : "coin load";
}

//! At most 15 characters, the most a thread name may have on Linux.
const char* ThreadName(GuiIpcWorker::Role role)
{
    return role == GuiIpcWorker::Role::Modal ? "gui-ipc-modal" : "gui-ipc-coins";
}
} // namespace

GuiIpcWorker::GuiIpcWorker(Role role) : m_role(role)
{
    GuiIpcWorker* none = nullptr;

    if (!Registered(m_role).compare_exchange_strong(none, this)) {
        LogPrintf("ERROR: %s: a %s worker exists already; this one is not used", __func__, RoleName(m_role));
    }
}

GuiIpcWorker::~GuiIpcWorker()
{
    GuiIpcWorker* self = this;
    Registered(m_role).compare_exchange_strong(self, nullptr);

    std::thread thread;

    {
        LOCK(m_mutex);
        m_stop = true;
        thread = std::move(m_thread);
    }

    m_cv.notify_all();

    if (thread.joinable()) thread.join();
}

GuiIpcWorker* GuiIpcWorker::Current(Role role)
{
    return Registered(role).load();
}

std::future<void> GuiIpcWorker::Post(std::function<void()> job)
{
    Job entry{std::move(job), {}};
    std::future<void> done = entry.done.get_future();

    LOCK(m_mutex);

    if (m_stop) return {};

    if (!m_thread.joinable()) {
        try {
            m_thread = std::thread([this] { Run(); });
        } catch (const std::system_error& e) {
            LogPrintf("ERROR: %s: could not start the %s worker's thread: %s", __func__, RoleName(m_role), e.what());
            return {};
        }
    }

    m_jobs.push_back(std::move(entry));
    m_cv.notify_one();

    return done;
}

void GuiIpcWorker::Run()
{
    util::ThreadRename(ThreadName(m_role));

    for (;;) {
        std::optional<Job> job;

        {
            WAIT_LOCK(m_mutex, lock);
            m_cv.wait(lock, [this]() EXCLUSIVE_LOCKS_REQUIRED(m_mutex) { return m_stop || !m_jobs.empty(); });

            // Stopped, with nothing left to run.
            if (m_jobs.empty()) return;

            job.emplace(std::move(m_jobs.front()));
            m_jobs.pop_front();
        }

        // Nothing may escape the thread.
        try {
            job->run();
        } catch (const std::exception& e) {
            LogPrintf("ERROR: %s: a job on the %s worker failed: %s", __func__, RoleName(m_role), e.what());
        } catch (...) {
            LogPrintf("ERROR: %s: a job on the %s worker failed", __func__, RoleName(m_role));
        }

        // Destroyed outside the mutex, and before its future is made ready, so that what it
        // captured is gone by then.
        job->run = nullptr;
        job->done.set_value();
    }
}
